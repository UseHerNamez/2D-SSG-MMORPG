// CustomGameInstanceSubsystem.cpp
#include "CustomGameInstanceSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMisc.h"
#include "CustomPlayerState.h"
#if WITH_SERVER_CODE
#include "DatabaseConnectionPool.h"
#endif

void UCustomGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Server only - skip on pure clients
    const UWorld* World = GetWorld();
    const bool bIsServerProcess = IsRunningDedicatedServer();

    if (!bIsServerProcess)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Persistence subsystem not initialized on client"));
        return;
    }

#if WITH_SERVER_CODE
    // Only initialize database if we're actually on a server
    if (bIsServerProcess)
    {
        try
        {
            if (!InitDbPool())
            {
                UE_LOG(LogTemp, Error, TEXT("Persistence subsystem failed to initialize DB pool"));
                return;
            }
        }
        catch (const std::exception& e)
        {
            UE_LOG(LogTemp, Error, TEXT("Persistence subsystem DB initialization failed: %s"), *FString(e.what()));
            return;
        }
        catch (...)
        {
            UE_LOG(LogTemp, Error, TEXT("Persistence subsystem DB initialization failed with unknown exception"));
            return;
        }

        // Start the write queue thread only if DB pool is valid
        if (DbPool.IsValid())
        {
            bStopWriterThread = false;
            WriterThread = std::thread(&UCustomGameInstanceSubsystem::QueueWorker, this);
            UE_LOG(LogTemp, Log, TEXT("Persistence subsystem write queue started"));
        }
    }
#endif
}

void UCustomGameInstanceSubsystem::Deinitialize()
{
    bStopWriterThread = true;
    WriteQueueCv.notify_all();
    if (WriterThread.joinable())
        WriterThread.join();

#if WITH_SERVER_CODE
    DbPool.Reset();
#endif

    Super::Deinitialize();
}


// Test Mode Functions
void UCustomGameInstanceSubsystem::SetTestMode(bool bEnabled)
{
    bTestMode = bEnabled;
    UE_LOG(LogTemp, Log, TEXT("Test Mode: %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
    
    if (bEnabled)
    {
        UE_LOG(LogTemp, Log, TEXT("Test mode enabled - database operations will be skipped"));
    }
}

void UCustomGameInstanceSubsystem::PopulateTestData(ACustomPlayerState* PlayerState)
{
    if (!PlayerState)
    {
        UE_LOG(LogTemp, Error, TEXT("PopulateTestData: PlayerState is null"));
        return;
    }

    if (!IsValid(PlayerState))
    {
        UE_LOG(LogTemp, Error, TEXT("PopulateTestData: PlayerState is not valid"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Populating test data for player"));

    // Set Identity (Name, Gender) - direct assignment for test
    PlayerState->Identity.Name = TEXT("TestPlayer");
    PlayerState->Identity.Gender = 0; // Female

    // Set Progression (XP, Level, AP) - direct assignment for test
    PlayerState->Progression.Level = 5;
    PlayerState->Progression.XP = 30;
    PlayerState->Progression.UnspentAP = 3;

    // Set Base Stats - direct assignment for test
    PlayerState->BaseStats.Str = 15;
    PlayerState->BaseStats.Dex = 12;
    PlayerState->BaseStats.Wis = 10;
    PlayerState->BaseStats.Luk = 8;
    PlayerState->BaseStats.Pur = 14;
    PlayerState->BaseStats.Vic = 11;

    // Set Public Inspect (what others see) - direct assignment for test
    PlayerState->PublicInspect.Level = PlayerState->Progression.Level;
    PlayerState->PublicInspect.BaseStats = PlayerState->BaseStats;
    PlayerState->PublicInspect.Appearance = TEXT("hair:0-0,face:0-0,Weapon:TestSword");

    // Force replication update
    PlayerState->ForceNetUpdate();

    // Set Server-only data
    FCharacterInitData_Server ServerData;
    ServerData.UserId = 999;
    ServerData.CharId = 123;
    PlayerState->SetServerOnlyData(ServerData);

    // Notify that initial data is loaded
    PlayerState->NotifyInitialDataLoaded_ServerOnly();

    UE_LOG(LogTemp, Log, TEXT("Test data populated successfully"));
}

#if WITH_SERVER_CODE
bool UCustomGameInstanceSubsystem::InitDbPool()
{
    try
    {
        // Read encryption key from env - safer UE API
        const FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("ENCRYPTION_KEY"));
        if (Key.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("ENCRYPTION_KEY missing - database features disabled"));
            return false;
        }

        // Read optional pool size from config - default 16
        GConfig->GetInt(TEXT("/Script/Engine.GameInstance"), TEXT("DbPoolSize"), PoolSize, GGameIni);

        const FString EncCfg = FPaths::Combine(FPaths::ProjectDir(), TEXT("Config/config.ini.encrypted"));
        
        // Check if config file exists
        if (!FPaths::FileExists(EncCfg))
        {
            UE_LOG(LogTemp, Warning, TEXT("Database config file not found: %s - database features disabled"), *EncCfg);
            return false;
        }

        const std::string ConfigPath = TCHAR_TO_UTF8(*EncCfg);
        const std::string EncKey = std::string(TCHAR_TO_UTF8(*Key));

        // Create the database pool with error handling
        DbPool = MakeShared<DatabaseConnectionPool>(ConfigPath, EncKey, PoolSize, WriterConnections);
        
        if (!DbPool.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create database pool"));
            return false;
        }

        UE_LOG(LogTemp, Log, TEXT("DB pool initialized in GameInstance subsystem (size=%d)"), PoolSize);
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogTemp, Error, TEXT("Database pool initialization failed: %s"), *FString(e.what()));
        DbPool.Reset();
        return false;
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("Database pool initialization failed with unknown exception"));
        DbPool.Reset();
        return false;
    }
}
#endif

void UCustomGameInstanceSubsystem::EnqueueSetLevel(int32 CharId, int32 NewLevel)
{
    if (!IsReady() || bTestMode) 
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetLevel - database not ready, skipping"));
        return;
    }

    auto Job = std::make_shared<FJobSetLevel>(CharId, NewLevel);
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetBaseStats(int32 CharId, const TArray<FSingleStat>& StatsToUpdate)
{
    if (!IsReady() || bTestMode) 
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetBaseStats - database not ready, skipping"));
        return;
    }

    auto Job = std::make_shared<FJobSetBaseStats>(CharId, StatsToUpdate);
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueAddItem(int32 CharId, int32 ItemId, int32 Qty)
{
    if (!IsReady() || bTestMode) { UE_LOG(LogTemp, Warning, TEXT("EnqueueAddItem - pool not ready")); return; }
    UE_LOG(LogTemp, Verbose, TEXT("EnqueueAddItem CharId=%d ItemId=%d Qty=%d"), CharId, ItemId, Qty);
}

void UCustomGameInstanceSubsystem::EnqueueCurrencyDelta(int32 CharId, int32 DeltaGold, int32 DeltaSoft, int32 DeltaHard)
{
    if (!IsReady() || bTestMode) { UE_LOG(LogTemp, Warning, TEXT("EnqueueCurrencyDelta - pool not ready")); return; }
    UE_LOG(LogTemp, Verbose, TEXT("EnqueueCurrencyDelta CharId=%d dG=%d dS=%d dH=%d"), CharId, DeltaGold, DeltaSoft, DeltaHard);
}

void UCustomGameInstanceSubsystem::FlushCharacterByPC(APlayerController* PC)
{
#if WITH_SERVER_CODE
    if (!PC || bTestMode) return;
    if (auto* PS = PC->GetPlayerState<ACustomPlayerState>())
    {
        FlushCharacterByPS(PS);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FlushCharacter(PC): no PlayerState"));
    }
#endif
}

void UCustomGameInstanceSubsystem::FlushCharacterByPS(ACustomPlayerState* PS)
{
#if WITH_SERVER_CODE
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("FlushCharacter: DB not ready (CharId=%d)"),
            PS ? PS->GetCharId_Server() : -1);
        return;
    }

    if (!PS || !PS->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("FlushCharacter: invalid PlayerState or not server authority"));
        return;
    }

    const int32 CharId = PS->GetCharId_Server();
    const int32 Level = PS->Progression.Level;
    const FCharStats Stats = PS->BaseStats;

    auto ToLower = [](const FString& In) {
        return In.ToLower(); // UE helper
    };

    TArray<FSingleStat> JobStats;
    JobStats.Emplace(ToLower(TEXT("Str")), Stats.Str);
    JobStats.Emplace(ToLower(TEXT("Dex")), Stats.Dex);
    JobStats.Emplace(ToLower(TEXT("Wis")), Stats.Wis);
    JobStats.Emplace(ToLower(TEXT("Luk")), Stats.Luk);
    JobStats.Emplace(ToLower(TEXT("Pur")), Stats.Pur);
    JobStats.Emplace(ToLower(TEXT("Vic")), Stats.Vic);

    EnqueueSetLevel(CharId, Level);
    EnqueueSetBaseStats(CharId, JobStats);

    UE_LOG(LogTemp, Log, TEXT("FlushCharacter: queued Level=%d & %d base stats for CharId=%d"),
        Level, JobStats.Num(), CharId);
#endif
}

void UCustomGameInstanceSubsystem::QueueWorker()
{
    while (!bStopWriterThread)
    {
        std::unique_lock<std::mutex> Lock(WriteQueueMutex);
        WriteQueueCv.wait(Lock, [this]() { return !WriteQueue.empty() || bStopWriterThread; });

        if (bStopWriterThread) break;

        // Take ownership of the job
        auto Job = std::move(WriteQueue.front());
        WriteQueue.pop();
        Lock.unlock();

#if WITH_SERVER_CODE
        if (DbPool.IsValid() && Job)
        {
            auto Conn = DbPool->Acquire(std::chrono::milliseconds(100), true); // true = writer connection
            if (Conn)
            {
                try
                {
                    switch (Job->Type)
                    {
                    case EPersistenceJobType::SetLevel:
                    {
                        auto* SetLevelJob = static_cast<FJobSetLevel*>(Job.get());
                        if (!SetLevelJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetLevel job"));
                            continue; // skip this job, but keep thread alive
                        }

                        if (!Conn->UpdateCharacterLevel(SetLevelJob->CharId, SetLevelJob->NewLevel))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update level for CharId %d"), SetLevelJob->CharId);
                        }
                        break;
                    }

                    case EPersistenceJobType::SetBaseStats:
                    {
                        auto* StatsJob = static_cast<FJobSetBaseStats*>(Job.get());
                        if (!StatsJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetBaseStats job"));
                            continue;
                        }

                        int32 CharId = StatsJob->CharId;
                        std::map<std::string, int> StatsToUpdate;
                        for (const auto& stat : StatsJob->StatsToUpdate)
                        {
                            StatsToUpdate[TCHAR_TO_UTF8(*stat.StatName)] = stat.Value;
                        }

                        if (!Conn->UpdateCharacterStats(CharId, StatsToUpdate))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update stats for CharId %d"), CharId);
                        }
                        break;
                    }

                    default:
                        UE_LOG(LogTemp, Warning, TEXT("Unknown job type: %d"), static_cast<int>(Job->Type));
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    UE_LOG(LogTemp, Error, TEXT("DB job failed: %s"), *FString(e.what()));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to acquire writer connection"));
            }
        }
#endif
    }
}


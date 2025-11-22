// CustomGameInstanceSubsystem.cpp
#include "CustomGameInstanceSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMisc.h"
#include "CustomPlayerState.h"
#include <memory>
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
    PlayerState->Progression.Level = 1;
    PlayerState->Progression.XP = 3;
    PlayerState->Progression.UnspentAP = 30;
    PlayerState->Progression.MaxExpToLvl = 5;

    // Set Base Stats - direct assignment for test
    PlayerState->BaseStats.Str = 15;
    PlayerState->BaseStats.Dex = 12;
    PlayerState->BaseStats.Wisd = 10;
    PlayerState->BaseStats.Luk = 8;
    PlayerState->BaseStats.Pur = 14;
    PlayerState->BaseStats.Vic = 11;

    // Set Public Inspect (what others see) - direct assignment for test
    PlayerState->PublicInspect.Level = PlayerState->Progression.Level;
    PlayerState->PublicInspect.BaseStats = PlayerState->BaseStats;
    PlayerState->PublicInspect.Appearance = TEXT("hair:0-0,face:0-0,Weapon:TestSword");

    // Set Vitals (owner-only)
    PlayerState->Vitals.MaxHPFromLevels = 120;
    PlayerState->Vitals.MaxMPFromLevels = 50;
    PlayerState->Vitals.CurrHP = 100;
    PlayerState->Vitals.CurrMP = 40;

    // Set Achievements and mirror to public
    PlayerState->Achievements.HighestMinDamageRange = 7;
    PlayerState->Achievements.HighestMaxDamageRange = 13;
    PlayerState->PublicInspect.HighestMinDamageRange = 7;
    PlayerState->PublicInspect.HighestMaxDamageRange = 13;

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

void UCustomGameInstanceSubsystem::EnqueueSetBaseStats(int32 CharId, const TArray<FSingleStat>& StatsToUpdate)
{
    if (!IsReady() || bTestMode) 
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetBaseStats - database not ready, skipping"));
        return;
    }

    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetBaseStats>(CharId, StatsToUpdate));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetCurrentHP(int32 CharId, int32 CurrHP)
{
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetCurrentHP - database not ready, skipping"));
        return;
    }
    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetCurrentHP>(CharId, CurrHP));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetCurrentMP(int32 CharId, int32 CurrMP)
{
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetCurrentMP - database not ready, skipping"));
        return;
    }
    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetCurrentMP>(CharId, CurrMP));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetAppearance(int32 CharId, const FString& Appearance)
{
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetAppearance - database not ready, skipping"));
        return;
    }
    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetAppearance>(CharId, Appearance));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetLevelUpSnapshot(int32 CharId, int32 NewLevel, int32 MaxExpToLvl, int32 MaxHpFromLvls, int32 MaxMpFromLvls, int32 UnspentAP, int32 CurrentXP)
{
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetLevelUpSnapshot - database not ready, skipping"));
        return;
    }

    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetLevelUpSnapshot>(CharId, NewLevel, MaxExpToLvl, MaxHpFromLvls, MaxMpFromLvls, UnspentAP, CurrentXP));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetCurrentXP(int32 CharId, int32 CurrentXP)
{
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetCurrentXP - database not ready, skipping"));
        return;
    }
    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetCurrentXP>(CharId, CurrentXP));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetUnspentAP(int32 CharId, int32 UnspentAP)
{
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetUnspentAP - database not ready, skipping"));
        return;
    }
    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetUnspentAP>(CharId, UnspentAP));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetDamageRangeRecord(int32 CharId, int32 HighestMinRange, int32 HighestMaxRange)
{
    if (!IsReady() || bTestMode)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EnqueueSetDamageRangeRecord - database not ready, skipping"));
        return;
    }
    std::shared_ptr<FJobBase> Job = std::static_pointer_cast<FJobBase>(std::make_shared<FJobSetDamageRangeRecord>(CharId, HighestMinRange, HighestMaxRange));
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

// Removed EnqueueSetMaxVitalsFromLevels; handled by level-up snapshot job

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

void UCustomGameInstanceSubsystem::FlushCharacterByPS(ACustomPlayerState* PS) //updates everything when a player disconnects for any reason
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
    const int32 MaxExpToLvl = PS->Progression.MaxExpToLvl;
    const int64 CurrentXP = PS->Progression.XP;
    const FCharStats Stats = PS->BaseStats;
    const FCombatVitals Vitals = PS->Vitals;

    auto ToLower = [](const FString& In) {
        return In.ToLower(); // UE helper
    };

    TArray<FSingleStat> JobStats;
    JobStats.Emplace(ToLower(TEXT("Str")), Stats.Str);
    JobStats.Emplace(ToLower(TEXT("Dex")), Stats.Dex);
    JobStats.Emplace(ToLower(TEXT("Wisd")), Stats.Wisd);
    JobStats.Emplace(ToLower(TEXT("Luk")), Stats.Luk);
    JobStats.Emplace(ToLower(TEXT("Pur")), Stats.Pur);
    JobStats.Emplace(ToLower(TEXT("Vic")), Stats.Vic);

    // Flush everything important to DB
    EnqueueSetBaseStats(CharId, JobStats);
    // Persist canonical progression snapshot including base max vitals from levels
    EnqueueSetLevelUpSnapshot(CharId, Level, MaxExpToLvl, Vitals.MaxHPFromLevels, Vitals.MaxMPFromLevels, PS->Progression.UnspentAP, static_cast<int32>(CurrentXP));
    // Persist current running vitals
    EnqueueSetCurrentHP(CharId, Vitals.CurrHP);
    EnqueueSetCurrentMP(CharId, Vitals.CurrMP);
    // Persist achievements snapshot
    EnqueueSetDamageRangeRecord(CharId, PS->Achievements.HighestMinDamageRange, PS->Achievements.HighestMaxDamageRange);

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
        std::shared_ptr<FJobBase> Job = std::move(WriteQueue.front());
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
                    case EPersistenceJobType::SetBaseStats:
                    {
                        FJobSetBaseStats* StatsJob = static_cast<FJobSetBaseStats*>(Job.get());
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

                        if (!Conn->UpdateBaseStats(CharId, StatsToUpdate))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update stats for CharId %d"), CharId);
                        }
                        break;
                    }

                    // Removed SetVitals case; use SetCurrentHP and SetCurrentMP

                    case EPersistenceJobType::SetCurrentHP:
                    {
                        FJobSetCurrentHP* HPJob = static_cast<FJobSetCurrentHP*>(Job.get());
                        if (!HPJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetCurrentHP job"));
                            continue;
                        }
                        if (!Conn->UpdateCurrentHP(HPJob->CharId, HPJob->CurrHP))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update CurrHP for CharId %d"), HPJob->CharId);
                        }
                        break;
                    }

                    case EPersistenceJobType::SetCurrentMP:
                    {
                        FJobSetCurrentMP* MPJob = static_cast<FJobSetCurrentMP*>(Job.get());
                        if (!MPJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetCurrentMP job"));
                            continue;
                        }
                        if (!Conn->UpdateCurrentMP(MPJob->CharId, MPJob->CurrMP))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update CurrMP for CharId %d"), MPJob->CharId);
                        }
                        break;
                    }

                    case EPersistenceJobType::SetLevelUpSnapshot:
                    {
                        FJobSetLevelUpSnapshot* LU = static_cast<FJobSetLevelUpSnapshot*>(Job.get());
                        if (!LU)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetLevelUpSnapshot job"));
                            continue;
                        }

                        if (!Conn->UpdateLevelUpSnapshot(LU->CharId,
                            LU->NewLevel,
                            LU->MaxExpToLvl,
                            LU->MaxHpFromLvls,
                            LU->MaxMpFromLvls,
                            LU->UnspentAP,
                            static_cast<int>(LU->CurrentXP)))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update LevelUp snapshot for CharId %d"), LU->CharId);
                        }
                        break;
                    }

                    case EPersistenceJobType::SetCurrentXP:
                    {
                        FJobSetCurrentXP* XpJob = static_cast<FJobSetCurrentXP*>(Job.get());
                        if (!XpJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetCurrentXP job"));
                            continue;
                        }
                        if (!Conn->UpdateCharacterXP(XpJob->CharId, static_cast<int>(XpJob->CurrentXP), -1))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update CurrentXP for CharId %d"), XpJob->CharId);
                        }
                        break;
                    }

                    case EPersistenceJobType::SetUnspentAP:
                    {
                        FJobSetUnspentAP* ApJob = static_cast<FJobSetUnspentAP*>(Job.get());
                        if (!ApJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetUnspentAP job"));
                            continue;
                        }
                        if (!Conn->UpdateCharacterUnspentAP(ApJob->CharId, ApJob->UnspentAP))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update UnspentAP for CharId %d"), ApJob->CharId);
                        }
                        break;
                    }

                    case EPersistenceJobType::SetDamageRangeRecord:
                    {
                        FJobSetDamageRangeRecord* RngJob = static_cast<FJobSetDamageRangeRecord*>(Job.get());
                        if (!RngJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetDamageRangeRecord job"));
                            continue;
                        }
                        if (!Conn->UpdateCharacterAchievements(RngJob->CharId, RngJob->HighestMinRange, RngJob->HighestMaxRange))
                        {
                            UE_LOG(LogTemp, Error, TEXT("Failed to update DamageRangeRecord for CharId %d"), RngJob->CharId);
                        }
                        break;
                    }

                    case EPersistenceJobType::SetAppearance:
                    {
                        FJobSetAppearance* AppJob = static_cast<FJobSetAppearance*>(Job.get());
                        if (!AppJob)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Invalid SetAppearance job"));
                            continue;
                        }
                        // TODO: implement in DatabaseConnector; for now, log only to keep pipeline intact
                        UE_LOG(LogTemp, Verbose, TEXT("QueueWorker: SetAppearance pending DB impl (CharId=%d Appearance=%s)"),
                            AppJob->CharId, *AppJob->Appearance);
                        // Example when implemented:
                        // if (!Conn->UpdateAppearance(AppJob->CharId, TCHAR_TO_UTF8(*AppJob->Appearance))) { ... }
                        break;
                    }

                    // Removed SetMaxVitalsFromLevels; handled by LevelUp snapshot

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


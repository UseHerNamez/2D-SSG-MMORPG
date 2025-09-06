// CustomGameInstanceSubsystem.cpp
#include "CustomGameInstanceSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMisc.h"
#include "DatabaseConnectionPool.h"

void UCustomGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Server only - skip on pure clients
    const UWorld* World = GetWorld();
    const bool bIsServerProcess =
        (World && World->GetNetMode() != NM_Client) ||
        IsRunningDedicatedServer();

    if (!bIsServerProcess)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Persistence subsystem not initialized on client"));
        return;
    }

    if (!InitDbPool())
    {
        UE_LOG(LogTemp, Error, TEXT("Persistence subsystem failed to initialize DB pool"));
        return;
    }

    // Start the write queue thread
    bStopWriterThread = false;
    WriterThread = std::thread(&UCustomGameInstanceSubsystem::QueueWorker, this);
    UE_LOG(LogTemp, Log, TEXT("Persistence subsystem write queue started"));
}

void UCustomGameInstanceSubsystem::Deinitialize()
{
    // Stop background threads first, then release pool
    // ...
    DbPool.Reset();

    bStopWriterThread = true;
    WriteQueueCv.notify_all();
    if (WriterThread.joinable())
        WriterThread.join();

    Super::Deinitialize();
}

bool UCustomGameInstanceSubsystem::InitDbPool()
{
    // Read encryption key from env - safer UE API
    const FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("ENCRYPTION_KEY"));
    if (Key.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("ENCRYPTION_KEY missing"));
        return false;
    }

    // Read optional pool size from config - default 16
    
    GConfig->GetInt(TEXT("/Script/Engine.GameInstance"), TEXT("DbPoolSize"), PoolSize, GGameIni); // is there such a setting set?

    const FString EncCfg = FPaths::Combine(FPaths::ProjectDir(), TEXT("Config/config.ini.encrypted"));
    const std::string ConfigPath = TCHAR_TO_UTF8(*EncCfg);
    const std::string EncKey = std::string(TCHAR_TO_UTF8(*Key));

    DbPool = MakeShared<DatabaseConnectionPool>(ConfigPath, EncKey, PoolSize, WriterConnections);
    UE_LOG(LogTemp, Log, TEXT("DB pool initialized in GameInstance subsystem (size=%d)"), PoolSize);
    return true;
}

// ---- BP stubs (server only). Right now they just log. Wire them to your queue/flusher next.

void UCustomGameInstanceSubsystem::EnqueueSetLevel(int32 CharId, int32 NewLevel)
{
    if (!IsReady()) return;

    auto Job = std::make_shared<FJobSetLevel>(CharId, NewLevel);
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueSetBaseStats(int32 CharId, const TArray<FSingleStat>& StatsToUpdate)
{
    if (!IsReady()) return;

    auto Job = std::make_shared<FJobSetBaseStats>(CharId, StatsToUpdate);
    {
        std::lock_guard<std::mutex> Lock(WriteQueueMutex);
        WriteQueue.push(Job);
    }
    WriteQueueCv.notify_one();
}

void UCustomGameInstanceSubsystem::EnqueueAddItem(int32 CharId, int32 ItemId, int32 Qty)
{
    if (!IsReady()) { UE_LOG(LogTemp, Warning, TEXT("EnqueueAddItem - pool not ready")); return; }
    UE_LOG(LogTemp, Verbose, TEXT("EnqueueAddItem CharId=%d ItemId=%d Qty=%d"), CharId, ItemId, Qty);
}

void UCustomGameInstanceSubsystem::EnqueueCurrencyDelta(int32 CharId, int32 DeltaGold, int32 DeltaSoft, int32 DeltaHard)
{
    if (!IsReady()) { UE_LOG(LogTemp, Warning, TEXT("EnqueueCurrencyDelta - pool not ready")); return; }
    UE_LOG(LogTemp, Verbose, TEXT("EnqueueCurrencyDelta CharId=%d dG=%d dS=%d dH=%d"), CharId, DeltaGold, DeltaSoft, DeltaHard);
}

void UCustomGameInstanceSubsystem::FlushCharacter(int32 CharId)
{
    if (!IsReady()) { UE_LOG(LogTemp, Warning, TEXT("FlushCharacter - pool not ready")); return; }
    UE_LOG(LogTemp, Log, TEXT("FlushCharacter CharId=%d"), CharId);
    // TODO: run a synchronous flush for this CharId in your flusher
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

        if (DbPool.IsValid() && Job)
        {
            auto Conn = DbPool->Acquire(std::chrono::milliseconds(100));
            if (Conn)
            {
                switch (Job->Type)
                {
                case EPersistenceJobType::SetLevel:
                {
                    auto* SetLevelJob = static_cast<FJobSetLevel*>(Job.get());
                    FString Sql = FString::Printf(TEXT("UPDATE characters SET level=%d WHERE id=%d"),
                        SetLevelJob->NewLevel,
                        SetLevelJob->CharId);
                    // Conn->Execute(Sql);
                    break;
                }
                case EPersistenceJobType::SetBaseStats:
                {
                    auto* StatsJob = static_cast<FJobSetBaseStats*>(Job.get());
                    if (!StatsJob) return; // sanity check

                    int32 CharId = StatsJob->CharId;
                    const TArray<FSingleStat>& StatsToUpdate = StatsJob->StatsToUpdate;
                    for (const auto& stat : StatsToUpdate)
                    {
                        // Build dynamic SQL
                        std::string sql = std::string("UPDATE character_stats SET ") + TCHAR_TO_UTF8(*stat.StatName)
                            + " = " + std::to_string(stat.Value)
                            + " WHERE character_id = " + std::to_string(CharId) + ";";

                        // Execute on the acquired connection
                        //Conn->Execute(sql);
                    }
                    break;
                }
                }
            }
        }
    }
}


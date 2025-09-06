// CustomGameInstanceSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "CharacterInitTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CustomGameInstanceSubsystem.generated.h"

// Forward declares
class DatabaseConnectionPool;

UCLASS()
class TWODSSG_API UCustomGameInstanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Subsystem lifecycle
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // C++ access
    TSharedPtr<DatabaseConnectionPool> GetPool() const { return DbPool; }
    bool IsReady() const { return DbPool.IsValid(); }

    // BP entry points (server BP only). Add more as you implement the write-behind.
    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void EnqueueSetLevel(int32 CharId, int32 NewLevel);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void EnqueueSetBaseStats(int32 CharId, const FCharStatsPublic& NewStats);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void EnqueueAddItem(int32 CharId, int32 ItemId, int32 Qty);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void EnqueueCurrencyDelta(int32 CharId, int32 DeltaGold, int32 DeltaSoft, int32 DeltaHard);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void FlushCharacter(int32 CharId);

private:
    // Own the pool here - one per server process
    TSharedPtr<DatabaseConnectionPool> DbPool;

    // flusher, journal, queue handles
    // 
    // Write queue and synchronization
    struct FDbCommand
    {
        std::string Sql;
    };

    std::queue<FDbCommand> WriteQueue;
    std::mutex WriteQueueMutex;
    std::condition_variable WriteQueueCv;
    std::atomic<bool> bStopWriterThread;
    std::thread WriterThread;

    void QueueWorker(); // the thread function that executes SQL
    void EnqueueSQL(const std::string& Sql); // pushes SQL to the queue


    // Helper to build the pool
    bool InitDbPool();
    int32 PoolSize = 15;
};

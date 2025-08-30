// CustomGameInstanceSubsystem.h
#pragma once

#include "CoreMinimal.h"
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
        void EnqueueAddItem(int32 CharId, int32 ItemId, int32 Qty);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void EnqueueCurrencyDelta(int32 CharId, int32 DeltaGold, int32 DeltaSoft, int32 DeltaHard);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void FlushCharacter(int32 CharId);

private:
    // Own the pool here - one per server process
    TSharedPtr<DatabaseConnectionPool> DbPool;

    // Optional: store your flusher, journal, queue handles here
    // ...

    // Helper to build the pool
    bool InitDbPool();
};

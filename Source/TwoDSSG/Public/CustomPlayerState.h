#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CharacterInitTypes.h"
#include "ServerInitTypes.h"
#include "CustomPlayerState.generated.h"

UCLASS()
class TWODSSG_API ACustomPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // -------- Public, minimal (everyone) --------
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Persistence|Live|Public")
        FIdentityState Identity; // rarely changes - name, gender

    UPROPERTY(ReplicatedUsing = OnRep_PublicInspect, BlueprintReadOnly, Category = "Persistence|Live|Public")
        FPublicInspectState PublicInspect; // level, stats, appearance
    UFUNCTION() void OnRep_PublicInspect(const FPublicInspectState& Previous);

    // -------- Owner-only (HUD/private) --------
    UPROPERTY(ReplicatedUsing = OnRep_Progression, BlueprintReadOnly, Category = "Persistence|Live|Owner")
        FProgressionState Progression; // contains  XP, AP - owner-only
    UFUNCTION() void OnRep_Progression(const FProgressionState& Previous);

        // NOTE: This is the *owner-view* copy. Public stats live in PublicInspect.
    UPROPERTY(ReplicatedUsing = OnRep_BaseStats, BlueprintReadOnly, Category = "Persistence|Live|Owner")
        FCharStats BaseStats;
    UFUNCTION() void OnRep_BaseStats(const FCharStats& Previous);

    // OnRep_ declarations
    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
        void BP_OnPublicInspectChanged_Keys(const TArray<FName>& ChangeKeys);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
        void BP_OnProgressionChanged_Keys(const TArray<FName>& ChangeKeys);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
        void BP_OnBaseStatsChanged_Keys(const TArray<FName>& ChangeKeys);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
        void BP_OnInitialDataLoaded();

    // ------- Server-only helpers (not replicated) -------
    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyLevel_ServerOnly(int32 NewLevel);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyBaseStats_ServerOnly(const TArray<FSingleStat>& InStats);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyXP_ServerOnly(int64 NewXP);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyUnspentAP_ServerOnly(int32 NewUnspentAP);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyAppearance_ServerOnly(const FString& NewAppearance);

    UFUNCTION(Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void NotifyInitialDataLoaded_ServerOnly();

    #if WITH_SERVER_CODE
        void SetServerOnlyData(const FCharacterInitData_Server& In);
    #endif

    UFUNCTION(BlueprintPure, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        int32 GetCharId_Server() const { check(HasAuthority()); return ServerOnlyData.CharId; }

    UFUNCTION(BlueprintPure, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        int32 GetUserId_Server() const { check(HasAuthority()); return ServerOnlyData.UserId; }

    // UI helpers
    UFUNCTION(BlueprintPure, Category = "UI")
        static bool NameToStateChangeKey(FName NameKey, EStateChangeKey& OutKey);

private:
    // Backing store for server-only data
    FCharacterInitData_Server ServerOnlyData;
};
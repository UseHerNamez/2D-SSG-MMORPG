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

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Persistence|Live|Public")
        FPublicInspectState PublicInspect; // level, stats, appearance

    // -------- Owner-only (HUD/private) --------
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Persistence|Live|Owner")
        FProgressionState Progression; // contains curr&max XP, AP, level - owner-only

        // NOTE: This is the *owner-view* copy. Public stats live in PublicInspect.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Persistence|Live|Owner")
        FCharStats BaseStats;

    // Owner-only vitals (HP/MP)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Persistence|Live|Owner")
        FCombatVitals Vitals;

    // Owner-only achievements (canonical)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Persistence|Live|Owner")
        FAchievementsState Achievements;

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
        void BP_OnInitialDataLoaded();

    // ------- Server-only helpers (not replicated) -------
    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyBaseStats_ServerOnly(const TArray<FSingleStat>& InStats);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyXP_ServerOnly(int32 NewXP);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyUnspentAP_ServerOnly(int32 NewUnspentAP);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyAppearance_ServerOnly(const FString& NewAppearance);

    // Update one entry (slot=value) in the serialized appearance string and persist (server only)
    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void UpdateAppearanceEntry_ServerOnly(FName Slot, const FString& Value);

    // HP/MP server-only apply helpers
    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyHP_ServerOnly(int32 NewCurrHP);

    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyMP_ServerOnly(int32 NewCurrMP);

    // Combined level-up apply to keep state changes consistent
    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyLevelUp_ServerOnly(int32 NewLevel, int32 NewMaxExpToLvl, int32 NewMaxHP, int32 NewMaxMP, int32 NewUnspentAP);

    // Achievements: track highest min/max damage range ever attained
    UFUNCTION(BlueprintCallable, Category = "Persistence", meta = (BlueprintAuthorityOnly))
        void ApplyDamageRangeRecord_ServerOnly(int32 NewMinRange, int32 NewMaxRange);

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
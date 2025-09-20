// CustomPlayerState.cpp
#include "CustomPlayerState.h"
#include "Net/UnrealNetwork.h"

void ACustomPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACustomPlayerState, Identity);
    DOREPLIFETIME(ACustomPlayerState, PublicInspect);

    FDoRepLifetimeParams Params;
    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS(ACustomPlayerState, Progression, Params);
    DOREPLIFETIME_WITH_PARAMS(ACustomPlayerState, BaseStats, Params);
}

// - - call client side to update UI in BP - -
void ACustomPlayerState::OnRep_PublicInspect()
{
    BP_OnPublicInspectChanged();
}


void ACustomPlayerState::OnRep_Progression()
{
    BP_OnProgressionChanged(); 
}

void ACustomPlayerState::OnRep_BaseStats()
{
    BP_OnBaseStatsChanged();
}
// - - call client side to update UI in BP - -

void ACustomPlayerState::NotifyInitialDataLoaded_ServerOnly()
{
    check(HasAuthority());
    BP_OnInitialDataLoaded();
}

// ====== Server-side mutation paths (non-RPC) ======
#if WITH_SERVER_CODE
void ACustomPlayerState::ApplyLevel_ServerOnly(int32 NewLevel)
{
    check(HasAuthority());
    Progression.Level = FMath::Max(1, NewLevel);
    // Keep PublicInspect in sync for others:
    PublicInspect.Level = Progression.Level;
    ForceNetUpdate();
}

void ACustomPlayerState::ApplyBaseStats_ServerOnly(const TArray<FSingleStat>& InStats)
{
    check(HasAuthority());
    // Update owner-view:
    for (const FSingleStat& S : InStats)
    {
        const FString Key = S.StatName.ToLower();
        if (Key == TEXT("str")) BaseStats.Str = S.Value;
        else if (Key == TEXT("dex")) BaseStats.Dex = S.Value;
        else if (Key == TEXT("wis")) BaseStats.Wis = S.Value;
        else if (Key == TEXT("luk")) BaseStats.Luk = S.Value;
        else if (Key == TEXT("pur")) BaseStats.Pur = S.Value;
        else if (Key == TEXT("vic")) BaseStats.Vic = S.Value;
    }
    // Mirror into public snapshot *only* for the subset you want visible:
    PublicInspect.BaseStats = BaseStats;
    ForceNetUpdate();
}

void ACustomPlayerState::SetServerOnlyData(const FCharacterInitData_Server& In)
{
    check(HasAuthority());
    ServerOnlyData = In;
}
#endif
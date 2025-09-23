// CustomPlayerState.cpp
#include "CustomPlayerState.h"
#include "Net/UnrealNetwork.h"

bool ACustomPlayerState::NameToStateChangeKey(FName NameKey, EStateChangeKey& OutKey)
{
    const FString S = NameKey.ToString();
    if (S.Equals(TEXT("Level"), ESearchCase::IgnoreCase)) { OutKey = EStateChangeKey::Level; return true; }
    if (S.Equals(TEXT("XP"), ESearchCase::IgnoreCase)) { OutKey = EStateChangeKey::XP; return true; }
    if (S.Equals(TEXT("UnspentAP"), ESearchCase::IgnoreCase)) { OutKey = EStateChangeKey::UnspentAP; return true; }
    if (S.Equals(TEXT("Stats"), ESearchCase::IgnoreCase)) { OutKey = EStateChangeKey::Stats; return true; }
    if (S.Equals(TEXT("Appearance"), ESearchCase::IgnoreCase)) { OutKey = EStateChangeKey::Appearance; return true; }
    return false;
}


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
void ACustomPlayerState::OnRep_PublicInspect(const FPublicInspectState& Previous)
{
    // Coarse keys: Level, Appearance, Stats
    TArray<FName> Keys;
    if (Previous.Level != PublicInspect.Level)
    {
        Keys.Add(TEXT("Level"));
    }
    if (Previous.Appearance != PublicInspect.Appearance)
    {
        Keys.Add(TEXT("Appearance"));
    }
    if (Previous.BaseStats.Str != PublicInspect.BaseStats.Str
        || Previous.BaseStats.Dex != PublicInspect.BaseStats.Dex
        || Previous.BaseStats.Wis != PublicInspect.BaseStats.Wis
        || Previous.BaseStats.Luk != PublicInspect.BaseStats.Luk
        || Previous.BaseStats.Pur != PublicInspect.BaseStats.Pur
        || Previous.BaseStats.Vic != PublicInspect.BaseStats.Vic)
    {
        Keys.Add(TEXT("Stats"));
    }

    if (Keys.Num() > 0)
    {
        BP_OnPublicInspectChanged_Keys(Keys);
    }
}


void ACustomPlayerState::OnRep_Progression(const FProgressionState& Previous)
{
    // Coarse keys: Level, XP, UnspentAP
    TArray<FName> Keys;
    if (Previous.Level != Progression.Level) { Keys.Add(TEXT("Level")); }
    if (Previous.XP != Progression.XP) { Keys.Add(TEXT("XP")); }
    if (Previous.UnspentAP != Progression.UnspentAP) { Keys.Add(TEXT("UnspentAP")); }

    if (Keys.Num() > 0)
    {
        BP_OnProgressionChanged_Keys(Keys);
    }
}

void ACustomPlayerState::OnRep_BaseStats(const FCharStats& Previous)
{
    BP_OnBaseStatsChanged_Keys();
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

void ACustomPlayerState::ApplyXP_ServerOnly(int64 NewXP)
{
    check(HasAuthority());
    Progression.XP = FMath::Max<int64>(0, NewXP);
    ForceNetUpdate();
}

void ACustomPlayerState::ApplyUnspentAP_ServerOnly(int32 NewUnspentAP)
{
    check(HasAuthority());
    Progression.UnspentAP = FMath::Max(0, NewUnspentAP);
    ForceNetUpdate();
}

void ACustomPlayerState::ApplyAppearance_ServerOnly(const FString& NewAppearance)
{
    check(HasAuthority());
    PublicInspect.Appearance = NewAppearance;
    ForceNetUpdate();
}

void ACustomPlayerState::SetServerOnlyData(const FCharacterInitData_Server& In)
{
    check(HasAuthority());
    ServerOnlyData = In;
}
#endif
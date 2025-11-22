// CustomPlayerState.cpp
#include "CustomPlayerState.h"
#include "CustomGameInstanceSubsystem.h"
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
    DOREPLIFETIME_WITH_PARAMS(ACustomPlayerState, Vitals, Params);
    DOREPLIFETIME_WITH_PARAMS(ACustomPlayerState, Achievements, Params);
}

// OnRep UI callbacks removed; UI is driven by BP after server-side calculations

void ACustomPlayerState::NotifyInitialDataLoaded_ServerOnly()
{
    check(HasAuthority());
    BP_OnInitialDataLoaded();
}

// ====== Server-side mutation paths (non-RPC) ======
#if WITH_SERVER_CODE
void ACustomPlayerState::ApplyBaseStats_ServerOnly(const TArray<FSingleStat>& InStats)
{
    check(HasAuthority());
    // Update owner-view:
    for (const FSingleStat& S : InStats)
    {
        const FString Key = S.StatName.ToLower();
        if (Key == TEXT("str")) BaseStats.Str = S.Value;
        else if (Key == TEXT("dex")) BaseStats.Dex = S.Value;
        else if (Key == TEXT("wisd")) BaseStats.Wisd = S.Value;
        else if (Key == TEXT("luk")) BaseStats.Luk = S.Value;
        else if (Key == TEXT("pur")) BaseStats.Pur = S.Value;
        else if (Key == TEXT("vic")) BaseStats.Vic = S.Value;
    }
    // Mirror into public snapshot *only* for the subset you want visible:
    PublicInspect.BaseStats = BaseStats;
    ForceNetUpdate();

    // Persist changes asynchronously
    if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
    {
        Sub->EnqueueSetBaseStats(GetCharId_Server(), InStats);
    }
}

void ACustomPlayerState::ApplyXP_ServerOnly(int32 NewXP)
{
    check(HasAuthority());
    Progression.XP = FMath::Max<int32>(0, NewXP);
    ForceNetUpdate();

    if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
    {
        Sub->EnqueueSetCurrentXP(GetCharId_Server(), Progression.XP);
    }
}

void ACustomPlayerState::ApplyUnspentAP_ServerOnly(int32 NewUnspentAP)
{
    check(HasAuthority());
    Progression.UnspentAP = FMath::Max(0, NewUnspentAP);
    ForceNetUpdate();

    if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
    {
        Sub->EnqueueSetUnspentAP(GetCharId_Server(), Progression.UnspentAP);
    }
}

void ACustomPlayerState::ApplyAppearance_ServerOnly(const FString& NewAppearance)
{
    check(HasAuthority());
    PublicInspect.Appearance = NewAppearance;
    ForceNetUpdate();

    if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
    {
        Sub->EnqueueSetAppearance(GetCharId_Server(), PublicInspect.Appearance);
    }
}

void ACustomPlayerState::ApplyHP_ServerOnly(int32 NewCurrHP)
{
    check(HasAuthority());
    Vitals.CurrHP = FMath::Max(0, NewCurrHP);
    ForceNetUpdate();

    if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
    {
        Sub->EnqueueSetCurrentHP(GetCharId_Server(), Vitals.CurrHP);
    }
}

void ACustomPlayerState::ApplyMP_ServerOnly(int32 NewCurrMP)
{
    check(HasAuthority());
    Vitals.CurrMP = FMath::Max(0, NewCurrMP);
    ForceNetUpdate();

    if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
    {
        Sub->EnqueueSetCurrentMP(GetCharId_Server(), Vitals.CurrMP);
    }
}

void ACustomPlayerState::ApplyLevelUp_ServerOnly(int32 NewLevel, int32 NewMaxExpToLvl, int32 NewMaxHP, int32 NewMaxMP, int32 NewUnspentAP)
{
    check(HasAuthority());
    // Update progression and vitals atomically on server
    Progression.Level = FMath::Max(1, NewLevel);
    Progression.MaxExpToLvl = FMath::Max(0, NewMaxExpToLvl);
    Progression.UnspentAP = FMath::Max(0, NewUnspentAP);

    Vitals.MaxHPFromLevels = FMath::Max(0, NewMaxHP);
    Vitals.MaxMPFromLevels = FMath::Max(0, NewMaxMP);

    // Keep public snapshot in sync
    PublicInspect.Level = Progression.Level;

    ForceNetUpdate();

    // Enqueue consolidated persistence write
    if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
    {
        Sub->EnqueueSetLevelUpSnapshot(GetCharId_Server(), Progression.Level, Progression.MaxExpToLvl, Vitals.MaxHPFromLevels, Vitals.MaxMPFromLevels, Progression.UnspentAP, Progression.XP);
    }
}

void ACustomPlayerState::ApplyDamageRangeRecord_ServerOnly(int32 NewMinRange, int32 NewMaxRange)
{
    check(HasAuthority());
    bool bChanged = false;
    if (NewMinRange > Achievements.HighestMinDamageRange) { Achievements.HighestMinDamageRange = NewMinRange; bChanged = true; }
    if (NewMaxRange > Achievements.HighestMaxDamageRange) { Achievements.HighestMaxDamageRange = NewMaxRange; bChanged = true; }
    // Mirror to public snapshot
    PublicInspect.HighestMinDamageRange = Achievements.HighestMinDamageRange;
    PublicInspect.HighestMaxDamageRange = Achievements.HighestMaxDamageRange;
    if (bChanged)
    {
        ForceNetUpdate();
        if (UCustomGameInstanceSubsystem* Sub = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>() : nullptr)
        {
            Sub->EnqueueSetDamageRangeRecord(GetCharId_Server(), PublicInspect.HighestMinDamageRange, PublicInspect.HighestMaxDamageRange);
        }
    }
}

void ACustomPlayerState::SetServerOnlyData(const FCharacterInitData_Server& In)
{
    check(HasAuthority());
    ServerOnlyData = In;
}
#endif

#if WITH_SERVER_CODE
// Helper to parse "key:value,key2:value2" into a map
static void ParseAppearanceString(const FString& In, TMap<FString, FString>& OutMap)
{
    OutMap.Empty();
    TArray<FString> Pairs;
    In.ParseIntoArray(Pairs, TEXT(","), true);
    for (const FString& P : Pairs)
    {
        FString K, V;
        if (P.Split(TEXT(":"), &K, &V))
        {
            OutMap.Add(K.TrimStartAndEnd(), V.TrimStartAndEnd());
        }
    }
}

static FString BuildAppearanceString(const TMap<FString, FString>& InMap)
{
    TArray<FString> Parts;
    Parts.Reserve(InMap.Num());
    for (const auto& It : InMap)
    {
        Parts.Add(It.Key + TEXT(":") + It.Value);
    }
    return FString::Join(Parts, TEXT(","));
}
#endif

void ACustomPlayerState::UpdateAppearanceEntry_ServerOnly(FName Slot, const FString& Value)
{
#if WITH_SERVER_CODE
    check(HasAuthority());
    TMap<FString, FString> Map;
    ParseAppearanceString(PublicInspect.Appearance, Map);
    const FString Key = Slot.ToString();
    if (Value.IsEmpty())
    {
        Map.Remove(Key);
    }
    else
    {
        Map.Add(Key, Value);
    }
    const FString NewAppearance = BuildAppearanceString(Map);
    ApplyAppearance_ServerOnly(NewAppearance);
#endif
}
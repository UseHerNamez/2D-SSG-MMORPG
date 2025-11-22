// PersistenceJobs.h
#pragma once

#include "CoreMinimal.h"
#include "CharacterInitTypes.h"

enum class EPersistenceJobType : uint8
{
    SetBaseStats,
    SetLevelUpSnapshot,
    SetCurrentXP,
    SetUnspentAP,
    SetDamageRangeRecord,
    SetCurrentHP,
    SetCurrentMP,
    SetAppearance,
    // there will be more
};

struct FJobBase
{
    EPersistenceJobType Type;
    int32 CharId;

    FJobBase(EPersistenceJobType InType, int32 InCharId)
        : Type(InType), CharId(InCharId) {}
    virtual ~FJobBase() {}
};


struct FJobSetBaseStats : public FJobBase
{
    TArray<FSingleStat> StatsToUpdate;

    FJobSetBaseStats(int32 InCharId, const TArray<FSingleStat>& InStats)
        : FJobBase(EPersistenceJobType::SetBaseStats, InCharId), StatsToUpdate(InStats) {}
};

struct FJobSetLevelUpSnapshot : public FJobBase
{
    int32 NewLevel;
    int32 MaxExpToLvl;
    int32 MaxHpFromLvls;
    int32 MaxMpFromLvls;
    int32 UnspentAP;
    int64 CurrentXP;

    FJobSetLevelUpSnapshot(int32 InCharId, int32 InNewLevel, int32 InMaxExpToLvl, int32 InMaxHpFromLvls, int32 InMaxMpFromLvls, int32 InUnspentAP, int32 InCurrentXP)
        : FJobBase(EPersistenceJobType::SetLevelUpSnapshot, InCharId)
        , NewLevel(InNewLevel)
        , MaxExpToLvl(InMaxExpToLvl)
        , MaxHpFromLvls(InMaxHpFromLvls)
        , MaxMpFromLvls(InMaxMpFromLvls)
        , UnspentAP(InUnspentAP)
        , CurrentXP(InCurrentXP)
    {}
};

struct FJobSetCurrentXP : public FJobBase
{
    int32 CurrentXP;

    FJobSetCurrentXP(int32 InCharId, int32 InCurrentXP)
        : FJobBase(EPersistenceJobType::SetCurrentXP, InCharId)
        , CurrentXP(InCurrentXP)
    {}
};

struct FJobSetUnspentAP : public FJobBase
{
    int32 UnspentAP;

    FJobSetUnspentAP(int32 InCharId, int32 InUnspentAP)
        : FJobBase(EPersistenceJobType::SetUnspentAP, InCharId)
        , UnspentAP(InUnspentAP)
    {}
};

struct FJobSetDamageRangeRecord : public FJobBase
{
    int32 HighestMinRange;
    int32 HighestMaxRange;

    FJobSetDamageRangeRecord(int32 InCharId, int32 InHighestMinRange, int32 InHighestMaxRange)
        : FJobBase(EPersistenceJobType::SetDamageRangeRecord, InCharId)
        , HighestMinRange(InHighestMinRange)
        , HighestMaxRange(InHighestMaxRange)
    {}
};


struct FJobSetCurrentHP : public FJobBase
{
    int32 CurrHP;

    FJobSetCurrentHP(int32 InCharId, int32 InCurrHP)
        : FJobBase(EPersistenceJobType::SetCurrentHP, InCharId)
        , CurrHP(InCurrHP)
    {}
};

struct FJobSetCurrentMP : public FJobBase
{
    int32 CurrMP;

    FJobSetCurrentMP(int32 InCharId, int32 InCurrMP)
        : FJobBase(EPersistenceJobType::SetCurrentMP, InCharId)
        , CurrMP(InCurrMP)
    {}
};

struct FJobSetAppearance : public FJobBase
{
    FString Appearance;

    FJobSetAppearance(int32 InCharId, const FString& InAppearance)
        : FJobBase(EPersistenceJobType::SetAppearance, InCharId)
        , Appearance(InAppearance)
    {}
};
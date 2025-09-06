// PersistenceJobs.h
#pragma once

#include "CoreMinimal.h"
#include "CharacterInitTypes.h"

enum class EPersistenceJobType : uint8
{
    SetLevel,
    SetBaseStats,
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

struct FJobSetLevel : public FJobBase
{
    int32 NewLevel;

    FJobSetLevel(int32 InCharId, int32 InLevel)
        : FJobBase(EPersistenceJobType::SetLevel, InCharId), NewLevel(InLevel) {}
};

struct FJobSetBaseStats : public FJobBase
{
    TArray<FSingleStat> StatsToUpdate;

    FJobSetBaseStats(int32 InCharId, const TArray<FSingleStat>& InStats)
        : FJobBase(EPersistenceJobType::SetBaseStats, InCharId), StatsToUpdate(InStats) {}
};
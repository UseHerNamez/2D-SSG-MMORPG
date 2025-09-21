#pragma once
#include "CoreMinimal.h"
#include "CharacterInitTypes.generated.h"

UENUM(BlueprintType)
enum class EStateChangeKey : uint8
{
    Level       UMETA(DisplayName = "Level"),
    XP          UMETA(DisplayName = "XP"),
    UnspentAP   UMETA(DisplayName = "UnspentAP"),
    Stats       UMETA(DisplayName = "Stats"),
    Appearance  UMETA(DisplayName = "Appearance")
};

USTRUCT(BlueprintType)
struct FCharStats
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Str = 0;
    UPROPERTY(BlueprintReadOnly) int32 Dex = 0;
    UPROPERTY(BlueprintReadOnly) int32 Wis = 0;
    UPROPERTY(BlueprintReadOnly) int32 Luk = 0;
    UPROPERTY(BlueprintReadOnly) int32 Pur = 0;
    UPROPERTY(BlueprintReadOnly) int32 Vic = 0;
};

USTRUCT(BlueprintType)
struct FSingleStat
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FString StatName; // "Str","Dex",...
    UPROPERTY(BlueprintReadWrite) int32   Value = 1;

    FSingleStat() {}
    FSingleStat(const FString& InName, int32 InValue) : StatName(InName), Value(InValue) {}
};

USTRUCT(BlueprintType)
struct FIdentityState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32   Gender = 0;
};

USTRUCT(BlueprintType)
struct FProgressionState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32  Level = 1;
    UPROPERTY(BlueprintReadOnly) int64  XP = 0;
    UPROPERTY(BlueprintReadOnly) int32  UnspentAP = 0;
};

// ---- Public inspect snapshot (global scope; not nested in a class) ----
USTRUCT(BlueprintType)
struct FPublicInspectState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(BlueprintReadOnly) FCharStats BaseStats; // public, lightweight
    UPROPERTY(BlueprintReadOnly) FString Appearance;
};

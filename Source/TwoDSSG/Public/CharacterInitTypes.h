#pragma once
#include "CoreMinimal.h"
#include "CharacterInitTypes.generated.h"

USTRUCT(BlueprintType)
struct FCharStatsPublic
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

        // The name of the stat, e.g., "Str", "Dex", "Wis"
        UPROPERTY(BlueprintReadWrite)
        FString StatName;

    // The value to set
    UPROPERTY(BlueprintReadWrite)
        int32 Value = 1;

    FSingleStat() {}
    FSingleStat(const FString& InName, int32 InValue) : StatName(InName), Value(InValue) {}
};

USTRUCT(BlueprintType)
struct FCharBasePublic
{
    GENERATED_BODY()
        UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Level;
    UPROPERTY(BlueprintReadOnly) int32   Gender = 0;
    UPROPERTY(BlueprintReadOnly) FString Appearance;
};

USTRUCT(BlueprintType)
struct FCharacterInitData_Client
{
    GENERATED_BODY()
        UPROPERTY(BlueprintReadOnly) FCharBasePublic  Base;
    UPROPERTY(BlueprintReadOnly) FCharStatsPublic Stats;
};

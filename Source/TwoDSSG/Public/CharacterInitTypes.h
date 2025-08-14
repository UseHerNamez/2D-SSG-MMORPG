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

#pragma once

#include "CoreMinimal.h"
#include "PaperDollAnimTypes.h"
#include "Engine/DataAsset.h"
#include "PaperDollSortRules.generated.h"

// Wrapper to allow per-frame arrays in a map (UHT forbids nested containers directly)
USTRUCT(BlueprintType)
struct FPerFramePriority
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<int32> Values;
};

// Base part priorities for a single state
USTRUCT(BlueprintType)
struct FPaperDollPartPriorities
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Torso = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Head = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ArmNear = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ArmFar = -1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 HandNear = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 HandFar = -3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 LegNear = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 LegFar = -1;
};

// Per-equipment slot offsets or absolute priorities
USTRUCT(BlueprintType)
struct FEquipmentSortPerState
{
	GENERATED_BODY()

    // If true, treat values as absolute priorities (already include bucket base). If false, apply as offset to parent.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAbsolute = true;

    // Default value: offset or absolute based on bAbsolute
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DefaultValue = 0;

	// Optional per-state overrides
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<EPaperDollAnimState, int32> PerStateOverrides;

    // Optional per-frame absolute overrides (used within the same state)
    // If provided and non-empty for a state, index = current frame % array length
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<EPaperDollAnimState, FPerFramePriority> PerFrameOverrides;
};

UCLASS(BlueprintType)
class TWODSSG_API UPaperDollSortRules : public UDataAsset
{
	GENERATED_BODY()

public:
	// Base part priorities per state
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<EPaperDollAnimState, FPaperDollPartPriorities> PartPriorities;

	// Equipment slot rules, addressed by tag/slot name (e.g., "WeaponNear", "Hat", "Hair")
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FEquipmentSortPerState> EquipmentRules;
};



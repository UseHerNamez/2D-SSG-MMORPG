#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PaperDollAnimTypes.h"
#include "PaperDollSortRules.h"
#include "PaperDoll2DCharacter.generated.h"

class UPaperFlipbook;
class UPaperFlipbookComponent;

USTRUCT(BlueprintType)
struct FPaperDollStateFlipbooks
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* Torso = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* Head = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* ArmNear = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* ArmFar = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* HandNear = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* HandFar = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* LegNear = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") UPaperFlipbook* LegFar = nullptr;
};

USTRUCT(BlueprintType)
struct FPaperDollVariants
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim")
    TArray<FPaperDollStateFlipbooks> Variants;
};

UCLASS()
class TWODSSG_API APaperDoll2DCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APaperDoll2DCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

UFUNCTION(BlueprintCallable, Category = "Anim")
void PlayAnimationState(EPaperDollAnimState NewState, bool bResetTime = true);

// Choose explicit variant, or pass -1 to auto-pick (random if multiple)
UFUNCTION(BlueprintCallable, Category = "Anim")
void PlayAnimationStateEx(EPaperDollAnimState NewState, int32 VariantIndex, bool bResetTime = true);

	// Components (individual body parts)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* Torso;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* Head;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* ArmNear;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* ArmFar;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* HandNear;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* HandFar;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* LegNear;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPaperFlipbookComponent* LegFar;

// Animation Sets per state; supports multiple variants per state (wrapper avoids nested containers)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim")
TMap<EPaperDollAnimState, FPaperDollVariants> AnimSets;

	// Base draw priorities; Arm/Leg far/near can offset around these
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting") int32 BasePriority_Torso = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting") int32 BasePriority_Head = 2; // ensure chin can overlap chest
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting") int32 BasePriority_Arms = 0; // treated as center around torso
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting") int32 BasePriority_Hands = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting") int32 BasePriority_Legs = 0; // treated as center around torso

	// Spacing between different players on a client to avoid overlap ranges
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting") int32 GlobalBucketStride = 50;

	// Animation playback control
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") bool bUseTorsoAsTimeMaster = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim") float GlobalPlayRate = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "Anim")
    void SetGlobalPlayRate(float NewRate);

    UFUNCTION(BlueprintPure, Category = "Anim")
    float GetGlobalPlayRate() const { return GlobalPlayRate; }

    UFUNCTION(BlueprintCallable, Category = "Sorting")
    void RefreshSorting();

	// Optional data asset to drive per-state sort priorities and equipment offsets
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting") UPaperDollSortRules* SortRules = nullptr;

protected:
	UFUNCTION()
	void OnRep_CurrentAnimState();

	UFUNCTION()
	void OnRep_SortBucketId();

	void UpdateFlipbooksForCurrentState();
	void UpdatePlaybackFrame(float DeltaSeconds);
	int32 ComputeFrameIndex(const UPaperFlipbook* Master, float TimeSeconds) const;
	UPaperFlipbook* GetMasterFlipbook() const;
	void ApplySortPriorities() const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentAnimState, VisibleAnywhere, BlueprintReadOnly, Category = "Anim")
	EPaperDollAnimState CurrentAnimState = EPaperDollAnimState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim")
	float CurrentAnimTimeSeconds = 0.0f;

// Variant index for the current state (replicated). -1 means default/first.
UPROPERTY(ReplicatedUsing = OnRep_CurrentAnimState, VisibleAnywhere, BlueprintReadOnly, Category = "Anim")
int32 CurrentAnimVariantIndex = -1;

// no per-facing toggles; layering is invariant to facing

	// Server-assigned bucket to spread sort priorities per client
	UPROPERTY(ReplicatedUsing = OnRep_SortBucketId, VisibleAnywhere, BlueprintReadOnly, Category = "Sorting")
	int32 SortBucketId = 0;

public:
	UFUNCTION(BlueprintCallable, Category = "Sorting", meta = (BlueprintAuthorityOnly))
	void AssignSortBucket_ServerOnly(int32 InBucket) { if (HasAuthority()) { SortBucketId = InBucket; OnRep_SortBucketId(); } }
};



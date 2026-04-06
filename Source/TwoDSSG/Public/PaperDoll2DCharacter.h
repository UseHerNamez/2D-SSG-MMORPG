#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PaperDollAnimTypes.h"
#include "PaperDollSortRules.h"
#include "PaperDoll2DCharacter.generated.h"

class UPaperFlipbook;
class UPaperFlipbookComponent;

USTRUCT(BlueprintType)
struct FSlotParentMap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip")
	TMap<FName, FName> Map; // SlotTag -> ParentPartName ("Torso","Head","ArmNear","ArmFar","HandNear","HandFar","LegNear","LegFar")
};

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

// Equipment flipbooks per state (for a slot tag)
USTRUCT(BlueprintType)
struct FEquipStateFlipbooks
{
    GENERATED_BODY()

    // Map: AnimState -> Flipbook for that slot in this state
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip")
    TMap<EPaperDollAnimState, UPaperFlipbook*> ByState;
};

UENUM(BlueprintType)
enum class EClimbAnimType : uint8
{
	Ladder  UMETA(DisplayName = "Ladder"),
	Rope    UMETA(DisplayName = "Rope")
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
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

UFUNCTION(BlueprintCallable, Category = "Anim")
void PlayAnimationState(EPaperDollAnimState NewState, bool bResetTime = true);

// Choose explicit variant, or pass -1 to auto-pick (random if multiple)
UFUNCTION(BlueprintCallable, Category = "Anim")
void PlayAnimationStateEx(EPaperDollAnimState NewState, int32 VariantIndex, bool bResetTime = true);

	// One-shot request with built-in priority rules and combat-aware fallback (Authority only).
	// Returns true if the request was accepted and state changed/kept; false if rejected by priority rules.
	UFUNCTION(BlueprintCallable, Category = "Anim", meta = (BlueprintAuthorityOnly))
	bool RequestAnimationStateWithPriority(EPaperDollAnimState DesiredState, bool bInCombat, int32 VariantIndex = -1, bool bResetTime = true);

	// Client-side visual prediction only (no authority, no replication). Safe to call on owning client for responsiveness.
	UFUNCTION(BlueprintCallable, Category = "Anim", meta = (BlueprintAuthorityOnly = false))
	void PredictAnimationStateLocal(EPaperDollAnimState DesiredState, int32 VariantIndex = -1, bool bResetTime = true);

	// Set climb anim type (ladder vs rope) before entering flying movement (server only)
	UFUNCTION(BlueprintCallable, Category = "Anim", meta = (BlueprintAuthorityOnly))
	void SetClimbAnimType(EClimbAnimType InType);

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
	// Additional per-character multiplier (e.g., movespeed/attackspeed driven). Effective rate = GlobalPlayRate * PlayRate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim") float PlayRate = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "Anim")
    void SetGlobalPlayRate(float NewRate);

    UFUNCTION(BlueprintPure, Category = "Anim")
    float GetGlobalPlayRate() const { return GlobalPlayRate; }

	UFUNCTION(BlueprintCallable, Category = "Anim")
	void SetPlayRate(float NewRate);

	UFUNCTION(BlueprintPure, Category = "Anim")
	float GetPlayRate() const { return PlayRate; }

	UFUNCTION(BlueprintPure, Category = "Anim")
	float GetEffectivePlayRate() const { return GlobalPlayRate * PlayRate; }

	// Helpers for timing/frames (for BP hit windows, timers)
	UFUNCTION(BlueprintPure, Category = "Anim")
	int32 GetCurrentFrameIndex() const;

	UFUNCTION(BlueprintPure, Category = "Anim")
	int32 GetMasterNumFrames() const;

	UFUNCTION(BlueprintPure, Category = "Anim")
	float GetMasterTotalDurationSeconds() const;

    UFUNCTION(BlueprintPure, Category = "Anim")
    EPaperDollAnimState GetCurrentAnimState() const { return CurrentAnimState; }

	UFUNCTION(BlueprintPure, Category = "Anim")
	bool IsFrameInRange(int32 FromInclusive, int32 ToInclusive) const;

	// Attack held helpers (server authority). Supports basic or bend attack; does not restart mid-swing.
	UFUNCTION(BlueprintCallable, Category = "Anim|Attack", meta=(BlueprintAuthorityOnly))
	void StartAttackHeld(float PlayRateMultiplier, int32 VariantIndex = -1);

	// New: explicit attack state (basic or bend). No restart mid-swing if already running.
	UFUNCTION(BlueprintCallable, Category = "Anim|Attack", meta=(BlueprintAuthorityOnly))
	void StartAttackHeldWithState(float PlayRateMultiplier, EPaperDollAnimState AttackState = EPaperDollAnimState::BasicAttack, int32 VariantIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Anim|Attack", meta=(BlueprintAuthorityOnly))
	void StopAttackHeld();

    UFUNCTION(BlueprintCallable, Category = "Sorting")
    void RefreshSorting();

    // Optional mapping from slot tags (e.g., Helmet, CapeBack) to parent part names (Torso, Head, ArmNear, ArmFar, HandNear, HandFar, LegNear, LegFar).
    // If not provided for a tag, a simple name-based heuristic is used to pick a parent.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equip")
    TMap<FName, FName> SlotToParentPart;

    // Optional per-slot per-state flipbook mapping. When state changes, equipment flipbooks are swapped automatically from this map if present.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equip")
    TMap<FName, FEquipStateFlipbooks> EquipFlipbooksBySlot;

	// Optional per-state overrides for which body part a slot should attach to
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equip")
	TMap<EPaperDollAnimState, FSlotParentMap> StateSlotParentOverrides;

    // One-call helper: registers mapping, ensures component, applies the current state's flipbook (always creates if missing)
    UFUNCTION(BlueprintCallable, Category = "Equip")
    UPaperFlipbookComponent* EquipOrSwapFlipbooks(FName SlotTag, const TMap<EPaperDollAnimState, UPaperFlipbook*>& MappingByState);

    // Lookup: find an equipped flipbook component by its slot tag/socket name
    UFUNCTION(BlueprintPure, Category = "Equip")
    bool FindEquipComponentBySlot(FName SlotTag, UPaperFlipbookComponent*& OutComponent) const;

	// Manage state-based parent overrides from BP
	UFUNCTION(BlueprintCallable, Category = "Equip")
	void SetSlotParentOverrideForState(EPaperDollAnimState State, FName SlotTag, FName ParentPartName);

	UFUNCTION(BlueprintCallable, Category = "Equip")
	void ClearSlotParentOverrideForState(EPaperDollAnimState State, FName SlotTag);

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
    void ApplyEquipFlipbooksForCurrentState();

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

    // Sorting rules asset (optional)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting")
    UPaperDollSortRules* SortRules = nullptr;

private:
	// Held attack loop
	bool bAttackHeld = false;
	FTimerHandle AttackCycleTimer;
	void HandleAttackCycleEnd();

    // Internal helper retained (non-UFUNCTION):
    UPaperFlipbookComponent* EquipOrSwapFlipbook(FName SlotTag, UPaperFlipbook* Flipbook, bool bCreateIfMissing = true);
    void InitializeSlotToParentPartDefaults();

	// Climb anim selection (server only; default ladder)
	EClimbAnimType ClimbAnimType = EClimbAnimType::Ladder;

	// Held attack state tracking (basic or bend)
	EPaperDollAnimState HeldAttackState = EPaperDollAnimState::BasicAttack;

public:
	UFUNCTION(BlueprintCallable, Category = "Sorting", meta = (BlueprintAuthorityOnly))
	void AssignSortBucket_ServerOnly(int32 InBucket) { if (HasAuthority()) { SortBucketId = InBucket; OnRep_SortBucketId(); } }
};



#include "PaperDoll2DCharacter.h"
#include "Net/UnrealNetwork.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "CustomPlayerState.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"

// Forward declare local helpers used before definition
static UPaperFlipbookComponent* FindParentForSlot(APaperDoll2DCharacter* Self, const FName& SlotTag);

APaperDoll2DCharacter::APaperDoll2DCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SetReplicates(true);

	// Hide the default skeletal mesh from ACharacter; we render via Paper2D flipbooks
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetVisibility(false, true);
		SkelMesh->SetHiddenInGame(true);
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkelMesh->SetGenerateOverlapEvents(false);
		SkelMesh->SetComponentTickEnabled(false);
	}

	// Create components
	Torso = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Torso"));
	Head = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Head"));
	ArmNear = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("ArmNear"));
	ArmFar = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("ArmFar"));
	LegNear = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("LegNear"));
	LegFar = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("LegFar"));
	HandNear = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("HandNear"));
	HandFar  = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("HandFar"));

	// Attach to root (capsule). Users can reposition in BP
	Torso->SetupAttachment(GetRootComponent());
	Head->SetupAttachment(Torso);
	ArmNear->SetupAttachment(Torso);
	ArmFar->SetupAttachment(Torso);
	LegNear->SetupAttachment(Torso);
	LegFar->SetupAttachment(Torso);
	HandNear->SetupAttachment(Torso);
	HandFar->SetupAttachment(Torso);

	Torso->SetIsReplicated(true);
	Head->SetIsReplicated(true);
	ArmNear->SetIsReplicated(true);
	ArmFar->SetIsReplicated(true);
	LegNear->SetIsReplicated(true);
	LegFar->SetIsReplicated(true);
	HandNear->SetIsReplicated(true);
	HandFar->SetIsReplicated(true);

	// Paper2D components don't auto-run on network; we'll drive frames manually
	Torso->SetLooping(true);
	Head->SetLooping(true);
	ArmNear->SetLooping(true);
	ArmFar->SetLooping(true);
	LegNear->SetLooping(true);
	LegFar->SetLooping(true);
	HandNear->SetLooping(true);
	HandFar->SetLooping(true);

	Torso->SetPlayRate(0.0f);
	Head->SetPlayRate(0.0f);
	ArmNear->SetPlayRate(0.0f);
	ArmFar->SetPlayRate(0.0f);
	LegNear->SetPlayRate(0.0f);
	LegFar->SetPlayRate(0.0f);
	HandNear->SetPlayRate(0.0f);
	HandFar->SetPlayRate(0.0f);

	// Pre-populate SlotToParentPart with common slot names (keys only, values set in Blueprint)
	InitializeSlotToParentPartDefaults();
}

void APaperDoll2DCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplySortPriorities();
	UpdateFlipbooksForCurrentState();
}

void APaperDoll2DCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Ensure skeletal mesh remains hidden in editor preview too
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetVisibility(false, true);
		SkelMesh->SetHiddenInGame(true);
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkelMesh->SetGenerateOverlapEvents(false);
		SkelMesh->SetComponentTickEnabled(false);
	}
}

void APaperDoll2DCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePlaybackFrame(DeltaSeconds);
}

void APaperDoll2DCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APaperDoll2DCharacter, CurrentAnimState);
    DOREPLIFETIME(APaperDoll2DCharacter, CurrentAnimVariantIndex);
    DOREPLIFETIME(APaperDoll2DCharacter, SortBucketId);
}

void APaperDoll2DCharacter::PlayAnimationState(EPaperDollAnimState NewState, bool bResetTime) // resetTime - if want to start the animation from frame 0
{
	if (HasAuthority())
	{
		CurrentAnimState = NewState;
        CurrentAnimVariantIndex = -1; // default variant
        if (bResetTime) { CurrentAnimTimeSeconds = 0.0f; }
		UpdateFlipbooksForCurrentState();
	}
	else
	{
		// Clients should not authoritatively change state; but local prediction could go here
	}
}

void APaperDoll2DCharacter::PlayAnimationStateEx(EPaperDollAnimState NewState, int32 VariantIndex, bool bResetTime) // Ex - if the anim has many variants (for example basicatk)
{
    if (HasAuthority())
    {
        CurrentAnimState = NewState;
        CurrentAnimVariantIndex = VariantIndex;
        if (bResetTime) { CurrentAnimTimeSeconds = 0.0f; }
        UpdateFlipbooksForCurrentState();
    }
}

void APaperDoll2DCharacter::OnRep_CurrentAnimState()
{
	CurrentAnimTimeSeconds = 0.0f;
	UpdateFlipbooksForCurrentState();
	ApplySortPriorities();
}

void APaperDoll2DCharacter::UpdateFlipbooksForCurrentState()
{
    const FPaperDollVariants* VariantsWrap = AnimSets.Find(CurrentAnimState);
    if (!VariantsWrap || VariantsWrap->Variants.Num() == 0)
	{
		return;
	}

    int32 VariantIndex = CurrentAnimVariantIndex;
    if (VariantIndex < 0 || VariantIndex >= VariantsWrap->Variants.Num())
    {
        VariantIndex = 0;
        if (VariantsWrap->Variants.Num() > 1 && CurrentAnimState == EPaperDollAnimState::BasicAttack)
        {
            VariantIndex = FMath::RandRange(0, VariantsWrap->Variants.Num() - 1);
            CurrentAnimVariantIndex = VariantIndex; // lock-in on server
        }
    }

    const FPaperDollStateFlipbooks& SetRef = VariantsWrap->Variants[VariantIndex];

	Torso->SetFlipbook(SetRef.Torso);
	Head->SetFlipbook(SetRef.Head);
	ArmNear->SetFlipbook(SetRef.ArmNear);
	ArmFar->SetFlipbook(SetRef.ArmFar);
	LegNear->SetFlipbook(SetRef.LegNear);
	LegFar->SetFlipbook(SetRef.LegFar);
	HandNear->SetFlipbook(SetRef.HandNear);
	HandFar->SetFlipbook(SetRef.HandFar);

    // Apply equipment flipbooks for this state if mapping is provided
    ApplyEquipFlipbooksForCurrentState();
}

int32 APaperDoll2DCharacter::ComputeFrameIndex(const UPaperFlipbook* Master, float TimeSeconds) const
{
	if (!Master || Master->GetNumFrames() <= 0)
	{
		return 0;
	}
	const float Duration = Master->GetTotalDuration();
	if (Duration <= 0.0f)
	{
		return 0;
	}
	const float EffectiveRate = FMath::Clamp(GlobalPlayRate * PlayRate, 0.0f, 10.0f);
	const float Normalized = FMath::Fmod(FMath::Max(0.0f, TimeSeconds * EffectiveRate), Duration);
	const int32 FrameIndex = Master->GetKeyFrameIndexAtTime(Normalized);
	return FMath::Clamp(FrameIndex, 0, Master->GetNumFrames() - 1);
}

UPaperFlipbook* APaperDoll2DCharacter::GetMasterFlipbook() const
{
    const FPaperDollVariants* VariantsWrap = AnimSets.Find(CurrentAnimState);
    if (!VariantsWrap || VariantsWrap->Variants.Num() == 0)
	{
		return nullptr;
	}
    int32 VariantIndex = CurrentAnimVariantIndex;
    if (VariantIndex < 0 || VariantIndex >= VariantsWrap->Variants.Num())
    {
        VariantIndex = 0;
    }
    const FPaperDollStateFlipbooks& SetRef = VariantsWrap->Variants[VariantIndex];

    // Prefer torso as time master; fallback to first valid
    if (bUseTorsoAsTimeMaster && SetRef.Torso)
	{
        return SetRef.Torso;
	}
    if (SetRef.Head) return SetRef.Head;
    if (SetRef.ArmNear) return SetRef.ArmNear;
    if (SetRef.ArmFar) return SetRef.ArmFar;
    if (SetRef.LegNear) return SetRef.LegNear;
    if (SetRef.LegFar) return SetRef.LegFar;
    if (SetRef.HandNear) return SetRef.HandNear;
    if (SetRef.HandFar) return SetRef.HandFar;
	return nullptr;
}

void APaperDoll2DCharacter::UpdatePlaybackFrame(float DeltaSeconds)
{
	UPaperFlipbook* Master = GetMasterFlipbook();
	if (!Master)
	{
		return;
	}
	CurrentAnimTimeSeconds += DeltaSeconds;
	const int32 Frame = ComputeFrameIndex(Master, CurrentAnimTimeSeconds);

    auto ApplySelf = [Frame](UPaperFlipbookComponent* Comp)
	{
		if (Comp && Comp->GetFlipbook())
		{
            // Clamp to the component's flipbook frame count (supports 1-frame equips)
            const int32 Num = Comp->GetFlipbook()->GetNumFrames();
            const int32 Clamped = (Num > 0) ? FMath::Clamp(Frame, 0, Num - 1) : 0;
            Comp->SetPlaybackPositionInFrames(Clamped, /*bFireEvents*/ false);
		}
	};

    auto ApplyWithChildren = [&](UPaperFlipbookComponent* Parent)
    {
        ApplySelf(Parent);
        if (!Parent) return;
        const TArray<USceneComponent*>& Children = Parent->GetAttachChildren();
        for (USceneComponent* Child : Children)
        {
            if (UPaperFlipbookComponent* Eq = Cast<UPaperFlipbookComponent>(Child))
            {
                ApplySelf(Eq);
            }
        }
    };

    ApplyWithChildren(Torso);
    ApplyWithChildren(Head);
    ApplyWithChildren(ArmNear);
    ApplyWithChildren(ArmFar);
    ApplyWithChildren(LegNear);
    ApplyWithChildren(LegFar);
    ApplyWithChildren(HandNear);
    ApplyWithChildren(HandFar);
}

void APaperDoll2DCharacter::ApplySortPriorities() const
{
	// Local pawn renders with base priorities; remote pawns are offset by bucket
	const bool bIsLocalView = IsLocallyControlled();
	const int32 BucketBase = bIsLocalView ? 0 : (SortBucketId * GlobalBucketStride);

    // Resolve base priorities from SortRules if provided, else use defaults
    int32 TorsoBase = BasePriority_Torso;
    int32 HeadBase = BasePriority_Head;
    int32 ArmNearBase = BasePriority_Torso + 1;
    int32 ArmFarBase  = BasePriority_Torso - 1;
    int32 LegNearBase = BasePriority_Torso + 1;
    int32 LegFarBase  = BasePriority_Torso - 1;
    int32 HandNearBase = BasePriority_Torso + 2;
    int32 HandFarBase  = BasePriority_Torso - 2;

    if (SortRules)
    {
        if (const FPaperDollPartPriorities* P = SortRules->PartPriorities.Find(CurrentAnimState))
        {
            TorsoBase = P->Torso;
            HeadBase = P->Head;
            ArmNearBase = P->ArmNear;
            ArmFarBase = P->ArmFar;
            LegNearBase = P->LegNear;
            LegFarBase = P->LegFar;
            HandNearBase = P->HandNear;
            HandFarBase = P->HandFar;
        }
    }

    const int32 TorsoFinal = BucketBase + TorsoBase;
    const int32 HeadFinal = BucketBase + HeadBase;
    const int32 ArmNearFinal = BucketBase + ArmNearBase;
    const int32 ArmFarFinal  = BucketBase + ArmFarBase;
    const int32 LegNearFinal = BucketBase + LegNearBase;
    const int32 LegFarFinal  = BucketBase + LegFarBase;
    const int32 HandNearFinal = BucketBase + HandNearBase;
    const int32 HandFarFinal  = BucketBase + HandFarBase;

    if (Torso) Torso->SetTranslucentSortPriority(TorsoFinal);
    if (Head) Head->SetTranslucentSortPriority(HeadFinal);
    if (ArmNear) ArmNear->SetTranslucentSortPriority(ArmNearFinal);
    if (ArmFar) ArmFar->SetTranslucentSortPriority(ArmFarFinal);
    if (LegNear) LegNear->SetTranslucentSortPriority(LegNearFinal);
    if (LegFar) LegFar->SetTranslucentSortPriority(LegFarFinal);
    if (HandNear) HandNear->SetTranslucentSortPriority(HandNearFinal);
    if (HandFar)  HandFar->SetTranslucentSortPriority(HandFarFinal);

    // Apply equipment offsets under each body part by component tag (slot name)
    auto ApplyChildren = [&](UPaperFlipbookComponent* ParentComp, int32 ParentPriority)
    {
        if (!ParentComp) return;
        const TArray<USceneComponent*>& AttachedChildren = ParentComp->GetAttachChildren();
        for (USceneComponent* Child : AttachedChildren)
        {
            if (UPaperFlipbookComponent* Eq = Cast<UPaperFlipbookComponent>(Child))
            {
                bool bUseAbsolute = false;
                int32 Value = 0;
                bool bHasPerFrame = false;
                if (SortRules)
                {
                    // Use first tag as slot key if present
                    FName SlotTag;
                    if (Eq->ComponentTags.Num() > 0)
                    {
                        SlotTag = Eq->ComponentTags[0];
                    }
                    const FEquipmentSortPerState* Rule = SlotTag.IsNone() ? nullptr : SortRules->EquipmentRules.Find(SlotTag);
                    if (Rule)
                    {
                        const int32* PerState = Rule->PerStateOverrides.Find(CurrentAnimState);
                        bUseAbsolute = Rule->bAbsolute;
                        Value = PerState ? *PerState : Rule->DefaultValue;
                        // Per-frame override support
                        if (const FPerFramePriority* PerFrame = Rule->PerFrameOverrides.Find(CurrentAnimState))
                        {
                            if (PerFrame->Values.Num() > 0)
                            {
                                bHasPerFrame = true;
                                if (UPaperFlipbook* Master = GetMasterFlipbook())
                                {
                                    const int32 Frame = ComputeFrameIndex(Master, CurrentAnimTimeSeconds);
                                    const int32 Index = Frame % PerFrame->Values.Num();
                                    Value = PerFrame->Values[Index];
                                }
                            }
                        }
                    }
                }
                if (bUseAbsolute)
                {
                    // Absolute value is relative to bucket base already? We'll still add BucketBase to keep per-observer shift
                    Eq->SetTranslucentSortPriority(BucketBase + Value);
                }
                else
                {
                    Eq->SetTranslucentSortPriority(ParentPriority + Value);
                }
            }
        }
    };

    ApplyChildren(Torso, TorsoFinal);
    ApplyChildren(Head, HeadFinal);
    ApplyChildren(ArmNear, ArmNearFinal);
    ApplyChildren(ArmFar, ArmFarFinal);
    ApplyChildren(LegNear, LegNearFinal);
    ApplyChildren(LegFar, LegFarFinal);
    ApplyChildren(HandNear, HandNearFinal);
    ApplyChildren(HandFar,  HandFarFinal);
}

void APaperDoll2DCharacter::OnRep_SortBucketId()
{
	ApplySortPriorities();
}

void APaperDoll2DCharacter::SetGlobalPlayRate(float NewRate)
{
	GlobalPlayRate = FMath::Clamp(NewRate, 0.0f, 10.0f);
}

void APaperDoll2DCharacter::RefreshSorting()
{
	ApplySortPriorities();
}

void APaperDoll2DCharacter::ApplyEquipFlipbooksForCurrentState()
{
	if (EquipFlipbooksBySlot.Num() == 0) return;

	for (const auto& Pair : EquipFlipbooksBySlot)
	{
		const FName SlotTag = Pair.Key;
		const FEquipStateFlipbooks& Map = Pair.Value;

		UPaperFlipbookComponent* Eq = nullptr;
		FindEquipComponentBySlot(SlotTag, Eq);
		if (!Eq)
		{
			// Ensure the component exists if mapping is present
			Eq = EquipOrSwapFlipbook(SlotTag, nullptr, true);
		}
		if (!Eq) continue;

		// Ensure correct parent for this state (supports dynamic re-parenting e.g., climb)
		if (UPaperFlipbookComponent* DesiredParent = FindParentForSlot(this, SlotTag))
		{
			if (Eq->GetAttachParent() != DesiredParent)
			{
				if (DesiredParent->DoesSocketExist(SlotTag))
				{
					Eq->AttachToComponent(DesiredParent, FAttachmentTransformRules::KeepRelativeTransform, SlotTag);
				}
				else
				{
					Eq->AttachToComponent(DesiredParent, FAttachmentTransformRules::KeepRelativeTransform);
				}
			}
		}

		// Apply flipbook for current state
		if (UPaperFlipbook* const* FB = Map.ByState.Find(CurrentAnimState))
		{
			Eq->SetFlipbook(*FB);
		}
	}
}

UPaperFlipbookComponent* APaperDoll2DCharacter::EquipOrSwapFlipbooks(FName SlotTag, const TMap<EPaperDollAnimState, UPaperFlipbook*>& MappingByState)
{
    // Store mapping
    FEquipStateFlipbooks Mapping;
    Mapping.ByState = MappingByState;
    EquipFlipbooksBySlot.Add(SlotTag, Mapping);
    // Ensure component
    UPaperFlipbookComponent* Comp = EquipOrSwapFlipbook(SlotTag, nullptr, true);
    // Apply current state's flipbook now
    ApplyEquipFlipbooksForCurrentState();
    // Safe to refresh sorting (cheap)
    RefreshSorting();
    return Comp;
}

bool APaperDoll2DCharacter::FindEquipComponentBySlot(FName SlotTag, UPaperFlipbookComponent*& OutComponent) const
{
    OutComponent = nullptr;
    if (SlotTag.IsNone())
    {
        return false;
    }

    auto FindUnderParent = [&](UPaperFlipbookComponent* Parent) -> UPaperFlipbookComponent*
    {
        if (!Parent) return nullptr;
        const TArray<USceneComponent*>& AttachedChildren = Parent->GetAttachChildren();
        for (USceneComponent* Child : AttachedChildren)
        {
            if (UPaperFlipbookComponent* Eq = Cast<UPaperFlipbookComponent>(Child))
            {
                const bool bTagMatch = (Eq->ComponentTags.Num() > 0 && Eq->ComponentTags[0] == SlotTag);
                const bool bSocketMatch = (Eq->GetAttachSocketName() == SlotTag);
                if (bTagMatch || bSocketMatch)
                {
                    return Eq;
                }
            }
        }
        return nullptr;
    };

    // Prefer the mapped/heuristic parent first
    UPaperFlipbookComponent* MappedParent = FindParentForSlot(const_cast<APaperDoll2DCharacter*>(this), SlotTag);
    if (UPaperFlipbookComponent* Found = FindUnderParent(MappedParent))
    {
        OutComponent = Found;
        return true;
    }

    // Fallback: search all body part parents
    UPaperFlipbookComponent* Parents[] = { Torso, Head, ArmNear, ArmFar, HandNear, HandFar, LegNear, LegFar };
    for (UPaperFlipbookComponent* P : Parents)
    {
        if (UPaperFlipbookComponent* Found = FindUnderParent(P))
        {
            OutComponent = Found;
            return true;
        }
    }

    return false;
}

void APaperDoll2DCharacter::SetSlotParentOverrideForState(EPaperDollAnimState State, FName SlotTag, FName ParentPartName)
{
	StateSlotParentOverrides.FindOrAdd(State).Map.Add(SlotTag, ParentPartName);
	if (State == CurrentAnimState)
	{
		ApplyEquipFlipbooksForCurrentState();
		RefreshSorting();
	}
}

void APaperDoll2DCharacter::ClearSlotParentOverrideForState(EPaperDollAnimState State, FName SlotTag)
{
	if (FSlotParentMap* M = StateSlotParentOverrides.Find(State))
	{
		M->Map.Remove(SlotTag);
	}
	if (State == CurrentAnimState)
	{
		ApplyEquipFlipbooksForCurrentState();
		RefreshSorting();
	}
}

static UPaperFlipbookComponent* ResolveParentByName(APaperDoll2DCharacter* Self, const FName& Name)
{
    if (!Self) return nullptr;
    if (Name == "Torso") return Self->Torso;
    if (Name == "Head") return Self->Head;
    if (Name == "ArmNear") return Self->ArmNear;
    if (Name == "ArmFar") return Self->ArmFar;
    if (Name == "HandNear") return Self->HandNear;
    if (Name == "HandFar") return Self->HandFar;
    if (Name == "LegNear") return Self->LegNear;
    if (Name == "LegFar") return Self->LegFar;
    return Self->Torso;
}

static UPaperFlipbookComponent* FindParentForSlot(APaperDoll2DCharacter* Self, const FName& SlotTag)
{
    if (!Self) return nullptr;
    // Check per-state override first
    if (const FSlotParentMap* StateMap = Self->StateSlotParentOverrides.Find(Self->GetCurrentAnimState()))
    {
        if (const FName* OverrideParentName = StateMap->Map.Find(SlotTag))
        {
            if (UPaperFlipbookComponent* C = ResolveParentByName(Self, *OverrideParentName))
            {
                return C;
            }
        }
    }
    // Prefer explicit mapping
    if (Self->SlotToParentPart.Contains(SlotTag))
    {
        if (UPaperFlipbookComponent* C = ResolveParentByName(Self, Self->SlotToParentPart[SlotTag]))
        {
            return C;
        }
    }
    // Try sockets by name across parts
    {
        UPaperFlipbookComponent* Parts[] = { Self->Head, Self->HandNear, Self->HandFar, Self->ArmNear, Self->ArmFar, Self->LegNear, Self->LegFar, Self->Torso };
        for (UPaperFlipbookComponent* P : Parts)
        {
            if (P && P->DoesSocketExist(SlotTag)) return P;
        }
    }
    return nullptr; // let caller fallback to heuristic
}

static bool NameContains(const FName& Tag, const TCHAR* Sub)
{
    return Tag.ToString().Contains(Sub, ESearchCase::IgnoreCase);
}

UPaperFlipbookComponent* APaperDoll2DCharacter::EquipOrSwapFlipbook(FName SlotTag, UPaperFlipbook* Flipbook, bool bCreateIfMissing)
{
    // Decide parent by mapping first, then by simple heuristics
    UPaperFlipbookComponent* Parent = FindParentForSlot(this, SlotTag);
    if (!Parent)
    {
        if (NameContains(SlotTag, TEXT("Head")) || NameContains(SlotTag, TEXT("Hair")) || NameContains(SlotTag, TEXT("Helmet")) || NameContains(SlotTag, TEXT("Earring")))
        {
            Parent = Head;
        }
        else if (NameContains(SlotTag, TEXT("HandNear")) || NameContains(SlotTag, TEXT("GloveNear")) || NameContains(SlotTag, TEXT("RingNear")) || NameContains(SlotTag, TEXT("WeaponNear")))
        {
            Parent = HandNear;
        }
        else if (NameContains(SlotTag, TEXT("HandFar")) || NameContains(SlotTag, TEXT("GloveFar")) || NameContains(SlotTag, TEXT("RingFar")) || NameContains(SlotTag, TEXT("WeaponFar")) || NameContains(SlotTag, TEXT("ShieldFar")))
        {
            Parent = HandFar;
        }
        else if (NameContains(SlotTag, TEXT("LegNear")) || NameContains(SlotTag, TEXT("PantsNear")) || NameContains(SlotTag, TEXT("BottomNear")) || NameContains(SlotTag, TEXT("ShoeNear")))
        {
            Parent = LegNear;
        }
        else if (NameContains(SlotTag, TEXT("LegFar")) || NameContains(SlotTag, TEXT("PantsFar")) || NameContains(SlotTag, TEXT("BottomFar")) || NameContains(SlotTag, TEXT("ShoeFar")))
        {
            Parent = LegFar;
        }
        else
        {
            // Cape/Aura/Coat default to Torso
            Parent = Torso;
        }
    }
    if (!Parent)
    {
        return nullptr;
    }

    // Find existing component with this tag under the parent
    UPaperFlipbookComponent* Target = nullptr;
    const TArray<USceneComponent*>& AttachedChildren = Parent->GetAttachChildren();
    for (USceneComponent* Child : AttachedChildren)
    {
        if (UPaperFlipbookComponent* Eq = Cast<UPaperFlipbookComponent>(Child))
        {
            if (Eq->ComponentTags.Num() > 0 && Eq->ComponentTags[0] == SlotTag)
            {
                Target = Eq;
                break;
            }
        }
    }

    bool bCreatedOrRetagged = false;

    if (!Target && bCreateIfMissing)
    {
        Target = NewObject<UPaperFlipbookComponent>(this);
        if (!Target) return nullptr;
        // Attach to socket if present; otherwise to the parent with no socket
        if (Parent->DoesSocketExist(SlotTag))
        {
            Target->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, SlotTag);
        }
        else
        {
            Target->SetupAttachment(Parent);
        }
        Target->RegisterComponent();
        Target->SetIsReplicated(false);
        Target->SetLooping(true);
        Target->SetPlayRate(0.0f);
        Target->ComponentTags.Reset();
        Target->ComponentTags.Add(SlotTag);
        bCreatedOrRetagged = true;
    }
    else if (Target && (Target->ComponentTags.Num() == 0 || Target->ComponentTags[0] != SlotTag))
    {
        Target->ComponentTags.Reset();
        Target->ComponentTags.Add(SlotTag);
        bCreatedOrRetagged = true;
    }
    // Ensure correct socket attachment if the parent now exposes the socket
    if (Target && Parent->DoesSocketExist(SlotTag))
    {
        Target->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, SlotTag);
    }

    if (Target)
    {
        Target->SetFlipbook(Flipbook);
        if (bCreatedOrRetagged)
        {
            RefreshSorting();
        }
    }
    return Target;
}
int32 APaperDoll2DCharacter::GetCurrentFrameIndex() const
{
	if (UPaperFlipbook* Master = GetMasterFlipbook())
	{
		return ComputeFrameIndex(Master, CurrentAnimTimeSeconds);
	}
	return 0;
}

int32 APaperDoll2DCharacter::GetMasterNumFrames() const
{
	if (UPaperFlipbook* Master = GetMasterFlipbook())
	{
		return Master->GetNumFrames();
	}
	return 0;
}

float APaperDoll2DCharacter::GetMasterTotalDurationSeconds() const
{
	if (UPaperFlipbook* Master = GetMasterFlipbook())
	{
		return Master->GetTotalDuration();
	}
	return 0.0f;
}

bool APaperDoll2DCharacter::IsFrameInRange(int32 FromInclusive, int32 ToInclusive) const
{
	const int32 F = GetCurrentFrameIndex();
	return F >= FromInclusive && F <= ToInclusive;
}

void APaperDoll2DCharacter::SetPlayRate(float NewRate)
{
	PlayRate = FMath::Clamp(NewRate, 0.0f, 10.0f);
}

void APaperDoll2DCharacter::StartAttackHeld(float PlayRateMultiplier, int32 VariantIndex)
{
	// Backward-compatible wrapper: defaults to BasicAttack; if VariantIndex < 0, randomize for basic.
	StartAttackHeldWithState(PlayRateMultiplier, EPaperDollAnimState::BasicAttack, VariantIndex);
}

void APaperDoll2DCharacter::StopAttackHeld()
{
	if (!HasAuthority()) return;
	bAttackHeld = false;
}

void APaperDoll2DCharacter::StartAttackHeldWithState(float PlayRateMultiplier, EPaperDollAnimState AttackState, int32 VariantIndex)
{
	if (!HasAuthority()) return;
	SetGlobalPlayRate(PlayRateMultiplier);

	// If an attack is already running, keep it held and update desired state; do not restart mid-swing.
	if (bAttackHeld)
	{
		HeldAttackState = AttackState;
		bAttackHeld = true;
		return;
	}

	// Fresh start
	HeldAttackState = AttackState;
	bAttackHeld = true;

	// Resolve variant
	int32 ResolvedVariant = VariantIndex;
	if (AttackState == EPaperDollAnimState::BasicAttack)
	{
		if (ResolvedVariant < 0)
		{
			if (const FPaperDollVariants* VariantsWrap = AnimSets.Find(EPaperDollAnimState::BasicAttack))
			{
				if (VariantsWrap->Variants.Num() > 1)
				{
					ResolvedVariant = FMath::RandRange(0, VariantsWrap->Variants.Num() - 1);
				}
			}
		}
	}
	else if (AttackState == EPaperDollAnimState::BendAttack)
	{
		ResolvedVariant = -1; // bend attack single/default
	}

	PlayAnimationStateEx(AttackState, ResolvedVariant, true);
	const float Dur = GetMasterTotalDurationSeconds() / FMath::Max(0.001f, GlobalPlayRate);
	GetWorldTimerManager().SetTimer(AttackCycleTimer, this, &APaperDoll2DCharacter::HandleAttackCycleEnd, Dur, false);
}

void APaperDoll2DCharacter::HandleAttackCycleEnd()
{
	if (!HasAuthority()) return;
	if (bAttackHeld)
	{
		int32 VariantIndex = -1;
		if (HeldAttackState == EPaperDollAnimState::BasicAttack)
		{
			if (const FPaperDollVariants* VariantsWrap = AnimSets.Find(EPaperDollAnimState::BasicAttack))
			{
				if (VariantsWrap->Variants.Num() > 1)
				{
					VariantIndex = FMath::RandRange(0, VariantsWrap->Variants.Num() - 1);
				}
			}
		}
		PlayAnimationStateEx(HeldAttackState, VariantIndex, true);
		const float Dur = GetMasterTotalDurationSeconds() / FMath::Max(0.001f, GlobalPlayRate);
		GetWorldTimerManager().SetTimer(AttackCycleTimer, this, &APaperDoll2DCharacter::HandleAttackCycleEnd, Dur, false);
	}
}

// Simple priority table (higher wins)
static int32 GetAnimPriority(EPaperDollAnimState State)
{
	switch (State)
	{
	case EPaperDollAnimState::ClimbLadder:
	case EPaperDollAnimState::ClimbRope:    return 100;
	case EPaperDollAnimState::BasicAttack:  return 90;
	case EPaperDollAnimState::Jump:         return 80;
	case EPaperDollAnimState::BendAttack:   return 75;
	case EPaperDollAnimState::Bend:         return 70;
	case EPaperDollAnimState::Walk:         return 60;
	case EPaperDollAnimState::Alert:        return 50;
	case EPaperDollAnimState::Idle:         return 40;
	default:                                return 40;
	}
}

bool APaperDoll2DCharacter::RequestAnimationStateWithPriority(EPaperDollAnimState DesiredState, bool bInCombat, int32 VariantIndex, bool bResetTime)
{
	if (!HasAuthority()) return false;

	// Normalize low-priority requests to combat-aware fallback
	if (DesiredState == EPaperDollAnimState::Alert || DesiredState == EPaperDollAnimState::Idle)
	{
		DesiredState = bInCombat ? EPaperDollAnimState::Alert : EPaperDollAnimState::Idle;
	}

	// Climb guard: only Jump can override climb
	if ((CurrentAnimState == EPaperDollAnimState::ClimbLadder || CurrentAnimState == EPaperDollAnimState::ClimbRope) &&
		DesiredState != EPaperDollAnimState::Jump)
	{
		return false;
	}

	// Attack guard: cannot attack while climbing
	if (DesiredState == EPaperDollAnimState::BasicAttack &&
		(CurrentAnimState == EPaperDollAnimState::ClimbLadder || CurrentAnimState == EPaperDollAnimState::ClimbRope))
	{
		return false;
	}

	// Priority comparison
	const int32 CurrP = GetAnimPriority(CurrentAnimState);
	const int32 DesiredP = GetAnimPriority(DesiredState);

	// If desired is lower priority than current, ignore
	if (DesiredP < CurrP)
	{
		return false;
	}

	// Apply the state
	if (DesiredState == EPaperDollAnimState::BasicAttack)
	{
		PlayAnimationStateEx(DesiredState, VariantIndex, bResetTime);
	}
	else
	{
		PlayAnimationState(DesiredState, bResetTime);
	}
	return true;
}

void APaperDoll2DCharacter::PredictAnimationStateLocal(EPaperDollAnimState DesiredState, int32 VariantIndex, bool bResetTime)
{
	// Purely client-side prediction for responsiveness; server will override via replication.
	if (HasAuthority()) return;
	CurrentAnimState = DesiredState;
	CurrentAnimVariantIndex = VariantIndex;
	if (bResetTime)
	{
		CurrentAnimTimeSeconds = 0.0f;
	}
	UpdateFlipbooksForCurrentState();
	ApplySortPriorities();
}

void APaperDoll2DCharacter::SetClimbAnimType(EClimbAnimType InType)
{
	if (!HasAuthority()) return;
	ClimbAnimType = InType;
}

void APaperDoll2DCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (!HasAuthority())
	{
		return;
	}

	const EMovementMode NewMode = GetCharacterMovement() ? GetCharacterMovement()->MovementMode : MOVE_None;

	// Auto-drive unique movement-mode states:
	// - Falling maps to Jump animation (used for jump/fall)
	// - Flying is often used for climb; map to ClimbLadder by default (adjust if you distinguish rope vs ladder elsewhere)
	switch (NewMode)
	{
	case MOVE_Falling:
		RequestAnimationStateWithPriority(EPaperDollAnimState::Jump, /*bInCombat=*/false, /*VariantIndex=*/-1, /*bResetTime=*/true);
		break;
	case MOVE_Flying:
		// If you distinguish rope vs ladder, map accordingly
		if (ClimbAnimType == EClimbAnimType::Rope)
		{
			RequestAnimationStateWithPriority(EPaperDollAnimState::ClimbRope, /*bInCombat=*/false, /*VariantIndex=*/-1, /*bResetTime=*/true);
		}
		else
		{
			RequestAnimationStateWithPriority(EPaperDollAnimState::ClimbLadder, /*bInCombat=*/false, /*VariantIndex=*/-1, /*bResetTime=*/true);
		}
		break;
	default:
		break;
	}
}

void APaperDoll2DCharacter::InitializeSlotToParentPartDefaults()
{
	// Pre-populate SlotToParentPart with all slot names and default body part mappings
	// Values can be overridden in Blueprint if needed
	
	// Head slots
	SlotToParentPart.Add(TEXT("Hat"), TEXT("Head"));
	SlotToParentPart.Add(TEXT("FaceAcc"), TEXT("Head"));
	SlotToParentPart.Add(TEXT("EyeAcc"), TEXT("Head"));
	SlotToParentPart.Add(TEXT("FrontEarring"), TEXT("Head"));
	SlotToParentPart.Add(TEXT("BackEarring"), TEXT("Head"));
	
	// Torso slots
	SlotToParentPart.Add(TEXT("Top"), TEXT("Torso"));
	SlotToParentPart.Add(TEXT("FrontCape"), TEXT("Torso"));
	SlotToParentPart.Add(TEXT("BackCape"), TEXT("Torso"));
	SlotToParentPart.Add(TEXT("FrontAura"), TEXT("Torso"));
	SlotToParentPart.Add(TEXT("BackAura"), TEXT("Torso"));
	SlotToParentPart.Add(TEXT("Pendant"), TEXT("Torso"));
	
	// Arm slots (sleeves attach to arms, not torso)
	SlotToParentPart.Add(TEXT("FrontSleeve"), TEXT("ArmNear"));
	SlotToParentPart.Add(TEXT("BackSleeve"), TEXT("ArmFar"));
	
	// Hand slots
	SlotToParentPart.Add(TEXT("FrontGlove"), TEXT("HandNear"));
	SlotToParentPart.Add(TEXT("BackGlove"), TEXT("HandFar"));
	SlotToParentPart.Add(TEXT("Ring1"), TEXT("HandNear"));
	SlotToParentPart.Add(TEXT("Ring2"), TEXT("HandNear"));
	SlotToParentPart.Add(TEXT("Ring3"), TEXT("HandNear"));
	SlotToParentPart.Add(TEXT("Ring4"), TEXT("HandNear"));
	SlotToParentPart.Add(TEXT("Weapon"), TEXT("HandNear"));
	SlotToParentPart.Add(TEXT("Secondary"), TEXT("HandFar"));
	SlotToParentPart.Add(TEXT("Shield"), TEXT("HandFar"));
	
	// Leg slots (includes pants/bottoms - entire leg from pelvis down)
	SlotToParentPart.Add(TEXT("FrontBottom"), TEXT("LegNear"));
	SlotToParentPart.Add(TEXT("BackBottom"), TEXT("LegFar"));
	SlotToParentPart.Add(TEXT("FrontShoe"), TEXT("LegNear"));
	SlotToParentPart.Add(TEXT("BackShoe"), TEXT("LegFar"));
}
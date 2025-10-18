#include "PaperDoll2DCharacter.h"
#include "Net/UnrealNetwork.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

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

void APaperDoll2DCharacter::PlayAnimationState(EPaperDollAnimState NewState, bool bResetTime)
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

void APaperDoll2DCharacter::PlayAnimationStateEx(EPaperDollAnimState NewState, int32 VariantIndex, bool bResetTime)
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
	const float Normalized = FMath::Fmod(FMath::Max(0.0f, TimeSeconds * GlobalPlayRate), Duration);
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

    auto Apply = [Frame](UPaperFlipbookComponent* Comp)
	{
		if (Comp && Comp->GetFlipbook())
		{
            Comp->SetPlaybackPositionInFrames(Frame, /*bFireEvents*/ false);
		}
	};

    Apply(Torso);
    Apply(Head);
    Apply(ArmNear);
    Apply(ArmFar);
    Apply(LegNear);
    Apply(LegFar);
    Apply(HandNear);
    Apply(HandFar);
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
        const TArray<USceneComponent*>& Children = ParentComp->GetAttachChildren();
        for (USceneComponent* Child : Children)
        {
            if (UPaperFlipbookComponent* Eq = Cast<UPaperFlipbookComponent>(Child))
            {
                bool bUseAbsolute = false;
                int32 Value = 0;
                bool bHasPerFrame = false;
                if (SortRules)
                {
                    // Use first tag as slot key if present
                    FName SlotName;
                    if (Eq->ComponentTags.Num() > 0)
                    {
                        SlotName = Eq->ComponentTags[0];
                    }
                    const FEquipmentSortPerState* Rule = SlotName.IsNone() ? nullptr : SortRules->EquipmentRules.Find(SlotName);
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
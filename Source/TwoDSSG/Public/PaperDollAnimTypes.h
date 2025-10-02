#pragma once

#include "CoreMinimal.h"
#include "PaperDollAnimTypes.generated.h"

UENUM(BlueprintType)
enum class EPaperDollAnimState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Walk		UMETA(DisplayName = "Walk"),
	Bend		UMETA(DisplayName = "Bend"),
	BendAttack	UMETA(DisplayName = "BendAttack"),
	ClimbLadder	UMETA(DisplayName = "ClimbLadder"),
	ClimbRope	UMETA(DisplayName = "ClimbRope"),
	Alert		UMETA(DisplayName = "Alert"),
	Jump		UMETA(DisplayName = "Jump"),
	BasicAttack	UMETA(DisplayName = "BasicAttack")
};



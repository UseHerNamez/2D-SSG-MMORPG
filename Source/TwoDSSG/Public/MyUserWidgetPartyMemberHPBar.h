#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyUserWidgetPartyMemberHPBar.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class TWODSSG_API UMyUserWidgetPartyMemberHPBar : public UUserWidget
{
	GENERATED_BODY()
protected:
	UPROPERTY( meta = (BindWidget))
		class UProgressBar* HPBar;
	UPROPERTY(meta = (BindWidget))
		class UProgressBar* MPBar;
};

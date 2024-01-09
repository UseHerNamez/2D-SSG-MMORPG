#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoginErrorWidget.generated.h"
/**
 *
 */
UCLASS(Abstract)
class TWODSSG_API ULoginErrorWidget : public UUserWidget
{
	GENERATED_BODY()
public:
    // Default constructor

    // Constructor with FObjectInitializer
    ULoginErrorWidget(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Custom Functions")
    void SetErrorMessage(const FString& ErrorMessage);

protected:
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* ErrorMessageText;
};

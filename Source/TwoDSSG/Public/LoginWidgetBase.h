// ULoginWidgetBase.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoginWidgetBase.generated.h"

/**
 * 
 */
UCLASS()
class TWODSSG_API ULoginWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
    // Constructor
    ULoginWidgetBase(const FObjectInitializer& ObjectInitializer);

protected:
    // Reference to the login manager instance
    class ALoginManager* LoginManager;
    // Bind these functions to the respective buttons in the widget
    UFUNCTION(BlueprintCallable, Category = "Login")
    void OnLoginButtonReleased(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void OnRegisterButtonReleased(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void ShowErrorWidget(const FString& errorMessage);

    UFUNCTION(BlueprintCallable, Category = "Login")
    ALoginManager* InitializeLoginManager();
};

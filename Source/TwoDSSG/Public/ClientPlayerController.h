#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ClientPlayerController.generated.h"

UCLASS()
class TWODSSG_API AClientPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void ShowErrorWidget(const FString& ErrorMessage);
    void ShowLoadingWidget();

protected:
    virtual void BeginPlay() override;

private:
    void HandleReturnedFromServer(const FString& ErrorMessage);
};

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
        void ShowLoadingWidget();

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
        void HideLoadingWidget();

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
        void ShowErrorWidget(const FString& ErrorMessage);

    UFUNCTION(Client, Reliable)
        void RPC_ShowLoadingWidget();

    UFUNCTION(Client, Reliable)
        void RPC_HideLoadingWidget();
              
protected:

    virtual void BeginPlay() override;

private:

    void HandleReturnedFromServer(const FString& ErrorMessage);
};

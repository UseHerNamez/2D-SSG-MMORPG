// LoginManager.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
//#include "Net/UnrealNetwork.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include <Kismet/GameplayStatics.h>
//#include <Networking/Public/Interfaces/IPv4/IPv4Address.h>
//#include <SocketSubsystem.h>
#include "LoginErrorWidget.h"
#include "ComputerSaviourGameInstance.h"
#include "LoginManager.generated.h"

UCLASS()
class TWODSSG_API ALoginManager : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ALoginManager();

    UFUNCTION(BlueprintCallable, Category = "Login")
    void Login(const FString& Username, const FString& Password);
    void Register(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void ShowErrorWidget(const FString& ErrorMessage);

    UFUNCTION(BlueprintCallable)
    void SetErrorWidget(ULoginErrorWidget * i_ErrorWidget);

protected:
    // Reference to the login error widget instance
    class ULoginErrorWidget* ErrorWidget;

private:
    void HandleResponse(const FString& Response);
    void SendLoginRequest(const FString& RequestData, bool retry);
    void LoadGameLevelMap();
    bool RetryLogin(float DeltaTime);
    FTimerHandle TimerHandle;
    int maxAttemptsToConnect;
    float timeoutBetweenRequests;
    FString LastLoginRequestData;
    int32 NumAttempts;
    bool bSuccessfulRequest;
    FDelegateHandle TickerDelegateHandle;
};
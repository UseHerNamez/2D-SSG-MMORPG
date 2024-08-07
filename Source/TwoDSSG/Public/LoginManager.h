// LoginManager.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Http.h"
//#include "Net/UnrealNetwork.h"
#include "HttpModule.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IHttpResponse.h"
#include <Kismet/GameplayStatics.h>
//#include <Networking/Public/Interfaces/IPv4/IPv4Address.h>
//#include <SocketSubsystem.h>
#include "LoginErrorWidget.h"
#include "LoginManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCheckNameResponseReceived, bool, bNameAvailable, FString, message, bool, bIsCreation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnsomething);

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

    void SetPlayerController(APlayerController* i_PlayerController);
    void SetWorld();

    UFUNCTION(BlueprintCallable, Category = "Login")
    void CheckName(const FString& charName, const bool isCreation, const FString& charData);

    UFUNCTION(BlueprintCallable, Category = "Test")
    void TestLogin();

    // Declare the delegate as a BlueprintAssignable property
    UPROPERTY(BlueprintAssignable, Category = "Login")
    FOnCheckNameResponseReceived OnCheckNameResponseReceived;

    UPROPERTY(BlueprintAssignable, Category = "Login")
    FOnsomething OnSomething;

protected:
    // Reference to the login error widget instance
    class ULoginErrorWidget* ErrorWidget;

private:
    APlayerController* playerController;
    UWorld* World;

    void HandleResponse(const FString& Response);
    void SendLoginRequest(const FString& RequestData, bool isRetry);
    void SendCheckNameRequest(const FString& RequestData, const bool isCreation, bool isRetry);
    void HandleCheckNameResponse(const FString& Response, const bool isCreation);
    void LoadGameLevelMap();
    FTimerHandle TimerHandle;
    int maxAttemptsToConnect;
    float timeoutBetweenRequests;
    FString LastLoginRequestData;
    int32 NumAttempts;
    bool bSuccessfulRequest, bCharacterNameAvailable;
    FDelegateHandle TickerDelegateHandle;
};
// LoginManager.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Http.h"
#include "HttpModule.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IHttpResponse.h"
#include <Kismet/GameplayStatics.h>
#include "Delegates/DelegateCombinations.h"
#include "LoginErrorWidget.h"
class UComputerSaviourGameInstance;
#include "LoginManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCheckNameResponseReceived, bool, bNameAvailable, FString, message, bool, bIsCreation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeleteCharResponse, FString, Response);


UCLASS()
class TWODSSG_API ULoginManager : public UObject
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ULoginManager();

    UFUNCTION(BlueprintCallable, Category = "Login")
    void Login(const FString& Username, const FString& Password);
    void Register(const FString& Username, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void ShowErrorWidget(const FString& ErrorMessage);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void DeleteCharFromDb(const FString& charName);

    UFUNCTION(BlueprintCallable)
    void SetErrorWidget(ULoginErrorWidget * i_ErrorWidget);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void CheckName(const FString& charName, const bool isCreation, const FString& charData);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void SelectCharacter(const FString& charName);

    UFUNCTION(BlueprintCallable, Category = "Test")
    void TestLogin();

    void setCSGameInstance(UComputerSaviourGameInstance* gameInstance);
    UComputerSaviourGameInstance* GetGameInstance();

    // Declare the delegate as a BlueprintAssignable property
    UPROPERTY(BlueprintAssignable, Category = "Login")
    FOnCheckNameResponseReceived OnCheckNameResponseReceived;

    UPROPERTY(BlueprintAssignable, Category = "Login")
    FOnDeleteCharResponse OnDeleteCharResponse;

protected:
    // Reference to the login error widget instance
    class ULoginErrorWidget* ErrorWidget;

private:
    UComputerSaviourGameInstance* ComputerSaviourGameInstance;
    const FString LoginServerURL = TEXT("https://localhost:8443");
    const FString DefaultServerAddress = TEXT("https://localhost:8444");
    //const FString ServerAddress = TEXT("ec2-xx-xx-xx-xx.compute-1.amazonaws.com:7777");


    FString CreateDeleteCharRequest(const FString& charName);
    void SendDeleteCharHttpRequest(const FString& RequestData, const FString& charName);
    void HandleDeleteCharResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, const FString& RequestData, const FString& charName);
    void HandleResponse(const FString& Response);
    void SendLoginRequest(const FString& RequestData, bool isRetry);
    void SendCheckNameRequest(const FString& RequestData, const bool isCreation, bool isRetry);
    void HandleCheckNameResponse(const FString& Response, const bool isCreation);
    void SendSelectCharRequest(const FString& RequestData, const FString& charName); // includes a lambda which handles the response
    //void LoadGameLevelMap();
    FTimerHandle TimerHandle;
    int maxAttemptsToConnect;
    float timeoutBetweenRequests;
    FString LastLoginRequestData;
    int32 NumAttempts;
    bool bSuccessfulRequest, bCharacterNameAvailable;
    FDelegateHandle TickerDelegateHandle;
};
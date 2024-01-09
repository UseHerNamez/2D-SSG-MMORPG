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

    UFUNCTION(BlueprintCallable, Category = "Login")
    void Register(const FString& Username, const FString& Password);

private:
    void HandleLoginResponse(const FString& Response);
    void HandleRegisterResponse(const FString& Response);
    void SendLoginRequest(const FString& RequestData);
    void LoadGameLevelMap();
    void ShowErrorWidget(const FString& ErrorMessage);
};
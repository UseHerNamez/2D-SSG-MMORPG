#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"    // FUniqueNetIdRepl
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include <optional>
#include "ServerGameMode.generated.h"

struct FUniqueNetIdRepl;
class AClientPlayerController;

// APlayerController*, int32(CharId)
DECLARE_DELEGATE_TwoParams(FOnTokenValidated, APlayerController*, int32);

UCLASS()
class TWODSSG_API AServerGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AServerGameMode();

    virtual FString InitNewPlayer(APlayerController* NewPlayerController,
        const FUniqueNetIdRepl& UniqueId,
        const FString& Options,
        const FString& Portal) override;

protected:
    virtual void BeginPlay() override;

private:
    void ValidateTokenWithLoginServer(APlayerController* PlayerController, const FString& Token);
    void OnTokenValidationComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void KickPlayer(APlayerController* PlayerController, const FString& Reason);
    void FetchCharacterDataFromDB(APlayerController* PlayerController, int32 CharId);

    // helpers
    APlayerController* FindControllerForToken(const FString& Token);
    void HandleTokenErrorStatus(const FString& TokenOrReason, const FString& ErrorReason);
    std::optional<int32> GetCharIdFromJWT(const FString& Token);



    FOnTokenValidated OnTokenValidatedDelegate;
    TMap<FString, TWeakObjectPtr<APlayerController>> TokenToControllerMap;

    const FString LoginServerURL = TEXT("http://localhost:12345");
};

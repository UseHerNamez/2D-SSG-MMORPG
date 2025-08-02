#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h" 
#include "Delegates/DelegateCombinations.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "ServerGameMode.generated.h"

class AClientPlayerController;
// Delegate for when token validation succeeds
DECLARE_DELEGATE_TwoParams(FOnTokenValidated, APlayerController*, int32);

UCLASS()
class TWODSSG_API AServerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AServerGameMode();
	FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal);

private:
	void ValidateTokenWithLoginServer(APlayerController* PlayerController, const FString& Token);
	void OnTokenValidationComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void KickPlayer(APlayerController* PlayerController, const FString& Reason);
	void FetchCharacterDataFromDB(APlayerController* PlayerController, int32 CharId);
	void BeginPlay();

	FOnTokenValidated OnTokenValidatedDelegate;
	TMap<FString, TWeakObjectPtr<APlayerController>> TokenToControllerMap;
	const FString LoginServerURL = TEXT("http://localhost:12345");
};


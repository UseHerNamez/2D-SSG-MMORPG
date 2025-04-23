#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h" 
#include "Delegates/DelegateCombinations.h"
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
	void ValidateTokenWithLoginServer(const FString& Token, APlayerController* PlayerController, int32& OutPlayerId);
	void KickPlayer(APlayerController* PlayerController, const FString& Reason);
	void FetchCharacterDataFromDB(const FString& CharacterID);
	void OnTokenValidated_Internal(APlayerController* PlayerController, int32 CharId);
	void BeginPlay();

	FOnTokenValidated OnTokenValidatedDelegate;

	const FString LoginServerURL = TEXT("http://localhost:12345");
};


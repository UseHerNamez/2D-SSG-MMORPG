#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"    // FUniqueNetIdRepl
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include <optional>
#include <memory>
#include "ServerGameMode.generated.h"

struct FUniqueNetIdRepl;
class DatabaseConnectionPool;
class AClientPlayerController;
class ACustomPlayerState;

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

    UFUNCTION(BlueprintImplementableEvent, Category = "Init")
        void BP_AfterPlayerSpawned(APlayerController* PC);

protected:
    virtual void BeginPlay() override;

private:
    void ValidateTokenWithLoginServer(APlayerController* PlayerController, const FString& Token);
    void OnTokenValidationComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void KickPlayer(APlayerController* PlayerController, const FString& Reason);
    void FetchCharacterDataFromDB(APlayerController* PlayerController, int32 CharId);
    int32 ParseGenderToInt(const FString& GenderStr);

    // helper
    bool GetIdsFromJWT(const FString& Token, ACustomPlayerState* PS, int32& o_CharId);

    struct FPendingAuth
    {
        TWeakObjectPtr<APlayerController> PC;
        FString SubmittedToken; // may be empty
        double StartTimeSeconds = 0.0; // UGameplayStatics::GetRealTimeSeconds(World) or FPlatformTime::Seconds()
    };
    TMap<FString, FPendingAuth> PendingAuthByRequestId; // RequestId -> pending

    void TickAuthCleanup();
    FTimerHandle AuthCleanupHandle;

    const FString LoginServerURL = TEXT("http://localhost:12345");
};

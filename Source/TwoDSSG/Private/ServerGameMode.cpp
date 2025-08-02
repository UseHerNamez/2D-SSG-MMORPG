#include "ServerGameMode.h"
#include "HttpModule.h"

#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "ClientPlayerController.h"

AServerGameMode::AServerGameMode()
{

}

FString AServerGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, 
    const FString& Options, const FString& Portal)
{
    FString Token;
    FParse::Value(*Options, TEXT("AuthToken="), Token); // suitable string?

    if (Token.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No AuthToken provided"));
        KickPlayer(NewPlayerController, TEXT("No AuthToken provided"));
        return TEXT("Missing AuthToken");
    }

    if (NewPlayerController)
    {
        NewPlayerController->DisableInput(NewPlayerController);

        for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
        {
            if (It->ActorHasTag("LoadingCamera"))
            {
                NewPlayerController->SetViewTarget(*It);
                break;
            }
        }

        if (AClientPlayerController* ClientPC = Cast<AClientPlayerController>(NewPlayerController))
        {
            ClientPC->RPC_ShowLoadingWidget(); // This shows the widget on the client
        }

        // Start async validation here
        ValidateTokenWithLoginServer(NewPlayerController, Token);
    }

    return FString(); // Return early — don’t finalize spawn yet
}

void AServerGameMode::ValidateTokenWithLoginServer(APlayerController* PlayerController, const FString& Token)
{
    if (!PlayerController) return;

    // Store token
    TokenToControllerMap.Add(Token, PlayerController);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

    HttpRequest->OnProcessRequestComplete().BindUObject(this, &AServerGameMode::OnTokenValidationComplete);
    HttpRequest->SetURL(LoginServerURL);
    HttpRequest->SetVerb("POST");
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain")); // Important: plain text

    FString RequestBody = FString::Printf(TEXT("VERIFY_TOKEN %s"), *Token);
    HttpRequest->SetContentAsString(RequestBody);

    UE_LOG(LogTemp, Log, TEXT("Sending token verification request: %s"), *RequestBody);

    HttpRequest->ProcessRequest();
}

void AServerGameMode::OnTokenValidationComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Token validation failed: No response or network error"));
        return;
    }

    FString ResponseString = Response->GetContentAsString().TrimStartAndEnd();
    UE_LOG(LogTemp, Log, TEXT("Login server response: %s"), *ResponseString);

    // Expected: "OK <Token>" or "ERROR <token> <Reason> or "ERROR <Reason> if empty token somehow"
    TArray<FString> Parts;
    ResponseString.ParseIntoArrayWS(Parts);

    if (Parts.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unexpected response format"));
        return;
    }

    const FString& Status = Parts[0];
    const FString& TokenOrReason = Parts[1];

    if (Status.Equals(TEXT("ERROR"), ESearchCase::IgnoreCase)) // catches if token is empty or if there is a reason and then kicks player.
    { // 1 - Should we just return void and let the exec continue unless we find something bad?
        // 2 - should use the Reason part here in the UE_LOG and maybe return this message..
        if (TokenOrReason.Equals("emptyToken", ESearchCase::IgnoreCase))
        {
            FString Reason = TokenOrReason;
            UE_LOG(LogTemp, Warning, TEXT("Token validation failed: %s"), *Reason);
            return;
        }
        const FString& CharIdOrReason = Parts[2];

        APlayerController* PlayerController = nullptr;
        if (TWeakObjectPtr<APlayerController>* FoundPtr = TokenToControllerMap.Find(TokenOrReason))
        {
            if (FoundPtr->IsValid())
            {
                PlayerController = FoundPtr->Get(); // gets the suitable playerController..
            }
        }
        FString Reason = CharIdOrReason;
        UE_LOG(LogTemp, Warning, TEXT("Token validation failed: %s"), *Reason);
        KickPlayer(PlayerController, Reason);
        return;
    }

    const FString& CharId = Parts[2];
    const FString& Token = TokenOrReason;
    APlayerController* PlayerController = nullptr;

    if (TWeakObjectPtr<APlayerController>* FoundPtr = TokenToControllerMap.Find(Token))
    {
        if (FoundPtr->IsValid())
        {
            PlayerController = FoundPtr->Get(); // gets the suitable playerController..
        }
    }

    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("No matching player controller for token: %s"), *Token); // weird
        return;
    }

    // Clean up the map
    TokenToControllerMap.Remove(Token); // why do I clean up the map? maybe should clean this value only if the player disconnects or changes maps?

    if (Status.Equals(TEXT("OK"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Log, TEXT("Token valid. CharID: %s"), *CharId);

        // Now continue game logic
        FetchCharacterDataFromDB(PlayerController, FCString::Atoi(*CharId));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Unknown status in response: %s"), *Status);
        KickPlayer(PlayerController, TEXT("Unknown response status"));
    }
}

void AServerGameMode::KickPlayer(APlayerController* PlayerController, const FString& Reason)
{
    if (PlayerController)
    {
        PlayerController->ClientTravel(TEXT("/Game/GameSystem/Maps/SelectionScreen"), ETravelType::TRAVEL_Absolute);
        PlayerController->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("Kicked player: %s"), *Reason);
    }
}

void AServerGameMode::FetchCharacterDataFromDB(APlayerController* PlayerController, int32 CharId)
{
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("On FetchCharacterDataFromDB called with null PlayerController"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Token validated! Proceeding to fetch character data for CharId: %d"), CharId);
    FString CharacterID = FString::FromInt(CharId);
}

void AServerGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Bind the delegate to your internal handler
    OnTokenValidatedDelegate.BindUObject(this, &AServerGameMode::OnTokenValidated_Internal);
}

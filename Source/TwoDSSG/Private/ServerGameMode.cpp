#include "ServerGameMode.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "ClientPlayerController.h" // Include your custom PlayerController

AServerGameMode::AServerGameMode()
{

}

FString AServerGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    FString Token;
    FParse::Value(*Options, TEXT("AuthToken="), Token);

    if (Token.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No AuthToken provided"));
        KickPlayer(NewPlayerController, TEXT("No AuthToken provided"));
        return TEXT("Missing AuthToken");
    }

    if (NewPlayerController)
    {
        // Disable player input
        NewPlayerController->DisableInput(NewPlayerController);

        // Set view to loading camera if available | TODO: need to create a camrea actor with this tag
        for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
        {
            if (It->ActorHasTag("LoadingCamera"))
            {
                NewPlayerController->SetViewTarget(*It);
                break;
            }
        }

        // Call client-side loading UI
        if (AClientPlayerController* ClientPC = Cast<AClientPlayerController>(NewPlayerController))
        {
            ClientPC->ShowLoadingWidget(); // TODO: should implement in BP
        }
    }

    int32 CharId = -1;
    ValidateTokenWithLoginServer(Token, NewPlayerController, CharId);

    return TEXT(""); // Delay player full spawn
}

void AServerGameMode::ValidateTokenWithLoginServer(const FString& Token, APlayerController* PlayerController, int32& OutPlayerId)
{
    TWeakObjectPtr<APlayerController> WeakPlayerController(PlayerController);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(LoginServerURL + "/validate-token");
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> JsonRequest = MakeShared<FJsonObject>();
    JsonRequest->SetStringField(TEXT("token"), Token);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonRequest.ToSharedRef(), Writer);
    Request->SetContentAsString(RequestBody);

    Request->OnProcessRequestComplete().BindLambda([this, WeakPlayerController, &OutPlayerId](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bWasSuccessful)
        {
            if (!WeakPlayerController.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("Player disconnected before validation finished"));
                return;
            }

            APlayerController* PC = WeakPlayerController.Get();

            if (!bWasSuccessful || !Resp.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Token validation request failed"));
                KickPlayer(PC, TEXT("Login server error"));
                return;
            }

            if (Resp->GetResponseCode() != 200)
            {
                UE_LOG(LogTemp, Warning, TEXT("Unexpected response code: %d"), Resp->GetResponseCode());
                KickPlayer(PC, TEXT("Invalid login response"));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response"));
                KickPlayer(PC, TEXT("Invalid login server response"));
                return;
            }

            // Check for validity and extract player ID
            bool bIsValid = Json->GetBoolField(TEXT("valid"));
            if (!bIsValid)
            {
                UE_LOG(LogTemp, Warning, TEXT("Token invalid"));
                KickPlayer(PC, TEXT("Invalid or expired token"));
                return;
            }

            OutPlayerId = Json->GetIntegerField(TEXT("character_id"));

            // Notify success (delegate or next logic)
            UE_LOG(LogTemp, Log, TEXT("Token validated. Character ID: %d"), OutPlayerId);
            OnTokenValidatedDelegate.ExecuteIfBound(PC, OutPlayerId);
        });

    Request->ProcessRequest();
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

void AServerGameMode::OnTokenValidated_Internal(APlayerController* PlayerController, int32 CharId)
{
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("OnTokenValidated_Internal called with null PlayerController"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Token validated! Proceeding to fetch character data for CharId: %d"), CharId);
    FString CharacterID = FString::FromInt(CharId);
    FetchCharacterDataFromDB(CharacterID);
}


void AServerGameMode::FetchCharacterDataFromDB(const FString& CharacterID)
{
	// TODO: Connect to DB and retrieve saved state for this character
	UE_LOG(LogTemp, Log, TEXT("Fetching character data for User: %s, Character: %s"), *CharacterID);
}

void AServerGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Bind the delegate to your internal handler
    OnTokenValidatedDelegate.BindUObject(this, &AServerGameMode::OnTokenValidated_Internal);
}

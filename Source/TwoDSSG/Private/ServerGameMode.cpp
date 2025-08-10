#include "ServerGameMode.h"
#include "HttpModule.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "EncryptionUtils.h"
#include "ClientPlayerController.h"

#include <string>

namespace {
    inline FString ToFString(const std::string& s) { return UTF8_TO_TCHAR(s.c_str()); }
    inline std::string FromFString(const FString& s) { return std::string(TCHAR_TO_UTF8(*s)); }
    inline int32 ToInt(const std::string& s) { return FCString::Atoi(*ToFString(s)); }

    // Parse "OK <token>" or "ERROR <token> <reason>" or "ERROR <reason>"
    inline bool ParseResponse(const FString& In, FString& OutStatus, FString& OutA1, FString& OutA2)
    {
        TArray<FString> Parts;
        In.ParseIntoArrayWS(Parts);
        if (Parts.Num() == 0) return false;
        OutStatus = Parts[0];
        OutA1 = (Parts.Num() >= 2) ? Parts[1] : FString();
        OutA2 = (Parts.Num() >= 3) ? Parts[2] : FString();
        return true;
    }

    inline const std::string& AsStringRef(const std::string& s) { return s; }
    inline std::string AsStringRef(const std::optional<std::string>& os) { return os.value_or(std::string()); }
}

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
        UE_LOG(LogTemp, Warning, TEXT("Token validation failed - network or response error"));
        return;
    }

    const FString Resp = Response->GetContentAsString().TrimStartAndEnd();
    UE_LOG(LogTemp, Log, TEXT("Login server response: %s"), *Resp);

    FString Status, A1, A2;
    if (!ParseResponse(Resp, Status, A1, A2))
    {
        UE_LOG(LogTemp, Warning, TEXT("Unexpected response format"));
        return;
    }

    if (Status.Equals(TEXT("ERROR"), ESearchCase::IgnoreCase))
    {
        // "ERROR emptyToken" or "ERROR <token> <reason>"
        const FString& ErrorReason = A2.IsEmpty() ? A1 : A2;
        HandleTokenErrorStatus(A1, ErrorReason);
        return;
    }

    // Status is OK - A1 is the token
    const FString& Token = A1;

    APlayerController* PC = FindControllerForToken(Token);
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("No matching player controller for token: %s"), *Token);
        return;
    }

    // token used - remove mapping
    TokenToControllerMap.Remove(Token);

    // Claims - need charId
    const std::optional<int32> CharIdOpt = GetCharIdFromJWT(Token);
    if (!CharIdOpt)
    {
        UE_LOG(LogTemp, Warning, TEXT("Token claims missing or invalid - charId"));
        KickPlayer(PC, TEXT("InvalidTokenClaims"));
        return;
    }

    const int32 CharId = CharIdOpt.value();
    UE_LOG(LogTemp, Log, TEXT("Token valid - CharId: %d"), CharId);

    // Use the delegate you bound in BeginPlay - keeps the flow clean
    OnTokenValidatedDelegate.ExecuteIfBound(PC, CharId);
    // If you prefer direct call instead of the delegate:
    // FetchCharacterDataFromDB(PC, CharId);
}

APlayerController* AServerGameMode::FindControllerForToken(const FString& Token)
{
    if (TWeakObjectPtr<APlayerController>* Found = TokenToControllerMap.Find(Token))
    {
        if (Found->IsValid()) return Found->Get();
    }
    return nullptr;
}

void AServerGameMode::HandleTokenErrorStatus(const FString& TokenOrReason, const FString& ErrorReason)
{
    APlayerController* PC = FindControllerForToken(TokenOrReason);
    UE_LOG(LogTemp, Warning, TEXT("Token validation failed: %s"), *ErrorReason);
    KickPlayer(PC, ErrorReason);
}

std::optional<int32> AServerGameMode::GetCharIdFromJWT(const FString& Token)
{
    TokenVerificationResult Res = validateAndExtractClaims(FromFString(Token));

    if (Res.status != TokenStatus::Valid || !Res.claims)
        return std::nullopt;

    // Works whether claims->charId is std::string or std::optional<std::string>
    const std::string CharIdStr = AsStringRef(Res.claims->charId);
    if (CharIdStr.empty())
        return std::nullopt;

    try {
        // FCString::Atoi needs FString, but std::stoi is fine here
        return static_cast<int32>(std::stoi(CharIdStr));
    }
    catch (...) {
        return std::nullopt;
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
}

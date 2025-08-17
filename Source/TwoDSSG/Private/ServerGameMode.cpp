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
#include "DatabaseConnectionPool.h"
#include <string>
#include "Misc/Paths.h"
#include "Misc/OutputDeviceDebug.h"
#include "CustomPlayerState.h"
#include "CharacterInitTypes.h"

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
    bStartPlayersAsSpectators = true;
    PlayerStateClass = ACustomPlayerState::StaticClass();
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

    // Create a correlation id for this request
    const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

    // Track who initiated this validation
    FPendingAuth Pending;
    Pending.PC = PlayerController;
    Pending.SubmittedToken = Token;
    Pending.StartTimeSeconds = FPlatformTime::Seconds();
    PendingAuthByRequestId.Add(RequestId, MoveTemp(Pending));

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &AServerGameMode::OnTokenValidationComplete);
    HttpRequest->SetURL(LoginServerURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));

    // Put the RequestId in a header so we can read it back in the completion callback
    HttpRequest->SetHeader(TEXT("X-Request-Id"), RequestId);

    const FString RequestBody = FString::Printf(TEXT("VERIFY_TOKEN %s"), *Token);
    HttpRequest->SetContentAsString(RequestBody);

    UE_LOG(LogTemp, Log, TEXT("Sending token verification request [req=%s]: %s"), *RequestId, *RequestBody);
    HttpRequest->ProcessRequest();
}

void AServerGameMode::OnTokenValidationComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    const FString RequestId = Request.IsValid() ? Request->GetHeader(TEXT("X-Request-Id")) : TEXT("");
    FPendingAuth Pending;
    const bool bHadPending = PendingAuthByRequestId.RemoveAndCopyValue(RequestId, Pending);

    APlayerController* PC = bHadPending ? Pending.PC.Get() : nullptr;

    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Token validation failed - network or response error [req=%s]"), *RequestId);
        if (PC) KickPlayer(PC, TEXT("AuthServerError"));
        return;
    }

    const FString Resp = Response->GetContentAsString().TrimStartAndEnd();
    UE_LOG(LogTemp, Log, TEXT("Login server response [req=%s]: %s"), *RequestId, *Resp);

    FString Status, A1, A2;
    if (!ParseResponse(Resp, Status, A1, A2))
    {
        UE_LOG(LogTemp, Warning, TEXT("Unexpected response format [req=%s]"), *RequestId);
        if (PC) KickPlayer(PC, TEXT("AuthMalformedResponse"));
        return;
    }

    if (Status.Equals(TEXT("ERROR"), ESearchCase::IgnoreCase))
    {
        // Forms:
        //  "ERROR emptyToken"            -> A1 = "emptyToken", A2 = ""
        //  "ERROR <token> <reason>"      -> A1 = token, A2 = reason
        const bool bHasExplicitReason = !A2.IsEmpty();
        const FString ErrorReason = bHasExplicitReason ? A2 : A1;

        UE_LOG(LogTemp, Warning, TEXT("Token validation error [req=%s]: %s"), *RequestId, *ErrorReason);

        if (PC) KickPlayer(PC, ErrorReason);
        return;
    }

    // OK path - A1 is the token
    const FString& Token = A1;

    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("Validation OK but player no longer valid [req=%s]"), *RequestId);
        return;
    }

    // Claims - need charId
    const std::optional<int32> CharIdOpt = GetCharIdFromJWT(Token);
    if (!CharIdOpt)
    {
        UE_LOG(LogTemp, Warning, TEXT("Token claims missing or invalid - charId [req=%s]"), *RequestId);
        KickPlayer(PC, TEXT("InvalidTokenClaims"));
        return;
    }

    const int32 CharId = CharIdOpt.value();
    UE_LOG(LogTemp, Log, TEXT("Token valid - CharId: %d [req=%s]"), CharId, *RequestId);

    FetchCharacterDataFromDB(PC, CharId);
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

int32 AServerGameMode::ParseGenderToInt(const FString& GenderStr)
{
    if (GenderStr.IsEmpty())
        return 0; // default to female

    return FCString::Atoi(*GenderStr);
}

void AServerGameMode::FetchCharacterDataFromDB(APlayerController* PlayerController, int32 CharId)
{
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("FetchCharacterDataFromDB called with null PlayerController"));
        return;
    }
    if (!DbPool)
    {
        UE_LOG(LogTemp, Error, TEXT("DbPool is null — cannot fetch character data"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Fetching character data for CharId: %d"), CharId);

    TWeakObjectPtr<APlayerController> PCWeak = PlayerController;
    auto PoolCopy = DbPool;

    // Off the game thread
    Async(EAsyncExecution::ThreadPool, [this, PoolCopy, PCWeak, CharId]()
        {
            FCharacterInitData_Client OutInit;
            bool bOk = false;

            // Acquire a connector from pool
            std::shared_ptr<DatabaseConnector> Conn = PoolCopy->Acquire();
            if (!Conn)
            {
                UE_LOG(LogTemp, Warning, TEXT("DB pool exhausted — scheduling retry"));
                // schedule a small retry on the game thread
                AsyncTask(ENamedThreads::GameThread, [this, PCWeak, CharId]()
                    {
                        if (!PCWeak.IsValid()) return;
                        FTimerHandle Handle;
                        GetWorldTimerManager().SetTimer(
                            Handle,
                            FTimerDelegate::CreateLambda([this, PCWeak, CharId]()
                                {
                                    if (PCWeak.IsValid()) FetchCharacterDataFromDB(PCWeak.Get(), CharId);
                                }),
                            0.25f, false
                                    );
                    });
                return; // important — do not continue on this thread
            }

            // Call static lib
            auto Opt = Conn->GetCharGameplayDataById(CharId);
            if (Opt.has_value())
            {
                // Unpack tuple
                const auto& T = Opt.value();
                const std::string& Name = std::get<0>(T);
                const std::string& GenderStr = std::get<1>(T);
                const int          Level = std::get<2>(T);
                const std::string& Appearance = std::get<3>(T);
                const int          Str = std::get<4>(T);
                const int          Dex = std::get<5>(T);
                const int          Wis = std::get<6>(T);
                const int          Luk = std::get<7>(T);
                const int          Pur = std::get<8>(T);
                const int          Vic = std::get<9>(T);

                // Fill public structs
                OutInit.Base.Name = UTF8_TO_TCHAR(Name.c_str());
                OutInit.Base.Level = FString::FromInt(Level);
                OutInit.Base.Gender = ParseGenderToInt(UTF8_TO_TCHAR(GenderStr.c_str()));
                OutInit.Base.Appearance = UTF8_TO_TCHAR(Appearance.c_str());

                OutInit.Stats.Str = Str;
                OutInit.Stats.Dex = Dex;
                OutInit.Stats.Wis = Wis;
                OutInit.Stats.Luk = Luk;
                OutInit.Stats.Pur = Pur;
                OutInit.Stats.Vic = Vic;

                bOk = !OutInit.Base.Name.IsEmpty();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("GetCharGameplayDataById returned empty for CharId %d"), CharId);
            }

            // Back to game thread — touch UObjects here
            AsyncTask(ENamedThreads::GameThread, [this, PCWeak, bOk, OutInit]()
                {
                    if (!PCWeak.IsValid()) return;

                    APlayerController* PC = PCWeak.Get();
                    ACustomPlayerState* PS = PC ? PC->GetPlayerState<ACustomPlayerState>() : nullptr;
                    if (!PS)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("PlayerState not ready when applying InitData"));
                        return;
                    }

                    if (!bOk)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Character data invalid — kicking player"));
                        KickPlayer(PC, TEXT("CharNotFoundOrInvalid"));
                        return;
                    }

                    // 1) Replicated payload — this is where CharacterInitTypes structs are used
                    PS->SetInitData_Server(OutInit);

                    // 2) Spawn now that data is ready
                    RestartPlayer(PC);
                });
        });
}

void AServerGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorldTimerManager().SetTimer(AuthCleanupHandle, this, &AServerGameMode::TickAuthCleanup, 15.0f, true);

    char* encryptionKeyRaw = nullptr;
    size_t size = 0;

    if (_dupenv_s(&encryptionKeyRaw, &size, "ENCRYPTION_KEY") != 0 || encryptionKeyRaw == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get ENCRYPTION_KEY from environment."));
        return;
    }

    std::string key(encryptionKeyRaw);
    free(encryptionKeyRaw);

#include "Misc/Paths.h"

    std::string configPath = TCHAR_TO_UTF8(*FPaths::Combine(FPaths::ProjectDir(), TEXT("Config/config.ini.encrypted")));
    // drop it where the exe lives, usually under .../YourProject/Binaries/Win64/

    DbPool = std::make_shared<DatabaseConnectionPool>(configPath, key, poolSize);
    UE_LOG(LogTemp, Log, TEXT("Database connection pool initialized in Gameplay Server."));
}

void AServerGameMode::TickAuthCleanup()
{
    const double Now = FPlatformTime::Seconds();
    const double TimeoutSec = 7.0; // tune for your infra

    for (auto It = PendingAuthByRequestId.CreateIterator(); It; ++It)
    {
        if (Now - It->Value.StartTimeSeconds > TimeoutSec)
        {
            if (APlayerController* PC = It->Value.PC.Get())
            {
                KickPlayer(PC, TEXT("AuthTimeout"));
            }
            It.RemoveCurrent();
        }
    }
}


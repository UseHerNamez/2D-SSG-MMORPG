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
#include "CustomGameInstanceSubsystem.h"

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

    inline bool TryParsePositiveInt32(const std::string& s, int32& Out)
    {
        if (s.empty()) return false;
        const FString Fs = ToFString(s);
        if (!Fs.IsNumeric()) return false;                       // reject "12abc" etc
        int64 Tmp = FCString::Atoi64(*Fs);                       // non-throwing
        if (Tmp < 0 || Tmp > MAX_int32) return false;            // bounds check
        Out = static_cast<int32>(Tmp);
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
    if (!ParseResponse(Resp, Status, A1, A2)) //A1 A2 are just answers from the server
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
    if (ACustomPlayerState* PS = PC->GetPlayerState<ACustomPlayerState>()) {
        int32 CharId = -1;
        if (!GetIdsFromJWT(Token, PS, CharId)) {
            UE_LOG(LogTemp, Warning, TEXT("Token claims missing or invalid"));
            KickPlayer(PC, TEXT("InvalidTokenClaims"));
            return;
        }
        UE_LOG(LogTemp, Log, TEXT("Token valid - UserId:%d CharId:%d"), PS->GetUserId_Server(), PS->GetCharId_Server());
        FetchCharacterDataFromDB(PC, CharId);
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("PlayerState not ready when setting ServerOnly data"));
    }
}

bool AServerGameMode::GetIdsFromJWT(const FString& Token, ACustomPlayerState* PS, int32& OutCharId)
{
    if (!PS) return false;

    TokenVerificationResult Res = validateAndExtractClaims(FromFString(Token));
    if (Res.status != TokenStatus::Valid || !Res.claims) return false;

    const std::string CharIdStr = AsStringRef(Res.claims->charId);
    const std::string UserIdStr = AsStringRef(Res.claims->userId);

    int32 CharId = -1, UserId = -1;

    if (!TryParsePositiveInt32(CharIdStr, CharId)) {
        UE_LOG(LogTemp, Warning, TEXT("Invalid CharId claim: %s"), *ToFString(CharIdStr));
        return false; // CharId is required
    }

    if (!UserIdStr.empty() && !TryParsePositiveInt32(UserIdStr, UserId)) {
        UE_LOG(LogTemp, Warning, TEXT("Invalid UserId claim: %s"), *ToFString(UserIdStr));
        UserId = -1; // optional - proceed without it
    }

    if (CharId < 0) return false;

#if WITH_SERVER_CODE
    FCharacterInitData_Server Srv;
    Srv.UserId = UserId;
    Srv.CharId = CharId;
    PS->SetServerOnlyData(Srv);
#endif

    OutCharId = CharId;
    return true;
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
    if (UCustomGameInstanceSubsystem* Sub = GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>()) 
    {
        if (!Sub->IsReady()) {
            UE_LOG(LogTemp, Error, TEXT("DbPool is null - cannot fetch character data"));
            return;
        }

        TSharedPtr<DatabaseConnectionPool> Pool = Sub->GetPool();
        UE_LOG(LogTemp, Log, TEXT("Fetching character data for CharId: %d"), CharId);

        TWeakObjectPtr<APlayerController> PCWeak = PlayerController;
    
        // Off the game thread
        Async(EAsyncExecution::ThreadPool, [this, PoolCopy = Pool, PCWeak, CharId]()
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
            AsyncTask(ENamedThreads::GameThread, [this, PCWeak, bOk, OutInit = MoveTemp(OutInit)]()
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

                if (APawn* P = PC->GetPawn())
                {
                    // Ensure the view is on the spawned pawn
                    PC->SetViewTargetWithBlend(P, 0.0f);

                    // Re-enable input on the pawn
                    P->EnableInput(PC);

                    // to update translucent priority var
                    BP_AfterPlayerSpawned(PC);

                    // clears ignore flags on the controller anywhere
                    PC->SetIgnoreMoveInput(false);
                    PC->SetIgnoreLookInput(false);

                    // notify the client to hide loading UI
                    if (AClientPlayerController* CPC = Cast<AClientPlayerController>(PC))
                    {
                        CPC->RPC_HideLoadingWidget();
                    }
                }
            });
        });
    }
}

void AServerGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorldTimerManager().SetTimer(AuthCleanupHandle, this, &AServerGameMode::TickAuthCleanup, 15.0f, true);

    if (UCustomGameInstanceSubsystem* Sub = GetGameInstance()->GetSubsystem<UCustomGameInstanceSubsystem>())
    {
        if (!Sub->IsReady())
        {
            UE_LOG(LogTemp, Error, TEXT("Persistence subsystem not ready - DB pool missing"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CustomGameInstanceSubsystem not found"));
    }
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


#include "ServerGameMode.h"
#include "HttpModule.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
    // Ensure use of client PC subclass so UI RPCs work
    PlayerControllerClass = AClientPlayerController::StaticClass();
}

FString AServerGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, 
    const FString& Options, const FString& Portal)
{
    // Check if test mode is enabled (simple bool)
    if (bTestMode)
    {
        return HandleTestModePlayer(NewPlayerController);
    }

    // Normal flow - token validation and database fetch
    return HandleNormalPlayer(NewPlayerController, Options);


    return FString(); // Return early - don't finalize spawn yet
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
        if (!Sub->IsReady() || Sub->IsTestMode())
        {
            UE_LOG(LogTemp, Error, TEXT("DbPool is null - cannot fetch character data"));
            return;
        }

        TSharedPtr<DatabaseConnectionPool> Pool = Sub->GetPool();
        UE_LOG(LogTemp, Log, TEXT("Fetching character data for CharId: %d"), CharId);

        TWeakObjectPtr<APlayerController> PCWeak = PlayerController;

        // Off the game thread
        Async(EAsyncExecution::ThreadPool, [this, PoolCopy = Pool, PCWeak, CharId]()
            {
                // ---- Worker-thread locals (no UObjects here) ----
                bool        bOk = false;

                FString     OutName;
                int32       OutGender = 0;
                FString     OutAppearanceStr;
                int32       DBLevel = 1;

                int32 Str = 0, Dex = 0, Wisd = 0, Luk = 0, Pur = 0, Vic = 0;

                // Acquire connector
                std::shared_ptr<DatabaseConnector> Conn = PoolCopy->Acquire();
                if (!Conn)
                {
                    // Retry shortly on game thread
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
                    return;
                }

                // Query static lib
                auto Opt = Conn->GetCharGameplayDataById(CharId);
                if (Opt.has_value())
                {
                    // Unpack tuple
                    const auto& T = Opt.value();
                    const std::string& Name = std::get<0>(T);
                    const std::string& GenderStr = std::get<1>(T);
                    const int          Level = std::get<2>(T);
                    const std::string& Appearance = std::get<3>(T);
                    Str = std::get<4>(T);
                    Dex = std::get<5>(T);
                    Wisd = std::get<6>(T);
                    Luk = std::get<7>(T);
                    Pur = std::get<8>(T);
                    Vic = std::get<9>(T);

                    const int MaxHpFromLvls = std::get<10>(T);
                    const int MaxMpFromLvls = std::get<11>(T);
                    const int CurrHp        = std::get<12>(T);
                    const int CurrMp        = std::get<13>(T);
                    const int MaxExpToLvl   = std::get<14>(T);
                    const int CurrExp       = std::get<15>(T);
                    const int HighMinRange       = std::get<16>(T);
                    const int HighMaxRange       = std::get<17>(T);

                    OutName = UTF8_TO_TCHAR(Name.c_str());
                    OutGender = ParseGenderToInt(UTF8_TO_TCHAR(GenderStr.c_str()));
                    OutAppearanceStr = UTF8_TO_TCHAR(Appearance.c_str());
                    DBLevel = FMath::Max(1, Level);

                    // Capture vitals/exp/achievements into outer-scope statics via lambda capture below
                    AsyncTask(ENamedThreads::GameThread, [this, PCWeak, bOk = true,
                        OutName = MoveTemp(OutName),
                        OutGender = OutGender,
                        OutAppearance = MoveTemp(OutAppearanceStr),
                        DBLevel = DBLevel,
                        Str = Str, Dex = Dex, Wisd = Wisd, Luk = Luk, Pur = Pur, Vic = Vic,
                        MaxHpFromLvls, MaxMpFromLvls, CurrHp, CurrMp, MaxExpToLvl, CurrExp, HighMinRange, HighMaxRange]()
                    {
                        if (!PCWeak.IsValid()) return;

                        APlayerController* PC = PCWeak.Get();
                        ACustomPlayerState* PS = PC ? PC->GetPlayerState<ACustomPlayerState>() : nullptr;
                        if (!PS)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("PlayerState not ready when applying character data"));
                            return;
                        }

                        // -------- Apply to live replicated props (server only) --------
                        // Public identity
                        PS->Identity.Name = OutName;
                        PS->Identity.Gender = OutGender;

                        PS->Progression.Level = DBLevel;
                        PS->BaseStats.Str = Str;
                        PS->BaseStats.Dex = Dex;
                        PS->BaseStats.Wisd = Wisd;
                        PS->BaseStats.Luk = Luk;
                        PS->BaseStats.Pur = Pur;
                        PS->BaseStats.Vic = Vic;

                        // Public snapshot
                        PS->PublicInspect.Level = PS->Progression.Level;
                        PS->PublicInspect.BaseStats = PS->BaseStats;

                        // Progression & vitals
                        PS->Progression.MaxExpToLvl = MaxExpToLvl;
                        PS->Progression.XP = CurrExp;
                        PS->Vitals.MaxHPFromLevels = MaxHpFromLvls;
                        PS->Vitals.MaxMPFromLevels = MaxMpFromLvls;
                        PS->Vitals.CurrHP = CurrHp;
                        PS->Vitals.CurrMP = CurrMp;

                        // Achievements
                        PS->Achievements.HighestMinDamageRange = HighMinRange;
                        PS->Achievements.HighestMaxDamageRange = HighMaxRange;
                        PS->PublicInspect.HighestMinDamageRange = HighMinRange;
                        PS->PublicInspect.HighestMaxDamageRange = HighMaxRange;

                        PS->NotifyInitialDataLoaded_ServerOnly();

                        // -------- Spawn now that data is ready --------
                        RestartPlayer(PC);

                        if (APawn* P = PC->GetPawn())
                        {
                        PC->SetViewTargetWithBlend(P, 0.0f);
                        // Prefer the pawn's tagged camera component if available
                        if (UCameraComponent* TaggedCam = [&]() -> UCameraComponent*
                        {
                            TArray<UActorComponent*> Cams = P->GetComponentsByClass(UCameraComponent::StaticClass());
                            for (UActorComponent* C : Cams)
                            {
                                if (C->ComponentHasTag(FName(TEXT("PlayerCamera"))))
                                {
                                    return Cast<UCameraComponent>(C);
                                }
                            }
                            return Cams.Num() > 0 ? Cast<UCameraComponent>(Cams[0]) : nullptr;
                        }())
                        {
                            TaggedCam->Activate();
                        }
                        PC->bAutoManageActiveCameraTarget = true;
                        PC->AutoManageActiveCameraTarget(Cast<APawn>(P));
                        PC->SetViewTargetWithBlend(P, 0.0f);
                        // Prefer the pawn's tagged camera component if available
                        if (UCameraComponent* TaggedCam = [&]() -> UCameraComponent*
                        {
                            TArray<UActorComponent*> Cams = P->GetComponentsByClass(UCameraComponent::StaticClass());
                            for (UActorComponent* C : Cams)
                            {
                                if (C->ComponentHasTag(FName(TEXT("PlayerCamera"))))
                                {
                                    return Cast<UCameraComponent>(C);
                                }
                            }
                            return Cams.Num() > 0 ? Cast<UCameraComponent>(Cams[0]) : nullptr;
                        }())
                        {
                            TaggedCam->Activate();
                        }
                        PC->bAutoManageActiveCameraTarget = true;
                        PC->AutoManageActiveCameraTarget(Cast<APawn>(P));
                            P->EnableInput(PC);

                            BP_AfterPlayerSpawned(PC);

                        PC->SetIgnoreMoveInput(false);
                        PC->SetIgnoreLookInput(false);

                            if (AClientPlayerController* CPC = Cast<AClientPlayerController>(PC))
                            {
                                CPC->RPC_HideLoadingWidget();
                            }
                        }
                    });

                    return; // already scheduled game-thread work
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("GetCharGameplayDataById returned empty for CharId %d"), CharId);
                }

                // ---- Back to game thread ----
                AsyncTask(ENamedThreads::GameThread, [this, PCWeak]()
                {
                    if (!PCWeak.IsValid()) return;
                    if (APlayerController* PC = PCWeak.Get())
                    {
                        KickPlayer(PC, TEXT("CharNotFoundOrInvalid"));
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

// Test mode helper functions

FString AServerGameMode::HandleTestModePlayer(APlayerController* NewPlayerController)
{
    UE_LOG(LogTemp, Log, TEXT("Test mode enabled - bypassing token validation and database fetch"));
    
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
            ClientPC->RPC_ShowLoadingWidget();
        }
        
        if (UGameInstance* GI = GetWorld()->GetGameInstance())
        {
            if (UCustomGameInstanceSubsystem* Subsystem = GI->GetSubsystem<UCustomGameInstanceSubsystem>())
            {
                if (ACustomPlayerState* PS = NewPlayerController->GetPlayerState<ACustomPlayerState>())
                {
                    Subsystem->PopulateTestData(PS);

                    // Spawn player with test data
                    RestartPlayer(NewPlayerController);

                    // Finish spawn flow like normal path
                    if (APawn* P = NewPlayerController->GetPawn())
                    {
                        NewPlayerController->SetViewTargetWithBlend(P, 0.0f);
                        // Prefer the pawn's tagged camera component if available
                        
                        if (UCameraComponent* TaggedCam = [&]() -> UCameraComponent*
                        {
                            TArray<UActorComponent*> Cams = P->GetComponentsByClass(UCameraComponent::StaticClass());
                            for (UActorComponent* C : Cams)
                            {
                                if (C->ComponentHasTag(FName(TEXT("PlayerCamera"))))
                                {
                                    return Cast<UCameraComponent>(C);
                                }
                            }
                            return Cams.Num() > 0 ? Cast<UCameraComponent>(Cams[0]) : nullptr;
                        }())
                        {
                            TaggedCam->Activate();
                        }
                        NewPlayerController->bAutoManageActiveCameraTarget = true;
                        NewPlayerController->AutoManageActiveCameraTarget(Cast<APawn>(P));
                        
                        P->EnableInput(NewPlayerController);

                        BP_AfterPlayerSpawned(NewPlayerController);

                        NewPlayerController->SetIgnoreMoveInput(false);
                        NewPlayerController->SetIgnoreLookInput(false);

                        if (AClientPlayerController* CPC = Cast<AClientPlayerController>(NewPlayerController))
                        {
                            CPC->RPC_HideLoadingWidget();
                        }
                    }
                }
            }
        }       
    }
    return FString();
}

FString AServerGameMode::HandleNormalPlayer(APlayerController* NewPlayerController, const FString& Options)
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

    return FString();
}


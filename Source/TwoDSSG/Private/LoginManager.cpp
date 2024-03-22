// LoginManager.cpp

#include "LoginManager.h"

ALoginManager::ALoginManager() : maxAttemptsToConnect(1), timeoutBetweenRequests(5.0f), NumAttempts(0)
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = false;
    bSuccessfulRequest = false;
}

void ALoginManager::SetErrorWidget(ULoginErrorWidget * i_ErrorWidget)
{
    // Create an instance of the LoginErrorWidget
    if (i_ErrorWidget)
    {
        ErrorWidget = i_ErrorWidget;
        // Set the error message on the widget (initially empty)
        ErrorWidget->SetErrorMessage(TEXT(""));

        // Add the widget to the viewport but make it initially not visible
        ErrorWidget->AddToViewport();
        ErrorWidget->SetVisibility(ESlateVisibility::Hidden);
    }
}

void ALoginManager::SetPlayerController(APlayerController* i_PlayerController)
{
    playerController = i_PlayerController;
}

void ALoginManager::SetWorld()
{
    World = playerController->GetWorld();
}

void ALoginManager::Login(const FString& Username, const FString& Password)
{
    // Construct the login request data
    FString LoginRequest = FString::Printf(TEXT("LOGIN %s %s"), *Username, *Password);

    // Send the login request to the server
    SendLoginRequest(LoginRequest, false);
}

void ALoginManager::Register(const FString& Username, const FString& Password)
{
    // Construct the registration request data
    FString RegisterRequest = FString::Printf(TEXT("REGISTER %s %s"), *Username, *Password);

    // Send the registration request to the server
    SendLoginRequest(RegisterRequest, false);
}

void ALoginManager::HandleResponse(const FString& Response)
{
    // Example response format: LOGIN_SUCCESS charinfo: charactersInfo + token: token"

    // Split the response into parts
    TArray<FString> ResponseParts;
    Response.ParseIntoArray(ResponseParts, TEXT(" "), true);

    if (ResponseParts.Num() >= 2)
    {
        FString ResponseType = ResponseParts[0];
        FString messagePart = Response.Mid(ResponseParts[0].Len() + 1);

        if (ResponseType == TEXT("LOGIN_SUCCESS") || ResponseType == TEXT("REGISTER_SUCCESS"))
        {
            int32 CharInfoIndex = ResponseParts.Find(TEXT("charinfo:"));
            if (CharInfoIndex != INDEX_NONE)
            {
                FString CharInfoPart = ResponseParts[CharInfoIndex + 1];
                // Now CharInfoPart contains "character1|level1|appearance1|character2|level2|appearance2"

                // Extract token from the response:
                FString Token = ResponseParts[ResponseParts.Num() - 1];

                // Store the token in the environment variable or a secure storage
                FPlatformMisc::SetEnvironmentVar(TEXT("GAME_TOKEN"), *Token);

                // Access the game instance and set characters info
                if (playerController != nullptr)
                {
                    UGameInstance* GameInstance = playerController->GetGameInstance();
                    if (GameInstance != nullptr)
                    {
                        UComputerSaviourGameInstance* ComputerSaviourGameInstance = Cast<UComputerSaviourGameInstance>(GameInstance);
                        if (ComputerSaviourGameInstance != nullptr)
                        {
                            ComputerSaviourGameInstance->setCharInfo(CharInfoPart);
                            // Transition to the "CharacterSelection" map
                            if(World)
                                UGameplayStatics::OpenLevel(World, TEXT("CharacterSelection"));
                            else ShowErrorWidget("error: World nullptr - cant open new map");
                        }
                        else {
                            ShowErrorWidget("error: Can't get game instance object.");
                        }
                    }
                    else ShowErrorWidget("error: World nullptr.");

                    /*if (ResponseType == TEXT("REGISTER_SUCCESS"))
                        ShowErrorWidget("REGISTER_SUCCESS");
                    else ShowErrorWidget("LOGIN_SUCCESS");*/
                }
                else { // no charinfo? then it must be an error - though this part should never hit because the type should be failure.
                    ShowErrorWidget("No charInfo received.");
                }
            }
            else if (ResponseType == TEXT("LOGIN_FAILURE") || ResponseType == TEXT("REGISTER_FAILURE")
                || ResponseType == TEXT("LOGIN_FAILURE_REGISTER_FAILURE"))
            {
                // Handle failure
                // Display an error message to the user
                ShowErrorWidget(messagePart);
            }
            else { //other error
                ShowErrorWidget(Response);
            }
        }
        else {
            ShowErrorWidget(Response);
        }
    }
}


void ALoginManager::ShowErrorWidget(const FString& ErrorMessage)
{
    // Set the error message on the widget
    if (ErrorWidget)
    {
        ErrorWidget->SetErrorMessage(ErrorMessage);

        // Make the widget visible
        ErrorWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void ALoginManager::SendLoginRequest(const FString& RequestData, bool retry) // retry - is it the first time we call the function or is it a call to retry? to prevent a loop
{
    bSuccessfulRequest = false;
    // Construct the full URL for your login server
    FString ServerURL = TEXT("http://localhost:12345");

    LastLoginRequestData = RequestData;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ServerURL);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));
    HttpRequest->SetContentAsString(RequestData);

    // Use a shared pointer to capture by value
    TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> ProcessRequestLambda =
        [this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            FString ServerResponse;

            if (bWasSuccessful && Response.IsValid())
            {
                // Process the server response
                ServerResponse = Response->GetContentAsString();
                HandleResponse(ServerResponse);
                bSuccessfulRequest = true;
                if (TickerDelegateHandle.IsValid())
                    FTicker::GetCoreTicker().RemoveTicker(TickerDelegateHandle);
            }   
    };

    HttpRequest->OnProcessRequestComplete().BindLambda(ProcessRequestLambda);

    HttpRequest->ProcessRequest();

    if(!retry && ! bSuccessfulRequest)
        TickerDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ALoginManager::RetryLogin), timeoutBetweenRequests);
}

bool ALoginManager::RetryLogin(float DeltaTime)
{
    // Implement your retry logic here
    // You might want to check the number of attempts and decide whether to retry or show an error

    if (NumAttempts < maxAttemptsToConnect)
    {
        // Retry the login
        SendLoginRequest(LastLoginRequestData, true);
        NumAttempts++;
        return true; // Continue ticking
    } 
    else
    {
        // Maximum attempts reached, handle accordingly
        ShowErrorWidget(TEXT("Error: Maximum attempts reached. Unable to connect to the server."));
        NumAttempts = 0;
        return false; // Stop ticking
    }
}


void ALoginManager::LoadGameLevelMap()
{
    // Assuming ServerIP is the IP address of your game server
    FString ServerIP = TEXT("127.0.0.1");  // will need to put EC2's ip addr of the last map the player was on.

    // URL format: "127.0.0.1:7777?name=PlayerName" - will need to put EC2's port.
    FString URL = ServerIP + TEXT(":7777?name=") + FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_TOKEN"));

    // Load the game level - will need to check where was the player last logged in
    UGameplayStatics::OpenLevel(GetWorld(), TEXT("YourGameMapName"), true, URL);
}



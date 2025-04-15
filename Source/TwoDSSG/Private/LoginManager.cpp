// LoginManager.cpp
#include "LoginManager.h"
#include "ComputerSaviourGameInstance.h" // This includes the .h file of the loginManager too
#include <iostream>

ALoginManager::ALoginManager() : maxAttemptsToConnect(1), timeoutBetweenRequests(5.0f), NumAttempts(0)
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = false;
    bCharacterNameAvailable = false;
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

void ALoginManager::HandleResponse(const FString& Response) // response is made of "status_message+' '+charInfo+' '+token:+' '+token.
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
                // Now CharInfoPart contains "character1|level1|gender1|appearance1|character2|level2|gender2|appearance2"

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

void ALoginManager::DeleteCharFromDb(const FString& charName)
{
    if (!playerController) return;

    UGameInstance* GameInstance = playerController->GetGameInstance();
    if (!GameInstance) return;

    UComputerSaviourGameInstance* ComputerSaviourGameInstance = Cast<UComputerSaviourGameInstance>(GameInstance);
    if (!ComputerSaviourGameInstance) return;

    FString RequestData = CreateDeleteCharRequest(charName);
    SendDeleteCharHttpRequest(RequestData, charName);
}

FString ALoginManager::CreateDeleteCharRequest(const FString& charName)
{
    UComputerSaviourGameInstance* GameInstance = Cast<UComputerSaviourGameInstance>(playerController->GetGameInstance());
    FString Token = GameInstance ? GameInstance->getToekenFromSysEnvVar() : TEXT("");
    return FString::Printf(TEXT("DELETEREQUEST %s %s"), *charName, *Token);
}

void ALoginManager::SendDeleteCharHttpRequest(const FString& RequestData, const FString& charName)
{
    FString ServerURL = TEXT("http://localhost:12345");

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ServerURL);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));
    HttpRequest->SetContentAsString(RequestData);

    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, RequestData, charName](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            HandleDeleteCharResponse(Request, Response, bWasSuccessful, RequestData, charName);
        });

    HttpRequest->ProcessRequest();
}

void ALoginManager::HandleDeleteCharResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, const FString& RequestData, const FString& charName)
{
    if (bWasSuccessful && Response.IsValid())
    {
        FString ServerResponse = Response->GetContentAsString();
        if (ServerResponse.Contains(TEXT("SUCCESS")))
        {
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Character '%s' deleted successfully."), *charName));
            NumAttempts = 0;
            return;
        }
    }

    if (++NumAttempts < maxAttemptsToConnect)
    {
        FTimerHandle RetryHandle;
        GetWorld()->GetTimerManager().SetTimer(RetryHandle, FTimerDelegate::CreateLambda([this, RequestData, charName]()
            {
                SendDeleteCharHttpRequest(RequestData, charName);
            }), timeoutBetweenRequests, false);
    }
    else
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to delete character after multiple attempts."));
        NumAttempts = 0;
    }
}

void ALoginManager::CheckName(const FString& charName, const bool isCreation, const FString& charData)
{
    if (playerController != nullptr)
    {
        UGameInstance* GameInstance = playerController->GetGameInstance();
        if (GameInstance != nullptr)
        {
            UComputerSaviourGameInstance* ComputerSaviourGameInstance = Cast<UComputerSaviourGameInstance>(GameInstance);
            if (ComputerSaviourGameInstance != nullptr)
            {
                FString Token = ComputerSaviourGameInstance->getToekenFromSysEnvVar();

                // "CHECKNAME <charName> <charData> <token> <isCreation>"
                FString CheckNameRequest = FString::Printf(TEXT("CHECKNAME %s %s %s %s"), *charName, *charData, *Token, isCreation ? TEXT("1") : TEXT("0"));
                NumAttempts = 0;
                // Send the check name request to the server
                SendCheckNameRequest(CheckNameRequest, isCreation, false); //Last bool is retry indicator - which is false when first trying to reach the server.
            } else 
            {
                if (GEngine)
                    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ComputerSaviourGameInstance is nullptr...(?)"));
            }
        } else {
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("GameInstance is nullptr...(?)"));
        }
    } else {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("playerController is nullptr...(?)"));
    }
}

void ALoginManager::TestLogin() //creates the game instance and populates it  with info without the need to contact the server.
{
    FString CharInfoPart = "Exter|1|0|hair:0-0,face:0-0";
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
                if (World)
                    UGameplayStatics::OpenLevel(World, TEXT("CharacterSelection"));
                else ShowErrorWidget("error: World nullptr - cant open new map");
            }
            else {
                ShowErrorWidget("error: Can't get game instance object.");
            }
        }
        else ShowErrorWidget("error: World nullptr.");
    }
}

void ALoginManager::SendLoginRequest(const FString& RequestData, bool isRetry) // retry - is it the first time we call the function or is it a call to retry? to prevent a loop
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
            bSuccessfulRequest = true;
            HandleResponse(ServerResponse);
            LastLoginRequestData = "";
        }
        else
        {
            // If not successful, retry
            if (!bSuccessfulRequest && NumAttempts < maxAttemptsToConnect)
            {
                NumAttempts++;
                SendLoginRequest(LastLoginRequestData, true);
            }
            else if (!bSuccessfulRequest)
            {
                // Maximum attempts reached, handle accordingly
                ShowErrorWidget(TEXT("Error: Maximum attempts reached. Unable to connect to the server."));
                NumAttempts = 0;
                LastLoginRequestData = "";
            }
        }
    };

    HttpRequest->OnProcessRequestComplete().BindLambda(ProcessRequestLambda);
    HttpRequest->ProcessRequest();
}

void ALoginManager::SendCheckNameRequest(const FString& RequestData, const bool isCreation, bool isRetry)
{
    bSuccessfulRequest = false;
    // Construct the full URL for your login server
    FString ServerURL = TEXT("http://localhost:12345"); //login server's addrs

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ServerURL);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));
    HttpRequest->SetContentAsString(RequestData);

    // Use a shared pointer to capture by value
    TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)> ProcessRequestLambda =
        [this, RequestData, isCreation](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        FString ServerResponse;
        if (bWasSuccessful && Response.IsValid())
        {
            // Process the server response
            ServerResponse = Response->GetContentAsString();
            HandleCheckNameResponse(ServerResponse, isCreation); // Call new function to handle response
            bSuccessfulRequest = true;
        }

        if (!bSuccessfulRequest && NumAttempts < maxAttemptsToConnect)
        {
            NumAttempts++;
            SendCheckNameRequest(RequestData, isCreation, true); // Retry directly
        }
        else if (!bSuccessfulRequest)
        {
            // Maximum attempts reached, handle accordingly
            FString o_message = "Login server is currently unreachable...";
            bCharacterNameAvailable = false;
            if (OnCheckNameResponseReceived.IsBound())
            {
                OnCheckNameResponseReceived.Broadcast(bCharacterNameAvailable, o_message, isCreation);
            }
            NumAttempts = 0;
        }
    };

    HttpRequest->OnProcessRequestComplete().BindLambda(ProcessRequestLambda);
    HttpRequest->ProcessRequest();
}

void ALoginManager::HandleCheckNameResponse(const FString& Response, const bool isCreation)
{
    FString o_message = "";
    if (Response == TEXT("exists"))
    {
        bCharacterNameAvailable = false;
        if (isCreation)
            o_message = "Looks like someone just captured your character's name before you created it! Try to find a different one.";
        else
            o_message = "Character's name is already in use. Please find a different one.";
    }
    else if (Response == TEXT("available"))
    {
        bCharacterNameAvailable = true;
        if (!isCreation)
            o_message = "Name is available.";
    }else if (Response == TEXT("success"))
    {
        bCharacterNameAvailable = true;
        o_message = "Created.";
    }
    else // some error from the login server or the DB.
    {
        bCharacterNameAvailable = false;
        o_message = Response;
    }

    // Notify Blueprint about the response
    if (OnCheckNameResponseReceived.IsBound())
    {
        OnCheckNameResponseReceived.Broadcast(bCharacterNameAvailable, o_message, isCreation);
    }
}

void ALoginManager::LoadGameLevelMap()
{
    // Assuming ServerIP is the IP address of your game server
    FString ServerIP = TEXT("127.0.0.1");  // will need to put EC2's ip addr of the last map the player was on.

    // URL format: "127.0.0.1:7777?name=PlayerName" - will need to put EC2's port.
    FString URL = ServerIP + TEXT(":7777?name=") + FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_TOKEN"));

    // Load the game level - will need to check where was the player last logged in
    UGameplayStatics::OpenLevel(GetWorld(), TEXT("WhereTheJourneyBegins"), true, URL);
}

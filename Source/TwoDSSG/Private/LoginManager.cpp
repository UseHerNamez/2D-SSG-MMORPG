// LoginManager.cpp
#include "LoginManager.h"
#include "ComputerSaviourGameInstance.h"
#include <iostream>

ULoginManager::ULoginManager() : maxAttemptsToConnect(1), timeoutBetweenRequests(5.0f), NumAttempts(0)
{
    bCharacterNameAvailable = false;
    bSuccessfulRequest = false;
}

void ULoginManager::SetErrorWidget(ULoginErrorWidget * i_ErrorWidget)
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

void ULoginManager::Login(const FString& Username, const FString& Password)
{
    // Construct the login request data
    FString LoginRequest = FString::Printf(TEXT("LOGIN %s %s"), *Username, *Password);

    // Send the login request to the server
    SendLoginRequest(LoginRequest, false);
}

void ULoginManager::Register(const FString& Username, const FString& Password)
{
    // Construct the registration request data
    FString RegisterRequest = FString::Printf(TEXT("REGISTER %s %s"), *Username, *Password);

    // Send the registration request to the server
    SendLoginRequest(RegisterRequest, false);
}

void ULoginManager::HandleResponse(const FString& Response) // response is made of "status_message+' '+charInfo+' '+token:+' '+token.
{
    // Example response format: LOGIN_SUCCESS charinfo: charactersInfo + token: token"

    // Split the response into parts
    TArray<FString> ResponseParts;
    Response.ParseIntoArray(ResponseParts, TEXT(" "), true); //divide by spaces, can be seen in the login server

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
                FPlatformMisc::SetEnvironmentVar(TEXT("CSGAME_TOKEN"), *Token);                      

                if (ComputerSaviourGameInstance != nullptr)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, CharInfoPart);
                    ComputerSaviourGameInstance->setCharInfo(CharInfoPart);
                    // Transition to the "CharacterSelection" map
                    if(ComputerSaviourGameInstance->GetCurrWorld())
                        UGameplayStatics::OpenLevel(ComputerSaviourGameInstance->GetCurrWorld(), TEXT("CharacterSelection"));
                    else ShowErrorWidget("error: World nullptr - cant open new map");
                }
                else {
                    ShowErrorWidget("error: Can't get game instance object.");
                }
            }
            else if (ResponseType == TEXT("LOGIN_FAILURE") || ResponseType == TEXT("REGISTER_FAILURE")
                || ResponseType == TEXT("ERROR"))
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
    } else ShowErrorWidget("Bad response format - not as expected");
}

void ULoginManager::ShowErrorWidget(const FString& ErrorMessage)
{
    // Set the error message on the widget
    if (ErrorWidget)
    {
        ErrorWidget->SetErrorMessage(ErrorMessage);

        // Make the widget visible
        ErrorWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void ULoginManager::DeleteCharFromDb(const FString& charName)
{
    FString RequestData = CreateDeleteCharRequest(charName);
    SendDeleteCharHttpRequest(RequestData, charName);
}

FString ULoginManager::CreateDeleteCharRequest(const FString& charName)
{
    FString Token = ComputerSaviourGameInstance ? ComputerSaviourGameInstance->getToekenFromSysEnvVar() : TEXT("");
    return FString::Printf(TEXT("DELETEREQUEST %s %s"), *charName, *Token);
}

void ULoginManager::SendDeleteCharHttpRequest(const FString& RequestData, const FString& charName)
{
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(LoginServerURL);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));
    HttpRequest->SetContentAsString(RequestData);

    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, RequestData, charName](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            HandleDeleteCharResponse(Request, Response, bWasSuccessful, RequestData, charName);
        });

    HttpRequest->ProcessRequest();
}

void ULoginManager::HandleDeleteCharResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, const FString& RequestData, const FString& charName)
{
    if (bWasSuccessful && Response.IsValid())
    {
        FString ServerResponse = Response->GetContentAsString();
        NumAttempts = 0;

        if (ServerResponse.Contains(TEXT("TOKEN")))
        {
            // Handle token issue: force logout or show login screen
            ComputerSaviourGameInstance->OnForceLogout();
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Session expired or invalid token. Returning to login screen..."));
            return;
        }
        else if (ServerResponse.Contains(TEXT("ERROR"))) //expecting ERROR some_reason
        {
            TArray<FString> ResponseParts;
            ServerResponse.ParseIntoArray(ResponseParts, TEXT(" "), true);

            ComputerSaviourGameInstance->OnForceLogout();
            if (GEngine)
            {
                if (ResponseParts.Num() > 1 && !ResponseParts[1].IsEmpty())
                    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, ResponseParts[1]);
                else GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR"));
            }
            return;
        }
        else if (ServerResponse.Contains(TEXT("CHARACTER_DELETED")) || ServerResponse.Contains(TEXT("CHARACTER_NOT_FOUND")))
        {
            OnDeleteCharResponse.Broadcast(ServerResponse);
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("DeleteChar Response: %s"), *ServerResponse));
            return;
        }
        else // some other weird unexpected return message..
        {
            ComputerSaviourGameInstance->OnForceLogout();
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Unknown Error"));
            return;
        }
    }
    else 
    {
        OnDeleteCharResponse.Broadcast("Response for character deletion operation was not successful.");
    }
}

void ULoginManager::CheckName(const FString& charName, const bool isCreation, const FString& charData)
{
    if (ComputerSaviourGameInstance != nullptr)
    {
        FString Token = ComputerSaviourGameInstance->getToekenFromSysEnvVar();

        // "CHECKNAME <charName> <charData> <token> <isCreation>"
        FString CheckNameRequest = FString::Printf(TEXT("CHECKNAME %s %s %s %s"), *charName, *charData, *Token, isCreation ? TEXT("1") : TEXT("0"));
        NumAttempts = 0;
        // Send the check name request to the server
        SendCheckNameRequest(CheckNameRequest, isCreation, false); //Last bool is retry indicator - which is false when its the first time trying to reach the server.
    } else 
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ComputerSaviourGameInstance is nullptr...(?)"));
    }
}

void ULoginManager::TestLogin() //creates the game instance and populates it  with info without the need to contact the server.
{
    FString CharInfoPart = "Exter|1|0|hair:0-0,face:0-0";

    if (ComputerSaviourGameInstance != nullptr)
    {
        ComputerSaviourGameInstance->setCharInfo(CharInfoPart);
        // Transition to the "CharacterSelection" map
        if (ComputerSaviourGameInstance->GetCurrWorld())
            UGameplayStatics::OpenLevel(ComputerSaviourGameInstance->GetCurrWorld(), TEXT("CharacterSelection"));
        else ShowErrorWidget("error: World nullptr - cant open new map");
    }
    else {
        ShowErrorWidget("error: Can't get game instance object.");
    } 
}

void ULoginManager::setCSGameInstance(UComputerSaviourGameInstance* gameInstance)
{
    if(gameInstance)
        ComputerSaviourGameInstance = gameInstance;
}

UComputerSaviourGameInstance* ULoginManager::GetGameInstance()
{
    return ComputerSaviourGameInstance;
}

void ULoginManager::SendLoginRequest(const FString& RequestData, bool isRetry) // retry - is it the first time we call the function or is it a call to retry? to prevent a loop
{
    bSuccessfulRequest = false;

    LastLoginRequestData = RequestData;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(LoginServerURL);
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

void ULoginManager::SendCheckNameRequest(const FString& RequestData, const bool isCreation, bool isRetry)
{
    bSuccessfulRequest = false;

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(LoginServerURL);
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
            FString o_message = "Login server is currently unreachable... try again in a minute";
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

void ULoginManager::HandleCheckNameResponse(const FString& Response, const bool isCreation)
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
        OnCheckNameResponseReceived.Broadcast(bCharacterNameAvailable, o_message, isCreation);
}

/*void ULoginManager::LoadGameLevelMap()
{
    // Assuming ServerIP is the IP address of your game server
    FString ServerIP = TEXT("127.0.0.1");  // will need to put EC2's ip addr of the last map the player was on.

    // URL format: "127.0.0.1:7777?name=PlayerName" - will need to put EC2's port.
    FString URL = ServerIP + TEXT(":7777?name=") + FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_TOKEN"));

    // Load the game level - will need to check where was the player last logged in
    UGameplayStatics::OpenLevel(GetWorld(), TEXT("WhereTheJourneyBegins"), true, URL);
}*/

void ULoginManager::SelectCharacter(const FString& charName)
{
    if (ComputerSaviourGameInstance != nullptr)
    {
        FString Token = ComputerSaviourGameInstance->getToekenFromSysEnvVar();
        FString SelectCharRequest = FString::Printf(TEXT("CHARSELECT %s %s"), *charName, *Token);
        NumAttempts = 0;

        SendSelectCharRequest(SelectCharRequest, charName);
    }
}

void ULoginManager::SendSelectCharRequest(const FString& RequestData, const FString& charName) // here we also handle response(inside lambda)
{
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(LoginServerURL);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));
    HttpRequest->SetContentAsString(RequestData);

    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, charName](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Character selection request failed."));
                return;
            }

            FString ResponseStr = Response->GetContentAsString();
            UE_LOG(LogTemp, Display, TEXT("Login server response: %s"), *ResponseStr);

            if (Response->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> JsonObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
                {
                    FString Token;
                    FString MapName;
                    FString ServerAddressFromResponse;

                    bool bHasToken = JsonObject->TryGetStringField(TEXT("token"), Token);
                    bool bHasMap = JsonObject->TryGetStringField(TEXT("map"), MapName);
                    bool bHasServerAddress = JsonObject->TryGetStringField(TEXT("server_address"), ServerAddressFromResponse);

                    if (!bHasToken)
                    {
                        UE_LOG(LogTemp, Error, TEXT("Server response missing token."));
                        return; // need to call a UE error for the user to see.
                    }

                    // If there's no map or the map is empty, use the default map name
                    if (!bHasMap || MapName.IsEmpty())
                    {
                        MapName = TEXT("WhereTheJourneyBegins");
                    }

                    // Use the server address from the response if it exists, otherwise fallback to default
                    FString FinalServerAddress = (bHasServerAddress && !ServerAddressFromResponse.IsEmpty())
                        ? ServerAddressFromResponse : DefaultServerAddress;

                    FString EncodedToken = FPlatformHttp::UrlEncode(Token);
                    FString TravelURL = FString::Printf(TEXT("%s?/%s?AuthToken=%s"),*FinalServerAddress, *MapName, *Token);
                    UE_LOG(LogTemp, Log, TEXT("Traveling to server: %s with token: %s"), *TravelURL, *Token);

                    UWorld* World = GetWorld();
                    if (World)
                    {
                        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
                        if (PC)
                        {
                            PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("PlayerController not found."));
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("World not found."));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response."));
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Character selection failed with code %d"), Response->GetResponseCode());
            }
        });

    HttpRequest->ProcessRequest();
}

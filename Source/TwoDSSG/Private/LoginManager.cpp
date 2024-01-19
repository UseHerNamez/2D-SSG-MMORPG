// LoginManager.cpp

#include "LoginManager.h"

ALoginManager::ALoginManager() : maxAttemptsToConnect(2), timeoutBetweenRequests(3.0f), NumAttempts(0)
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

void ALoginManager::HandleLoginResponse(const FString& Response)
{
    // Example response format: "LOGIN_SUCCESS Token12345"

    // Split the response into parts
    TArray<FString> ResponseParts;
    Response.ParseIntoArray(ResponseParts, TEXT(" "), true);

    if (ResponseParts.Num() >= 2)
    {
        FString ResponseType = ResponseParts[0];

        if (ResponseType == TEXT("LOGIN_SUCCESS"))
        {
            // Extract token from the response
            FString Token = ResponseParts[1];

            // Store the token in the environment variable or a secure storage
            FPlatformMisc::SetEnvironmentVar(TEXT("GAME_TOKEN"), *Token);

            // TODO: Transition to the main game level!!
        }
        else if (ResponseType == TEXT("LOGIN_FAILURE") || ResponseType == TEXT("LOGIN_FAILURE_REGISTER_FAILURE"))
        {
            // Handle login failure
            // Display an error message to the user
            FString errorMessage = ResponseParts[1];
            ShowErrorWidget(errorMessage);
        }
        else { //other error
            ShowErrorWidget(Response);
        }
    }
    else {
        ShowErrorWidget(Response);
    }
}

void ALoginManager::HandleRegisterResponse(const FString& Response)
{
    // Example response format: "REGISTER_SUCCESS Token12345"

    // Split the response into parts
    TArray<FString> ResponseParts;
    Response.ParseIntoArray(ResponseParts, TEXT(" "), true);

    if (ResponseParts.Num() >= 2)
    {
        FString ResponseType = ResponseParts[0];

        if (ResponseType == TEXT("REGISTER_SUCCESS"))
        {
            // Extract token from the response
            FString Token = ResponseParts[1];

            // Store the token in the environment variable or a secure storage
            FPlatformMisc::SetEnvironmentVar(TEXT("GAME_TOKEN"), *Token);

            // TODO: Transition to the main game level!!

            // Uncomment the following line when ready to transition
            // LoadGameLevelMap();
        }
        else if (ResponseType == TEXT("REGISTER_FAILURE") || ResponseType == TEXT("LOGIN_FAILURE_REGISTER_FAILURE"))
        {
            // Handle registration failure
            // Display an error message to the user
            FString errorMessage = ResponseParts[1];
            ShowErrorWidget(errorMessage);
        }
        else { //ther error
            ShowErrorWidget(Response);
        }
    }
    else {
        ShowErrorWidget(Response);
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

void ALoginManager::SendLoginRequest(const FString& RequestData, bool retry)
{
    // Construct the full URL for your login server
    FString ServerURL = TEXT("http://localhost:12345");

    LastLoginRequestData = RequestData;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ServerURL);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
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
                HandleLoginResponse(ServerResponse);
                bSuccessfulRequest = true;
            }   
    };

    HttpRequest->OnProcessRequestComplete().BindLambda(ProcessRequestLambda);

    HttpRequest->ProcessRequest();

    if(!retry && ! bSuccessfulRequest)
        FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ALoginManager::RetryLogin), timeoutBetweenRequests);
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



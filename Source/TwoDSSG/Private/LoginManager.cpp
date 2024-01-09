// LoginManager.cpp

#include "LoginManager.h"

ALoginManager::ALoginManager()
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = false;
}

void ALoginManager::Login(const FString& Username, const FString& Password)
{
    // Construct the login request data
    FString LoginRequest = FString::Printf(TEXT("LOGIN %s %s"), *Username, *Password);

    // Send the login request to the server
    SendLoginRequest(LoginRequest);
}

void ALoginManager::Register(const FString& Username, const FString& Password)
{
    // Construct the registration request data
    FString RegisterRequest = FString::Printf(TEXT("REGISTER %s %s"), *Username, *Password);

    // Send the registration request to the server
    SendLoginRequest(RegisterRequest);
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
        else if (ResponseType == TEXT("LOGIN_FAILURE"))
        {
            // Handle login failure
            // Display an error message to the user
            // need to load an appropriate widget.
        }
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
        else if (ResponseType == TEXT("REGISTER_FAILURE"))
        {
            // Handle registration failure
            // Display an error message to the user
            // TODO: Load an appropriate widget for registration failure
        }
    }
}

void ALoginManager::ShowErrorWidget(const FString& ErrorMessage)
{
    // Create an instance of the LoginErrorWidget
    ULoginErrorWidget* ErrorWidget = CreateWidget<ULoginErrorWidget>(GetWorld(), ULoginErrorWidget::StaticClass());

    if (ErrorWidget)
    {
        // Set the error message on the widget
        ErrorWidget->SetErrorMessage(ErrorMessage);

        // Add the widget to the viewport
        ErrorWidget->AddToViewport();
    }
}

void ALoginManager::SendLoginRequest(const FString& RequestData)
{
    // Construct the full URL for your login server
    FString ServerURL = TEXT("http://localhost:12345");

    // Create an HTTP request
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ServerURL);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
    HttpRequest->SetContentAsString(RequestData);

    FString ServerResponse;
    // Set up a lambda function to handle the HTTP response
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, &ServerResponse](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                // Process the server response
                ServerResponse = Response->GetContentAsString();
                HandleLoginResponse(ServerResponse);
            }
            else
            {
                // Handle the error
                ServerResponse = TEXT("Error: Unable to connect to the server.");
                HandleLoginResponse(ServerResponse);
            }
        });

    // Send the request
    HttpRequest->ProcessRequest();
}

void ALoginManager::LoadGameLevelMap()
{
    // Assuming ServerIP is the IP address of your game server
    FString ServerIP = TEXT("xxx.xxx.xxx.xxx");  // will need to put EC2's ip addr of the last map the player was on.

    // URL format: "127.0.0.1:7777?name=PlayerName" - will need to put EC2's port.
    FString URL = ServerIP + TEXT(":7777?name=") + FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_TOKEN"));

    // Load the game level - will need to check where was the player last logged in
    UGameplayStatics::OpenLevel(GetWorld(), TEXT("YourGameMapName"), true, URL);
}



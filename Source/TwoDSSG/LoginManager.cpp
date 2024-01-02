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
    // TODO: Parse and handle the login response from the server
    // Extract the token and other information

    // Example:
    if (Response.StartsWith(TEXT("LOGIN_SUCCESS")))
    {
        // Extract token from the response
        // FString Token = ExtractTokenFromResponse(Response);

        // Store the token in the environment variable or a secure storage
        FPlatformMisc::SetEnvironmentVar(TEXT("GAME_TOKEN"), *Token);

        // TODO: Transition to the main game level
    }
    else if (Response.StartsWith(TEXT("LOGIN_FAILURE")))
    {
        // Handle login failure
        // Display an error message to the user
    }
}

void ALoginManager::HandleRegisterResponse(const FString& Response)
{
    // TODO: Parse and handle the registration response from the server
    // Extract the token and other information

    // Example:
    if (Response.StartsWith(TEXT("REGISTER_SUCCESS")))
    {
        // Extract token from the response
        // FString Token = ExtractTokenFromResponse(Response);

        // Store the token in the environment variable or a secure storage
        FPlatformMisc::SetEnvironmentVar(TEXT("GAME_TOKEN"), *Token);

        // TODO: Transition to the main game level
    }
    else if (Response.StartsWith(TEXT("REGISTER_FAILURE")))
    {
        // Handle registration failure
        // Display an error message to the user
    }
}

void ALoginManager::SendLoginRequest(const FString& RequestData)
{
    // Construct the full URL for your login server
    FString ServerURL = TEXT("https://your-login-server-url.com/login");

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

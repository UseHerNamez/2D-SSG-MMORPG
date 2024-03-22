// ULoginWidgetBase.cpp

#include "LoginWidgetBase.h"
#include "LoginManager.h"

ULoginWidgetBase::ULoginWidgetBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    
}

ALoginManager* ULoginWidgetBase::InitializeLoginManager(APlayerController* i_playerController)
{
    LoginManager = NewObject<ALoginManager>(this);
    LoginManager->SetPlayerController(i_playerController);
    LoginManager->SetWorld();
    return LoginManager;
}

void ULoginWidgetBase::OnLoginButtonReleased(const FString& Username, const FString& Password)
{
    LoginManager->Login(Username, Password);
}

void ULoginWidgetBase::OnRegisterButtonReleased(const FString& Username, const FString& Password)
{
    // Implement your logic here
    // call ALoginManager::Register
    LoginManager->Register(Username, Password);
}

void ULoginWidgetBase::ShowErrorWidget(const FString& errorMessage)
{
    if (LoginManager)
    {
        LoginManager->ShowErrorWidget(errorMessage);
    }
}


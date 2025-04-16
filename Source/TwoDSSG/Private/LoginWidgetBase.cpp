// ULoginWidgetBase.cpp

#include "LoginWidgetBase.h"
#include "LoginManager.h"
#include "ComputerSaviourGameInstance.h"

ULoginWidgetBase::ULoginWidgetBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    
}

ULoginManager* ULoginWidgetBase::InitializeLoginManager(APlayerController* i_playerController)
{
    LoginManager = NewObject<ULoginManager>(this);
    if (i_playerController != nullptr)
    {
        UGameInstance* GameInstance = i_playerController->GetGameInstance();
        if (GameInstance != nullptr)
        {
            UComputerSaviourGameInstance* ComputerSaviourGameInstance = Cast<UComputerSaviourGameInstance>(GameInstance);
            if (ComputerSaviourGameInstance != nullptr)
            {
                ComputerSaviourGameInstance->SetLoginManager(LoginManager);
                LoginManager->setCSGameInstance(ComputerSaviourGameInstance);
            }
        }
    }
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


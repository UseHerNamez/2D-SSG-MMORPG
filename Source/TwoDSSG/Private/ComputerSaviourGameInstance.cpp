#include "ComputerSaviourGameInstance.h"
#include "LoginManager.h"

UComputerSaviourGameInstance::UComputerSaviourGameInstance()
{
    // Initialize default values
    CharactersInfo = FString(TEXT("EMPTY")); // Or initialize with any default data if needed
}

void UComputerSaviourGameInstance::setCharInfo(const FString& charInfo)
{
    CharactersInfo = charInfo;
}

FString UComputerSaviourGameInstance::getToekenFromSysEnvVar()
{
    FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_TOKEN"));

    // Trim leading and trailing spaces or newline characters
    Token = Token.TrimStartAndEnd();

    if (Token.IsEmpty())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Can't find the game token as a sys var."));
        }
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Found game token!"));
        }
    }
    return Token;
}

bool UComputerSaviourGameInstance::isCharInfoEmpty()
{
    if(CharactersInfo.Equals(TEXT("EMPTY")) && !CharactersInfo.IsEmpty())
    {
        return true;
    }
    return false;
}

FString UComputerSaviourGameInstance::getCharInfo()
{
    return CharactersInfo;
}

void UComputerSaviourGameInstance::SetLoginManager(ULoginManager* const i_loginManager)
{
    loginManagerREF = i_loginManager;
}

ULoginManager* UComputerSaviourGameInstance::GetLoginManager() const
{
    return loginManagerREF;
}

APlayerController* UComputerSaviourGameInstance::GetPlayerController()
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    return UGameplayStatics::GetPlayerController(World, 0); // 0 = first local player
}

UWorld * UComputerSaviourGameInstance::GetCurrWorld()
{
    UWorld* World = GetWorld();
    return World;
}
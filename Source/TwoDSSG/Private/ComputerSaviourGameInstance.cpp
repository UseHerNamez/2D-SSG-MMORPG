#include "ComputerSaviourGameInstance.h"

UComputerSaviourGameInstance::UComputerSaviourGameInstance()
{
    // Initialize default values
    CharactersInfo = FString(TEXT("EMPTY")); // Or initialize with any default data if needed
}

void UComputerSaviourGameInstance::setCharInfo(const FString& charInfo)
{
    CharactersInfo = charInfo;
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

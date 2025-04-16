

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
class LoginManager;
#include "ComputerSaviourGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class TWODSSG_API UComputerSaviourGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
    UComputerSaviourGameInstance();

    UFUNCTION(BlueprintCallable)
    void SetLoginManager(ULoginManager* const i_loginManager);

    UFUNCTION(BlueprintCallable)
    ULoginManager* GetLoginManager() const;

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    bool isCharInfoEmpty();

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    FString getCharInfo();

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    void setCharInfo(const FString& charInfo);

    UFUNCTION(BlueprintCallable)
    FString getToekenFromSysEnvVar();

    APlayerController* UComputerSaviourGameInstance::GetPlayerController();
    UWorld* UComputerSaviourGameInstance::GetCurrWorld();

private:

    FString CharactersInfo;

    UPROPERTY()
    ULoginManager* loginManagerREF;

    // Add any functions or additional data you need here
};

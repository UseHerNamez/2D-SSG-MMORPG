

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LoginManager.h"
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
    void SetLoginManager(ALoginManager* const i_loginManager);

    UFUNCTION(BlueprintCallable)
    void CreateLoginManager(APlayerController* i_playerController);

    UFUNCTION(BlueprintCallable)
    ALoginManager* GetLoginManager() const;

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    bool isCharInfoEmpty();

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    FString getCharInfo();

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    void setCharInfo(const FString& charInfo);

    UFUNCTION(BlueprintCallable)
    FString getToekenFromSysEnvVar();

private:

    FString CharactersInfo;
    ALoginManager* loginManagerREF;

    // Add any functions or additional data you need here
};

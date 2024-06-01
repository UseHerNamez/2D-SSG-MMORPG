

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

    void SetLoginManager(ALoginManager* const i_loginManager);

    UFUNCTION(BlueprintCallable)
    ALoginManager* GetLoginManager() const;

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    bool isCharInfoEmpty();

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    FString getCharInfo();

    UFUNCTION(BlueprintCallable, Category = "Character Info")
    void setCharInfo(const FString& charInfo);
private:
    FString CharactersInfo;
    ALoginManager* loginManagerREF;

    // Add any functions or additional data you need here
};

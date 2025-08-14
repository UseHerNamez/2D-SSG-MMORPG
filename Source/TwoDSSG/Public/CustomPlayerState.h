// CustomPlayerState.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CharacterInitTypes.h"
#include "ServerInitTypes.h"
#include "CustomPlayerState.generated.h"

UCLASS()
class TWODSSG_API ACustomPlayerState : public APlayerState {
    GENERATED_BODY()
public:
    UPROPERTY(ReplicatedUsing = OnRep_InitData, BlueprintReadOnly, Category = "Init")
        FCharacterInitData_Client InitData;

    UFUNCTION() void OnRep_InitData();


    UFUNCTION(BlueprintImplementableEvent, Category = "Init")
        void BP_OnInitDataReceived(); // optional BP event for UI

    // called by server GameMode once DB returns
    void SetInitData_Server(const FCharacterInitData_Client& In)
    {
        check(HasAuthority());
        InitData = In;
        OnRep_InitData(); // let server side react too if needed
    }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;


    #if WITH_SERVER_CODE
    private:
        // server only - never compiled into client builds
        FCharacterInitData_Server ServerOnly;
    public:
        void SetServerOnlyData(const FCharacterInitData_Server& In) { check(HasAuthority()); ServerOnly = In; }
        const FCharacterInitData_Server& GetServerOnlyData() const { check(HasAuthority()); return ServerOnly; }
    #endif
};
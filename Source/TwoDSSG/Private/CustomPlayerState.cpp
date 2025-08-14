// CustomPlayerState.cpp
#include "CustomPlayerState.h"
#include "Net/UnrealNetwork.h"

void ACustomPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACustomPlayerState, InitData);
}

void ACustomPlayerState::OnRep_InitData()
{
    BP_OnInitDataReceived(); // BP can read InitData and update UI
}

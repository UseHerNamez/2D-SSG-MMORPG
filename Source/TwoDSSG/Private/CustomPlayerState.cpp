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

int32 ACustomPlayerState::GetCharId_Server() const
{
    // Runtime guard so it only makes sense on authority
    if (!HasAuthority()) { return -1; }

#if WITH_SERVER_CODE
    return ServerOnly.CharId;
#else
    return -1;
#endif
}

int32 ACustomPlayerState::GetUserId_Server() const
{
    if (!HasAuthority()) { return -1; }

#if WITH_SERVER_CODE
    return ServerOnly.UserId;
#else
    return -1;
#endif
}
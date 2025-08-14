#pragma once
#include "CoreMinimal.h"

// Only compiled for server builds
#if WITH_SERVER_CODE

struct FCharacterInitData_Server
{
    int32 UserId = -1;
    int32 CharId = -1;

};

#endif // WITH_SERVER_CODE

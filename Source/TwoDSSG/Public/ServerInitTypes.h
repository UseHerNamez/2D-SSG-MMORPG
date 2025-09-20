#pragma once
#include "CoreMinimal.h"

// NOTE: Plain C++ struct, always defined, not a USTRUCT, not reflected.
// Safe to include in headers and use as private members.
struct FCharacterInitData_Server
{
    int32 UserId = -1;
    int32 CharId = -1;
};

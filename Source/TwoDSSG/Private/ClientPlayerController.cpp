#include "ClientPlayerController.h"
#include "Kismet/GameplayStatics.h"


void AClientPlayerController::ShowLoadingWidget()
{
}

void AClientPlayerController::BeginPlay()
{
    Super::BeginPlay();

    FString ErrorMessage = GetWorld()->URL.GetOption(TEXT("error"), TEXT(""));

    if (!ErrorMessage.IsEmpty())
    {
        HandleReturnedFromServer(ErrorMessage);
    }
}

void AClientPlayerController::HandleReturnedFromServer(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Warning, TEXT("Got error from travel: %s"), *ErrorMessage);

    ShowErrorWidget(ErrorMessage);
}
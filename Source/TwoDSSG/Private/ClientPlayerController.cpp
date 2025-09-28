#include "ClientPlayerController.h"
#include "Kismet/GameplayStatics.h"

void AClientPlayerController::RPC_ShowLoadingWidget_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("RPC_ShowLoadingWidget received on client"));
    ShowLoadingWidget();
}

void AClientPlayerController::RPC_HideLoadingWidget_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("RPC_HideLoadingWidget received on client"));
    HideLoadingWidget();
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
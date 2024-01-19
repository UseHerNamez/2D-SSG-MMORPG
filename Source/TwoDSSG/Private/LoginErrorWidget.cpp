#include "LoginErrorWidget.h"
#include "Components/TextBlock.h"

// Constructor with FObjectInitializer
ULoginErrorWidget::ULoginErrorWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Initialize your variables here if needed
    ErrorMessageText = nullptr;
}

void ULoginErrorWidget::SetErrorMessage(const FString& ErrorMessage)
{
    if (ErrorMessageText)
    {
        ErrorMessageText->SetText(FText::FromString(ErrorMessage));
    }
}
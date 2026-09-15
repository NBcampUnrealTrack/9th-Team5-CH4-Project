#include "DRLoadingViewModel.h"

void UDRLoadingViewModel::SetProgress(float InProgress)
{
	const float NewProgress = FMath::IsFinite(InProgress) ? FMath::Clamp(InProgress, 0.f, 1.f) : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(Progress, NewProgress);
}

void UDRLoadingViewModel::SetLoadingMessage(const FString& InMessage)
{
	UE_MVVM_SET_PROPERTY_VALUE(LoadingMessage, InMessage);
}

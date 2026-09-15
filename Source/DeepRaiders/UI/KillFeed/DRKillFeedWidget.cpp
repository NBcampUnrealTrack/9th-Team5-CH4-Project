#include "DRKillFeedWidget.h"

#include "DeepRaiders/Player/DRPlayerController.h"

void UDRKillFeedWidget::InitializeKillFeed(
	ADRPlayerController* InPlayerController)
{
	if (!IsValid(InPlayerController))
	{
		return;
	}

	PlayerController = InPlayerController;

	InPlayerController->OnKillFeedEntry.AddDynamic(
		this,
		&ThisClass::HandleKillFeedEntry);
}

void UDRKillFeedWidget::NativeDestruct()
{
	if (PlayerController.IsValid())
	{
		PlayerController->OnKillFeedEntry.RemoveDynamic(
			this,
			&ThisClass::HandleKillFeedEntry);
	}

	PlayerController.Reset();

	Super::NativeDestruct();
}

void UDRKillFeedWidget::HandleKillFeedEntry(
	FString KillerName,
	FString VictimName)
{
	AddKillFeedEntry(KillerName, VictimName);
}
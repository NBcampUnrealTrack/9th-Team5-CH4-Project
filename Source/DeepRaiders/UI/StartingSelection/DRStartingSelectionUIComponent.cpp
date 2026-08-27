#include "DRStartingSelectionUIComponent.h"

#include "DRStartingSelectionWidget.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/Components/DRStartingSelectionComponent.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

UDRStartingSelectionUIComponent::UDRStartingSelectionUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRStartingSelectionUIComponent::ShowStartingSelection(
	UDRStartingSelectionComponent* InSelectionComponent)
{
	PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(InSelectionComponent)
		|| InSelectionComponent->IsSelectionComplete()
		|| IsValid(StartingSelectionWidget))
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UIManager = IsValid(LocalPlayer)
		? LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>()
		: nullptr;

	if (!IsValid(UIManager))
	{
		return;
	}

	StartingSelectionWidget = Cast<UDRStartingSelectionWidget>(
		UIManager->PushScreen(DRGameplayTags::UI_Screen_StartingSelection));

	if (!IsValid(StartingSelectionWidget))
	{
		return;
	}

	StartingSelectionWidget->OnSelectionCompleted.AddDynamic(
		this,
		&ThisClass::HideStartingSelection);
	StartingSelectionWidget->InitializeSelection(InSelectionComponent);
	PlayerController->FlushPressedKeys();
	PlayerController->SetIgnoreMoveInput(true);
	IsMoveInputBlocked = true;
}

void UDRStartingSelectionUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	HideStartingSelection();
	PlayerController = nullptr;
	UIManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UDRStartingSelectionUIComponent::HideStartingSelection()
{
	if (IsValid(StartingSelectionWidget))
	{
		StartingSelectionWidget->OnSelectionCompleted.RemoveDynamic(
			this,
			&ThisClass::HideStartingSelection);

		if (IsValid(UIManager))
		{
			UIManager->PopScreen(DRGameplayTags::UI_Screen_StartingSelection);
		}
		else
		{
			StartingSelectionWidget->RemoveFromParent();
		}
	}

	StartingSelectionWidget = nullptr;

	if (IsValid(PlayerController) && IsMoveInputBlocked)
	{
		PlayerController->FlushPressedKeys();
		PlayerController->SetIgnoreMoveInput(false);
	}

	IsMoveInputBlocked = false;
}

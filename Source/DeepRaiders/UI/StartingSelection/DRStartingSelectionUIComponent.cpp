#include "DRStartingSelectionUIComponent.h"

#include "DRStartingSelectionWidget.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
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

void UDRStartingSelectionUIComponent::InitializeStartingSelection(
	UDRStartingSelectionComponent* InSelectionComponent)
{
	PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(InSelectionComponent))
	{
		return;
	}

	SelectionComponent = InSelectionComponent;
	MiningGameState = PlayerController->GetWorld()->GetGameState<ADRMiningGameStateBase>();
	if (!IsValid(MiningGameState))
	{
		return;
	}

	MiningGameState->OnGameTimerChanged.AddDynamic(
		this,
		&ThisClass::HandleGameTimerChanged);

	if (MiningGameState->IsGameStarted())
	{
		ShowStartingSelection();
	}
}

void UDRStartingSelectionUIComponent::ShowStartingSelection()
{
	if (!IsValid(SelectionComponent)
		|| SelectionComponent->IsSelectionComplete()
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
	PlayerController->FlushPressedKeys();
	PlayerController->SetIgnoreMoveInput(true);
	IsMoveInputBlocked = true;
	StartingSelectionWidget->InitializeSelection(SelectionComponent);
}

void UDRStartingSelectionUIComponent::HandleGameTimerChanged(
	int32,
	bool IsGameStarted,
	bool)
{
	if (IsGameStarted)
	{
		ShowStartingSelection();
	}
}

void UDRStartingSelectionUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MiningGameState))
	{
		MiningGameState->OnGameTimerChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameTimerChanged);
	}

	HideStartingSelection();
	PlayerController = nullptr;
	SelectionComponent = nullptr;
	MiningGameState = nullptr;
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

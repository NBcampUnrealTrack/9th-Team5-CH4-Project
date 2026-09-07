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
	MiningGameState->OnGameFlowStateChanged.AddDynamic(
		this,
		&ThisClass::HandleGameFlowStateChanged);

	bIsGameLoading = MiningGameState->GetGameFlowState() == EDRGameFlowState::Loading
		|| MiningGameState->GetGameFlowState() == EDRGameFlowState::Countdown;
	if (bIsGameLoading || MiningGameState->IsGameStarted())
	{
		ShowStartingSelection();
	}
	RefreshMoveInput();
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
	UIManager->RegisterCloseHandler(
		StartingSelectionWidget, FSimpleDelegate::CreateUObject(this, &ThisClass::HideStartingSelection));
	PlayerController->FlushPressedKeys();
	StartingSelectionWidget->InitializeSelection(SelectionComponent);
	RefreshMoveInput();
}

void UDRStartingSelectionUIComponent::HandleGameFlowStateChanged(EDRGameFlowState GameFlowState)
{
	bIsGameLoading = GameFlowState == EDRGameFlowState::Loading
		|| GameFlowState == EDRGameFlowState::Countdown;
	if (bIsGameLoading)
	{
		ShowStartingSelection();
	}
	RefreshMoveInput();
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
		MiningGameState->OnGameFlowStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameFlowStateChanged);
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

	RefreshMoveInput();
}

void UDRStartingSelectionUIComponent::RefreshMoveInput()
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	const bool bShouldBlock = bIsGameLoading || IsValid(StartingSelectionWidget);
	if (IsMoveInputBlocked == bShouldBlock)
	{
		return;
	}

	PlayerController->FlushPressedKeys();
	PlayerController->SetIgnoreMoveInput(bShouldBlock);
	IsMoveInputBlocked = bShouldBlock;
}

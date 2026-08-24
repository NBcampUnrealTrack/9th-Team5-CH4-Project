#include "DRStartingWeaponUIComponent.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/StartingWeapon/DRStartingWeaponSelectWidget.h"
#include "Engine/LocalPlayer.h"

UDRStartingWeaponUIComponent::UDRStartingWeaponUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRStartingWeaponUIComponent::BeginPlay()
{
	Super::BeginPlay();
	TryInitialize();
}

void UDRStartingWeaponUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	CloseSelection();
	UIManager = nullptr;
	PlayerController = nullptr;
	IsSelectionOpened = false;
	Super::EndPlay(EndPlayReason);
}

void UDRStartingWeaponUIComponent::OpenSelection()
{
	if (IsSelectionOpened || !TryInitialize())
	{
		return;
	}

	SelectionWidget = Cast<UDRStartingWeaponSelectWidget>(
		UIManager->PushScreen(DRGameplayTags::UI_Screen_StartingWeapon));

	if (!IsValid(SelectionWidget))
	{
		return;
	}

	IsSelectionOpened = true;
	SelectionWidget->InitializeSelection(
		PlayerController,
		PlayerController->GetStartingWeaponTable());
}

void UDRStartingWeaponUIComponent::CloseSelection()
{
	if (!IsValid(SelectionWidget))
	{
		return;
	}

	if (IsValid(UIManager))
	{
		UIManager->PopScreen(DRGameplayTags::UI_Screen_StartingWeapon);
	}
	else
	{
		SelectionWidget->RemoveFromParent();
	}

	SelectionWidget = nullptr;
	IsSelectionOpened = false;
}

bool UDRStartingWeaponUIComponent::TryInitialize()
{
	if (!IsValid(PlayerController))
	{
		PlayerController = Cast<ADRPlayerController>(GetOwner());
	}

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return false;
	}

	if (!IsValid(UIManager))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
		}
	}

	return IsValid(UIManager);
}

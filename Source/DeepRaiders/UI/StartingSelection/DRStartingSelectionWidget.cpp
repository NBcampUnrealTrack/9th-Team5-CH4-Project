#include "DRStartingSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "DRStartingSkillSelectWidget.h"
#include "DeepRaiders/Player/Components/DRStartingSelectionComponent.h"
#include "DeepRaiders/UI/StartingWeapon/DRStartingWeaponSelectWidget.h"

void UDRStartingSelectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);

	for (UWidget* Widget : Widgets)
	{
		if (!IsValid(WeaponPanel))
		{
			WeaponPanel = Cast<UDRStartingWeaponSelectWidget>(Widget);
		}

		if (!IsValid(SkillPanel))
		{
			SkillPanel = Cast<UDRStartingSkillSelectWidget>(Widget);
		}
	}
}

void UDRStartingSelectionWidget::InitializeSelection(
	UDRStartingSelectionComponent* InSelectionComponent)
{
	DeinitializeSelection();

	if (!IsValid(InSelectionComponent))
	{
		return;
	}

	SelectionComponent = InSelectionComponent;
	SelectionComponent->OnSelectionAvailabilityChanged.AddUObject(
		this,
		&ThisClass::HandleSelectionAvailabilityChanged);

	if (IsValid(WeaponPanel))
	{
		WeaponPanel->InitializeSelection(SelectionComponent);
	}

	if (IsValid(SkillPanel))
	{
		SkillPanel->InitializeSelection(SelectionComponent);
	}

	HandleSelectionAvailabilityChanged(SelectionComponent->IsSelectionAvailable());
}

void UDRStartingSelectionWidget::NativeDestruct()
{
	DeinitializeSelection();
	Super::NativeDestruct();
}

void UDRStartingSelectionWidget::HandleSelectionAvailabilityChanged(
	bool)
{
	if (!IsValid(SelectionComponent))
	{
		return;
	}

	const bool IsWeaponPending = IsValid(WeaponPanel)
		&& SelectionComponent->IsWeaponSelectionAvailable();
	const bool IsSkillPending = IsValid(SkillPanel)
		&& SelectionComponent->IsSkillSelectionAvailable();
	const bool IsUsingPanelSwitcher = IsValid(SelectionPanelSwitcher)
		&& IsValid(WeaponPanel)
		&& IsValid(SkillPanel)
		&& SelectionPanelSwitcher->HasChild(WeaponPanel)
		&& SelectionPanelSwitcher->HasChild(SkillPanel);

	if (IsUsingPanelSwitcher)
	{
		SelectionPanelSwitcher->SetVisibility(ESlateVisibility::Visible);

		if (IsWeaponPending)
		{
			WeaponPanel->SetVisibility(ESlateVisibility::Visible);
			SelectionPanelSwitcher->SetActiveWidget(WeaponPanel);
		}
		else if (IsSkillPending)
		{
			SkillPanel->SetVisibility(ESlateVisibility::Visible);
			SelectionPanelSwitcher->SetActiveWidget(SkillPanel);
		}
		else
		{
			SelectionPanelSwitcher->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		if (IsValid(WeaponPanel))
		{
			WeaponPanel->SetVisibility(
				IsWeaponPending
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed);
		}

		if (IsValid(SkillPanel))
		{
			const bool IsSkillStepAvailable = !IsValid(WeaponPanel)
				|| !IsWeaponPending;

			SkillPanel->SetVisibility(
				IsSkillStepAvailable && IsSkillPending
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed);
		}
	}

	if (!IsWeaponPending && !IsSkillPending)
	{
		OnSelectionCompleted.Broadcast();
	}
}

void UDRStartingSelectionWidget::DeinitializeSelection()
{
	if (IsValid(SelectionComponent))
	{
		SelectionComponent->OnSelectionAvailabilityChanged.RemoveAll(this);
	}

	if (IsValid(WeaponPanel))
	{
		WeaponPanel->DeinitializeSelection();
	}

	if (IsValid(SkillPanel))
	{
		SkillPanel->DeinitializeSelection();
	}

	SelectionComponent = nullptr;
}

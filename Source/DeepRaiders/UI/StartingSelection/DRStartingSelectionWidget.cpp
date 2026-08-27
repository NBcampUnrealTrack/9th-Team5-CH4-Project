#include "DRStartingSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
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
	bool IsAvailable)
{
	if (!IsAvailable)
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

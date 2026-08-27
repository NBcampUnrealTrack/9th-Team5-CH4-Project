#include "DRStartingWeaponSelectWidget.h"

#include "DRStartingWeaponEntryWidget.h"
#include "DeepRaiders/UI/ViewModel/DRStartingWeaponViewModel.h"
#include "DeepRaiders/Player/Components/DRStartingSelectionComponent.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

UDRStartingWeaponSelectWidget::UDRStartingWeaponSelectWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UDRStartingWeaponSelectWidget::InitializeSelection(
	UDRStartingSelectionComponent* InSelectionComponent)
{
	DeinitializeSelection();
	ViewModel = NewObject<UDRStartingWeaponViewModel>(this);

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!IsValid(View))
	{
		UE_LOG(LogTemp, Error, TEXT("Starting weapon MVVM View is invalid. Widget=%s"), *GetName());
		return;
	}

	if (!View->SetViewModel(ViewModelName, ViewModel))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Starting weapon ViewModel '%s' was not registered on %s"),
			*ViewModelName.ToString(),
			*GetName());
		return;
	}

	ViewModel->Initialize(InSelectionComponent);
}

void UDRStartingWeaponSelectWidget::DeinitializeSelection()
{
	if (IsValid(ViewModel))
	{
		ViewModel->Deinitialize();
	}

	ViewModel = nullptr;
}

void UDRStartingWeaponSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(WeaponListView) || !IsValid(ConfirmButton))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Starting weapon widget requires WeaponListView and ConfirmButton. Widget=%s"),
			*GetName());
		return;
	}

	const TSubclassOf<UUserWidget> EntryWidgetClass = WeaponListView->GetEntryWidgetClass();
	if (!EntryWidgetClass
		|| !EntryWidgetClass->IsChildOf(UDRStartingWeaponEntryWidget::StaticClass()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("WeaponListView EntryWidgetClass must derive from DRStartingWeaponEntryWidget. Widget=%s"),
			*GetName());
	}

	WeaponListView->OnItemClicked().AddUObject(this, &ThisClass::HandleWeaponClicked);
	ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
}

void UDRStartingWeaponSelectWidget::NativeDestruct()
{
	if (IsValid(WeaponListView))
	{
		WeaponListView->OnItemClicked().RemoveAll(this);
	}

	if (IsValid(ConfirmButton))
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}

	DeinitializeSelection();
	Super::NativeDestruct();
}

void UDRStartingWeaponSelectWidget::HandleWeaponClicked(UObject* Item)
{
	if (UDRStartingWeaponEntryViewModel* WeaponEntry = Cast<UDRStartingWeaponEntryViewModel>(Item))
	{
		WeaponEntry->Select();
	}
}

void UDRStartingWeaponSelectWidget::HandleConfirmClicked()
{
	if (IsValid(ViewModel))
	{
		ViewModel->ConfirmSelection();
	}
}

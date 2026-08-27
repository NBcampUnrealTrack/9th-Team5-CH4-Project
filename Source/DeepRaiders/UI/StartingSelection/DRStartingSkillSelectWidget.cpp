#include "DRStartingSkillSelectWidget.h"

#include "Components/Button.h"
#include "Components/ListView.h"
#include "DeepRaiders/UI/ViewModel/DRStartingSkillViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

UDRStartingSkillSelectWidget::UDRStartingSkillSelectWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UDRStartingSkillSelectWidget::InitializeSelection(
	UDRStartingSelectionComponent* InSelectionComponent)
{
	DeinitializeSelection();
	ViewModel = NewObject<UDRStartingSkillViewModel>(this);

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!IsValid(View))
	{
		UE_LOG(LogTemp, Error, TEXT("Starting skill MVVM View is invalid. Widget=%s"), *GetName());
		return;
	}

	if (!View->SetViewModel(ViewModelName, ViewModel))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Starting skill ViewModel '%s' was not registered on %s"),
			*ViewModelName.ToString(),
			*GetName());
		return;
	}

	ViewModel->Initialize(InSelectionComponent);
}

void UDRStartingSkillSelectWidget::DeinitializeSelection()
{
	if (IsValid(ViewModel))
	{
		ViewModel->Deinitialize();
	}

	ViewModel = nullptr;
}

void UDRStartingSkillSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SkillListView->OnItemClicked().AddUObject(this, &ThisClass::HandleSkillClicked);
	ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
}

void UDRStartingSkillSelectWidget::NativeDestruct()
{
	SkillListView->OnItemClicked().RemoveAll(this);
	ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	DeinitializeSelection();
	Super::NativeDestruct();
}

void UDRStartingSkillSelectWidget::HandleSkillClicked(UObject* Item)
{
	if (UDRStartingSkillEntryViewModel* SkillEntry = Cast<UDRStartingSkillEntryViewModel>(Item))
	{
		SkillEntry->Select();
	}
}

void UDRStartingSkillSelectWidget::HandleConfirmClicked()
{
	if (IsValid(ViewModel))
	{
		ViewModel->ConfirmSelection();
	}
}

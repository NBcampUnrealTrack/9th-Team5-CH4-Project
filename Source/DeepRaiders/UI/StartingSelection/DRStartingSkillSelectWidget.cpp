#include "DRStartingSkillSelectWidget.h"

#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
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
	ViewModel->OnSelectionGuideTextChanged.AddUObject(
		this,
		&ThisClass::HandleSelectionGuideTextChanged);

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
		ViewModel->OnSelectionGuideTextChanged.RemoveAll(this);
		ViewModel->Deinitialize();
	}

	ViewModel = nullptr;
}

void UDRStartingSkillSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(SkillListView) || !IsValid(ConfirmButton))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Starting skill widget requires SkillListView and ConfirmButton. Widget=%s"),
			*GetName());
		return;
	}

	SkillListView->OnItemClicked().AddUObject(this, &ThisClass::HandleSkillClicked);
	ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
}

void UDRStartingSkillSelectWidget::NativeDestruct()
{
	if (IsValid(SkillListView))
	{
		SkillListView->OnItemClicked().RemoveAll(this);
	}

	if (IsValid(ConfirmButton))
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}

	DeinitializeSelection();
	Super::NativeDestruct();
}

void UDRStartingSkillSelectWidget::HandleSelectionGuideTextChanged(
	const FText& SelectionGuideText)
{
	if (IsValid(SelectionText))
	{
		SelectionText->SetText(SelectionGuideText);
	}
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

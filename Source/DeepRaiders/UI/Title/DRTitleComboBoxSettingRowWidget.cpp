#include "DRTitleComboBoxSettingRowWidget.h"

#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"

void UDRTitleComboBoxSettingRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(ListName))
	{
		ListName->SetText(InName);
	}

}

void UDRTitleComboBoxSettingRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(ComboBox))
	{
		return;
	}

	ComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleSelectionChanged);
	ComboBox->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleSelectionChanged);
	SetOptions(Options);
}

void UDRTitleComboBoxSettingRowWidget::SetOptions(const TArray<FString>& NewOptions)
{
	Options = NewOptions;
	if (!IsValid(ComboBox))
	{
		return;
	}

	ComboBox->ClearOptions();
	for (const FString& Option : Options)
	{
		ComboBox->AddOption(Option);
	}

	SetSelectedIndex(SelectedIndex);
}

void UDRTitleComboBoxSettingRowWidget::SetSelectedIndex(int32 NewIndex)
{
	SelectedIndex = Options.IsValidIndex(NewIndex) ? NewIndex : INDEX_NONE;
	if (IsValid(ComboBox) && SelectedIndex != INDEX_NONE)
	{
		ComboBox->SetSelectedIndex(SelectedIndex);
	}
}

int32 UDRTitleComboBoxSettingRowWidget::GetSelectedIndex() const
{
	return IsValid(ComboBox) ? ComboBox->GetSelectedIndex() : SelectedIndex;
}

void UDRTitleComboBoxSettingRowWidget::HandleSelectionChanged(
	FString SelectedItem,
	ESelectInfo::Type)
{
	SelectedIndex = Options.IndexOfByKey(SelectedItem);
}

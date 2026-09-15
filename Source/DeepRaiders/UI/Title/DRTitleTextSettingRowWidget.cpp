#include "DRTitleTextSettingRowWidget.h"

#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UDRTitleTextSettingRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(ListName))
	{
		ListName->SetText(InName);
	}

	SetSettingText(InText);
}

void UDRTitleTextSettingRowWidget::SetSettingText(const FText& NewText)
{
	InText = NewText;

	if (IsValid(EditValue))
	{
		EditValue->SetText(InText);
	}
}

FText UDRTitleTextSettingRowWidget::GetSettingText() const
{
	return IsValid(EditValue) ? EditValue->GetText() : InText;
}

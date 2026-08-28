#include "DRTitleSettingRowWidget.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"

void UDRTitleSettingRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(ListName))
	{
		ListName->SetText(InName);
	}

	SetSettingValue(InSliderValue);
}

void UDRTitleSettingRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ListSlider))
	{
		ListSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSliderValueChanged);
		ListSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderValueChanged);
	}
}

void UDRTitleSettingRowWidget::SetSettingValue(float NewValue)
{
	InSliderValue = FMath::Clamp(NewValue, MinValue, MaxValue);
	if (IsValid(ListSlider))
	{
		ListSlider->SetValue(FMath::GetMappedRangeValueClamped(
			FVector2D(MinValue, MaxValue),
			FVector2D(0.f, 1.f),
			InSliderValue));
	}

	RefreshValueText(InSliderValue);
}

float UDRTitleSettingRowWidget::GetSettingValue() const
{
	return IsValid(ListSlider)
		? FMath::Lerp(MinValue, MaxValue, ListSlider->GetValue())
		: InSliderValue;
}

void UDRTitleSettingRowWidget::HandleSliderValueChanged(float NormalizedValue)
{
	InSliderValue = FMath::Lerp(MinValue, MaxValue, NormalizedValue);
	RefreshValueText(InSliderValue);
}

void UDRTitleSettingRowWidget::RefreshValueText(float Value)
{
	if (!IsValid(ListVolumn))
	{
		return;
	}

	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = FractionalDigits;
	Format.MaximumFractionalDigits = FractionalDigits;
	ListVolumn->SetText(FText::AsNumber(Value, &Format));
}

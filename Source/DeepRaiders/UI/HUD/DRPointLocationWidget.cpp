#include "DRPointLocationWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UDRPointLocationWidget::SetIndicatorColor(const FLinearColor& TeamColor)
{
	ApplyColorPreservingAlpha(PointIndicator_Back, TeamColor);
	ApplyColorPreservingAlpha(PointIndicator_Front, TeamColor);
}

void UDRPointLocationWidget::SetDisplayName(const FText& DisplayName)
{
	if (IsValid(PointIndicator_Text))
	{
		PointIndicator_Text->SetText(DisplayName);
	}
}

void UDRPointLocationWidget::ApplyColorPreservingAlpha(UImage* Image, const FLinearColor& TeamColor)
{
	if (!IsValid(Image))
	{
		return;
	}

	FLinearColor IndicatorColor = TeamColor;
	IndicatorColor.A = Image->GetColorAndOpacity().A;
	Image->SetColorAndOpacity(IndicatorColor);
}

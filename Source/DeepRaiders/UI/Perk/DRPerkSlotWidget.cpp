#include "DRPerkSlotWidget.h"

#include "Components/Image.h"
#include "DeepRaiders/Perk/DRPerkDefinition.h"

void UDRPerkSlotWidget::SetPerkDefinition(
	const UDRPerkDefinition* PerkDefinition)
{
	if (!IsValid(PerkIcon))
	{
		return;
	}

	UTexture2D* Icon = IsValid(PerkDefinition)
		? PerkDefinition->Icon
		: nullptr;

	PerkIcon->SetBrushFromTexture(Icon);
	PerkIcon->SetVisibility(
		IsValid(Icon)
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
}

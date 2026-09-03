#include "DRPlayerNameplateViewModel.h"

void UDRPlayerNameplateViewModel::SetDisplayData(const FText& InDisplayName, const FLinearColor& InNameColor)
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(NameColor, FSlateColor(InNameColor));
}

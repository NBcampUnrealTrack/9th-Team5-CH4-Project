#include "DRPerkResetTestWidget.h"

#include "Components/Button.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"

void UDRPerkResetTestWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.AddDynamic(
			this,
			&ThisClass::HandleResetButtonClicked);
	}
}

void UDRPerkResetTestWidget::NativeDestruct()
{
	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleResetButtonClicked);
	}

	Super::NativeDestruct();
}

void UDRPerkResetTestWidget::HandleResetButtonClicked()
{
	ADRPlayerState* PlayerState = GetOwningPlayerState<ADRPlayerState>();
	UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;

	if (IsValid(PerkComponent))
	{
		PerkComponent->RequestResetPerks();
	}
}

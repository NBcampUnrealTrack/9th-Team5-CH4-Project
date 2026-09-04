#include "DRHUDViewModel.h"

#include "DeepRaiders/Core/Interaction/DRInteractionTypes.h"
#include "DeepRaiders/Player/Components/DRInteractionComponent.h"

void UDRHUDViewModel::HandleFocusedInteractableChanged(
	AActor*,
	const FDRInteractionPromptData&)
{
	RefreshInteractionPrompt();
}

void UDRHUDViewModel::RefreshInteractionPrompt()
{
	AActor* FocusedTarget = InteractionComponent.IsValid()
		? InteractionComponent->GetFocusedTarget()
		: nullptr;

	const bool bNewVisible = IsValid(FocusedTarget);

	const FDRInteractionPromptData PromptData = bNewVisible ?
		InteractionComponent->GetFocusedPromptData() : FDRInteractionPromptData();

	UE_MVVM_SET_PROPERTY_VALUE(bIsInteractionPromptVisible, bNewVisible);
	UE_MVVM_SET_PROPERTY_VALUE(InteractionActionText, PromptData.ActionText);
	UE_MVVM_SET_PROPERTY_VALUE(InteractionTitleText, PromptData.TitleText);
	UE_MVVM_SET_PROPERTY_VALUE(InteractionDetailText, PromptData.DetailText);
}

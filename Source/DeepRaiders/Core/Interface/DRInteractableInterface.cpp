
#include "DRInteractableInterface.h"

bool IDRInteractableInterface::GetInteractionPromptData_Implementation(APawn* Interactor,
	FDRInteractionPromptData& OutPromptData) const
{
	OutPromptData.ActionText = NSLOCTEXT("DRInteraction", "DefaultInteractionAction", "상호작용");
	
	return true;
}


bool IDRInteractableInterface::GetInteractionLocation_Implementation(
	APawn* Interactor,
	FVector& OutInteractionLocation) const
{
	OutInteractionLocation = FVector::ZeroVector;
	return false;
}

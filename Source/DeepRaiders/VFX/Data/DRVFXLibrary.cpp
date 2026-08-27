
#include "DRVFXLibrary.h"

const FDRVFXDefinition* UDRVFXLibrary::FindDefinition(const FGameplayTag& VFXTag) const
{
	return Definitions.Find(VFXTag);
}

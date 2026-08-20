#include "DRSoundLibrary.h"

const FDRSoundDefinition* UDRSoundLibrary::FindDefinition(const FGameplayTag& SoundTag) const
{
	return Definitions.Find(SoundTag);
}

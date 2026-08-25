#include "DRWorldItemPresentationProfile.h"

const FDRWorldItemRarityVisual& UDRWorldItemPresentationProfile::GetRarityVisual(EDRItemRarity Rarity) const
{
	if (const FDRWorldItemRarityVisual* Visual = RarityVisuals.Find(Rarity))
	{
		return *Visual;
	}
	
	return DefaultRarityVisual;
}

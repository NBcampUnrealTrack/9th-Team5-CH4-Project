#include "DRWorldItemPresentationProfile.h"

#include "NiagaraComponent.h"

const FDRWorldItemRarityVisual& UDRWorldItemPresentationProfile::GetRarityVisual(EDRItemRarity Rarity) const
{
	if (const FDRWorldItemRarityVisual* Visual = RarityVisuals.Find(Rarity))
	{
		return *Visual;
	}
	
	return DefaultRarityVisual;
}

void UDRWorldItemPresentationProfile::ApplyRarityParameters(UNiagaraComponent* NiagaraComponent,
	EDRItemRarity Rarity) const
{
	if (!IsValid(NiagaraComponent))
	{
		return;
	}
	
	const FDRWorldItemRarityVisual& RarityVisual = GetRarityVisual(Rarity);
	

	NiagaraComponent->SetVariableLinearColor(TEXT("User.RarityColor"),
		RarityVisual.RarityColor);
	NiagaraComponent->SetVariableLinearColor(TEXT("User.IntensityColor"),
	RarityVisual.RarityColor * RarityVisual.Intensity);
}

#include "DRSnowProjectile.h"

#include "DeepRaiders/Snow/Components/DRSnowAddComponent.h"

ADRSnowProjectile::ADRSnowProjectile()
{
	SnowAddComponent = CreateDefaultSubobject<UDRSnowAddComponent>(TEXT("SnowAddComponent"));
}

void ADRSnowProjectile::HandleWorldImpact(const FHitResult& ImpactResult)
{
	if (!HasAuthority()
		|| !IsValid(SnowAddComponent))
	{
		return;
	}

	const FDRProjectileWorldImpactData& ImpactData = GetWorldImpactData();
	if (!ImpactData.bAddSnow)
	{
		return;
	}

	SnowAddComponent->SetTeamIdOverride(GetSourceTeamId());
	SnowAddComponent->SetAddSettings(ImpactData.SnowRadius, ImpactData.SnowAmount);
	SnowAddComponent->SetAddEditTool(ImpactData.SnowEditTool);
	SnowAddComponent->TryAddSnowFromHit(ImpactResult);
}

#include "DRSnowProjectile.h"

#include "DeepRaiders/Snow/Components/DRSnowAddComponent.h"

ADRSnowProjectile::ADRSnowProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SnowAddComponent = CreateDefaultSubobject<UDRSnowAddComponent>(TEXT("SnowAddComponent"));
}

float ADRSnowProjectile::GetConfiguredInitialSpeed() const
{
	return FMath::Max(InitialSpeed, 1.0f);
}

void ADRSnowProjectile::HandleWorldImpact(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || !IsValid(SnowAddComponent))
	{
		return;
	}

	const FDRProjectileWorldImpactData& ImpactData = GetWorldImpactData();

	if (!ImpactData.bAddSnow)
	{
		return;
	}

	SnowAddComponent->SetTeamIdOverride(GetSourceTeamId());

	SnowAddComponent->SetAddSettings(ImpactData.SnowRadius * GetCurrentFalloffStrength(), ImpactData.SnowAmount * GetCurrentFalloffStrength());

	SnowAddComponent->SetAddEditTool(ImpactData.SnowEditTool);

	SnowAddComponent->SetAllowVirtualSurfaceFallback(ImpactData.bAllowVirtualSurfaceFallback);

	SnowAddComponent->TryAddSnowFromHit(ImpactResult);
}

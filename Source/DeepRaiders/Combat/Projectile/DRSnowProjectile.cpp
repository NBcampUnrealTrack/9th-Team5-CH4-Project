#include "DRSnowProjectile.h"

#include "DeepRaiders/Snow/Components/DRSnowAddComponent.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogDRSnowProjectile, Log, All);
}

ADRSnowProjectile::ADRSnowProjectile(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
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
		UE_LOG(LogDRSnowProjectile, Warning, TEXT("World impact ignored: bAddSnow=0 Actor=%s Component=%s Point=%s"),
			*GetNameSafe(ImpactResult.GetActor()), *GetNameSafe(ImpactResult.GetComponent()),
			*ImpactResult.ImpactPoint.ToString());
		return;
	}

	UE_LOG(LogDRSnowProjectile, Log, TEXT("World impact: Actor=%s Component=%s Point=%s Normal=%s Blocking=%d"),
		*GetNameSafe(ImpactResult.GetActor()), *GetNameSafe(ImpactResult.GetComponent()),
		*ImpactResult.ImpactPoint.ToString(), *ImpactResult.ImpactNormal.ToString(), ImpactResult.bBlockingHit);

	SnowAddComponent->SetTeamIdOverride(GetSourceTeamId());
	SnowAddComponent->SetAddSettings(ImpactData.SnowRadius, ImpactData.SnowAmount);
	SnowAddComponent->SetAddEditTool(ImpactData.SnowEditTool);
	SnowAddComponent->SetAllowVirtualSurfaceFallback(ImpactData.bAllowVirtualSurfaceFallback);
	SnowAddComponent->TryAddSnowFromHit(ImpactResult);
}

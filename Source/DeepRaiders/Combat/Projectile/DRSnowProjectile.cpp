#include "DRSnowProjectile.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
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

void ADRSnowProjectile::ExecuteImpactGameplayCue(const FHitResult& ImpactResult)
{
	Super::ExecuteImpactGameplayCue(ImpactResult);

	UAbilitySystemComponent* SourceASC = GetSourceAbilitySystem();
	if (!HasAuthority() || !IsValid(SourceASC))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();

	FHitResult SanitizedHit = ImpactResult;
	SanitizedHit.Component = nullptr;
	EffectContext.AddHitResult(SanitizedHit, true);

	FGameplayCueParameters Parameters(EffectContext);
	Parameters.Location = ImpactResult.Location;
	Parameters.Normal = ImpactResult.ImpactNormal;
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = this;
	Parameters.SourceObject = GetPresentationSourceObject();

	SourceASC->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Sound_Projectile_Snow_Impact, Parameters);
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

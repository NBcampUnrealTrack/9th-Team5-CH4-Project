#include "DRSpearProjectile.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

void ADRSpearProjectile::InitializeSpearProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
	int32 InSourceTeamId,
	float InKnockbackStrength)
{
	KnockbackStrength = FMath::Max(InKnockbackStrength, 0.f);

	InitializeProjectile(
		InSourceAbilitySystem,
		InImpactEffectSpecs,
		0.f,
		FDRProjectileWorldImpactData(),
		InSourceTeamId,
		nullptr);
}

void ADRSpearProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(ImpactResult.GetActor());
	if (HasAuthority()
		&& IsValid(TargetCharacter)
		&& !IsFriendlyTarget(TargetCharacter)
		&& KnockbackStrength > 0.f)
	{
		const FVector KnockbackDirection = GetActorForwardVector().GetSafeNormal2D();
		TargetCharacter->LaunchCharacter(
			KnockbackDirection * KnockbackStrength,
			true,
			false);
	}

	Super::HandleImpact(ImpactResult);
}

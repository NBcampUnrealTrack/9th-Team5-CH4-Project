#include "DRSpearProjectile.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

void ADRSpearProjectile::InitializeSpearProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
	int32 InSourceTeamId,
	float InKnockbackStrength,
	float InInitialSpeed,
	float InMaximumRange)
{
	KnockbackStrength = FMath::Max(InKnockbackStrength, 0.f);
	ConfigureProjectileMovement(InInitialSpeed, 0.f);

	InitializeProjectile(
		InSourceAbilitySystem,
		InImpactEffectSpecs,
		0.f,
		FDRProjectileWorldImpactData(),
		InSourceTeamId,
		nullptr,
		InMaximumRange);
}

void ADRSpearProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	ApplyKnockback(ImpactResult);
	Super::HandleImpact(ImpactResult);
}

void ADRSpearProjectile::ApplyKnockback(const FHitResult& ImpactResult) const
{
	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(ImpactResult.GetActor());
	if (!HasAuthority()
		|| !IsValid(TargetCharacter)
		|| IsFriendlyTarget(TargetCharacter)
		|| KnockbackStrength <= 0.f)
	{
		return;
	}

	const FVector KnockbackDirection = GetActorForwardVector().GetSafeNormal2D();
	TargetCharacter->LaunchCharacter(
		KnockbackDirection * KnockbackStrength,
		true,
		false);
}

#include "DRSlowProjectile.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

void ADRSlowProjectile::InitializeSlowProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	const FGameplayEffectSpecHandle& InSlowEffectSpec,
	const FGameplayEffectSpecHandle& InAllySpeedEffectSpec,
	const FDRThrowableItemSettings& InItemSettings,
	const FDRThrowActionSettings& InActionSettings,
	int32 InSourceTeamId,
	const UObject* InPresentationSourceObject)
{
	if (!HasAuthority() || !InSlowEffectSpec.IsValid())
	{
		return;
	}

	SlowEffectSpec = InSlowEffectSpec;
	AllySpeedEffectSpec = InAllySpeedEffectSpec;
	InitializeThrowable(
		InSourceAbilitySystem,
		{},
		InItemSettings,
		InActionSettings,
		InSourceTeamId,
		InPresentationSourceObject);
}

bool ADRSlowProjectile::IsValidEffectTarget(const AActor* TargetActor) const
{
	return IsValid(TargetActor)
		&& (!IsFriendlyTarget(TargetActor) || AllySpeedEffectSpec.IsValid());
}

void ADRSlowProjectile::ApplyEffectToTarget(
	AActor* TargetActor,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FHitResult& ImpactResult)
{
	if (!IsValid(TargetAbilitySystem)
		|| TargetAbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Dead))
	{
		return;
	}

	if (!IsFriendlyTarget(TargetActor))
	{
		if (ApplySpeedEffectToTarget(
			TargetAbilitySystem,
			SlowEffectSpec,
			ImpactResult))
		{
			ExecutePlayerHitGameplayCue(TargetAbilitySystem, ImpactResult);
		}
		return;
	}

	ApplySpeedEffectToTarget(
		TargetAbilitySystem,
		AllySpeedEffectSpec,
		ImpactResult);
}

bool ADRSlowProjectile::ApplySpeedEffectToTarget(
	UAbilitySystemComponent* TargetAbilitySystem,
	const FGameplayEffectSpecHandle& EffectSpec,
	const FHitResult& ImpactResult)
{
	UAbilitySystemComponent* SourceAbilitySystemComponent = GetSourceAbilitySystem();
	if (!IsValid(SourceAbilitySystemComponent)
		|| !IsValid(TargetAbilitySystem)
		|| !EffectSpec.IsValid())
	{
		return false;
	}

	FGameplayEffectSpec AppliedSpec(*EffectSpec.Data.Get());
	AppliedSpec.GetContext().AddHitResult(ImpactResult, true);
	return SourceAbilitySystemComponent
		->ApplyGameplayEffectSpecToTarget(AppliedSpec, TargetAbilitySystem)
		.WasSuccessfullyApplied();
}

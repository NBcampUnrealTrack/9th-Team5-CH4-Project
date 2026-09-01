#include "DRSlowProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

void ADRSlowProjectile::InitializeSlowProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	const FGameplayEffectSpecHandle& InSlowEffectSpec,
	int32 InSourceTeamId,
	float InEffectRadius)
{
	if (!HasAuthority() || !InSlowEffectSpec.IsValid())
	{
		return;
	}

	EffectRadius = FMath::Max(InEffectRadius, 1.f);

	TArray<FGameplayEffectSpecHandle> SlowEffectSpecs;
	SlowEffectSpecs.Add(InSlowEffectSpec);

	InitializeProjectile(
		InSourceAbilitySystem,
		SlowEffectSpecs,
		0.f,
		FDRProjectileWorldImpactData(),
		InSourceTeamId,
		nullptr);
}

void ADRSlowProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		Destroy();
		return;
	}

	const FVector EffectLocation = ImpactResult.ImpactPoint;

#if ENABLE_DRAW_DEBUG
	MulticastDisplayEffectRadius(EffectLocation, EffectRadius);
#endif

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRSlowProjectile), false, this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(GetInstigator());

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		EffectLocation,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(EffectRadius),
		QueryParams);

	TSet<AActor*> AppliedActors;
	FHitResult EffectHit = ImpactResult;
	EffectHit.Location = EffectLocation;
	EffectHit.ImpactPoint = EffectLocation;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!IsValid(TargetActor)
			|| AppliedActors.Contains(TargetActor)
			|| IsFriendlyTarget(TargetActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetAbilitySystem =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!IsValid(TargetAbilitySystem))
		{
			continue;
		}

		AppliedActors.Add(TargetActor);
		ApplyImpactEffect(TargetAbilitySystem, EffectHit);
	}

	Destroy();
}

void ADRSlowProjectile::MulticastDisplayEffectRadius_Implementation(
	const FVector& EffectLocation,
	float InEffectRadius)
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	DrawDebugSphere(
		World,
		EffectLocation,
		InEffectRadius,
		32,
		FColor::Cyan,
		false,
		EffectRadiusDisplayDuration,
		0,
		2.f);
#endif
}

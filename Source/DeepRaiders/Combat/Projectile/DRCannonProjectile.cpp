#include "DRCannonProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADRCannonProjectile::ADRCannonProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBoxComponent>(CollisionComponentName))
{
	UBoxComponent* BoxCollision = CastChecked<UBoxComponent>(CollisionComponent);

	BoxCollision->InitBoxExtent(FVector(40.f, 12.f, 12.f));
}

void ADRCannonProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		Destroy();
		return;
	}

	const FVector ExplosionLocation = ImpactResult.ImpactPoint;

	TArray<FOverlapResult> OverlapResults;

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRCannonExplosion), false);

	QueryParams.AddIgnoredActor(this);

	if (IsValid(GetOwner()))
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	if (IsValid(GetInstigator()))
	{
		QueryParams.AddIgnoredActor(GetInstigator());
	}

	World->OverlapMultiByObjectType(
		OverlapResults, ExplosionLocation, FQuat::Identity, ObjectQuery, FCollisionShape::MakeSphere(ExplosionRadius), QueryParams);

	TSet<AActor*> ProcessedActors;

	for (const FOverlapResult& Overlap : OverlapResults)
	{
		AActor* TargetActor = Overlap.GetActor();

		if (!IsValid(TargetActor) || ProcessedActors.Contains(TargetActor) || IsFriendlyTarget(TargetActor))
		{
			continue;
		}

		ProcessedActors.Add(TargetActor);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

		if (!IsValid(TargetASC))
		{
			continue;
		}

		FHitResult ExplosionHit = ImpactResult;
		ExplosionHit.Location = ExplosionLocation;
		ExplosionHit.ImpactPoint = ExplosionLocation;

		ApplyImpactEffect(TargetASC, ExplosionHit);
	}

	// 폭발 VFX / Sound Cue
	ExecuteImpactGameplayCue(ImpactResult);

	// DRSnowProjectile의 눈 생성
	HandleWorldImpact(ImpactResult);

	Destroy();
}

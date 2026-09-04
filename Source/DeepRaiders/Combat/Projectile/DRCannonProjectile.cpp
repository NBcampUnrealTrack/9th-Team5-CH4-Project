#include "DRCannonProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "DeepRaiders/Snow/Components/DRSnowAddComponent.h"
#include "DrawDebugHelpers.h"

ADRCannonProjectile::ADRCannonProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBoxComponent>(CollisionComponentName))
{
	UBoxComponent* BoxCollision = CastChecked<UBoxComponent>(CollisionComponent);

	BoxCollision->InitBoxExtent(FVector(40.f, 12.f, 12.f));
	
	SnowAddComponent = CreateDefaultSubobject<UDRSnowAddComponent>(TEXT("SnowAddComponent"));
}

float ADRCannonProjectile::GetConfiguredInitialSpeed() const
{
	return FMath::Max(InitialSpeed, 1.0f);
}

float ADRCannonProjectile::GetConfiguredGravityScale() const
{
	return FMath::Max(GravityScale, 0.0f);
}

void ADRCannonProjectile::BeginPlay()
{
	ConfigureProjectileMovement(InitialSpeed, GravityScale);

	Super::BeginPlay();
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
	
	const FDRProjectileWorldImpactData& ImpactData = GetWorldImpactData();
	if (ImpactData.bAddSnow && IsValid(SnowAddComponent))
	{
		SnowAddComponent->SetTeamIdOverride(GetSourceTeamId());
		SnowAddComponent->SetAddSettings(ImpactData.SnowRadius, ImpactData.SnowAmount);
		SnowAddComponent->SetAddEditTool(ImpactData.SnowEditTool);
		SnowAddComponent->SetAllowVirtualSurfaceFallback(ImpactData.bAllowVirtualSurfaceFallback);

		SnowAddComponent->TryAddSnowFromHit(ImpactResult);
	}
	
	// 직접 충돌한 Breakable에는 기존 Projectile과 동일하게 충돌 데미지를 적용한다.
	// 폭발 반경 내 Breakable AoE 처리는 별도 정책으로 둔다.
	ApplyBreakableDamage(ImpactResult);
	
	const FVector ExplosionLocation = ImpactResult.ImpactPoint;

	TArray<FOverlapResult> OverlapResults;

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRCannonExplosion), false);

	QueryParams.AddIgnoredActor(this);

	World->OverlapMultiByObjectType(
		OverlapResults, ExplosionLocation, FQuat::Identity, ObjectQuery, FCollisionShape::MakeSphere(ExplosionRadius), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawExplosionDebug)
	{
		DrawDebugSphere(
			World,
			ExplosionLocation,
			ExplosionRadius,
			24,
			FColor::Orange,
			false,
			2.f,
			0,
			2.f);
	}
#endif
	
	TSet<AActor*> UniqueActors;
	TArray<AActor*> CandidateActors;

	// 폭발 범위 안의 Pawn을 Actor 단위로 중복 제거해서 수집한다.
	for (const FOverlapResult& Overlap : OverlapResults)
	{
		AActor* TargetActor = Overlap.GetActor();

		if (!IsValid(TargetActor)
			|| UniqueActors.Contains(TargetActor))
		{
			continue;
		}

		UniqueActors.Add(TargetActor);
		CandidateActors.Add(TargetActor);
	}

	/*
	 * 폭발 위치와 대상 사이의 World Geometry 차폐 검사.
	 *
	 * Pawn끼리는 폭발을 가리지 않도록 폭발 범위 내 후보 Pawn은 Trace에서 제외한다.
	 * 발사자 자신도 차폐 대상이 아니라 데미지 대상이므로 Trace 충돌에서는 제외한다.
	 */
	FCollisionQueryParams OcclusionQuery(
		SCENE_QUERY_STAT(DRCannonExplosionOcclusion),
		false);

	OcclusionQuery.AddIgnoredActor(this);
	OcclusionQuery.AddIgnoredActors(CandidateActors);

	for (AActor* TargetActor : CandidateActors)
	{
		if (!IsValid(TargetActor))
		{
			continue;
		}

		const bool bIsSelf =
			TargetActor == GetInstigator()
			|| TargetActor == GetOwner();

		// 본인은 Friendly여도 자해 허용.
		if (!bIsSelf && IsFriendlyTarget(TargetActor))
		{
			continue;
		}

		FHitResult OcclusionHit;

		const FVector TraceStart =
			ExplosionLocation
			+ ImpactResult.ImpactNormal * 2.f;

		const FVector TraceEnd =
			TargetActor->GetActorLocation();

		const bool bOccluded =
			World->LineTraceSingleByChannel(
				OcclusionHit,
				TraceStart,
				TraceEnd,
				OcclusionTraceChannel,
				OcclusionQuery);

#if ENABLE_DRAW_DEBUG
		if (bDrawExplosionDebug)
		{
			DrawDebugLine(
				World,
				TraceStart,
				TraceEnd,
				bOccluded ? FColor::Red : FColor::Green,
				false,
				2.f,
				0,
				1.5f);
		}
#endif

		if (bOccluded)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::
			GetAbilitySystemComponent(TargetActor);

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

	Destroy();
}


#include "DRThrowableProjectile.h"

#include "DeepRaiders/Item/DRThrowableItemDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/Snow/Components/DRSnowAddComponent.h"
#include "DeepRaiders/Snow/Components/DRSnowRemoveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

ADRThrowableProjectile::ADRThrowableProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SnowAddComponent = CreateDefaultSubobject<UDRSnowAddComponent>(TEXT("SnowAddComponent"));
	SnowRemoveComponent = CreateDefaultSubobject<UDRSnowRemoveComponent>(TEXT("SnowRemoveComponent"));
}

void ADRThrowableProjectile::InitializeThrowable(
	UAbilitySystemComponent* InSourceAbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
	const FDRThrowableItemSettings& InItemSettings,
	const FDRThrowActionSettings& InActionSettings,
	int32 InSourceTeamId,
	const UObject* InPresentationSourceObject)
{
	ItemSettings = InItemSettings;
	OcclusionTraceChannel = InActionSettings.ExplosionOcclusionTraceChannel;

	const UDRThrowableItemDefinition* ThrowableItemDefinition = Cast<UDRThrowableItemDefinition>(InPresentationSourceObject);
	if (IsValid(ThrowableItemDefinition))
	{
		ImpactGameplayCueTag = ThrowableItemDefinition->ImpactGameplayCueTag;
	}

	ConfigureProjectileMovement(InItemSettings.InitialSpeed, InItemSettings.GravityScale);

	InitializeProjectile(
		InSourceAbilitySystem,
		InImpactEffectSpecs,
		0.f,
		ItemSettings.WorldImpactData,
		InSourceTeamId,
		InPresentationSourceObject);
}

void ADRThrowableProjectile::HandleImpact(const FHitResult& ImpactResult)
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
	
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	
	FCollisionQueryParams OverlapQuery(SCENE_QUERY_STAT(DRThrowableProjectile), false);
	OverlapQuery.AddIgnoredActor(this);
	// Owner와 Instigator 대상 제외
	OverlapQuery.AddIgnoredActor(GetOwner());
	OverlapQuery.AddIgnoredActor(GetInstigator());
	
	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(OverlapResults, ExplosionLocation, FQuat::Identity, ObjectQuery,
		FCollisionShape::MakeSphere(ItemSettings.ExplosionRadius), OverlapQuery);
	
	TSet<AActor*> UniqueActors;
	TArray<AActor*> CandidateActors;
	
	for (const FOverlapResult& Overlap : OverlapResults)
	{
		AActor* TargetActor = Overlap.GetActor();
		
		if (IsValid(TargetActor)
			&& !UniqueActors.Contains(TargetActor))
		{
			UniqueActors.Add(TargetActor);
			CandidateActors.Add(TargetActor);
		}
	}
	
	FCollisionQueryParams OcclusionQuery(SCENE_QUERY_STAT(DRThrowableOcclusion), false);
	OcclusionQuery.AddIgnoredActor(this);
	// Owner와 Instigator 대상 제외
	OcclusionQuery.AddIgnoredActor(GetOwner());
	OcclusionQuery.AddIgnoredActor(GetInstigator());
	// Pawn에 의해서는 가려지지 않는다.
	OcclusionQuery.AddIgnoredActors(CandidateActors);
	
	for (AActor* TargetActor : CandidateActors)
	{
		if (!IsValid(TargetActor)
			|| IsFriendlyTarget(TargetActor))
		{
			continue;
		}
		
		FHitResult OcclusionHit;
		
		// 벽으로 가려지진 않았는지 차폐여부 검사.
		// 눈에 의해 쉽게 가려질 것 같지만 일단 LineTrace로 차폐
		const bool bOccluded = World->LineTraceSingleByChannel(OcclusionHit,
			ExplosionLocation + ImpactResult.ImpactNormal * 2.f, TargetActor->GetActorLocation(),
			OcclusionTraceChannel, OcclusionQuery);
	
		if (bOccluded)
		{
			continue;
		}
		
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
	
	ExecuteImpactGameplayCue(ImpactResult);
	HandleWorldImpact(ImpactResult);
	Destroy();
}

void ADRThrowableProjectile::HandleWorldImpact(const FHitResult& ImpactResult)
{
	if (!HasAuthority())
	{
		return;
	}

	const FDRProjectileWorldImpactData& ImpactData = GetWorldImpactData();
	if (ImpactData.bAddSnow
		&& IsValid(SnowAddComponent))
	{
		SnowAddComponent->SetTeamIdOverride(GetSourceTeamId());
		SnowAddComponent->SetAddSettings(ImpactData.SnowRadius, ImpactData.SnowAmount);
		SnowAddComponent->SetAddEditTool(ImpactData.SnowEditTool);
		SnowAddComponent->SetAllowVirtualSurfaceFallback(ImpactData.bAllowVirtualSurfaceFallback);
		SnowAddComponent->TryAddSnowFromHit(ImpactResult);
	}
	else if (!ImpactData.bAddSnow
		&& IsValid(SnowRemoveComponent))
	{
		FDRSnowRemovalSpec RemovalSpec;
		RemovalSpec.SnowAbsorbRadius = ImpactData.SnowRadius;
		RemovalSpec.SnowAbsorbPower = ImpactData.SnowAmount;
		RemovalSpec.RemovalMode = EDRSnowRemovalMode::ContactBrush;
		
		SnowRemoveComponent->SetTeamIdOverride(GetSourceTeamId());
		SnowRemoveComponent->TryRemoveSnowFromHit(ImpactResult, RemovalSpec);
	}
}

void ADRThrowableProjectile::ExecuteImpactGameplayCue(const FHitResult& ImpactResult)
{
	UAbilitySystemComponent* SourceASC = GetSourceAbilitySystem();

	if (!HasAuthority() || !IsValid(SourceASC) || !ImpactGameplayCueTag.IsValid())
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();

	EffectContext.AddHitResult(ImpactResult, true);

	FGameplayCueParameters Parameters(EffectContext);
	Parameters.Location = ImpactResult.Location;
	Parameters.Normal = ImpactResult.ImpactNormal;
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = this;
	Parameters.SourceObject = GetPresentationSourceObject();

	SourceASC->ExecuteGameplayCue(ImpactGameplayCueTag, Parameters);
}

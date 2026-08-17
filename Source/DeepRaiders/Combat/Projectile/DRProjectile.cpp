
#include "DRProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "DeepRaiders/Player/DRPlayerState.h"

ADRProjectile::ADRProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates=  true;
	SetReplicateMovement(true);
	InitialLifeSpan = 5.0f;
	
	CollisionComponent=  CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	
	CollisionComponent->InitSphereRadius(12.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("DRProjectile"));
	CollisionComponent->SetCanEverAffectNavigation(false);
	
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void ADRProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (IsValid(GetOwner()))
	{
		CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
	}
	
	if (IsValid(GetInstigator()))
	{
		CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
	}
	
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ThisClass::HandleProjectileStop);
	
	ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileMovement->InitialSpeed;
}

void ADRProjectile::InitializeProjectile(UAbilitySystemComponent* InSourceAbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs, const FDRProjectileWorldImpactData& InWorldImpactData,
	int32 InSourceTeamId)
{
	if (!HasAuthority())
	{
		return;
	}
	
	SourceAbilitySystem = InSourceAbilitySystem;
	ImpactEffectSpecs = InImpactEffectSpecs;
	WorldImpactData = InWorldImpactData;
	SourceTeamId = InSourceTeamId;	
}

void ADRProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || bImpactHandled)
	{
		return;
	}
	
	bImpactHandled = true;
	
	AActor* HitActor = ImpactResult.GetActor();
	
	if (IsValid(HitActor)
		&& HitActor != GetOwner()
		&& HitActor != GetInstigator())
	{
		UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		
		// ASC가 있는 Actor와 충돌
		if (IsValid(TargetAbilitySystem))
		{
			if (!IsFriendlyTarget(HitActor))
			{
				ApplyImpactEffect(TargetAbilitySystem, ImpactResult);
			}
			
			Destroy();
			return;
		}
	}
	
	HandleWorldImpact(ImpactResult);
	Destroy();
}

void ADRProjectile::ApplyImpactEffect(UAbilitySystemComponent* TargetAbilitySystem, const FHitResult& ImpactResult)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	
	if (!IsValid(SourceASC)
		|| !IsValid(TargetAbilitySystem))
	{
		return;
	}
	
	for (const FGameplayEffectSpecHandle& SpecHandle : ImpactEffectSpecs)
	{
		if (!SpecHandle.IsValid())
		{
			continue;
		}
		
		FGameplayEffectSpec ImpactSpec(*SpecHandle.Data.Get());
		ImpactSpec.GetContext().AddHitResult(ImpactResult, true);
		
		SourceASC->ApplyGameplayEffectSpecToTarget(ImpactSpec, TargetAbilitySystem);
	}
}

bool ADRProjectile::IsFriendlyTarget(const AActor* TargetActor) const
{
	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	
	if (!IsValid(TargetPawn))
	{
		return false;
	}
	
	const ADRPlayerState* TargetPlayerState = TargetPawn->GetPlayerState<ADRPlayerState>();
	
	if (!IsValid(TargetPlayerState))
	{
		return false;
	}
	
	// 발사자 팀이 지정되지 않은 경우, Effect 적용 거부
	// if (SourceTeamId == INDEX_NONE)
	// {
	// 	return true;
	// }
	
	// 테스트 신다인
	// 팀 지정 기능이 없으므로 항상 적용되도록 하여 테스트
	if (SourceTeamId == INDEX_NONE)
	{
		return false;
	}
	// ======================================
	
	return TargetPlayerState->GetTeamId() == SourceTeamId;
}

void ADRProjectile::HandleWorldImpact(const FHitResult& ImpactResult)
{
	if (!WorldImpactData.bAddSnow)
	{
		return;
	}
	
	// World 지형 변동 관련 코드 추가 위치
}

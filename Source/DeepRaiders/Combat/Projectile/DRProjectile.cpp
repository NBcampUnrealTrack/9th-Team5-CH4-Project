
#include "DRProjectile.h"

#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"

#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "Kismet/GameplayStatics.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

const FName ADRProjectile::CollisionComponentName(TEXT("CollisionComponent"));

ADRProjectile::ADRProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 5.0f;

	CollisionComponent = ObjectInitializer.CreateDefaultSubobject<UShapeComponent, USphereComponent>(this, CollisionComponentName, false);

	SetRootComponent(CollisionComponent);

	if (USphereComponent* Sphere = Cast<USphereComponent>(CollisionComponent))
	{
		Sphere->InitSphereRadius(12.0f);
	}

	CollisionComponent->SetCollisionProfileName(TEXT("DRProjectile"));

	CollisionComponent->SetCanEverAffectNavigation(false);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));

	MeshComponent->SetupAttachment(CollisionComponent);

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));

	ProjectileMovement->UpdatedComponent = CollisionComponent;

	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void ADRProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HasAuthority())
	{
		// 클라에서의 충돌을 무시
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileMovement->InitialSpeed;
		
		return;
	}
	
	if (IsValid(GetOwner()))
	{
		CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
	}
	
	if (IsValid(GetInstigator()))
	{
		CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
	}
	
	if (ShouldIgnoreFriendlyBlockingHit())
	{
		RefreshFriendlyCollisionIgnores();
	}
	
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ThisClass::HandleProjectileStop);
	
	ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileMovement->InitialSpeed;
}

void ADRProjectile::InitializeProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
	float InBreakableDamageAmount,
	const FDRProjectileWorldImpactData& InWorldImpactData,
	int32 InSourceTeamId,
	const UObject* InPresentationSourceObject)
{
	if (!HasAuthority())
	{
		return;
	}

	SourceAbilitySystem = InSourceAbilitySystem;
	ImpactEffectSpecs = InImpactEffectSpecs;
	BreakableDamageAmount = FMath::Max(0.f, InBreakableDamageAmount);

	WorldImpactData = InWorldImpactData;
	SourceTeamId = InSourceTeamId;

	// UObject API가 const-correct하지 않은 경계에서만 해제.
	PresentationSourceObject = const_cast<UObject*>(InPresentationSourceObject);
}

void ADRProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || bImpactHandled)
	{
		return;
	}

	AActor* HitActor = ImpactResult.GetActor();

	// 아군과 충돌하면 무시하고 계속 진행
	if (ShouldIgnoreFriendlyBlockingHit()
		&& IsValid(HitActor) 
		&& IsFriendlyTarget(HitActor))
	{
		CollisionComponent->IgnoreActorWhenMoving(HitActor, true);
		ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileMovement->InitialSpeed;

		ProjectileMovement->Activate(true);
		ProjectileMovement->UpdateComponentVelocity();

		return;
	}

	bImpactHandled = true;

	HandleImpact(ImpactResult);
}

void ADRProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	AActor* HitActor = ImpactResult.GetActor();
	if (IsValid(HitActor) && HitActor != GetOwner() && HitActor != GetInstigator())
	{
		// Breakable
		if (ApplyBreakableDamage(ImpactResult))
		{
			ExecuteImpactGameplayCue(ImpactResult);

			Destroy();
			return;
		}

		// GAS Actor
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);

		if (IsValid(TargetASC))
		{
			ApplyImpactEffect(TargetASC, ImpactResult);
			ExecuteImpactGameplayCue(ImpactResult);

			Destroy();
			return;
		}
	}

	// 일반 World
	ExecuteImpactGameplayCue(ImpactResult);
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

bool ADRProjectile::ApplyBreakableDamage(const FHitResult& ImpactResult)
{
	ADRBreakableActor* BreakableTarget = Cast<ADRBreakableActor>(ImpactResult.GetActor());

	if (!IsValid(BreakableTarget) 
		|| BreakableTarget->IsBroken() 
		|| BreakableDamageAmount <= 0.f)
	{
		return false;
	}

	FVector DamageDirection = ProjectileMovement->Velocity.GetSafeNormal();

	if (DamageDirection.IsNearlyZero())
	{
		DamageDirection = GetActorForwardVector();
	}

	const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
			BreakableTarget,
			BreakableDamageAmount,
			DamageDirection,
			ImpactResult,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass());

	return AppliedDamage > KINDA_SMALL_NUMBER;
}

bool ADRProjectile::IsFriendlyTarget(const AActor* TargetActor) const
{
	// 테스트 신다인
	// 팀 지정 기능이 없으므로 INDEX_NONE에 대하여 항상 적군
	if (SourceTeamId == INDEX_NONE)
		return false;
	
	return DRCombatTeam::IsFriendlyTarget(SourceTeamId, TargetActor);
}

void ADRProjectile::HandleWorldImpact(const FHitResult& /*ImpactResult*/)
{
}

void ADRProjectile::RefreshFriendlyCollisionIgnores()
{
	if (!HasAuthority()
		|| !IsValid(CollisionComponent)
		|| SourceTeamId == INDEX_NONE)
	{
		return;
	}
	
	TArray<APawn*> FriendlyPawns;
	
	DRCombatTeam::GetFriendlyPawns(GetWorld(), SourceTeamId, FriendlyPawns);
	
	for (APawn* FriendlyPawn : FriendlyPawns)
	{
		if (IsValid(FriendlyPawn)
			&& FriendlyPawn !=  GetInstigator())
		{
			CollisionComponent->IgnoreActorWhenMoving(FriendlyPawn, true);
		}
	}
}

void ADRProjectile::ExecuteImpactGameplayCue(const FHitResult& ImpactResult)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();

	if (!HasAuthority() || !IsValid(SourceASC))
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
	Parameters.SourceObject = PresentationSourceObject.Get();

	SourceASC->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Weapon_Projectile_Impact, Parameters);
}

void ADRProjectile::ConfigureProjectileMovement(float InitialSpeed, float GravityScale)
{
	const float SafeSpeed = FMath::Max(InitialSpeed, 1.f);
	
	ProjectileMovement->InitialSpeed = SafeSpeed;
	ProjectileMovement->MaxSpeed = SafeSpeed;
	ProjectileMovement->ProjectileGravityScale = FMath::Max(GravityScale, 0.f);
	
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->Velocity = GetActorForwardVector() * SafeSpeed;
}

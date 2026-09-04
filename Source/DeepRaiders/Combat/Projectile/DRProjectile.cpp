
#include "DRProjectile.h"

#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
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
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"

const FName ADRProjectile::CollisionComponentName(TEXT("CollisionComponent"));

ADRProjectile::ADRProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

void ADRProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ReplicatedSizeMultiplier);
}

float ADRProjectile::GetConfiguredInitialSpeed() const
{
	return IsValid(ProjectileMovement)
		? FMath::Max(ProjectileMovement->InitialSpeed, 1.0f)
		: 1.0f;
}

float ADRProjectile::GetConfiguredGravityScale() const
{
	return IsValid(ProjectileMovement)
		? FMath::Max(ProjectileMovement->ProjectileGravityScale, 0.0f)
		: 0.0f;
}

void ADRProjectile::SetInitialLaunchVelocity(const FVector& InLaunchVelocity)
{
	InitialLaunchVelocity = InLaunchVelocity.ContainsNaN()
		? FVector::ZeroVector
		: InLaunchVelocity;
}

void ADRProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	InitialActorScale = GetActorScale3D();
}

void ADRProjectile::BeginPlay()
{
	Super::BeginPlay();

	/*
	 * 서버는 GA에서 전달한 ballistic velocity를 사용한다.
	 * 클라이언트는 SpawnRotation 기반 초기 속도로 시작하고 ReplicateMovement로
	 * 서버의 실제 궤적을 이어받는다.
	 */
	const FVector LaunchVelocity =
		HasAuthority() && !InitialLaunchVelocity.IsNearlyZero()
			? InitialLaunchVelocity
			: GetActorForwardVector() * ProjectileMovement->InitialSpeed;
	
	if (!HasAuthority())
	{
		// 클라에서의 충돌을 무시
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProjectileMovement->Velocity = LaunchVelocity;
		
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
	ProjectileMovement->Velocity = LaunchVelocity;
}

void ADRProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || EffectiveMaxRange <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	if (FVector::DistSquared(LaunchLocation, CurrentLocation) >= FMath::Square(EffectiveMaxRange))
	{
		Destroy();
		return;
	}

	UpdateFalloffAtLocation(CurrentLocation);
}

void ADRProjectile::InitializeProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
	float InBreakableDamageAmount,
	const FDRProjectileWorldImpactData& InWorldImpactData,
	int32 InSourceTeamId,
	const UObject* InPresentationSourceObject,
	float InEffectiveMaxRange,
	const FDRProjectileFalloffSettings& InFalloffSettings)
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
	LaunchLocation = GetActorLocation();
	EffectiveMaxRange = FMath::Max(InEffectiveMaxRange, 0.f);
	FalloffSettings = InFalloffSettings;
	SetActorTickEnabled(FalloffSettings.bEnabled && EffectiveMaxRange > KINDA_SMALL_NUMBER);

	// 사거리 강화로 비행 시간이 기존 InitialLifeSpan을 넘더라도 먼저 제거되지 않게 한다.
	if (FalloffSettings.bEnabled && EffectiveMaxRange > KINDA_SMALL_NUMBER)
	{
		const float ProjectileSpeed = FMath::Max(ProjectileMovement->InitialSpeed, 1.f);
		InitialLifeSpan = FMath::Max(InitialLifeSpan, EffectiveMaxRange / ProjectileSpeed + 1.f);
	}

	// Deferred Spawn 직후부터 발사자 캡슐과 겹칠 수 있으므로,
	// BeginPlay를 기다리지 않고 FinishSpawningActor 이전에 충돌을 무시한다.
	if (IsValid(GetOwner()))
	{
		CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
	}

	if (IsValid(GetInstigator()))
	{
		CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);

		// 캐릭터가 이동할 때도 투사체를 Blocking Hit로 처리하지 않도록 양방향 무시를 설정한다.
		if (UPrimitiveComponent* InstigatorRootComponent =
			Cast<UPrimitiveComponent>(GetInstigator()->GetRootComponent()))
		{
			InstigatorRootComponent->IgnoreActorWhenMoving(this, true);
		}
	}

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
	const FVector ImpactLocation = ImpactResult.bBlockingHit
		? FVector(ImpactResult.ImpactPoint)
		: GetActorLocation();

	if (EffectiveMaxRange > KINDA_SMALL_NUMBER
		&& FVector::DistSquared(LaunchLocation, ImpactLocation) >= FMath::Square(EffectiveMaxRange))
	{
		Destroy();
		return;
	}

	UpdateFalloffAtLocation(ImpactLocation);

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
	if (!IsValid(SourceASC) || !IsValid(TargetAbilitySystem))
	{
		return;
	}

	bool bAppliedAnyEffect = false;
	
	for (const FGameplayEffectSpecHandle& SpecHandle : ImpactEffectSpecs)
	{
		if (!SpecHandle.IsValid())
		{
			continue;
		}

		FGameplayEffectSpec ImpactSpec(*SpecHandle.Data.Get());
		ImpactSpec.GetContext().AddHitResult(ImpactResult, true);
		ScaleImpactSetByCallerMagnitude(ImpactSpec, DRGameplayTags::Data_Damage);
		ScaleImpactSetByCallerMagnitude(ImpactSpec, DRGameplayTags::Data_Freeze_Amount);

		SourceASC->ApplyGameplayEffectSpecToTarget(ImpactSpec, TargetAbilitySystem);

		bAppliedAnyEffect = true;
	}

	if (bAppliedAnyEffect)
	{
		ExecutePlayerHitGameplayCue(TargetAbilitySystem, ImpactResult);
	}
}

void ADRProjectile::ExecutePlayerHitGameplayCue(UAbilitySystemComponent* TargetAbilitySystem, const FHitResult& ImpactResult)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	if (!HasAuthority() || !IsValid(SourceASC) || !IsValid(TargetAbilitySystem))
	{
		return;
	}

	// ASC Owner는 PlayerState이므로 AvatarActor로 실제 Character인지 확인한다.
	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(TargetAbilitySystem->GetAvatarActor());
	if (!IsValid(TargetCharacter))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddHitResult(ImpactResult, true);

	FGameplayCueParameters Parameters(EffectContext);
	Parameters.Location = ImpactResult.ImpactPoint;
	Parameters.Normal = ImpactResult.ImpactNormal;
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = this;
	Parameters.SourceObject = PresentationSourceObject.Get();

    // 모든 Player 피격 공통 Presentation.
	TargetAbilitySystem->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Player_Hit, Parameters);
    // Snowball Projectile에 맞았을 때만 재생하는 피격음.
	TargetAbilitySystem->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Sound_Player_Snowball_Impact, Parameters);
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
			BreakableDamageAmount * CurrentFalloffStrength,
			DamageDirection,
			ImpactResult,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass());

	return AppliedDamage > KINDA_SMALL_NUMBER;
}

bool ADRProjectile::IsFriendlyTarget(const AActor* TargetActor) const
{
	// INDEX_NONE에 대하여 항상 적군
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

float ADRProjectile::EvaluateFalloffStrengthAtLocation(const FVector& Location) const
{
	if (!FalloffSettings.bEnabled || EffectiveMaxRange <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	const float NormalizedDistance = FMath::Clamp(
		FVector::Distance(LaunchLocation, Location) / EffectiveMaxRange,
		0.f,
		1.f);

	if (IsValid(FalloffSettings.StrengthCurve))
	{
		return FMath::Clamp(FalloffSettings.StrengthCurve->GetFloatValue(NormalizedDistance), 0.f, 1.f);
	}

	const float FullStrengthRatio = FMath::Clamp(FalloffSettings.FullStrengthRangeRatio, 0.f, 0.99f);
	return 1.f - FMath::GetMappedRangeValueClamped(
		FVector2D(FullStrengthRatio, 1.f),
		FVector2D(0.f, 1.f),
		NormalizedDistance);
}

void ADRProjectile::UpdateFalloffAtLocation(const FVector& Location)
{
	CurrentFalloffStrength = EvaluateFalloffStrengthAtLocation(Location);
	ApplyFalloffScale(CurrentFalloffStrength);
}

void ADRProjectile::ApplyFalloffScale(float Strength)
{
	const float SizeMultiplier = FMath::Lerp(
		FMath::Clamp(FalloffSettings.MinSizeMultiplier, 0.f, 1.f),
		1.f,
		FMath::Clamp(Strength, 0.f, 1.f));
	ApplySizeMultiplier(SizeMultiplier);

	const uint8 NewReplicatedSize = static_cast<uint8>(FMath::RoundToInt(SizeMultiplier * MAX_uint8));
	if (ReplicatedSizeMultiplier != NewReplicatedSize)
	{
		ReplicatedSizeMultiplier = NewReplicatedSize;
	}
}

void ADRProjectile::ApplySizeMultiplier(float SizeMultiplier)
{
	if (FMath::IsNearlyEqual(LastAppliedSizeMultiplier, SizeMultiplier, 0.005f))
	{
		return;
	}

	SetActorScale3D(InitialActorScale * SizeMultiplier);
	LastAppliedSizeMultiplier = SizeMultiplier;
}

void ADRProjectile::ScaleImpactSetByCallerMagnitude(
	FGameplayEffectSpec& ImpactSpec,
	const FGameplayTag& DataTag) const
{
	const float* Magnitude = ImpactSpec.SetByCallerTagMagnitudes.Find(DataTag);
	if (Magnitude == nullptr)
	{
		return;
	}

	ImpactSpec.SetSetByCallerMagnitude(DataTag, *Magnitude * CurrentFalloffStrength);
}

void ADRProjectile::OnRep_SizeMultiplier()
{
	ApplySizeMultiplier(static_cast<float>(ReplicatedSizeMultiplier) / MAX_uint8);
}

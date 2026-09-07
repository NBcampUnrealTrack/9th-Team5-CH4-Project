
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
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Skill/Barrier/DRBarrierGenerator.h"

#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "Kismet/GameplayStatics.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

namespace DRProjectilePresentation
{
	TAutoConsoleVariable<int32> CVarShowOwnerServerProjectile(
		TEXT("dr.Projectile.ShowOwnerServerProjectile"),
		0,
		TEXT("Owner authoritative projectile visibility. 0=hide only when matching local prediction exists, 1=always show."));

	TAutoConsoleVariable<float> CVarPredictionConfirmTimeout(
		TEXT("dr.Projectile.PredictionConfirmTimeout"),
		0.75f,
		TEXT(
			"Seconds an owning-client local predicted projectile waits "
			"for its authoritative projectile. "
			"<= 0 disables the prediction confirmation timeout."));
	
	bool IsPredictionDebugEnabled()
	{
		if (const IConsoleVariable* CVar =
			IConsoleManager::Get().FindConsoleVariable(
				TEXT("dr.Projectile.LocalVisualDebug")))
		{
			return CVar->GetInt() != 0;
		}

		return false;
	}
}

namespace DRProjectilePredictionRegistry
{
	struct FKey
	{
		TWeakObjectPtr<AActor> Owner;
		uint32 ShotSequence = 0;

		bool operator==(const FKey& Other) const
		{
			return Owner == Other.Owner
				&& ShotSequence == Other.ShotSequence;
		}

		friend uint32 GetTypeHash(const FKey& Key)
		{
			return HashCombineFast(
				GetTypeHash(Key.Owner),
				GetTypeHash(Key.ShotSequence));
		}
	};

	TMap<FKey, TWeakObjectPtr<ADRProjectile>> LocalPredictedProjectiles;

	FKey MakeKey(AActor* Owner, uint32 ShotSequence)
	{
		FKey Key;
		Key.Owner = Owner;
		Key.ShotSequence = ShotSequence;
		return Key;
	}

	ADRProjectile* Find(AActor* Owner, uint32 ShotSequence)
	{
		if (!IsValid(Owner) || ShotSequence == 0)
		{
			return nullptr;
		}

		const FKey Key = MakeKey(Owner, ShotSequence);
		TWeakObjectPtr<ADRProjectile>* Found =
			LocalPredictedProjectiles.Find(Key);

		if (Found == nullptr)
		{
			return nullptr;
		}

		ADRProjectile* Projectile = Found->Get();
		if (!IsValid(Projectile))
		{
			LocalPredictedProjectiles.Remove(Key);
			return nullptr;
		}

		return Projectile;
	}
}

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
	CollisionComponent->SetCollisionResponseToChannel(DRCollisionChannels::BarrierTrace, ECR_Ignore);
	CollisionComponent->SetGenerateOverlapEvents(true);

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
	DOREPLIFETIME_CONDITION(ThisClass, ShotSequence, COND_OwnerOnly);
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

void ADRProjectile::ConfigureAsLocalVisualProjectile(
	const FVector& InLaunchVelocity,
	float LifetimeSeconds,
	uint32 InShotSequence)
{
	bLocalVisualProjectile = true;
	ShotSequence = InShotSequence;

	bReplicates = false;
	SetReplicateMovement(false);

	SetInitialLaunchVelocity(InLaunchVelocity);
	InitialLifeSpan = FMath::Max(LifetimeSeconds, 0.01f);
}

void ADRProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	InitialActorScale = GetActorScale3D();
}

void ADRProjectile::BeginPlay()
{
	Super::BeginPlay();

	// BP에 저장된 예전 Profile 값과 무관하게 히트스캔 전용 구체는 물리탄이 항상 무시한다.
	CollisionComponent->SetCollisionResponseToChannel(
		DRCollisionChannels::BarrierTrace,
		ECR_Ignore);

	if (bLocalVisualProjectile)
	{
		/*
		 * Owner client의 gameplay 없는 predicted visual proxy.
		 *
		 * 기존 DRProjectile collision profile은 유지하되 Pawn은 무시한다.
		 * 따라서 Voxel/WorldStatic/WorldDynamic 등 서버 탄이 막히는 월드 표면은
		 * 로컬에서도 시각적으로 정지할 수 있고, Player gameplay 판정은 만들지 않는다.
		 */
		SetReplicates(false);
		SetReplicateMovement(false);
		SetActorEnableCollision(true);

		CollisionComponent->SetCollisionEnabled(
			ECollisionEnabled::QueryOnly);
		CollisionComponent->SetGenerateOverlapEvents(false);
		CollisionComponent->SetCollisionResponseToChannel(
			ECC_Pawn,
			ECR_Ignore);
		CollisionComponent->SetCollisionResponseToChannel(
			DRCollisionChannels::BarrierTrace,
			ECR_Ignore);

		if (IsValid(GetOwner()))
		{
			CollisionComponent->IgnoreActorWhenMoving(
				GetOwner(),
				true);
		}

		if (IsValid(GetInstigator()))
		{
			CollisionComponent->IgnoreActorWhenMoving(
				GetInstigator(),
				true);
		}

		const FVector VisualLaunchVelocity =
			!InitialLaunchVelocity.IsNearlyZero()
				? InitialLaunchVelocity
				: GetActorForwardVector()
					* ProjectileMovement->InitialSpeed;

		ProjectileMovement->OnProjectileStop.AddDynamic(
			this,
			&ThisClass::HandleProjectileStop);
		ProjectileMovement->Velocity = VisualLaunchVelocity;
		ProjectileMovement->Activate(true);
		ProjectileMovement->UpdateComponentVelocity();

		RegisterLocalPrediction();

		const float ConfirmTimeout =
			FMath::Clamp(
				DRProjectilePresentation::
					CVarPredictionConfirmTimeout
					.GetValueOnGameThread(),
				0.f,
				5.f);

		if (ConfirmTimeout > KINDA_SMALL_NUMBER)
		{
			FTimerHandle TimeoutHandle;

			GetWorldTimerManager().SetTimer(
				TimeoutHandle,
				this,
				&ThisClass::HandleLocalPredictionConfirmTimeout,
				ConfirmTimeout,
				false);
		}

		return;
	}

	/*
	 * 서버는 GA에서 전달한 ballistic velocity를 사용한다.
	 * 클라이언트 replica는 server movement replication을 이어받는다.
	 */
	const FVector LaunchVelocity =
		HasAuthority() && !InitialLaunchVelocity.IsNearlyZero()
			? InitialLaunchVelocity
			: GetActorForwardVector() * ProjectileMovement->InitialSpeed;

	if (!HasAuthority())
	{
		CollisionComponent->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
		ProjectileMovement->Velocity = LaunchVelocity;

		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (IsValid(OwnerPawn) && OwnerPawn->IsLocallyControlled())
		{
			TryReconcileOwnerPrediction();
		}

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

	ProjectileMovement->OnProjectileStop.AddDynamic(
		this,
		&ThisClass::HandleProjectileStop);
	ProjectileMovement->Velocity = LaunchVelocity;
}

void ADRProjectile::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (bLocalVisualProjectile)
	{
		UnregisterLocalPrediction();
	}
	else if (!HasAuthority() && ShotSequence != 0)
	{
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (IsValid(OwnerPawn) && OwnerPawn->IsLocallyControlled())
		{
			if (ADRProjectile* LocalPrediction =
				DRProjectilePredictionRegistry::Find(
					GetOwner(),
					ShotSequence))
			{
				if (DRProjectilePresentation::IsPredictionDebugEnabled())
				{
					UE_LOG(
						LogTemp,
						Log,
						TEXT("[ProjectilePrediction][SERVER_END] Shot=%u Server=%s Local=%s Reason=%d"),
						ShotSequence,
						*GetNameSafe(this),
						*GetNameSafe(LocalPrediction),
						static_cast<int32>(EndPlayReason));
				}

				LocalPrediction->Destroy();
			}
		}
	}

	Super::EndPlay(EndPlayReason);
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
	SetActorTickEnabled(EffectiveMaxRange > KINDA_SMALL_NUMBER);

	// Deferred Spawn 내부에서 시작할 경우 FinishSpawning의 최초 충돌 등록이 BeginPlay보다 먼저다.
	// 따라서 BP Profile에 저장된 값과 무관하게 이 시점에 Trace 전용 배리어를 Ignore해야 한다.
	CollisionComponent->SetCollisionResponseToChannel(DRCollisionChannels::BarrierTrace, ECR_Ignore);
	CollisionComponent->SetGenerateOverlapEvents(true);

	// 사거리 강화로 비행 시간이 기존 InitialLifeSpan을 넘더라도 먼저 제거되지 않게 한다.
	if (EffectiveMaxRange > KINDA_SMALL_NUMBER)
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
	if (bLocalVisualProjectile)
	{
		if (bImpactHandled)
		{
			return;
		}

		bImpactHandled = true;

		if (IsValid(ProjectileMovement))
		{
			ProjectileMovement->StopMovementImmediately();
		}

		if (IsValid(CollisionComponent))
		{
			CollisionComponent->SetCollisionEnabled(
				ECollisionEnabled::NoCollision);
		}

		// Registry에는 남겨 server ShotSequence와 계속 매칭하되, local visual만 사라진다.
		ApplyOwnerServerProjectileVisibility(false);

		if (DRProjectilePresentation::IsPredictionDebugEnabled())
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[ProjectilePrediction][LOCAL_WORLD_HIT] Shot=%u Projectile=%s Actor=%s Location=%s Confirmed=%d"),
				ShotSequence,
				*GetNameSafe(this),
				*GetNameSafe(ImpactResult.GetActor()),
				*ImpactResult.ImpactPoint.ToCompactString(),
				bAuthoritativeConfirmed ? 1 : 0);
		}

		return;
	}

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

void ADRProjectile::HandleBarrierOverlap(ADRBarrierGenerator* BarrierGenerator)
{
	if (!HasAuthority()
		|| bImpactHandled
		|| !IsValid(BarrierGenerator)
		|| BarrierGenerator->IsBroken())
	{
		return;
	}

	// 아군탄은 애초에 Blocking Hit가 발생하지 않았으므로 아무 처리 없이 그대로 비행한다.
	if (IsFriendlyTarget(BarrierGenerator))
	{
		return;
	}

	bImpactHandled = true;

	const FVector ImpactPoint = GetActorLocation();
	FVector ImpactNormal = (ImpactPoint - BarrierGenerator->GetActorLocation()).GetSafeNormal();
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = -GetActorForwardVector();
	}

	FHitResult BarrierHit(
		BarrierGenerator,
		BarrierGenerator->GetBarrierCollisionComponent(),
		ImpactPoint,
		ImpactNormal);
	BarrierHit.bBlockingHit = true;
	BarrierHit.TraceStart = LaunchLocation;
	BarrierHit.TraceEnd = ImpactPoint;
	BarrierHit.Location = ImpactPoint;
	BarrierHit.ImpactPoint = ImpactPoint;

	UpdateFalloffAtLocation(ImpactPoint);
	HandleImpact(BarrierHit);
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

bool ADRProjectile::ApplyBreakableDamage(
	AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	FVector DamageDirection =
		ProjectileMovement->Velocity.GetSafeNormal();

	if (DamageDirection.IsNearlyZero())
	{
		DamageDirection = GetActorForwardVector();
	}

	const FVector HitLocation =
		TargetActor->GetActorLocation();

	UPrimitiveComponent* HitComponent =
		Cast<UPrimitiveComponent>(
			TargetActor->GetRootComponent());

	FHitResult SyntheticHit(
		TargetActor,
		HitComponent,
		HitLocation,
		-DamageDirection);

	SyntheticHit.bBlockingHit = true;
	SyntheticHit.TraceStart = GetActorLocation();
	SyntheticHit.TraceEnd = HitLocation;
	SyntheticHit.Location = HitLocation;
	SyntheticHit.ImpactPoint = HitLocation;

	return ApplyBreakableDamage(SyntheticHit);
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
			&& FriendlyPawn != GetInstigator())
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

void ADRProjectile::OnRep_ShotSequence()
{
	TryReconcileOwnerPrediction();
}

void ADRProjectile::RegisterLocalPrediction()
{
	if (!bLocalVisualProjectile
		|| ShotSequence == 0
		|| !IsValid(GetOwner()))
	{
		return;
	}

	const DRProjectilePredictionRegistry::FKey Key =
		DRProjectilePredictionRegistry::MakeKey(
			GetOwner(),
			ShotSequence);

	if (ADRProjectile* Existing =
		DRProjectilePredictionRegistry::Find(
			GetOwner(),
			ShotSequence))
	{
		if (Existing != this)
		{
			Existing->Destroy();
		}
	}

	DRProjectilePredictionRegistry::LocalPredictedProjectiles.Add(
		Key,
		this);
}

void ADRProjectile::UnregisterLocalPrediction()
{
	if (ShotSequence == 0 || !IsValid(GetOwner()))
	{
		return;
	}

	const DRProjectilePredictionRegistry::FKey Key =
		DRProjectilePredictionRegistry::MakeKey(
			GetOwner(),
			ShotSequence);

	TWeakObjectPtr<ADRProjectile>* Found =
		DRProjectilePredictionRegistry::LocalPredictedProjectiles.Find(Key);

	if (Found != nullptr && Found->Get() == this)
	{
		DRProjectilePredictionRegistry::LocalPredictedProjectiles.Remove(Key);
	}
}

void ADRProjectile::TryReconcileOwnerPrediction()
{
	if (HasAuthority()
		|| bLocalVisualProjectile
		|| bOwnerPredictionReconciled)
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	const bool bForceShowServerProjectile =
		DRProjectilePresentation::
			CVarShowOwnerServerProjectile
			.GetValueOnGameThread() != 0;

	if (ShotSequence == 0)
	{
		ApplyOwnerServerProjectileVisibility(true);
		return;
	}

	ADRProjectile* LocalPrediction =
		DRProjectilePredictionRegistry::Find(
			GetOwner(),
			ShotSequence);

	if (!IsValid(LocalPrediction))
	{
		/*
		 * Prediction이 없거나 이미 만료된 경우에는 authoritative projectile을
		 * fallback으로 보여준다. Shotgun처럼 아직 local prediction을 지원하지 않는
		 * 무기까지 실수로 숨기지 않기 위한 안전장치다.
		 */
		ApplyOwnerServerProjectileVisibility(true);

		if (DRProjectilePresentation::IsPredictionDebugEnabled())
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[ProjectilePrediction][RECONCILE_MISS] Shot=%u Server=%s ServerLocation=%s"),
				ShotSequence,
				*GetNameSafe(this),
				*GetActorLocation().ToCompactString());
		}

		return;
	}

	LocalPrediction->bAuthoritativeConfirmed = true;
	bOwnerPredictionReconciled = true;
	
	/*
	 * matching local proxy가 있으면 owner는 local proxy를 계속 본다.
	 * server replica를 local 위치로 snap하거나 local을 server 위치로 되감지 않는다.
	 * server replica는 gameplay authority/종료 신호 역할만 하고 시각적으로 숨긴다.
	 */
	ApplyOwnerServerProjectileVisibility(
		bForceShowServerProjectile);

	if (DRProjectilePresentation::IsPredictionDebugEnabled())
	{
		const float PositionGapCm = FVector::Distance(
			LocalPrediction->GetActorLocation(),
			GetActorLocation());

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[ProjectilePrediction][RECONCILE_OK] Shot=%u Server=%s Local=%s GapCm=%.2f ServerVisible=%d LocalImpactDone=%d ServerLocation=%s LocalLocation=%s"),
			ShotSequence,
			*GetNameSafe(this),
			*GetNameSafe(LocalPrediction),
			PositionGapCm,
			bForceShowServerProjectile ? 1 : 0,
			LocalPrediction->bImpactHandled ? 1 : 0,
			*GetActorLocation().ToCompactString(),
			*LocalPrediction->GetActorLocation().ToCompactString());
	}
}

void ADRProjectile::ApplyOwnerServerProjectileVisibility(
	bool bVisible)
{
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (IsValid(PrimitiveComponent))
		{
			PrimitiveComponent->SetVisibility(
				bVisible,
				true);
		}
	}
}

void ADRProjectile::HandleLocalPredictionConfirmTimeout()
{
	if (!bLocalVisualProjectile
		|| bAuthoritativeConfirmed
		|| ShotSequence == 0)
	{
		return;
	}

	if (DRProjectilePresentation::IsPredictionDebugEnabled())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[ProjectilePrediction][LOCAL_TIMEOUT] "
				"Shot=%u Projectile=%s "
				"Location=%s ImpactDone=%d"),
			ShotSequence,
			*GetNameSafe(this),
			*GetActorLocation().ToCompactString(),
			bImpactHandled ? 1 : 0);
	}

	/*
	 * 서버 projectile을 confirmation window 안에 받지 못했다.
	 *
	 * 이 actor는 gameplay 결과를 만들지 않으므로
	 * 그냥 제거하면 된다.
	 *
	 * 나중에 authoritative projectile이 도착하면
	 * registry miss -> server projectile fallback 표시.
	 */
	Destroy();
}
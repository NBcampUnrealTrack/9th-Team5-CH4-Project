#include "DRGA_FireProjectile.h"

#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"

namespace DRProjectileAim
{
	constexpr float MinAimDistance = 1.0f;
	constexpr float MaxClientViewLocationError = 500.0f;
}

bool UDRGA_FireProjectile::IsAttackConfigurationValid(
	const UDRProjectileWeaponItemDefinition* WeaponDefinition) const
{
	return Super::IsAttackConfigurationValid(WeaponDefinition)
		&& WeaponDefinition->ProjectileClass != nullptr;
}

void UDRGA_FireProjectile::OnRangedWeaponActivated()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo != nullptr
		&& ActorInfo->IsNetAuthority()
		&& !ActorInfo->IsLocallyControlled())
	{
		/*
		 * Projectile은 발사 시점의 클라이언트 조준점을 사용한다.
		 * Generic InputPressed 이벤트에는 위치 payload가 없으므로 TargetData를 수신한다.
		 */
		RegisterServerShotTargetDataDelegate();
	}
}

void UDRGA_FireProjectile::OnRangedWeaponEnded()
{
	UnregisterServerShotTargetDataDelegate();
}

bool UDRGA_FireProjectile::SendLocalShotRequest()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr
		|| !ActorInfo->IsLocallyControlled())
	{
		return false;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		return false;
	}

	FHitResult CameraHit;
	if (!TraceCameraAim(ViewLocation, ViewDirection, CameraHit))
	{
		return false;
	}

	/*
	 * Crosshair가 실제 Geometry를 맞춘 경우에는 그 ImpactPoint가 발사 의도다.
	 *
	 * 허공인 경우 MaxAttackDistance 끝점까지 탄도를 맞추면 중력탄은 물리적으로
	 * 도달 불가능한 거리를 목표로 삼기 쉽다. 별도 NoHitAimDistance에서
	 * 카메라 Ray와 Projectile 궤적이 교차하도록 한다.
	 */
	const float NoHitAimDistance = FMath::Clamp(
		WeaponDefinition->NoHitAimDistance,
		DRProjectileAim::MinAimDistance,
		FMath::Max(WeaponDefinition->MaxAttackDistance, DRProjectileAim::MinAimDistance));

	const FVector AimPoint = CameraHit.bBlockingHit
		? FVector(CameraHit.ImpactPoint)
		: ViewLocation + ViewDirection * NoHitAimDistance;

	if (AimPoint.ContainsNaN())
	{
		return false;
	}

	FHitResult ShotAimHit = CameraHit;
	ShotAimHit.TraceStart = ViewLocation;
	ShotAimHit.TraceEnd = AimPoint;
	ShotAimHit.Location = AimPoint;
	ShotAimHit.ImpactPoint = AimPoint;

	/*
	 * 실제 Projectile Spawn과 Presentation 모두 Character의 GameplayFireAnchor를
	 * 동일한 기준으로 사용한다. Socket/Weapon mesh 위치는 Gameplay 판정 기준이 아니다.
	 */
	FVector GameplayFireOrigin;
	if (!ResolveGameplayFireOrigin(ViewDirection, GameplayFireOrigin))
	{
		return false;
	}

	if (!ActorInfo->IsNetAuthority())
	{
		PlayLocalFirePresentation(GameplayFireOrigin, AimPoint);
	}

	FGameplayAbilityTargetDataHandle TargetData(
		new FGameplayAbilityTargetData_SingleTargetHit(ShotAimHit));

	if (ActorInfo->IsNetAuthority())
	{
		HandleServerShotTargetData(TargetData, FGameplayTag());
		return true;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	/*
	 * 부모 TryRequestLocalShot()이 생성한 PredictionKey를 그대로 사용해
	 * "발사 버튼을 누른 순간"의 Camera Aim을 서버로 전달한다.
	 */
	AbilitySystem->ServerSetReplicatedTargetData(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActivationInfo().GetActivationPredictionKey(),
		TargetData,
		FGameplayTag(),
		AbilitySystem->ScopedPredictionKey);

	return true;
}

void UDRGA_FireProjectile::RegisterServerShotTargetDataDelegate()
{
	if (ServerShotTargetDataDelegateHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem =
		GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	const FGameplayAbilitySpecHandle SpecHandle =
		GetCurrentAbilitySpecHandle();
	const FPredictionKey PredictionKey =
		GetCurrentActivationInfo().GetActivationPredictionKey();

	ServerShotTargetDataDelegateHandle =
		AbilitySystem->AbilityTargetDataSetDelegate(
			SpecHandle,
			PredictionKey).AddUObject(
				this,
				&ThisClass::HandleServerShotTargetData);

	/*
	 * TargetData가 Delegate 등록보다 먼저 서버에 도착한 경우에도
	 * 캐시된 데이터를 즉시 소비할 수 있게 한다.
	 */
	AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(
		SpecHandle,
		PredictionKey);
}

void UDRGA_FireProjectile::UnregisterServerShotTargetDataDelegate()
{
	if (!ServerShotTargetDataDelegateHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem =
		GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->AbilityTargetDataSetDelegate(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActivationInfo().GetActivationPredictionKey())
			.Remove(ServerShotTargetDataDelegateHandle);
	}

	ServerShotTargetDataDelegateHandle.Reset();
}

void UDRGA_FireProjectile::HandleServerShotTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FGameplayTag /*ApplicationTag*/)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}

	FVector AimPoint;
	FVector AimDirection;
	const bool bValidAim = ValidateServerShotTargetData(
		TargetData,
		AimPoint,
		AimDirection);

	/*
	 * 원격 클라이언트 TargetData는 성공/실패와 관계없이 이번 Shot 데이터로 소비한다.
	 * 자동사격의 다음 Shot이 동일한 Ability/PredictionKey에 새 TargetData를 보낼 수 있다.
	 */
	if (!ActorInfo->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* AbilitySystem =
			ActorInfo->AbilitySystemComponent.Get())
		{
			AbilitySystem->ConsumeClientReplicatedTargetData(
				GetCurrentAbilitySpecHandle(),
				GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}

	if (!bValidAim)
	{
		return;
	}

	if (!TryCommitServerShot())
	{
		return;
	}

	// 실제 Projectile이 하나 이상 생성된 발사만 Heat를 누적한다.
	if (ExecuteServerProjectileShot(AimPoint, AimDirection))
	{
		ApplyHeatForSuccessfulShot();
	}
}

bool UDRGA_FireProjectile::ValidateServerShotTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutAimPoint,
	FVector& OutAimDirection) const
{
	OutAimPoint = FVector::ZeroVector;
	OutAimDirection = FVector::ZeroVector;

	if (TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* AimHit = Data != nullptr
		? Data->GetHitResult()
		: nullptr;

	if (AimHit == nullptr)
	{
		return false;
	}

	const FVector ClientViewLocation = AimHit->TraceStart;
	const FVector AimPoint = AimHit->bBlockingHit
		? FVector(AimHit->ImpactPoint)
		: FVector(AimHit->TraceEnd);

	if (ClientViewLocation.ContainsNaN()
		|| AimPoint.ContainsNaN())
	{
		return false;
	}

	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	if (!GetViewPoint(ServerViewLocation, ServerViewRotation))
	{
		return false;
	}

	/*
	 * Dedicated Server의 현재 Camera 위치와 클라이언트 발사 순간 Camera 위치는
	 * 네트워크 지연 때문에 조금 다를 수 있다. 완전히 다른 위치에서 만든 TargetData만 거절한다.
	 */
	if (FVector::DistSquared(ClientViewLocation, ServerViewLocation)
		> FMath::Square(DRProjectileAim::MaxClientViewLocationError))
	{
		return false;
	}

	const FVector ToAim = AimPoint - ClientViewLocation;
	const float AimDistance = ToAim.Size();
	const float MaxAttackDistance =
		FMath::Max(GetMaxAttackDistance(), DRProjectileAim::MinAimDistance);

	if (AimDistance < DRProjectileAim::MinAimDistance
		|| AimDistance > MaxAttackDistance + DRProjectileAim::MaxClientViewLocationError)
	{
		return false;
	}

	const FVector ClientAimDirection = ToAim / AimDistance;
	const FVector ServerViewDirection =
		ServerViewRotation.Vector().GetSafeNormal();

	if (ServerViewDirection.IsNearlyZero())
	{
		return false;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	const float MaxDeviationDegrees = FMath::Clamp(
		WeaponDefinition->MaxServerAimDeviationDegrees,
		0.0f,
		180.0f);

	const float AimDot = FMath::Clamp(
		FVector::DotProduct(ServerViewDirection, ClientAimDirection),
		-1.0f,
		1.0f);
	const float AimDeviationDegrees =
		FMath::RadiansToDegrees(FMath::Acos(AimDot));

	if (AimDeviationDegrees > MaxDeviationDegrees)
	{
		return false;
	}

	OutAimPoint = AimPoint;
	OutAimDirection = ClientAimDirection;
	return true;
}

bool UDRGA_FireProjectile::ExecuteServerProjectileShot(
	const FVector& AimPoint,
	const FVector& AimDirection)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!IsValid(AvatarActor))
	{
		return false;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	/*
	 * Projectile은 Character의 고정 GameplayFireAnchor에서 출발한다.
	 * AimDirection은 클라이언트 발사 순간의 Crosshair 방향이며,
	 * 실제 중력 보정은 아래 ResolveProjectileLaunchVelocity()가 담당한다.
	 */
	FVector GameplayFireOrigin;
	if (!ResolveGameplayFireOrigin(AimDirection, GameplayFireOrigin))
	{
		return false;
	}

	FVector BaseLaunchVelocity;
	if (!ResolveProjectileLaunchVelocity(
		GameplayFireOrigin,
		AimPoint,
		BaseLaunchVelocity))
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);

	const int32 SafeProjectileCount = GetWeaponProjectileCount();
	const float SpreadRadians = FMath::DegreesToRadians(
		FMath::Max(
			WeaponDefinition->SpreadHalfAngleDegrees,
			0.f));
	const float LaunchSpeed = BaseLaunchVelocity.Size();
	const FVector BaseLaunchDirection =
		BaseLaunchVelocity.GetSafeNormal();

	if (LaunchSpeed <= KINDA_SMALL_NUMBER
		|| BaseLaunchDirection.IsNearlyZero())
	{
		return false;
	}

	bool bSpawnedAnyProjectile = false;

	for (int32 ProjectileIndex = 0;
		ProjectileIndex < SafeProjectileCount;
		++ProjectileIndex)
	{
		FVector ProjectileDirection = BaseLaunchDirection;

		if (SpreadRadians > KINDA_SMALL_NUMBER)
		{
			ProjectileDirection =
				FMath::VRandCone(
					BaseLaunchDirection,
					SpreadRadians);
		}

		const FVector LaunchVelocity =
			ProjectileDirection * LaunchSpeed;

		if (SpawnProjectile(
			GameplayFireOrigin,
			LaunchVelocity,
			AvatarActor,
			AbilitySystem,
			ImpactEffectSpecs))
		{
			bSpawnedAnyProjectile = true;
		}
	}

	if (!bSpawnedAnyProjectile)
	{
		return false;
	}

	PlayServerFirePresentation(
		GameplayFireOrigin,
		AimPoint);

	return true;
}

bool UDRGA_FireProjectile::ResolveProjectileLaunchVelocity(
	const FVector& SpawnLocation,
	const FVector& AimPoint,
	FVector& OutLaunchVelocity) const
{
	OutLaunchVelocity = FVector::ZeroVector;

	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		GetCurrentWeaponDefinition();

	if (!IsValid(WeaponDefinition)
		|| !WeaponDefinition->ProjectileClass)
	{
		return false;
	}

	const ADRProjectile* ProjectileDefault =
		WeaponDefinition->ProjectileClass->GetDefaultObject<ADRProjectile>();

	if (!IsValid(ProjectileDefault))
	{
		return false;
	}

	const float LaunchSpeed =
		FMath::Max(
			ProjectileDefault->GetConfiguredInitialSpeed(),
			1.0f);
	const float GravityScale =
		FMath::Max(
			ProjectileDefault->GetConfiguredGravityScale(),
			0.0f);

	const FVector DirectDirection =
		(AimPoint - SpawnLocation).GetSafeNormal();

	if (DirectDirection.IsNearlyZero())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	// 중력이 없는 Projectile은 기존과 동일하게 AimPoint 직선 방향으로 발사한다.
	if (GravityScale <= KINDA_SMALL_NUMBER)
	{
		OutLaunchVelocity =
			DirectDirection * LaunchSpeed;
		return true;
	}

	const float OverrideGravityZ =
		World->GetGravityZ() * GravityScale;

	FCollisionResponseParams ResponseParams =
	FCollisionResponseParams::DefaultResponseParam;

	TArray<AActor*> ActorsToIgnore;

	if (UGameplayStatics::SuggestProjectileVelocity(
		this,
		OutLaunchVelocity,
		SpawnLocation,
		AimPoint,
		LaunchSpeed,
		false, // bHighArc - 낮은 탄도 우선
		0.0f,  // CollisionRadius
		OverrideGravityZ,
		ESuggestProjVelocityTraceOption::DoNotTrace,
		ResponseParams,
		ActorsToIgnore,
		false, // bDrawDebug
		true)  // bAcceptClosestOnNoSolutions
		&& !OutLaunchVelocity.IsNearlyZero())
	{
		return true;
	}

	/*
	 * 마지막 fallback.
	 * Solver 실패 때문에 발사 자체가 먹히는 것보다는 기존 직선 발사 동작을 유지한다.
	 */
	OutLaunchVelocity =
		DirectDirection * LaunchSpeed;

	return true;
}

bool UDRGA_FireProjectile::SpawnProjectile(
	const FVector& SpawnLocation,
	const FVector& LaunchVelocity,
	AActor* AvatarActor,
	UAbilitySystemComponent* AbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs)
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		GetCurrentWeaponDefinition();

	if (!IsValid(AvatarActor)
		|| !IsValid(AbilitySystem)
		|| !IsValid(WeaponDefinition)
		|| !WeaponDefinition->ProjectileClass)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const FVector SafeLaunchVelocity =
		LaunchVelocity.ContainsNaN()
			? FVector::ZeroVector
			: LaunchVelocity;
	const FVector SafeDirection =
		SafeLaunchVelocity.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return false;
	}

	const FTransform SpawnTransform(
		SafeDirection.Rotation(),
		SpawnLocation);

	const FDRProjectileWeaponSnowAddSettings& SnowAddSettings =
		WeaponDefinition->SnowAddSettings;

	FDRProjectileWorldImpactData WorldImpactData;
	WorldImpactData.bAddSnow = SnowAddSettings.bEnabled;
	WorldImpactData.SnowRadius = SnowAddSettings.Radius;
	WorldImpactData.SnowAmount = SnowAddSettings.Amount;
	WorldImpactData.SnowEditTool = SnowAddSettings.EditTool;
	WorldImpactData.bAllowVirtualSurfaceFallback =
		SnowAddSettings.bAllowVirtualSurfaceFallback;

	ADRProjectile* Projectile =
		World->SpawnActorDeferred<ADRProjectile>(
			WeaponDefinition->ProjectileClass,
			SpawnTransform,
			AvatarActor,
			Cast<APawn>(AvatarActor),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(Projectile))
	{
		return false;
	}

	Projectile->InitializeProjectile(
		AbilitySystem,
		ImpactEffectSpecs,
		GetBreakableDamageAmount(),
		WorldImpactData,
		GetSourceTeamId(),
		WeaponDefinition,
		GetMaxAttackDistance(),
		WeaponDefinition->FalloffSettings);

	/*
	 * FinishSpawningActor -> BeginPlay 전에 ballistic velocity를 저장한다.
	 * Cannon처럼 BeginPlay에서 ProjectileMovement 설정을 바꾸는 자식도
	 * Super::BeginPlay()에서 이 속도를 최종 발사 속도로 사용한다.
	 */
	Projectile->SetInitialLaunchVelocity(
		SafeLaunchVelocity);

	UGameplayStatics::FinishSpawningActor(
		Projectile,
		SpawnTransform);

	return true;
}

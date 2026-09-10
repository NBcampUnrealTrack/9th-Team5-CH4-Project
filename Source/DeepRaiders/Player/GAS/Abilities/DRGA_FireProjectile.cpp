#include "DRGA_FireProjectile.h"

#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"
#include "HAL/IConsoleManager.h"

namespace DRProjectileAim
{
	constexpr float MinAimDistance = 1.0f;
	constexpr float MaxClientViewLocationError = 500.0f;
	constexpr float MuzzleObstructionTraceDistance = 100.0f;
}

namespace DRLocalProjectilePrediction
{
	TAutoConsoleVariable<int32> CVarShowLocalPredicted(
		TEXT("dr.Projectile.ShowLocalPredicted"),
		0,
		TEXT("Show owning-client local predicted projectile. 0=off, 1=on."));

	TAutoConsoleVariable<float> CVarLifetimeSeconds(
		TEXT("dr.Projectile.LocalVisualLifetime"),
		0.0f,
		TEXT("Local predicted projectile lifetime. 0=auto from range/speed, >0=manual seconds."));

	TAutoConsoleVariable<int32> CVarDebug(
		TEXT("dr.Projectile.LocalVisualDebug"),
		0,
		TEXT("Log local projectile prediction/reconciliation. 0=off, 1=on."));
}

bool UDRGA_FireProjectile::IsAttackConfigurationValid(
	const UDRProjectileWeaponItemDefinition* WeaponDefinition) const
{
	return Super::IsAttackConfigurationValid(WeaponDefinition)
		&& WeaponDefinition->ProjectileClass != nullptr
		&& WeaponDefinition->InitialSpeed > 0.f
		&& WeaponDefinition->ProjectileScaleMultiplier > 0.f;
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

	// 기존 탄도 조준용 허공 교차 거리 보정. 단순 직선 발사 정책에서는 카메라 Trace 끝점을 그대로 사용한다.
#if 0
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
#endif

	const FVector AimPoint = CameraHit.bBlockingHit ? FVector(CameraHit.ImpactPoint) : FVector(CameraHit.TraceEnd);

	if (AimPoint.ContainsNaN())
	{
		return false;
	}

	FHitResult ShotAimHit = CameraHit;
	ShotAimHit.TraceStart = ViewLocation;
	ShotAimHit.TraceEnd = AimPoint;
	ShotAimHit.Location = AimPoint;
	ShotAimHit.ImpactPoint = AimPoint;
	ShotAimHit.Component = nullptr;

	/*
	 * 실제 Projectile Spawn과 Presentation 모두 Character의 GameplayFireAnchor를
	 * 캐릭터 전방으로 이동한 고정 원점으로 사용한다.
	 */
	FVector GameplayFireOrigin;
	if (!ResolveGameplayFireOrigin(AvatarActor->GetActorForwardVector(), GameplayFireOrigin))
	{
		return false;
	}

	FVector ProjectileAimPoint;
	if (!ResolveProjectileAimPoint(GameplayFireOrigin, AimPoint, ProjectileAimPoint))
	{
		return false;
	}

	++LocalShotSequence;
	if (LocalShotSequence == 0)
	{
		++LocalShotSequence;
	}

	const uint32 ShotSequence = LocalShotSequence;

	if (!ActorInfo->IsNetAuthority())
	{
		PlayLocalFirePresentation(GameplayFireOrigin, ProjectileAimPoint);
		TrySpawnLocalVisualProjectile(
			GameplayFireOrigin,
			ProjectileAimPoint,
			ShotSequence);
	}

	FGameplayAbilityTargetDataHandle TargetData(
		new FDRGameplayAbilityTargetData_ProjectileShot(
			ShotAimHit,
			ShotSequence));

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
	uint32 ShotSequence = 0;
	const bool bValidAim = ValidateServerShotTargetData(
		TargetData,
		AimPoint,
		AimDirection,
		ShotSequence);

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
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[ProjectilePrediction][SERVER_REJECT] "
				"Shot=%u Reason=AimValidation"),
			ShotSequence);

		return;
	}

	if (!TryCommitServerShot())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[ProjectilePrediction][SERVER_REJECT] "
				"Shot=%u Reason=Commit"),
			ShotSequence);

		return;
	}

	if (ExecuteServerProjectileShot(
		AimPoint,
		AimDirection,
		ShotSequence))
	{
		ApplyHeatForSuccessfulShot();
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[ProjectilePrediction][SERVER_REJECT] "
				"Shot=%u Reason=ProjectileSpawn"),
			ShotSequence);
	}
}

bool UDRGA_FireProjectile::ValidateServerShotTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutAimPoint,
	FVector& OutAimDirection,
	uint32& OutShotSequence) const
{
	OutAimPoint = FVector::ZeroVector;
	OutAimDirection = FVector::ZeroVector;
	OutShotSequence = 0;

	if (TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	if (Data == nullptr
		|| Data->GetScriptStruct()
			!= FDRGameplayAbilityTargetData_ProjectileShot::StaticStruct())
	{
		return false;
	}

	const auto* ShotData =
		static_cast<const FDRGameplayAbilityTargetData_ProjectileShot*>(Data);

	const FHitResult* AimHit = ShotData->GetHitResult();
	if (AimHit == nullptr || ShotData->ShotSequence == 0)
	{
		return false;
	}

	OutShotSequence = ShotData->ShotSequence;

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

	// if (AimDistance < DRProjectileAim::MinAimDistance
	// 	|| AimDistance > MaxAttackDistance + DRProjectileAim::MaxClientViewLocationError)
	// {
	// 	return false;
	// }

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
	const FVector& AimDirection,
	uint32 ShotSequence)
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
	 * Projectile은 GameplayFireAnchor에서 Character 전방으로 이동한 고정 원점에서 출발한다.
	 * AimDirection은 클라이언트 발사 순간의 Crosshair 방향이며,
	 * 실제 발사 방향은 GameplayFireOrigin에서 보정된 AimPoint를 향하도록 계산한다.
	 */
	FVector GameplayFireOrigin;
	if (!ResolveGameplayFireOrigin(AvatarActor->GetActorForwardVector(), GameplayFireOrigin))
	{
		return false;
	}

	FVector ProjectileAimPoint;
	if (!ResolveProjectileAimPoint(GameplayFireOrigin, AimPoint, ProjectileAimPoint))
	{
		return false;
	}

	FVector BaseLaunchVelocity;
	if (!ResolveProjectileLaunchVelocity(
		GameplayFireOrigin,
		ProjectileAimPoint,
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
			ImpactEffectSpecs,
			ShotSequence))
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
		ProjectileAimPoint);

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

	const float LaunchSpeed = FMath::Max(WeaponDefinition->InitialSpeed, 1.f);
	const FVector DirectDirection = (AimPoint - SpawnLocation).GetSafeNormal();
	if (DirectDirection.IsNearlyZero())
	{
		return false;
	}

	OutLaunchVelocity = DirectDirection * LaunchSpeed;
	return true;

	// 기존 ProjectileClass 속도와 중력 보정 탄도 계산. 단순 직선 발사 정책에서는 사용하지 않는다.
#if 0
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

	UGameplayStatics::FSuggestProjectileVelocityParameters Params(
		this,
		SpawnLocation,
		AimPoint,
		LaunchSpeed);

	Params.bFavorHighArc = false;
	Params.CollisionRadius = 0.0f;
	Params.OverrideGravityZ = OverrideGravityZ;
	Params.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
	Params.bDrawDebug = false;
	Params.bAcceptClosestOnNoSolutions = true;

	if (UGameplayStatics::SuggestProjectileVelocity(
		Params,
		OutLaunchVelocity)
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
#endif
}

bool UDRGA_FireProjectile::ResolveProjectileAimPoint(const FVector& FireOrigin, const FVector& CameraAimPoint,
	FVector& OutAimPoint) const
{
	OutAimPoint = CameraAimPoint;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor) || FireOrigin.ContainsNaN() || CameraAimPoint.ContainsNaN())
	{
		return false;
	}

	const FVector CharacterForward = AvatarActor->GetActorForwardVector().GetSafeNormal();
	if (CharacterForward.IsNearlyZero())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const FVector ObstructionTraceEnd =
		FireOrigin + CharacterForward * DRProjectileAim::MuzzleObstructionTraceDistance;
	FCollisionQueryParams QueryParams;
	BuildWeaponTraceQueryParams(QueryParams);

	FHitResult ObstructionHit;
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		ObstructionHit, FireOrigin, ObstructionTraceEnd, DRCollisionChannels::Projectile, QueryParams);
	if (bBlockingHit)
	{
		OutAimPoint = ObstructionHit.ImpactPoint;
	}

	return !OutAimPoint.ContainsNaN() && !OutAimPoint.Equals(FireOrigin, KINDA_SMALL_NUMBER);
}

void UDRGA_FireProjectile::TrySpawnLocalVisualProjectile(
	const FVector& SpawnLocation,
	const FVector& AimPoint,
	uint32 ShotSequence)
{
	if (DRLocalProjectilePrediction::CVarShowLocalPredicted.GetValueOnGameThread() == 0
		|| ShotSequence == 0)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr
		|| ActorInfo->IsNetAuthority()
		|| !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		GetCurrentWeaponDefinition();

	if (!IsValid(WeaponDefinition)
		|| !WeaponDefinition->ProjectileClass)
	{
		return;
	}

	/*
	 * 현재 reconciliation은 Rifle 검증 단계다.
	 * 1발 + 무산포만 local predicted proxy를 만든다.
	 */
	if (GetWeaponProjectileCount() != 1
		|| WeaponDefinition->SpreadHalfAngleDegrees > KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector LaunchVelocity;
	if (!ResolveProjectileLaunchVelocity(
			SpawnLocation,
			AimPoint,
			LaunchVelocity)
		|| LaunchVelocity.ContainsNaN()
		|| LaunchVelocity.IsNearlyZero())
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	if (!IsValid(World) || !IsValid(AvatarActor))
	{
		return;
	}

	const FVector LaunchDirection = LaunchVelocity.GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		return;
	}

	const float ConfiguredLifetime =
		DRLocalProjectilePrediction::CVarLifetimeSeconds.GetValueOnGameThread();

	const float ProjectileSpeed = FMath::Max(LaunchVelocity.Size(), 1.f);
	const float FallDuration = WeaponDefinition->FlightSettings.Mode == EDRProjectileFlightMode::CruiseThenFall
		? WeaponDefinition->FlightSettings.HorizontalDecelerationDuration
		: 0.f;
	const float AutoLifetime = FMath::Clamp(
		GetMaxAttackDistance() / ProjectileSpeed + FallDuration + 1.f,
		0.15f,
		5.0f);

	const float LifetimeSeconds =
		ConfiguredLifetime > KINDA_SMALL_NUMBER
			? FMath::Clamp(ConfiguredLifetime, 0.02f, 5.0f)
			: AutoLifetime;

	const FTransform SpawnTransform(
		LaunchDirection.Rotation(),
		SpawnLocation);

	ADRProjectile* VisualProjectile =
		World->SpawnActorDeferred<ADRProjectile>(
			WeaponDefinition->ProjectileClass,
			SpawnTransform,
			AvatarActor,
			Cast<APawn>(AvatarActor),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(VisualProjectile))
	{
		return;
	}

	/*
	 * InitializeProjectile()는 절대 호출하지 않는다.
	 * 이 인스턴스에는 ASC / EffectSpec / Snow / Team gameplay data가 없다.
	 */
	VisualProjectile->ConfigureWeaponLaunch(
		LaunchVelocity, WeaponDefinition->InitialSpeed, WeaponDefinition->ProjectileScaleMultiplier,
		GetMaxAttackDistance(), WeaponDefinition->FlightSettings, WeaponDefinition->FalloffSettings);
	VisualProjectile->ConfigureAsLocalVisualProjectile(LaunchVelocity, LifetimeSeconds, ShotSequence);

	UGameplayStatics::FinishSpawningActor(
		VisualProjectile,
		SpawnTransform);

	if (DRLocalProjectilePrediction::CVarDebug.GetValueOnGameThread() != 0)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[ProjectilePrediction][LOCAL_SPAWN] Shot=%u Weapon=%s Projectile=%s Origin=%s Velocity=%s Lifetime=%.3f"),
			ShotSequence,
			*GetNameSafe(WeaponDefinition),
			*GetNameSafe(VisualProjectile),
			*SpawnLocation.ToCompactString(),
			*LaunchVelocity.ToCompactString(),
			LifetimeSeconds);
	}
}

bool UDRGA_FireProjectile::SpawnProjectile(
	const FVector& SpawnLocation,
	const FVector& LaunchVelocity,
	AActor* AvatarActor,
	UAbilitySystemComponent* AbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs,
	uint32 ShotSequence)
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
	const float SnowAddAmountMultiplier = FMath::Max(0.f, AbilitySystem->GetNumericAttribute(
		UDRPlayerAttributeSet::GetWeaponSnowAddAmountMultiplierAttribute()));
	WorldImpactData.SnowAmount = SnowAddSettings.Amount * SnowAddAmountMultiplier;
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

	Projectile->SetShotSequence(ShotSequence);
	Projectile->ConfigureWeaponLaunch(
		SafeLaunchVelocity, WeaponDefinition->InitialSpeed, WeaponDefinition->ProjectileScaleMultiplier,
		GetMaxAttackDistance(), WeaponDefinition->FlightSettings, WeaponDefinition->FalloffSettings);

	Projectile->InitializeProjectile(
		AbilitySystem,
		ImpactEffectSpecs,
		GetBreakableDamageAmount(),
		WorldImpactData,
		GetSourceTeamId(),
		WeaponDefinition,
		GetMaxAttackDistance(),
		WeaponDefinition->FalloffSettings);

	UGameplayStatics::FinishSpawningActor(
		Projectile,
		SpawnTransform);

	return true;
}

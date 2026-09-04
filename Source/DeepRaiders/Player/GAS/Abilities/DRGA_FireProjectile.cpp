
#include "DRGA_FireProjectile.h"

#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

bool UDRGA_FireProjectile::IsAttackConfigurationValid(const UDRProjectileWeaponItemDefinition* WeaponDefinition) const
{
	return Super::IsAttackConfigurationValid(WeaponDefinition) && WeaponDefinition->ProjectileClass != nullptr;
}

void UDRGA_FireProjectile::OnRangedWeaponActivated()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	// 서버에서
	if (ActorInfo != nullptr
		&& ActorInfo->IsNetAuthority()
		&& !ActorInfo->IsLocallyControlled())
	{
		// GA를 활성화하며 InputPressed 이벤트 Delegate 연결
		RegisterServerShotDelegate();
	}
}

void UDRGA_FireProjectile::OnRangedWeaponEnded()
{
	// GA를 종료하며 InputPressed 이벤트 Delegate 해제
	UnregisterServerShotDelegate();
}

// 로컬 플레이어의 발사 요청 처리
bool UDRGA_FireProjectile::SendLocalShotRequest()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr
		|| !ActorInfo->IsLocallyControlled())
	{
		return false;
	}
	
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}
	
	FHitResult CameraHit;
	if (!TraceCameraAim(ViewLocation, ViewRotation.Vector(),CameraHit))
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!IsValid(AvatarActor))
	{
		return false;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	// 원거리 무기 공통 캐릭터 로컬 StartOffset에서 눈총을 발사한다.
	const FVector GameplayFireOrigin = AvatarActor->GetActorLocation()
		+ AvatarActor->GetActorTransform().TransformVectorNoScale(
			WeaponDefinition->StartOffset);

	const FVector AimPoint = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraHit.TraceEnd;
	if (!ActorInfo->IsNetAuthority())
	{
		PlayLocalFirePresentation(GameplayFireOrigin, AimPoint);
	}

	if (ActorInfo->IsNetAuthority())
	{
		HandleServerShotRequest();
		return true;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(AbilitySystem))
	{
		return false;
	}
	
	// 부모 GA에서 TryRequestLocalShot() 함수가 호출되고 내부에서 생성한 PredictionKey가 공유됨
	AbilitySystem->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
		GetCurrentAbilitySpecHandle(),GetCurrentActivationInfo().GetActivationPredictionKey(),
		AbilitySystem->ScopedPredictionKey);

	return true;
}

void UDRGA_FireProjectile::RegisterServerShotDelegate()
{
	if (ServerShotDelegateHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();

	if (!IsValid(AbilitySystem))
	{
		return;
	}

	const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();

	const FPredictionKey PredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey();

	ServerShotDelegateHandle = AbilitySystem->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed,
			SpecHandle,PredictionKey).AddUObject(this,&ThisClass::HandleServerShotRequest);

	AbilitySystem->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputPressed,SpecHandle,PredictionKey);
}

void UDRGA_FireProjectile::UnregisterServerShotDelegate()
{
	if (!ServerShotDelegateHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed,	
			GetCurrentAbilitySpecHandle(),
			GetCurrentActivationInfo().GetActivationPredictionKey()).Remove(ServerShotDelegateHandle);
	}

	ServerShotDelegateHandle.Reset();
}

void UDRGA_FireProjectile::HandleServerShotRequest()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}
	
	if (!ActorInfo->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get())
		{
			AbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
				GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
	
	// CommitAbility 시도
	if (!TryCommitServerShot())
	{
		return;
	}
	
	// 투사체 발사
	ExecuteServerProjectileShot();
}

bool UDRGA_FireProjectile::ExecuteServerProjectileShot()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	FHitResult CameraHit;
	if (!TraceCameraAim(ViewLocation, ViewRotation.Vector(), CameraHit))
	{
		return false;
	}

	const FVector AimPoint = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraHit.TraceEnd;

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!IsValid(AvatarActor))
	{
		return false;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	// 원거리 무기 공통 캐릭터 로컬 StartOffset에서 눈총을 발사한다.
	const FVector GameplayFireOrigin = AvatarActor->GetActorLocation()
		+ AvatarActor->GetActorTransform().TransformVectorNoScale(
			WeaponDefinition->StartOffset);

	const FVector BaseProjectileDirection = WeaponDefinition->ResolveCameraAimDirection(
		ViewRotation.Vector(),
		GameplayFireOrigin,
		AimPoint);

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();	
	if (!IsValid(AvatarActor) || !IsValid(AbilitySystem))
	{
		return false;
	}

	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);

	const int32 SafeProjectileCount = FMath::Max(WeaponDefinition->ProjectileCount, 1);
	const float SpreadRadians = FMath::DegreesToRadians(FMath::Max(WeaponDefinition->SpreadHalfAngleDegrees, 0.f));
	bool bSpawnedAnyProjectile = false;
	for (int32 ProjectileIndex = 0; ProjectileIndex < SafeProjectileCount; ++ProjectileIndex)
	{
		FVector ProjectileDirection = BaseProjectileDirection;

		if (SpreadRadians > KINDA_SMALL_NUMBER)
		{
			ProjectileDirection = FMath::VRandCone(BaseProjectileDirection, SpreadRadians);
		}

		if (SpawnProjectile(GameplayFireOrigin, ProjectileDirection, AvatarActor, AbilitySystem, ImpactEffectSpecs))
		{
			bSpawnedAnyProjectile = true;
		}
	}

	if (!bSpawnedAnyProjectile)
	{
		return false;
	}

	PlayServerFirePresentation(GameplayFireOrigin, AimPoint);

	return true;
}

bool UDRGA_FireProjectile::SpawnProjectile(
	const FVector& SpawnLocation,
	const FVector& ProjectileDirection,
	AActor* AvatarActor,
	UAbilitySystemComponent* AbilitySystem,
	const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs)
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	
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

	const FVector SafeDirection = ProjectileDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return false;
	}

	const FTransform SpawnTransform(SafeDirection.Rotation(), SpawnLocation);

	const FDRProjectileWeaponSnowAddSettings& SnowAddSettings = WeaponDefinition->SnowAddSettings;
	FDRProjectileWorldImpactData WorldImpactData;
	WorldImpactData.bAddSnow = SnowAddSettings.bEnabled;
	WorldImpactData.SnowRadius = SnowAddSettings.Radius;
	WorldImpactData.SnowAmount = SnowAddSettings.Amount;
	WorldImpactData.SnowEditTool = SnowAddSettings.EditTool;
	WorldImpactData.bAllowVirtualSurfaceFallback = SnowAddSettings.bAllowVirtualSurfaceFallback;

	ADRProjectile* Projectile = World->SpawnActorDeferred<ADRProjectile>(WeaponDefinition->ProjectileClass,
		SpawnTransform, AvatarActor, Cast<APawn>(AvatarActor), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

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

	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);

	return true;
}

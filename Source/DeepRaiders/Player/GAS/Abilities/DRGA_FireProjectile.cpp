
#include "DRGA_FireProjectile.h"

#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayPrediction.h"
#include "Kismet/GameplayStatics.h"

bool UDRGA_FireProjectile::IsAttackConfigurationValid() const
{
	return Super::IsAttackConfigurationValid() && ProjectileClass != nullptr;
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

	FVector MuzzleLocation;

	if (!ResolveMuzzleLocation(ViewRotation.Vector(),MuzzleLocation))
	{
		return false;
	}
	
	const FVector AimPoint = CameraHit.bBlockingHit	? CameraHit.ImpactPoint	: CameraHit.TraceEnd;
	if (!ActorInfo->IsNetAuthority())
	{
		PlayLocalFirePresentation(MuzzleLocation,AimPoint);
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

	FScopedPredictionWindow PredictionWindow(AbilitySystem,true);

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
		AbilitySystem->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed,	GetCurrentAbilitySpecHandle(),
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
	
	FVector MuzzleLocation;
	
	if (!ResolveMuzzleLocation(ViewRotation.Vector(), MuzzleLocation))
	{
		return false;
	}
	
	FVector ProjectileDirection = AimPoint - MuzzleLocation;
	if (!ProjectileDirection.Normalize())
	{
		ProjectileDirection = ViewRotation.Vector();
	}
	
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	UWorld* World = GetWorld();
	
	if (!IsValid(AvatarActor)
		|| !IsValid(AbilitySystem)
		|| !IsValid(World))
	{
		return false;
	}
	
	const FTransform SpawnTransform(ProjectileDirection.Rotation(), MuzzleLocation);

	ADRProjectile* Projectile =	World->SpawnActorDeferred<ADRProjectile>(ProjectileClass, SpawnTransform, AvatarActor,
			Cast<APawn>(AvatarActor), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(Projectile))
	{
		return false;
	}

	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);

	Projectile->InitializeProjectile(AbilitySystem, ImpactEffectSpecs, WorldImpactData,
		GetImpactGameplayCueTag(), GetSourceTeamId());

	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);

	PlayServerFirePresentation(MuzzleLocation, AimPoint);

	return true;	
}



















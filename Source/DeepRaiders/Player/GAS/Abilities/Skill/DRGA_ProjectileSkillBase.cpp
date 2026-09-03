#include "DRGA_ProjectileSkillBase.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"

#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

bool UDRGA_ProjectileSkillBase::ResolveProjectileLaunch(
	const FGameplayAbilityActorInfo* ActorInfo,
	float MaxAimDistance,
	FVector& OutSpawnLocation,
	FVector& OutProjectileDirection) const
{
	OutSpawnLocation = FVector::ZeroVector;
	OutProjectileDirection = FVector::ZeroVector;

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	AController* Controller = ActorInfo != nullptr ? ActorInfo->PlayerController.Get() : nullptr;
	UWorld* World = IsValid(Character) ? Character->GetWorld() : nullptr;
	if (!IsValid(Character)
		|| !IsValid(Controller)
		|| !IsValid(World)
		|| MaxAimDistance <= 0.f)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero()
		|| !Character->CalculateSkillFireOrigin(OutSpawnLocation))
	{
		return false;
	}

	const FVector TraceEnd = ViewLocation + ViewDirection * MaxAimDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRProjectileSkillAim), false, Character);
	FHitResult AimHit;
	const bool IsBlockingHit = World->LineTraceSingleByChannel(
		AimHit,
		ViewLocation,
		TraceEnd,
		DRCollisionChannels::Projectile,
		QueryParams);
	const FVector AimPoint = IsBlockingHit ? AimHit.ImpactPoint : TraceEnd;
	const FVector AimDirection = (AimPoint - OutSpawnLocation).GetSafeNormal();

	OutProjectileDirection = FVector::DotProduct(ViewDirection, AimDirection) > 0.f
		? AimDirection
		: ViewDirection;

	return !OutProjectileDirection.IsNearlyZero();
}

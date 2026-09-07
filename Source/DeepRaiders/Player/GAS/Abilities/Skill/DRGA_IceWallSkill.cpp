#include "DRGA_IceWallSkill.h"

#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTargetActor.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/IceWall/DRIceWall.h"
#include "DeepRaiders/Skill/IceWall/DRIceWallSegment.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDRGA_IceWallSkill::UDRGA_IceWallSkill()
{
	FGameplayTagContainer IceWallAbilityTags;
	IceWallAbilityTags.AddTag(DRGameplayTags::Ability_Skill);
	IceWallAbilityTags.AddTag(DRGameplayTags::Ability_Skill_IceWall);
	SetAssetTags(IceWallAbilityTags);

	TargetActorClass = ADRPlacementTargetActor::StaticClass();
	IceWallClass = ADRIceWall::StaticClass();
}

void UDRGA_IceWallSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsValid(GetPlayerCharacter(ActorInfo)) || !TargetActorClass || !IceWallClass || !SegmentClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartTargeting();
}

void UDRGA_IceWallSkill::StartTargeting()
{
	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(this, TEXT("IceWallTargetData"),
		EGameplayTargetingConfirmation::UserConfirmed, TargetActorClass);
	if (!IsValid(TargetDataTask))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCanceled);
	TargetDataTask->ReadyForActivation();

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (!TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		return;
	}

	ADRPlacementTargetActor* PlacementTargetActor = Cast<ADRPlacementTargetActor>(SpawnedTargetActor);
	if (!IsValid(PlacementTargetActor))
	{
		if (IsValid(SpawnedTargetActor))
		{
			SpawnedTargetActor->Destroy();
		}
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	PlacementTargetActor->Configure(PlacementSettings, PreviewActorClass, WallDimensions);
	TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
}

void UDRGA_IceWallSkill::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive() || TargetData.Num() != 1)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		FTransform WallTransform;
		if (!ValidateServerTargetData(TargetData, WallTransform) || !CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		const ADRPlayerState* PlayerState = Cast<APawn>(ActorInfo->AvatarActor.Get())
			? Cast<APawn>(ActorInfo->AvatarActor.Get())->GetPlayerState<ADRPlayerState>()
			: nullptr;
		UWorld* World = GetWorld();
		ADRIceWall* IceWall = IsValid(World) && IsValid(PlayerState)
			? World->SpawnActorDeferred<ADRIceWall>(IceWallClass, WallTransform, ActorInfo->AvatarActor.Get(),
				Cast<APawn>(ActorInfo->AvatarActor.Get()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!IsValid(IceWall))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		IceWall->ConfigureWall(PlayerState->GetTeamId(), WallDimensions, SegmentCount,
			RiseDuration, WallLifeSpan, SegmentMaxHealth, SegmentClass);
		IceWall->FinishSpawning(WallTransform);
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}

	if (ActorInfo->IsLocallyControlled() && !CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
	}
}

void UDRGA_IceWallSkill::HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
	}
}

bool UDRGA_IceWallSkill::ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData,
	FTransform& OutWallTransform) const
{
	OutWallTransform = FTransform::Identity;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* ClientHit = Data != nullptr ? Data->GetHitResult() : nullptr;
	APlayerController* PlayerController = ActorInfo != nullptr ? ActorInfo->PlayerController.Get() : nullptr;
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = GetWorld();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority() || ClientHit == nullptr || !IsValid(PlayerController)
		|| !IsValid(AvatarActor) || !IsValid(World))
	{
		return false;
	}

	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	PlayerController->GetPlayerViewPoint(ServerViewLocation, ServerViewRotation);
	if (FVector::Dist(ServerViewLocation, ClientHit->TraceStart) > PlacementSettings.ServerViewOriginTolerance)
	{
		return false;
	}

	const FVector ClientAimDirection = (ClientHit->TraceEnd - ClientHit->TraceStart).GetSafeNormal();
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(PlacementSettings.ServerAimAngleTolerance, 0.f, 90.f)));
	if (ClientAimDirection.IsNearlyZero() || FVector::DotProduct(ClientAimDirection, ServerViewRotation.Vector()) < MinimumDot)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRIceWallServerAim), false);
	QueryParams.AddIgnoredActor(AvatarActor);
	FHitResult ServerHit;
	const FVector TraceEnd = ServerViewLocation + ClientAimDirection * PlacementSettings.MaxDistance;
	if (!World->LineTraceSingleByChannel(ServerHit, ServerViewLocation, TraceEnd, PlacementSettings.AimTraceChannel, QueryParams)
		|| !ADRPlacementTargetActor::IsValidPlacementSurface(ServerHit, PlacementSettings))
	{
		return false;
	}

	OutWallTransform = MakeWallTransform(ServerHit.ImpactPoint, ServerViewRotation);
	return true;
}

FTransform UDRGA_IceWallSkill::MakeWallTransform(const FVector& ImpactPoint, const FRotator& ViewRotation) const
{
	FVector AimDirection = ViewRotation.Vector().GetSafeNormal2D();
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = FVector::ForwardVector;
	}

	const FVector WallLengthDirection = FVector::CrossProduct(FVector::UpVector, AimDirection).GetSafeNormal();
	const FQuat WallRotation = FRotationMatrix::MakeFromXZ(WallLengthDirection, FVector::UpVector).ToQuat();
	return FTransform(WallRotation, ImpactPoint + FVector::UpVector * (WallDimensions.Z * 0.5f));
}

void UDRGA_IceWallSkill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (IsValid(TargetDataTask))
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

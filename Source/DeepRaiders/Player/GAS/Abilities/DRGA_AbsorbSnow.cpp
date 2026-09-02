// Fill out your copyright notice in the Description page of Project Settings.

#include "DRGA_AbsorbSnow.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace DRAbsorbSnow
{
	constexpr float CameraAimCorrectionMinDistance = 100.0f;
}

UDRGA_AbsorbSnow::UDRGA_AbsorbSnow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = true;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(DRGameplayTags::Ability_Snow_Absorb);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);
	
	ActivationOwnedTags.AddTag(DRGameplayTags::State_Absorbing);
}

void UDRGA_AbsorbSnow::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UDRSnowRemoveComponent* SnowRemoveComponent =
		IsValid(AvatarActor) ? AvatarActor->FindComponentByClass<UDRSnowRemoveComponent>() : nullptr;

	if (!IsValid(ASC) || !IsValid(AvatarActor) || !IsValid(SnowRemoveComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	PerformAbsorbTick();
}

void UDRGA_AbsorbSnow::HandleAbsorbDelayFinished()
{
	PerformAbsorbTick();
}

void UDRGA_AbsorbSnow::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDRGA_AbsorbSnow::PerformAbsorbTick()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	if (!IsValid(ASC) || !IsValid(AvatarActor))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	FDRSnowRemovalSpec RemovalSpec;
	if (!BuildRemovalSpec(RemovalSpec))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	UDRSnowRemoveComponent* SnowRemoveComponent = AvatarActor->FindComponentByClass<UDRSnowRemoveComponent>();
	if (!IsValid(SnowRemoveComponent))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (AController* Controller = ActorInfo->PlayerController.Get())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		AvatarActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	// 흡수 프러스텀은 무기 투사체 스폰점이 아닌 캐릭터 원점에서 시작한다.
	const FVector AbsorbOrigin = AvatarActor->GetActorLocation();

	const UDRRangedWeaponDefinition* WeaponDefinition = Cast<UDRRangedWeaponDefinition>(
		GetSourceObject(GetCurrentAbilitySpecHandle(), ActorInfo));
	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	const FVector AbsorbFrustumOrigin = AbsorbOrigin + AvatarActor->GetActorTransform().TransformVectorNoScale(
		RemovalSpec.SnowAbsorbStartOffset);
	FVector AbsorbDirection = ViewDirection;

	if (IsValid(WeaponDefinition)
		&& WeaponDefinition->SnowAbsorbSettings.bUseCameraAimCorrection)
	{
		constexpr float CameraAimTraceDistance = 10000.0f;
		const FVector CameraTraceEnd = ViewLocation + ViewDirection * CameraAimTraceDistance;
		FCollisionQueryParams CameraQueryParams(SCENE_QUERY_STAT(DRAbsorbCameraAim), false);
		CameraQueryParams.AddIgnoredActor(AvatarActor);

		FHitResult CameraHit;
		const bool bCameraHit = GetWorld()->LineTraceSingleByChannel(
			CameraHit, ViewLocation, CameraTraceEnd, DRCollisionChannels::Projectile, CameraQueryParams);
		const FVector CameraAimPoint = bCameraHit ? CameraHit.ImpactPoint : CameraTraceEnd;
		const float CameraAimDistance = FVector::Distance(AbsorbFrustumOrigin, CameraAimPoint);

		if (CameraAimDistance >= DRAbsorbSnow::CameraAimCorrectionMinDistance)
		{
			const FVector CameraAimDirection = (CameraAimPoint - AbsorbFrustumOrigin).GetSafeNormal();
			if (!CameraAimDirection.IsNearlyZero())
			{
				const float DirectionDot = FMath::Clamp(
					FVector::DotProduct(ViewDirection, CameraAimDirection), -1.0f, 1.0f);
				const float CorrectionAngleRadians = FMath::Acos(DirectionDot);
				const float MaxCorrectionAngleRadians = FMath::DegreesToRadians(
					FMath::Clamp(WeaponDefinition->SnowAbsorbSettings.MaxCameraAimCorrectionAngleDegrees, 0.0f, 90.0f));

				if (CorrectionAngleRadians <= MaxCorrectionAngleRadians)
				{
					AbsorbDirection = CameraAimDirection;
				}
				else if (CorrectionAngleRadians > KINDA_SMALL_NUMBER && MaxCorrectionAngleRadians > 0.0f)
				{
					const FQuat CorrectionRotation = FQuat::FindBetweenNormals(ViewDirection, CameraAimDirection);
					AbsorbDirection = FQuat::Slerp(
						FQuat::Identity,
						CorrectionRotation,
						MaxCorrectionAngleRadians / CorrectionAngleRadians).RotateVector(ViewDirection).GetSafeNormal();
				}
			}
		}
	}

	// 캐릭터 로컬 StartOffset을 먼저 적용해 시작점을 고정한다.
	// 이후에만 보정된 AbsorbDirection을 적용해 프러스텀 축이 시작점 주위를 회전하게 한다.
	FDRSnowRemovalSpec EffectiveRemovalSpec = RemovalSpec;
	EffectiveRemovalSpec.SnowAbsorbStartOffset = FVector::ZeroVector;

	const float RemovedAmount =
		SnowRemoveComponent->TryRemoveSnowAlongDirection(
			AbsorbFrustumOrigin, AbsorbDirection, EffectiveRemovalSpec);
	
	ApplySnowGaugeGain(ASC, RemovedAmount);

	ScheduleNextAbsorbTick();
}

void UDRGA_AbsorbSnow::ScheduleNextAbsorbTick()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;

	if (!IsValid(ASC) || !IsValid(AvatarActor))
	{
		return;
	}

	FDRSnowRemovalSpec RemovalSpec;
	if (!BuildRemovalSpec(RemovalSpec))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	const float AbsorbSpeed = FMath::Max(0.f, RemovalSpec.SnowAbsorbSpeed);
	if (AbsorbSpeed <= UE_SMALL_NUMBER)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	UAbilityTask_WaitDelay* AbsorbDelayTask =
		UAbilityTask_WaitDelay::WaitDelay(this, 1.f / AbsorbSpeed);
	if (!IsValid(AbsorbDelayTask))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	AbsorbDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleAbsorbDelayFinished);
	AbsorbDelayTask->ReadyForActivation();
}

bool UDRGA_AbsorbSnow::BuildRemovalSpec(FDRSnowRemovalSpec& OutRemovalSpec) const
{
	const UDRRangedWeaponDefinition* WeaponDefinition =
		Cast<UDRRangedWeaponDefinition>(
			GetSourceObject(
				GetCurrentAbilitySpecHandle(),
				GetCurrentActorInfo()));
	if (!IsValid(WeaponDefinition) || !WeaponDefinition->SnowAbsorbSettings.bEnabled)
	{
		return false;
	}

	const FDRProjectileWeaponSnowAbsorbSettings& SnowAbsorbSettings = WeaponDefinition->SnowAbsorbSettings;
	OutRemovalSpec.SnowAbsorbPower = SnowAbsorbSettings.Power;
	OutRemovalSpec.SnowAbsorbRadius = SnowAbsorbSettings.Radius;
	OutRemovalSpec.SnowAbsorbSpeed = SnowAbsorbSettings.Speed;
	OutRemovalSpec.SnowAbsorbRange = SnowAbsorbSettings.Range;
	OutRemovalSpec.SnowAbsorbStartOffset = SnowAbsorbSettings.StartOffset;
	OutRemovalSpec.SnowAbsorbSweepRadius = SnowAbsorbSettings.SweepRadius;
	OutRemovalSpec.SnowAbsorbMaxSweepsPerTick = SnowAbsorbSettings.MaxSweepsPerTick;
	OutRemovalSpec.bUseAdaptiveAbsorbQuery = SnowAbsorbSettings.bUseAdaptiveQuery;
	OutRemovalSpec.SnowAbsorbInnerRadiusRatio = SnowAbsorbSettings.InnerRadiusRatio;
	OutRemovalSpec.RemovalBrushShape = SnowAbsorbSettings.BrushShape;
	OutRemovalSpec.RemovalMode = SnowAbsorbSettings.RemovalMode;

	return OutRemovalSpec.SnowAbsorbPower > 0.f &&
		OutRemovalSpec.SnowAbsorbRadius > 0.f &&
		OutRemovalSpec.SnowAbsorbSpeed > 0.f &&
		OutRemovalSpec.SnowAbsorbRange > 0.f;
}

void UDRGA_AbsorbSnow::ApplySnowGaugeGain(
	UAbilitySystemComponent* AbilitySystemComponent,
	float RemovedAmount) const
{
	if (!IsValid(AbilitySystemComponent) || RemovedAmount <= 0.f)
	{
		return;
	}

	if (!SnowGainEffectClass)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	FGameplayEffectSpecHandle EffectSpec = MakeOutgoingGameplayEffectSpec(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		SnowGainEffectClass,
		GetAbilityLevel());

	if (!EffectSpec.IsValid())
	{
		return;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Snow_Amount, RemovedAmount);
	ApplyGameplayEffectSpecToOwner(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		EffectSpec);
}

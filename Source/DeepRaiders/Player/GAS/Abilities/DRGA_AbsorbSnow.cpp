// Fill out your copyright notice in the Description page of Project Settings.

#include "DRGA_AbsorbSnow.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UDRGA_AbsorbSnow::UDRGA_AbsorbSnow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = true;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(DRGameplayTags::Ability_Snow_Absorb);
	SetAssetTags(AssetTags);

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

	FVector AbsorbOrigin = AvatarActor->GetActorLocation();
	if (const ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(AvatarActor))
	{
		if (const UStaticMeshComponent* WeaponMesh = Character->GetWorldHandEquipmentMesh())
		{
			AbsorbOrigin = WeaponMesh->GetComponentLocation();
		}
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

	const float RemovedAmount = SnowRemoveComponent->TryRemoveSnowAlongDirection(
		AbsorbOrigin,
		ViewRotation.Vector(),
		RemovalSpec);
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
	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		Cast<UDRProjectileWeaponItemDefinition>(
			GetSourceObject(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo()));
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

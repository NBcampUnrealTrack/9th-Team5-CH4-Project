// Fill out your copyright notice in the Description page of Project Settings.

#include "DRGA_AbsorbSnow.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "HAL/IConsoleManager.h"

namespace
{
TAutoConsoleVariable<int32> CVarDRSnowAbsorbGaugeSummary(
	TEXT("dr.Snow.Absorb.GaugeSummary"),
	0,
	TEXT("눈 흡수 종료 시 게이지 획득 요약을 한 줄로 남긴다. 0: 끔, 1: 켬"),
	ECVF_Default);

float CalculateSnowGaugeGain(
	const float RemovedAmount,
	const float SolidRemovalPerSnowGauge)
{
	if (RemovedAmount <= 0.f || SolidRemovalPerSnowGauge <= UE_SMALL_NUMBER)
	{
		return 0.f;
	}

	// 제거량을 그대로 재화로 쓰지 않고 무기 데이터의 환산 단위로만 나눈다.
	// 따라서 흡수 강도 강화가 실제 고체 눈 제거량을 늘리면 게이지 획득량도 함께 증가한다.
	return RemovedAmount / SolidRemovalPerSnowGauge;
}
}

UDRGA_AbsorbSnow::UDRGA_AbsorbSnow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = true;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(DRGameplayTags::Ability_Action);
	AssetTags.AddTag(DRGameplayTags::Ability_Snow_Absorb);
	SetAssetTags(AssetTags);

	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Attack_Ranged);
	
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

	// Presentation
	// LocalPredicted Ability이므로 owning client에서는 즉시 재생되고,
	// server montage state를 통해 simulated proxy에도 전달된다.
	if (IsValid(AbsorbMontage))
	{
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				NAME_None,
				AbsorbMontage,
				1.f,
				NAME_None,
				true); // Ability 종료 시 Montage 자동 정지

		if (IsValid(MontageTask))
		{
			MontageTask->ReadyForActivation();
		}
	}

	// 실제 Snow Absorb는 기존처럼 서버 전용
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	if (!IsValid(ASC) || !IsValid(AvatarActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartAbsorbGameplayCue();
	
	UDRSnowRemoveComponent* SnowRemoveComponent =
		IsValid(AvatarActor)
			? AvatarActor->FindComponentByClass<UDRSnowRemoveComponent>()
			: nullptr;

	if (!IsValid(SnowRemoveComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ResetAbsorbSummary();
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

void UDRGA_AbsorbSnow::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	StopAbsorbGameplayCue();
	LogAbsorbSummary();
	ResetAbsorbSummary();
	
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
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
	
	FVector AbsorbOrigin = AvatarActor->GetActorLocation();
	if (ADRPlayerCharacter* DRPlayerCharacter = Cast<ADRPlayerCharacter>(AvatarActor))
	{
		DRPlayerCharacter->CalculateGameplayFireOrigin(AvatarActor->GetActorForwardVector(), AbsorbOrigin);
	}

	const UDRRangedWeaponDefinition* WeaponDefinition = Cast<UDRRangedWeaponDefinition>(
		GetSourceObject(GetCurrentAbilitySpecHandle(), ActorInfo));
	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	const FVector AbsorbFrustumOrigin = AbsorbOrigin + AvatarActor->GetActorTransform().TransformVectorNoScale(
		RemovalSpec.SnowAbsorbStartOffset);
	FVector AbsorbDirection = ViewDirection;

	if (IsValid(WeaponDefinition)
		&& WeaponDefinition->AimCorrectionSettings.bUseCameraAimCorrection)
	{
		constexpr float CameraAimTraceDistance = 10000.0f;
		const FVector CameraTraceEnd = ViewLocation + ViewDirection * CameraAimTraceDistance;
		FCollisionQueryParams CameraQueryParams(SCENE_QUERY_STAT(DRAbsorbCameraAim), false);
		CameraQueryParams.AddIgnoredActor(AvatarActor);

		FHitResult CameraHit;
		const bool bCameraHit = GetWorld()->LineTraceSingleByChannel(
			CameraHit, ViewLocation, CameraTraceEnd, DRCollisionChannels::Projectile, CameraQueryParams);
		const FVector CameraAimPoint = bCameraHit ? CameraHit.ImpactPoint : CameraTraceEnd;

		AbsorbDirection = WeaponDefinition->ResolveCameraAimDirection(
			ViewDirection,
			AbsorbFrustumOrigin,
			CameraAimPoint);
	}

	// 캐릭터 로컬 StartOffset을 먼저 적용해 시작점을 고정한다.
	// 이후에만 보정된 AbsorbDirection을 적용해 프러스텀 축이 시작점 주위를 회전하게 한다.
	FDRSnowRemovalSpec EffectiveRemovalSpec = RemovalSpec;
	EffectiveRemovalSpec.SnowAbsorbStartOffset = FVector::ZeroVector;

	const float RemovedAmount =
		SnowRemoveComponent->TryRemoveSnowAlongDirection(
			AbsorbFrustumOrigin, AbsorbDirection, EffectiveRemovalSpec);
	
	const IConsoleVariable* AbsorbLog = IConsoleManager::Get().FindConsoleVariable(TEXT("dr.Snow.Absorb.Log"));
	const bool bLogGauge = AbsorbLog && AbsorbLog->GetInt() > 0;
	const float GaugeBefore = bLogGauge ? ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetSnowGaugeAttribute()) : 0.f;
	const float SolidRemovalPerSnowGauge = IsValid(WeaponDefinition)
		? WeaponDefinition->SnowAbsorbSettings.SolidRemovalPerSnowGauge
		: 0.f;
	const float RequestedGaugeGain = ApplySnowGaugeGain(
		ASC,
		RemovedAmount,
		SolidRemovalPerSnowGauge);
	++AbsorbSummaryTickCount;
	AbsorbSummaryRemovedAmount += RemovedAmount;
	AbsorbSummaryGaugeGain += RequestedGaugeGain;
	AbsorbSummaryPower = EffectiveRemovalSpec.SnowAbsorbPower;
	AbsorbSummarySolidRemovalPerGauge = SolidRemovalPerSnowGauge;
	if (bLogGauge)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[DRSnowAbsorb][Gauge] Avatar=%s Removed=%.6f PerGauge=%.6f Gain=%.6f Before=%.6f After=%.6f Max=%.6f Effect=%s"),
			*GetPathNameSafe(GetAvatarActorFromActorInfo()), RemovedAmount, SolidRemovalPerSnowGauge, RequestedGaugeGain, GaugeBefore,
			ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetSnowGaugeAttribute()),
			ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute()),
			*GetNameSafe(SnowGainEffectClass.Get()));
	}

	ScheduleNextAbsorbTick();
}

void UDRGA_AbsorbSnow::ResetAbsorbSummary()
{
	AbsorbSummaryTickCount = 0;
	AbsorbSummaryStartTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.f;
	AbsorbSummaryRemovedAmount = 0.f;
	AbsorbSummaryGaugeGain = 0.f;
	AbsorbSummaryPower = 0.f;
	AbsorbSummarySolidRemovalPerGauge = 0.f;
}

void UDRGA_AbsorbSnow::LogAbsorbSummary() const
{
	if (CVarDRSnowAbsorbGaugeSummary.GetValueOnGameThread() == 0
		|| AbsorbSummaryTickCount <= 0)
	{
		return;
	}

	const float EndTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : AbsorbSummaryStartTime;
	const float Duration = FMath::Max(0.f, EndTime - AbsorbSummaryStartTime);
	const float AverageGainPerTick = AbsorbSummaryGaugeGain / AbsorbSummaryTickCount;
	const float GainPerSecond = Duration > UE_SMALL_NUMBER
		? AbsorbSummaryGaugeGain / Duration
		: 0.f;
	UE_LOG(LogTemp, Log,
		TEXT("[DRSnowAbsorb][Summary] Ticks=%d Duration=%.2fs Power=%.2f PerGauge=%.2f Removed=%.2f Gauge=%.2f AvgTick=%.2f Rate=%.2f/s"),
		AbsorbSummaryTickCount,
		Duration,
		AbsorbSummaryPower,
		AbsorbSummarySolidRemovalPerGauge,
		AbsorbSummaryRemovedAmount,
		AbsorbSummaryGaugeGain,
		AverageGainPerTick,
		GainPerSecond);
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

void UDRGA_AbsorbSnow::StartAbsorbGameplayCue()
{
	if (bAbsorbGameplayCueActive)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	if (ActorInfo == nullptr || !IsValid(ASC) || !IsValid(AvatarActor))
	{
		return;
	}

	UObject* SourceObject = GetSourceObject(GetCurrentAbilitySpecHandle(), ActorInfo);
	if (!IsValid(Cast<UDRRangedWeaponDefinition>(SourceObject)))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = AvatarActor->GetActorLocation();
	Parameters.Normal = AvatarActor->GetActorForwardVector();
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;
	Parameters.SourceObject = SourceObject;
	ASC->AddGameplayCue(DRGameplayTags::GameplayCue_Weapon_Absorb_Active, Parameters);
	bAbsorbGameplayCueActive = true;
}

void UDRGA_AbsorbSnow::StopAbsorbGameplayCue()
{
	if (!bAbsorbGameplayCueActive)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveGameplayCue(DRGameplayTags::GameplayCue_Weapon_Absorb_Active);
	}

	bAbsorbGameplayCueActive = false;
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
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UAbilitySystemComponent* ASC = ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const float AbsorbPowerMultiplier = IsValid(ASC)
		? FMath::Max(0.f, ASC->GetNumericAttribute(
			UDRPlayerAttributeSet::GetWeaponSnowAbsorbPowerMultiplierAttribute()))
		: 1.f;
	OutRemovalSpec.SnowAbsorbPower = SnowAbsorbSettings.Power * AbsorbPowerMultiplier;
	OutRemovalSpec.SnowAbsorbRadius = SnowAbsorbSettings.Radius;
	OutRemovalSpec.SnowAbsorbSpeed = SnowAbsorbSettings.Speed;
	OutRemovalSpec.SnowAbsorbRange = SnowAbsorbSettings.Range;
	OutRemovalSpec.SnowAbsorbStartOffset = WeaponDefinition->StartOffset;
	OutRemovalSpec.SnowAbsorbSweepRadius = SnowAbsorbSettings.SweepRadius;
	OutRemovalSpec.SnowAbsorbMaxSweepsPerTick = SnowAbsorbSettings.MaxSweepsPerTick;
	OutRemovalSpec.bUseAdaptiveAbsorbQuery = SnowAbsorbSettings.bUseAdaptiveQuery;
	OutRemovalSpec.SnowAbsorbInnerRadiusRatio = SnowAbsorbSettings.InnerRadiusRatio;
	OutRemovalSpec.RemovalBrushShape = SnowAbsorbSettings.BrushShape;
	OutRemovalSpec.RemovalMode = SnowAbsorbSettings.RemovalMode;

	return OutRemovalSpec.SnowAbsorbPower > 0.f &&
		OutRemovalSpec.SnowAbsorbRadius > 0.f &&
		OutRemovalSpec.SnowAbsorbSpeed > 0.f &&
		OutRemovalSpec.SnowAbsorbRange > 0.f &&
		SnowAbsorbSettings.SolidRemovalPerSnowGauge > UE_SMALL_NUMBER;
}

float UDRGA_AbsorbSnow::ApplySnowGaugeGain(
	UAbilitySystemComponent* AbilitySystemComponent,
	const float RemovedAmount,
	const float SolidRemovalPerSnowGauge) const
{
	const float GaugeGain = CalculateSnowGaugeGain(RemovedAmount, SolidRemovalPerSnowGauge);
	if (!IsValid(AbilitySystemComponent) || GaugeGain <= UE_SMALL_NUMBER)
	{
		return 0.f;
	}

	if (!SnowGainEffectClass)
	{
		return 0.f;
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
		return 0.f;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Snow_Amount, GaugeGain);
	ApplyGameplayEffectSpecToOwner(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		EffectSpec);

	return GaugeGain;
}

#include "DRGA_SpraySnow.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "Engine/OverlapResult.h"
#include "DeepRaiders/Item/DRSprayerWeaponDefinition.h"
#include "DrawDebugHelpers.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"
#include "Animation/AnimMontage.h"

UDRGA_SpraySnow::UDRGA_SpraySnow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer InitialTags;
	InitialTags.AddTag(DRGameplayTags::Ability_Attack_Ranged);
	SetAssetTags(InitialTags);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_BlinkRecovery);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_MovementAction_Zipline);
}

bool UDRGA_SpraySnow::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();

	if (ActorInfo == nullptr 
		|| !IsValid(WeaponDefinition)
		|| WeaponDefinition->SprayTickInterval <= 0.f 
		|| WeaponDefinition->SprayRange <= 0.f 
		|| WeaponDefinition->ImpactEffects.IsEmpty())
	{
		return false;
	}


	const UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	if (!IsValid(AbilitySystem)
		|| AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_MovementAction_Zipline))
	{
		return false;
	}
	
	const float TickCost = WeaponDefinition->SnowCostPerSecond * WeaponDefinition->SprayTickInterval;
	
	const float CurrentSnow = AbilitySystem->GetNumericAttribute(UDRPlayerAttributeSet::GetSnowGaugeAttribute());
	return CurrentSnow + KINDA_SMALL_NUMBER >= TickCost;
}

void UDRGA_SpraySnow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (ActorInfo == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	/*
	 * 기존 RangedWeaponAttack과 동일하게
	 * Release를 Ability 안에서 기다린다.
	 */
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);

	if (IsValid(ReleaseTask))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
		ReleaseTask->ReadyForActivation();
	}

	StartSprayMontage();
	StartSprayPresentation();

	if (ActorInfo->IsNetAuthority())
	{
		StartServerSpray();
	}
	
#if ENABLE_DRAW_DEBUG

	if (ActorInfo->IsLocallyControlled())
	{
		StartLocalDebugDraw();
	}

#endif
}

void UDRGA_SpraySnow::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	StopSprayMontage();
	StopSprayPresentation();

	if (ActorInfo != nullptr
		&& ActorInfo->IsNetAuthority())
	{
		StopServerSpray();
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

void UDRGA_SpraySnow::HandleInputReleased(float /*TimeHeld*/)
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UDRGA_SpraySnow::StartServerSpray()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	LastHitReactionTimes.Reset();
	
	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();

	if (!IsValid(WeaponDefinition))
	{
		return;
	}
	
	/*
	 * 즉발 Tick은 넣지 않는다.
	 *
	 * 0초 즉발 + 0.1초 반복을 하면
	 * 첫 1초 동안 11번 판정되는 식으로
	 * DPS / Cost 계산이 미묘하게 어긋날 수 있다.
	 */
	World->GetTimerManager().SetTimer(SprayTimerHandle, this, &ThisClass::HandleSprayTick,
		WeaponDefinition->SprayTickInterval, true, WeaponDefinition->SprayTickInterval);
}

void UDRGA_SpraySnow::StopServerSpray()
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}
	
	LastHitReactionTimes.Reset();
	World->GetTimerManager().ClearTimer(SprayTimerHandle);
}

void UDRGA_SpraySnow::HandleSprayTick()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (!IsActive() || ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	if (!IsValid(AbilitySystem)
		|| AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_MovementAction_Zipline))
	{
		EndAbility(
			GetCurrentAbilitySpecHandle(),
			ActorInfo,
			GetCurrentActivationInfo(),
			true,
			true);
		return;
	}

	/*
	 * 이번 Tick 비용을 낼 수 없으면
	 * 분사를 즉시 종료.
	 */
	if (!TryConsumeSnowCost())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);

		return;
	}

	FVector Origin;
	FVector Direction;

	if (!ResolveSprayOriginAndDirection(Origin, Direction))
	{
		return;
	}

	ApplySprayToTargets(Origin, Direction);
}

bool UDRGA_SpraySnow::ResolveSprayOriginAndDirection(FVector& OutOrigin, FVector& OutDirection) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	if (!IsValid(AvatarActor))
	{
		return false;
	}

	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();

	if (!IsValid(WeaponDefinition))
	{
		return false;
	}
	
	FRotator AimRotation = AvatarActor->GetActorRotation();

	if (const AController* Controller = ActorInfo->PlayerController.Get())
	{
		AimRotation = Controller->GetControlRotation();
	}

	OutDirection = AimRotation.Vector().GetSafeNormal();

	if (OutDirection.IsNearlyZero())
	{
		return false;
	}

	/*
	 * 위치는 애니메이션된 Weapon Socket을 사용하지 않는다.
	 *
	 * 방향은 Pitch까지 포함한 실제 Aim 방향.
	 */
	FVector HorizontalDirection = OutDirection;

	HorizontalDirection.Z = 0.f;

	if (!HorizontalDirection.Normalize())
	{
		HorizontalDirection = AvatarActor->GetActorForwardVector();
	}

	OutOrigin = AvatarActor->GetActorLocation() + FVector::UpVector * WeaponDefinition->SprayOriginHeightOffset 
		+ HorizontalDirection * WeaponDefinition->SprayOriginForwardOffset;

	return true;
}

void UDRGA_SpraySnow::ApplySprayToTargets(const FVector& Origin, const FVector& Direction)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	UAbilitySystemComponent* SourceAbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	UWorld* World = GetWorld();
	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();

	if (!IsValid(AvatarActor) || !IsValid(SourceAbilitySystem) || !IsValid(World) || !IsValid(WeaponDefinition))
	{
		return;
	}
	
	// --------------------------------
	// 1. Range 안 Pawn 후보 검색
	// --------------------------------

	TArray<FOverlapResult> OverlapResults;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams OverlapQueryParams(SCENE_QUERY_STAT(DRSnowSprayerOverlap), false);

	OverlapQueryParams.AddIgnoredActor(AvatarActor);

	const bool bHasOverlap = World->OverlapMultiByObjectType(OverlapResults, Origin
		, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(WeaponDefinition->SprayRange), OverlapQueryParams);

	if (!bHasOverlap)
	{
		return;
	}

	// --------------------------------
	// 2. EffectSpec은 Tick당 한 번 생성
	// --------------------------------

	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);

	if (ImpactEffectSpecs.IsEmpty())
	{
		return;
	}

	// --------------------------------
	// 3. LOS Trace 설정
	// --------------------------------

	FCollisionQueryParams TraceQueryParams(SCENE_QUERY_STAT(DRSnowSprayerLOS), false);

	TraceQueryParams.AddIgnoredActor(AvatarActor);

	/*
	 * 기존 Projectile과 동일하게
	 * 아군은 공격 경로를 막지 않도록 한다.
	 */
	const int32 SourceTeamId = GetSourceTeamId();

	if (SourceTeamId != INDEX_NONE)
	{
		TArray<APawn*> FriendlyPawns;

		DRCombatTeam::GetFriendlyPawns(World, SourceTeamId, FriendlyPawns);

		for (APawn* FriendlyPawn : FriendlyPawns)
		{
			if (IsValid(FriendlyPawn))
			{
				TraceQueryParams.AddIgnoredActor(FriendlyPawn);
			}
		}
	}

	const float MinDot = FMath::Cos(FMath::DegreesToRadians(WeaponDefinition->SprayHalfAngleDegrees));

	/*
	 * 하나의 Actor가 Component 여러 개 때문에
	 * OverlapResults에 중복으로 들어오는 것을 방지.
	 */
	TSet<AActor*> ProcessedActors;

	// --------------------------------
	// 4. Cone + LOS + Effect
	// --------------------------------

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();

		if (!IsValid(TargetActor) || TargetActor == AvatarActor || ProcessedActors.Contains(TargetActor))
		{
			continue;
		}

		ProcessedActors.Add(TargetActor);

		if (IsFriendlyTarget(TargetActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

		if (!IsValid(TargetAbilitySystem))
		{
			continue;
		}

		const FVector TargetLocation = TargetActor->GetActorLocation();

		const FVector ToTarget = TargetLocation - Origin;

		const float DistanceSquared = ToTarget.SizeSquared();

		if (DistanceSquared > FMath::Square(WeaponDefinition->SprayRange))
		{
			continue;
		}

		const FVector TargetDirection = ToTarget.GetSafeNormal();

		if (TargetDirection.IsNearlyZero())
		{
			continue;
		}

		const float Dot = FVector::DotProduct(Direction, TargetDirection);

		if (Dot < MinDot)
		{
			continue;
		}

		/*
		 * Cone 안이어도 벽 뒤면 맞지 않는다.
		 */
		if (!HasLineOfSightToTarget(Origin, TargetActor, TraceQueryParams))
		{
			continue;
		}

		const float HealthBefore =
	TargetAbilitySystem->GetNumericAttribute(
		UDRPlayerAttributeSet::GetHealthAttribute());

		const float FreezeGaugeBefore =
			TargetAbilitySystem->GetNumericAttribute(
				UDRPlayerAttributeSet::GetFreezeGaugeAttribute());

		const bool bWasFrozen =
			TargetAbilitySystem->HasMatchingGameplayTag(
				DRGameplayTags::State_Frozen);

		for (const FGameplayEffectSpecHandle& SpecHandle : ImpactEffectSpecs)
		{
			if (!SpecHandle.IsValid())
			{
				continue;
			}

			FGameplayEffectSpec ImpactSpec(*SpecHandle.Data.Get());

			SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
				ImpactSpec,
				TargetAbilitySystem);
		}

		const float HealthAfter =
			TargetAbilitySystem->GetNumericAttribute(
				UDRPlayerAttributeSet::GetHealthAttribute());

		const float FreezeGaugeAfter =
			TargetAbilitySystem->GetNumericAttribute(
				UDRPlayerAttributeSet::GetFreezeGaugeAttribute());

		const bool bIsFrozen =
			TargetAbilitySystem->HasMatchingGameplayTag(
				DRGameplayTags::State_Frozen);

		const bool bHealthDamaged =
			HealthAfter < HealthBefore - KINDA_SMALL_NUMBER;

		const bool bFreezeIncreased =
			FreezeGaugeAfter > FreezeGaugeBefore + KINDA_SMALL_NUMBER;

		const bool bBecameFrozen =
			!bWasFrozen && bIsFrozen;

		/*
		 * Sprayer는 일반 상태에서는 FreezeGauge를 증가시키고,
		 * Frozen 상태에서는 Health Damage를 줄 수 있다.
		 *
		 * 둘 중 하나라도 실제 피격 결과가 발생했거나
		 * 이번 Tick에 Frozen 상태로 전환됐다면 HitReaction 후보로 처리한다.
		 */
		if (bHealthDamaged
			|| bFreezeIncreased
			|| bBecameFrozen)
		{
			TryExecuteHitReaction(
				TargetActor,
				TargetAbilitySystem,
				Origin);
		}
	}
}

bool UDRGA_SpraySnow::HasLineOfSightToTarget(const FVector& Origin, const AActor* TargetActor, const FCollisionQueryParams& QueryParams) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	const FVector TargetLocation = TargetActor->GetActorLocation();

	FHitResult HitResult;

	const bool bBlockingHit = World->LineTraceSingleByChannel(HitResult, Origin, TargetLocation, DRCollisionChannels::Projectile, QueryParams);

	/*
	 * 아무것도 안 막았으면 보임.
	 */
	if (!bBlockingHit)
	{
		return true;
	}

	/*
	 * 첫 Blocking Actor가 Target이면 보임.
	 *
	 * 다른 적이나 Wall이 먼저 맞으면
	 * 뒤쪽 Target은 차단된다.
	 */
	return HitResult.GetActor() == TargetActor;
}

bool UDRGA_SpraySnow::TryConsumeSnowCost()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	const float TickCost = WeaponDefinition->SnowCostPerSecond * WeaponDefinition->SprayTickInterval;

	const float CurrentSnow = AbilitySystem->GetNumericAttribute(UDRPlayerAttributeSet::GetSnowGaugeAttribute());

	if (CurrentSnow + KINDA_SMALL_NUMBER < TickCost)
	{
		return false;
	}

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();

	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	if (!WeaponDefinition->SnowCostEffectClass)
	{
		return false;
	}

	FGameplayEffectSpecHandle CostSpec =
		MakeOutgoingGameplayEffectSpec(
			GetCurrentAbilitySpecHandle(),
			ActorInfo,
			GetCurrentActivationInfo(),
			WeaponDefinition->SnowCostEffectClass,
			GetAbilityLevel(
				GetCurrentAbilitySpecHandle(),
				ActorInfo));
	
	if (!CostSpec.IsValid())
	{
		return false;
	}

	CostSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Snow_Amount, -TickCost);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);

	return true;
}

void UDRGA_SpraySnow::BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const
{
	OutEffectSpecs.Reset();

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	if (!IsValid(AbilitySystem))
	{
		return;
	}

	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return;
	}
	
	UObject* SourceObject = GetSourceObject(GetCurrentAbilitySpecHandle(), ActorInfo);
	
	for (const FDRGameplayEffectData& EffectData : WeaponDefinition->ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();

		if (IsValid(SourceObject))
		{
			EffectContext.AddSourceObject(SourceObject);
		}

		FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(EffectData.EffectClass,
			EffectData.EffectLevel, EffectContext);

		if (!EffectSpec.IsValid())
		{
			continue;
		}

		for (const TPair<FGameplayTag, float>& Pair : EffectData.SetByCallerMagnitudes)
		{
			if (Pair.Key.IsValid())
			{
				EffectSpec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
			}
		}

		OutEffectSpecs.Add(EffectSpec);
	}
}

bool UDRGA_SpraySnow::IsFriendlyTarget(const AActor* TargetActor) const
{
	/*
	 * 기존 Projectile과 동일한 테스트 정책.
	 * Team이 아직 없으면 적으로 취급.
	 */
	const int32 SourceTeamId = GetSourceTeamId();

	if (SourceTeamId == INDEX_NONE)
	{
		return false;
	}

	return DRCombatTeam::IsFriendlyTarget(SourceTeamId, TargetActor);
}

int32 UDRGA_SpraySnow::GetSourceTeamId() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return INDEX_NONE;
	}

	const int32 OwnerTeamId = DRCombatTeam::GetActorTeamId(ActorInfo->OwnerActor.Get());

	if (OwnerTeamId != INDEX_NONE)
	{
		return OwnerTeamId;
	}

	return DRCombatTeam::GetActorTeamId(ActorInfo->AvatarActor.Get());
}

const UDRSprayerWeaponDefinition* UDRGA_SpraySnow::GetSprayerDefinition() const
{
	return Cast<UDRSprayerWeaponDefinition>(GetSourceObject(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo()));
}

void UDRGA_SpraySnow::StartSprayMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		ActorInfo->AbilitySystemComponent.Get();

	const UDRSprayerWeaponDefinition* WeaponDefinition =
		GetSprayerDefinition();

	if (!IsValid(ASC)
		|| !IsValid(WeaponDefinition)
		|| !IsValid(WeaponDefinition->ItemAnimationSet))
	{
		return;
	}

	UAnimMontage* SprayMontage =
		WeaponDefinition->ItemAnimationSet->PrimaryActionMontage;

	if (!IsValid(SprayMontage))
	{
		return;
	}

	ASC->PlayMontage(
		this,
		GetCurrentActivationInfo(),
		SprayMontage,
		1.f);
}

void UDRGA_SpraySnow::StopSprayMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		ActorInfo->AbilitySystemComponent.Get();

	if (!IsValid(ASC))
	{
		return;
	}

	const UDRSprayerWeaponDefinition* WeaponDefinition =
		GetSprayerDefinition();

	if (!IsValid(WeaponDefinition)
		|| !IsValid(WeaponDefinition->ItemAnimationSet))
	{
		return;
	}

	UAnimMontage* SprayMontage =
		WeaponDefinition->ItemAnimationSet->PrimaryActionMontage;

	if (ASC->GetCurrentMontage() == SprayMontage)
	{
		MontageStop(0.15f);
	}
}

void UDRGA_SpraySnow::TryExecuteHitReaction(
	AActor* TargetActor,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FVector& SprayOrigin)
{
	if (!IsValid(TargetActor) || !IsValid(TargetAbilitySystem))
	{
		return;
	}

	UWorld* World = GetWorld();
	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();

	if (!IsValid(World) 
		|| !IsValid(WeaponDefinition))
	{
		return;
	}
	
	const float CurrentTime = World->GetTimeSeconds();
	if (const float* LastTime = LastHitReactionTimes.Find(TargetActor))
	{
		if (CurrentTime - *LastTime < WeaponDefinition->HitReactionInterval)
		{
			return;
		}
	}

	LastHitReactionTimes.Add(TargetActor, CurrentTime);

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	AActor* SourceActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;

	FGameplayCueParameters Parameters;
	/*
	 * Spray는 Projectile ImpactPoint가 없으므로
	 * Spray Origin을 피격 방향 판정 기준으로 전달한다.
	 */
	Parameters.Location = SprayOrigin;
	Parameters.Instigator = SourceActor;
	Parameters.EffectCauser = SourceActor;
	Parameters.SourceObject = GetSourceObject(GetCurrentAbilitySpecHandle(), ActorInfo);

	TargetAbilitySystem->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Player_Hit, Parameters);
}

void UDRGA_SpraySnow::StartSprayPresentation()
{
	if (ActivePresentationEffectHandle.IsValid())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo =
		GetCurrentActorInfo();

	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();
	
	if (ActorInfo == nullptr
		|| !IsValid(WeaponDefinition)
		|| !WeaponDefinition->ActivePresentationEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		ActorInfo->AbilitySystemComponent.Get();

	UObject* SourceObject =
		GetSourceObject(
			GetCurrentAbilitySpecHandle(),
			ActorInfo);
	
	if (!IsValid(ASC)
		|| !IsValid(SourceObject)
		|| !SourceObject->IsA<UDRSprayerWeaponDefinition>())
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext =
		ASC->MakeEffectContext();

	EffectContext.AddSourceObject(SourceObject);

	FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(
			WeaponDefinition->ActivePresentationEffectClass,
			GetAbilityLevel(
				GetCurrentAbilitySpecHandle(),
				ActorInfo),
			EffectContext);

	if (!SpecHandle.IsValid())
	{
		return;
	}

	ActivePresentationEffectHandle =
		ApplyGameplayEffectSpecToOwner(
			GetCurrentAbilitySpecHandle(),
			ActorInfo,
			GetCurrentActivationInfo(),
			SpecHandle);
}

void UDRGA_SpraySnow::StopSprayPresentation()
{
	if (!ActivePresentationEffectHandle.IsValid())
	{
		return;
	}

	BP_RemoveGameplayEffectFromOwnerWithHandle(
		ActivePresentationEffectHandle,
		-1);

	ActivePresentationEffectHandle.Invalidate();
}

#if ENABLE_DRAW_DEBUG

void UDRGA_SpraySnow::StartLocalDebugDraw()
{
	if (!bDrawDebugSpray)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	HandleDebugDrawTick();

	World->GetTimerManager().SetTimer(
		DebugDrawTimerHandle,
		this,
		&ThisClass::HandleDebugDrawTick,
		0.05f,
		true,
		0.05f);
}

void UDRGA_SpraySnow::StopLocalDebugDraw()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			DebugDrawTimerHandle);
	}
}

void UDRGA_SpraySnow::HandleDebugDrawTick()
{
	if (!bDrawDebugSpray)
	{
		return;
	}

	FVector Origin;
	FVector Direction;

	if (!ResolveSprayOriginAndDirection(
			Origin,
			Direction))
	{
		return;
	}

	DrawDebugSpray(
		Origin,
		Direction);
}

void UDRGA_SpraySnow::DrawDebugSpray(
	const FVector& Origin,
	const FVector& Direction) const
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}
	
	const UDRSprayerWeaponDefinition* WeaponDefinition = GetSprayerDefinition();

	if (!IsValid(WeaponDefinition))
	{
		return;
	}

	const float AngleRadians =
		FMath::DegreesToRadians(
			WeaponDefinition->SprayHalfAngleDegrees);

	DrawDebugCone(
		World,
		Origin,
		Direction,
		WeaponDefinition->SprayRange,
		AngleRadians,
		AngleRadians,
		24,
		FColor::Cyan,
		false,
		0.06f,
		0,
		1.5f);

	DrawDebugSphere(
		World,
		Origin,
		8.f,
		12,
		FColor::Yellow,
		false,
		0.06f,
		0,
		1.5f);
}

#endif

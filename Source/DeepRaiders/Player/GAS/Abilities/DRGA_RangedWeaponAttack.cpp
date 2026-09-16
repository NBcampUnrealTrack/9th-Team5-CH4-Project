
#include "DRGA_RangedWeaponAttack.h"

#include "DeepRaiders/DeepRaiders.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "DeepRaiders/Skill/Barrier/DRBarrierGenerator.h"

#include "Kismet/GameplayStatics.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameplayPrediction.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

UDRGA_RangedWeaponAttack::UDRGA_RangedWeaponAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 모든 원거리 공격 Ability 식별용
	FGameplayTagContainer InitialTags;
	InitialTags.AddTag(DRGameplayTags::Ability_Action);
	InitialTags.AddTag(DRGameplayTags::Ability_Attack_Ranged);

	SetAssetTags(InitialTags);

	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Snow_Absorb);
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::State_GamePreparing);
	
	ActivationBlockedTags.AddTag(DRGameplayTags::State_BlinkRecovery);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_MovementAction_Zipline);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);
}

bool UDRGA_RangedWeaponAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (IsValid(AbilitySystem)
		&& AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_MovementAction_Zipline))
	{
		return false;
	}
	
	return IsAttackConfigurationValid(GetWeaponDefinition(Handle, ActorInfo));
}

void UDRGA_RangedWeaponAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	
	UDRInventoryComponent* Inventory = nullptr;
	const FDRItemInstance* ItemInstance = nullptr;
	
	if (!IsAttackConfigurationValid(WeaponDefinition)
		|| !ResolveSelectedWeaponInstance(ActorInfo, WeaponDefinition, Inventory, ItemInstance))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 자식 GA에서 사용할 Ability Active 시점 함수 
	OnRangedWeaponActivated();
	
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	
	if (IsValid(ReleaseTask))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
		
		ReleaseTask->ReadyForActivation();
	}
	
	// CommitAbility 에서 이루어질 Cost, CoolDown 처리
	if (ActorInfo != nullptr 
		&& ActorInfo->IsLocallyControlled())
	{
		TryRequestLocalShot();
	}
}

void UDRGA_RangedWeaponAttack::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	
	const UDRProjectileWeaponItemDefinition* WeaponItemDefinition = GetCurrentWeaponDefinition();
	
	if (IsValid(WeaponItemDefinition)
		&& WeaponItemDefinition->bAutomaticFire
		&& ActorInfo != nullptr
		&& ActorInfo->IsLocallyControlled())
	{
		TryRequestLocalShot();
	}
}

void UDRGA_RangedWeaponAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 자식 GA Ability End 시점 함수 
	OnRangedWeaponEnded();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UDRGA_RangedWeaponAttack::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ActorInfo == nullptr)
	{
		return false;
	}

	if (ActorInfo->IsNetAuthority())
	{
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
	}

	if (!ActorInfo->IsLocallyControlled())
	{
		return false;
	}

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(ActorInfo->PlayerController.Get());
	const UDRQuickSlotComponent* QuickSlot = IsValid(PlayerController) ?
		PlayerController->GetQuickSlotComponent() : nullptr;

	return IsValid(QuickSlot)
		&& QuickSlot->CanRequestLocalWeaponShot(QuickSlot->GetSelectedInstanceId());
}

void UDRGA_RangedWeaponAttack::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	
	if (ActorInfo == nullptr 
		|| !CooldownGameplayEffectClass
		|| !IsValid(WeaponDefinition))
	{
		return;
	}
	
	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	
	if (!CooldownSpec.IsValid())
	{
		return;
	}
	
	CooldownSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Cooldown_Duration, GetWeaponFireInterval());
	
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec);
}

bool UDRGA_RangedWeaponAttack::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)
		|| ActorInfo == nullptr)
	{
		return false;
	}
	
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	UDRInventoryComponent* Inventory = nullptr;
	const FDRItemInstance* ItemInstance = nullptr;
	
	if (!ResolveSelectedWeaponInstance(ActorInfo, WeaponDefinition, Inventory, ItemInstance))
	{
		return false;
	}
	
	switch (WeaponDefinition->ResourceType)
	{
	case EDRProjectileWeaponResourceType::SnowGauge:
	{
		const float SnowCost = GetWeaponSnowCostPerShot();

		if (SnowCost <= KINDA_SMALL_NUMBER)
		{
			return true;
		}

		if (!WeaponDefinition->SnowCostEffectClass)
		{
			return false;
		}

		const UAbilitySystemComponent* AbilitySystem =
			ActorInfo->AbilitySystemComponent.Get();

		if (!IsValid(AbilitySystem))
		{
			return false;
		}

		const float CurrentSnow = AbilitySystem->GetNumericAttribute(
			UDRPlayerAttributeSet::GetSnowGaugeAttribute());

		const bool bHasEnoughSnow = CurrentSnow + KINDA_SMALL_NUMBER >= SnowCost;
		if (!bHasEnoughSnow && OptionalRelevantTags != nullptr)
		{
			OptionalRelevantTags->AddTag(DRGameplayTags::Ability_ActivateFail_Weapon_ResourceEmpty);
		}

		return bHasEnoughSnow;
	}
	case EDRProjectileWeaponResourceType::InstanceAmmo:
	{
		const FDRProjectileWeaponRuntimeState* WeaponState =
			ItemInstance->RuntimeState.GetPtr<FDRProjectileWeaponRuntimeState>();

		if (WeaponState == nullptr)
		{
			return false;
		}

		const bool bHasAmmo = WeaponState->CurrentAmmo > 0;
		if (!bHasAmmo && OptionalRelevantTags != nullptr)
		{
			OptionalRelevantTags->AddTag(DRGameplayTags::Ability_ActivateFail_Weapon_ResourceEmpty);
		}

		return bHasAmmo;
	}

	default:
		DR_ERROR(TEXT("[%s] Invalid ranged weapon resource type"), *GetName());
		return false;
	}
}

void UDRGA_RangedWeaponAttack::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	
	
	
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}
	
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	UDRInventoryComponent* Inventory = nullptr;
	const FDRItemInstance* ItemInstance = nullptr;
	
	if (!ResolveSelectedWeaponInstance(ActorInfo, WeaponDefinition, Inventory, ItemInstance))
	{
		return;
	}
	
	switch (WeaponDefinition->ResourceType)
	{
	case EDRProjectileWeaponResourceType::SnowGauge:
	{
		const float SnowCost = GetWeaponSnowCostPerShot();

		if (SnowCost <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		if (!WeaponDefinition->SnowCostEffectClass)
		{
			return;
		}

		FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(Handle,ActorInfo,ActivationInfo,
			WeaponDefinition->SnowCostEffectClass, GetAbilityLevel(Handle, ActorInfo));

		if (!CostSpec.IsValid())
		{
			return;
		}

		CostSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Snow_Amount, -SnowCost);

		ApplyGameplayEffectSpecToOwner(Handle,ActorInfo,ActivationInfo,CostSpec);

		return;
	}

	case EDRProjectileWeaponResourceType::InstanceAmmo:
	{
		const FGuid InstanceId = ItemInstance->InstanceId;
		bool bDepleted = false;

		const bool bConsumed = Inventory->ModifyItemInstance(InstanceId,
			[&bDepleted](FDRItemInstance& Candidate)
			{
				FDRProjectileWeaponRuntimeState* WeaponState =
					Candidate.RuntimeState.GetMutablePtr<FDRProjectileWeaponRuntimeState>();

				if (WeaponState == nullptr 
					|| WeaponState->CurrentAmmo <= 0)
				{
					return false;
				}

				--WeaponState->CurrentAmmo;
				bDepleted = WeaponState->CurrentAmmo <= 0;
				return true;
			});

		ensureMsgf(bConsumed, TEXT("Failed to consume ammo from item instance %s"), *InstanceId.ToString());

		if (bConsumed && bDepleted)
		{
			const UAnimMontage* FireMontage = IsValid(WeaponDefinition->ItemAnimationSet)
				? WeaponDefinition->ItemAnimationSet->PrimaryActionMontage.Get()
				: nullptr;
			const float EffectivePlayRate = IsValid(FireMontage) ? FMath::Abs(FireMontage->RateScale) : 0.f;
			const float RemovalDelay = EffectivePlayRate > KINDA_SMALL_NUMBER
				? FireMontage->GetPlayLength() / EffectivePlayRate
				: 0.f;

			// 마지막 발사 AnimNotify가 현재 무기의 Presentation을 읽을 때까지 아이템과 AbilitySet을 유지한다.
			QueueDepletedInstanceAmmoRemoval(Inventory, InstanceId, RemovalDelay);
		}

		return;
	}

	default:
		DR_ERROR(TEXT("[%s] Invalid ranged weapon resource type"), *GetName());
		return;
	}
}

bool UDRGA_RangedWeaponAttack::IsAttackConfigurationValid(const UDRProjectileWeaponItemDefinition* WeaponDefinition) const
{
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	
	// Cooldown GE가 없으면 발사가 불가능
	return IsValid(WeaponDefinition) && WeaponDefinition->BaseFireInterval > 0.0f 
		&& WeaponDefinition->MaxAttackDistance > 0.0f && CooldownGameplayEffectClass != nullptr
		&& CooldownTags != nullptr && CooldownTags->HasTagExact(DRGameplayTags::Cooldown_Weapon_Ranged);
}

bool UDRGA_RangedWeaponAttack::SendLocalShotRequest()
{
	// 자식 GA에서 재정의 필요
	return false;
}

void UDRGA_RangedWeaponAttack::TryRequestLocalShot()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (!IsActive()
		|| ActorInfo == nullptr
		|| !ActorInfo->IsLocallyControlled())
	{
		return;
	}
	
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(AbilitySystem))
	{
		return;
	}
	
	if (AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Frozen)
		|| AbilitySystem->HasMatchingGameplayTag(
			DRGameplayTags::State_VoxelContained)
		|| AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Dead))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);

		return;
	}

	// 이미 활성화된 자동/연발 Ability가 Zipline 진입 뒤에도 shot을 계속 내지 못하게 한다.
	if (AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_MovementAction_Zipline))
	{
		EndAbility(
			GetCurrentAbilitySpecHandle(),
			ActorInfo,
			GetCurrentActivationInfo(),
			true,
			true);
		return;
	}
	
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();	

	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		return;
	}
	
	if (!CheckCooldown(Handle, ActorInfo, nullptr))
	{
		return;
	}
	
	FGameplayTagContainer CostFailureTags;
	if (!CheckCost(Handle, ActorInfo, &CostFailureTags))
	{
		if (CostFailureTags.HasTagExact(DRGameplayTags::Ability_ActivateFail_Weapon_ResourceEmpty))
		{
			PlayLocalResourceEmptyFeedback();
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(ActorInfo->PlayerController.Get());
	UDRQuickSlotComponent* QuickSlot = IsValid(PlayerController) ?
		PlayerController->GetQuickSlotComponent() : nullptr;

	if (!IsValid(QuickSlot))
	{
		return;
	}

	const FGuid WeaponInstanceId = QuickSlot->GetSelectedInstanceId();

	if (!QuickSlot->CanRequestLocalWeaponShot(WeaponInstanceId))
	{
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		if (SendLocalShotRequest())
		{
			QuickSlot->RecordLocalWeaponShot(WeaponInstanceId, GetWeaponFireInterval());
		}

		return;
	}
	
	FScopedPredictionWindow PredictionWindow(AbilitySystem, true);
	if (!SendLocalShotRequest())
	{
		return;
	}
	
	/*
 	* 이 상태는 로컬 요청 빈도만 제한한다.
 	* 실제 발사와 쿨다운 GE 적용 여부는 서버가 결정한다.
 	*/
	QuickSlot->RecordLocalWeaponShot(
		WeaponInstanceId,
		GetWeaponFireInterval());
}

bool UDRGA_RangedWeaponAttack::TryCommitServerShot()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (!IsActive() 
		|| ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

	// 탑승 직전에 보낸 RPC/TargetData가 늦게 서버에 도착해도 실제 발사는 승인하지 않는다.
	if (!IsValid(AbilitySystem)
		|| AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_MovementAction_Zipline))
	{
		EndAbility(
			GetCurrentAbilitySpecHandle(),
			ActorInfo,
			GetCurrentActivationInfo(),
			true,
			true);
		return false;
	}

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();	

	
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	
	UDRInventoryComponent* Inventory = nullptr;
	const FDRItemInstance* ItemInstance = nullptr;
	
	if (!ResolveSelectedWeaponInstance(ActorInfo, WeaponDefinition, Inventory, ItemInstance))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		
		return false;
	}
	
	// Resource 검사는 그대로 서버 authoritative
	if (!CheckCost(
		Handle,
		ActorInfo,
		nullptr))
	{
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			false);

		return false;
	}

	// Weapon fire cadence도 서버 authoritative.
	// 단 GAS Cooldown GE가 아니라 별도 cadence clock으로 검사.
	if (!TryConsumeServerFireInterval())
	{
		return false;
	}

	// 실제 accepted shot에 대해서만 Cost 적용
	ApplyCost(
		Handle,
		ActorInfo,
		ActivationInfo);

	return true;
}

#pragma region Ranged Weapon Function
bool UDRGA_RangedWeaponAttack::GetViewPoint(FVector& OutViewLocation, FRotator& OutViewRotation) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		return false;
	}
	
	if (AController* Controller = ActorInfo->PlayerController.Get())
	{
		Controller->GetPlayerViewPoint(OutViewLocation, OutViewRotation);
		
		return true;
	}
	
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!IsValid(AvatarActor))
	{
		return false;
	}
	
	AvatarActor->GetActorEyesViewPoint(OutViewLocation, OutViewRotation);
	
	return true;
}

bool UDRGA_RangedWeaponAttack::TraceCameraAim(const FVector& ViewLocation, const FVector& ViewDirection,
	FHitResult& OutHitResult) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}
	
	const FVector SafeDirection = ViewDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return false;
	}
	
	const FVector TraceEnd = ViewLocation + SafeDirection * CameraTraceDistance;
	
	FCollisionQueryParams QueryParams;
	BuildWeaponTraceQueryParams(QueryParams);

	// 아군 배리어처럼 팀을 가진 비 Pawn Actor도 조준점을 가로막지 않게 한다.
	// 내부에서 배리어 출구 면이 발사 원점보다 가까운 조준점으로 선택되면
	// Projectile 발사 방향이 뒤집힐 수 있으므로 실제 발사 전에 제외해야 한다.
	constexpr int32 MaxFriendlyPassThroughIterations = 16;
	for (int32 Iteration = 0; Iteration < MaxFriendlyPassThroughIterations; ++Iteration)
	{
		const bool bBlockingHit = World->LineTraceSingleByChannel(
			OutHitResult,
			ViewLocation,
			TraceEnd,
			DRCollisionChannels::Projectile,
			QueryParams);

		if (!bBlockingHit)
		{
			OutHitResult = FHitResult(ViewLocation, TraceEnd);
			OutHitResult.Location = TraceEnd;
			OutHitResult.ImpactPoint = TraceEnd;
			return true;
		}

		AActor* HitActor = OutHitResult.GetActor();
		const ADRBarrierGenerator* BarrierGenerator = Cast<ADRBarrierGenerator>(HitActor);
		const bool bStartedInsideBarrier = IsValid(BarrierGenerator)
			&& BarrierGenerator->ContainsPoint(ViewLocation);
		if (!IsValid(HitActor) || (!IsFriendlyTarget(HitActor) && !bStartedInsideBarrier))
		{
			return true;
		}

		QueryParams.AddIgnoredActor(HitActor);
	}

	// 비정상적으로 많은 아군 Actor가 겹친 경우에도 역방향 조준점은 만들지 않는다.
	OutHitResult = FHitResult(ViewLocation, TraceEnd);
	OutHitResult.Location = TraceEnd;
	OutHitResult.ImpactPoint = TraceEnd;
	return true;
}

bool UDRGA_RangedWeaponAttack::ResolveGameplayFireOrigin(
	const FVector& AimDirection,
	FVector& OutFireOrigin) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return false;
	}

	const ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());

	if (!IsValid(Character))
	{
		return false;
	}

	return Character->CalculateGameplayFireOrigin(AimDirection, OutFireOrigin);
}

void UDRGA_RangedWeaponAttack::BuildWeaponTraceQueryParams(FCollisionQueryParams& OutQueryParams) const
{
	OutQueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(DRRangedWeaponTrace), false);
	
	OutQueryParams.bReturnPhysicalMaterial = true;
	
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		return;
	}
	
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (IsValid(AvatarActor))
	{
		OutQueryParams.AddIgnoredActor(AvatarActor);
	}
	
	TArray<APawn*> FriendlyPawns;
	DRCombatTeam::GetFriendlyPawns(GetWorld(), GetSourceTeamId(), FriendlyPawns);
	
	for (APawn* FriendlyPawn : FriendlyPawns)
	{
		if (IsValid(FriendlyPawn))
		{
			OutQueryParams.AddIgnoredActor(FriendlyPawn);
		}
	}
}

bool UDRGA_RangedWeaponAttack::IsFriendlyTarget(const AActor* TargetActor) const
{
	return DRCombatTeam::IsFriendlyTarget(GetSourceTeamId(), TargetActor);
}

int32 UDRGA_RangedWeaponAttack::GetSourceTeamId() const
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

void UDRGA_RangedWeaponAttack::BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const
{
	OutEffectSpecs.Reset();
	
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		return;
	}
	
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetWeaponDefinition(GetCurrentAbilitySpecHandle(), ActorInfo);
	
	if (!IsValid(AbilitySystem)
		|| !IsValid(WeaponDefinition))
	{
		return;
	}
	
	for (const FDRGameplayEffectData& EffectData : WeaponDefinition->ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}
		
		FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(WeaponDefinition);
		
		FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(EffectData.EffectClass, EffectData.EffectLevel,
			EffectContext);

		if (!EffectSpec.IsValid())
		{
			continue;
		}
		
		for (const TPair<FGameplayTag, float>& Pair : EffectData.SetByCallerMagnitudes)
		{
			if (Pair.Key.IsValid())
			{
				const float Magnitude = Pair.Key == DRGameplayTags::Data_Damage
					? Pair.Value * GetWeaponStatMultiplier(UDRPlayerAttributeSet::GetWeaponDamageMultiplierAttribute())
					: Pair.Value;
				EffectSpec.Data->SetSetByCallerMagnitude(Pair.Key, Magnitude);
			}
		}
		
		OutEffectSpecs.Add(EffectSpec);
	}
}

float UDRGA_RangedWeaponAttack::GetBreakableDamageAmount() const
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	const float DamageMultiplier = GetWeaponStatMultiplier(UDRPlayerAttributeSet::GetWeaponDamageMultiplierAttribute());

	return IsValid(WeaponDefinition)
		? FMath::Max(0.f, WeaponDefinition->BreakableDamage * DamageMultiplier)
		: 0.f;
}

float UDRGA_RangedWeaponAttack::GetWeaponFireInterval() const
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	const float Multiplier = GetWeaponStatMultiplier(UDRPlayerAttributeSet::GetWeaponFireIntervalMultiplierAttribute());

	return IsValid(WeaponDefinition) ? FMath::Max(0.01f, WeaponDefinition->BaseFireInterval * Multiplier) : 0.01f;
}

float UDRGA_RangedWeaponAttack::GetWeaponSnowCostPerShot() const
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	const float Multiplier = GetWeaponStatMultiplier(UDRPlayerAttributeSet::GetWeaponSnowCostMultiplierAttribute());

	return IsValid(WeaponDefinition) ? FMath::Max(0.0f, WeaponDefinition->SnowCostPerShot * Multiplier) : 0.0f;
}

int32 UDRGA_RangedWeaponAttack::GetWeaponProjectileCount() const
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	const float Multiplier = GetWeaponStatMultiplier(
		UDRPlayerAttributeSet::GetWeaponProjectileCountMultiplierAttribute());
	const float ProjectileCount = IsValid(WeaponDefinition) ? WeaponDefinition->ProjectileCount * Multiplier : 1.0f;

	return FMath::Max(1, FMath::RoundToInt(ProjectileCount));
}

bool UDRGA_RangedWeaponAttack::TryApplyBreakableDamage(const FHitResult& HitResult) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr 
		|| !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	ADRBreakableActor* BreakableTarget = Cast<ADRBreakableActor>(HitResult.GetActor());

	if (!IsValid(BreakableTarget) 
		|| BreakableTarget->IsBroken())
	{
		return false;
	}

	const float DamageAmount = GetBreakableDamageAmount();

	if (DamageAmount <= 0.f)
	{
		return false;
	}

	AActor* DamageCauser = ActorInfo->AvatarActor.Get();

	if (!IsValid(DamageCauser))
	{
		return false;
	}

	FVector DamageDirection = HitResult.TraceEnd - HitResult.TraceStart;

	if (!DamageDirection.Normalize())
	{
		DamageDirection = DamageCauser->GetActorForwardVector();
	}

	AController* InstigatorController = ActorInfo->PlayerController.Get();

	const float AppliedDamage =	UGameplayStatics::ApplyPointDamage(
			BreakableTarget,
			DamageAmount,
			DamageDirection,
			HitResult,
			InstigatorController,
			DamageCauser,
			UDamageType::StaticClass());

	return AppliedDamage > KINDA_SMALL_NUMBER;	
}

const UDRProjectileWeaponItemDefinition* UDRGA_RangedWeaponAttack::GetCurrentWeaponDefinition() const
{
	return GetWeaponDefinition(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo());
}

void UDRGA_RangedWeaponAttack::ApplyImpactEffectSpecs(UAbilitySystemComponent* TargetAbilitySystem,
                                                      const FHitResult& HitResult, const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr 
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}
	
	UAbilitySystemComponent* SourceAbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	
	if (!IsValid(SourceAbilitySystem)
		|| !IsValid(TargetAbilitySystem))
	{
		return;
	}

	bool bAppliedAnyEffect = false;
	
	for (const FGameplayEffectSpecHandle& SpecHandle : ImpactEffectSpecs)
	{
		if (!SpecHandle.IsValid())
		{
			continue;
		}
		
		FGameplayEffectSpec ImpactSpec(*SpecHandle.Data.Get());
		ImpactSpec.GetContext().AddHitResult(HitResult, true);
		
		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(ImpactSpec, TargetAbilitySystem);
		bAppliedAnyEffect = true;
	}

	if (!bAppliedAnyEffect)
	{
		return;
	}

	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(TargetAbilitySystem->GetAvatarActor());
	if (!IsValid(TargetCharacter))
	{
		return;
	}

	AActor* SourceActor = ActorInfo->AvatarActor.Get();
	FGameplayCueParameters Parameters;
	Parameters.Location = HitResult.ImpactPoint;
	Parameters.Normal = HitResult.ImpactNormal;
	Parameters.Instigator = SourceActor;
	Parameters.EffectCauser = SourceActor;
	Parameters.SourceObject = const_cast<UDRProjectileWeaponItemDefinition*>(GetCurrentWeaponDefinition());

	TargetAbilitySystem->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Player_Hit, Parameters);
}


void UDRGA_RangedWeaponAttack::ApplyHeatForSuccessfulShot()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition) || !WeaponDefinition->HeatSettings.bEnabled ||
		WeaponDefinition->HeatSettings.HeatPerShot <= 0.f)
	{
		return;
	}

	ADRPlayerState* PlayerState = Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get());
	if (!IsValid(PlayerState))
	{
		return;
	}

	const float HeatGenerationMultiplier = GetWeaponStatMultiplier(
		UDRPlayerAttributeSet::GetWeaponHeatGenerationMultiplierAttribute());
	const float HeatAmount = FMath::Max(0.f, WeaponDefinition->HeatSettings.HeatPerShot * HeatGenerationMultiplier);

	PlayerState->AddWeaponHeat(
		HeatAmount,
		WeaponDefinition->HeatSettings.DecayDelay,
		WeaponDefinition->HeatSettings.RecoveryDuration);
}

void UDRGA_RangedWeaponAttack::PlayLocalFirePresentation(
	const FVector& FireOrigin,
	const FVector& TargetLocation)
{
	const FGameplayAbilityActorInfo* ActorInfo =
		GetCurrentActorInfo();

	if (ActorInfo == nullptr
		|| !ActorInfo->IsLocallyControlled()
		|| ActorInfo->IsNetAuthority())
	{
		return;
	}

	UE_LOG(
	LogTemp,
	Warning,
	TEXT("[FIRE] LOCAL_MONTAGE_START T=%.6f Frame=%llu"),
	FPlatformTime::Seconds(),
	GFrameCounter);

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Character))
	{
		return;
	}

	Character->PrepareProjectileFirePresentation(TargetLocation);

	PlayFireMontage();
}

void UDRGA_RangedWeaponAttack::PlayServerFirePresentation(
	const FVector& FireOrigin,
	const FVector& TargetLocation)
{
	const FGameplayAbilityActorInfo* ActorInfo =
		GetCurrentActorInfo();

	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Character))
	{
		return;
	}

	Character->PrepareProjectileFirePresentation(TargetLocation);

	// TargetLocation은 Character 상태로 복제하고, Montage는 ASC 복제를 사용한다.
	// 실제 SFX / VFX는 각 클라이언트가 AnimNotify(FireMoment)에서 재생한다.
	PlayFireMontage();
}

bool UDRGA_RangedWeaponAttack::ResolveSelectedWeaponInstance(const FGameplayAbilityActorInfo* ActorInfo,
	const UDRProjectileWeaponItemDefinition* ExpectedDefinition, UDRInventoryComponent*& OutInventory,
	const FDRItemInstance*& OutItemInstance) const
{
	OutInventory = nullptr;
	OutItemInstance = nullptr;
	
	if (ActorInfo == nullptr
		|| !IsValid(ExpectedDefinition))
	{
		return false;
	}
	
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(ActorInfo->PlayerController.Get());
	
	if (!IsValid(PlayerController))
	{
		return false;
	}
	
	UDRInventoryComponent* Inventory = PlayerController->GetInventoryComponent();
	UDRQuickSlotComponent* QuickSlot = PlayerController->GetQuickSlotComponent();
	if (!IsValid(Inventory) || !IsValid(QuickSlot))
	{
		return false;
	}
	
	const FGuid SelectedInstanceId = QuickSlot->GetSelectedInstanceId();
	const FDRItemInstance* SelectedItem = Inventory->FindItemInstance(SelectedInstanceId);
	
	if (SelectedItem == nullptr 
		|| SelectedItem->Definition.Get() != ExpectedDefinition)
	{
		return false;
	}
	
	OutInventory = Inventory;
	OutItemInstance = SelectedItem;
	return true;
}

const UDRProjectileWeaponItemDefinition* UDRGA_RangedWeaponAttack::GetWeaponDefinition(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<UDRProjectileWeaponItemDefinition>(GetSourceObject(Handle, ActorInfo));
}

void UDRGA_RangedWeaponAttack::HandleInputReleased(float TimeHeld)
{
	if (!IsActive())
	{
		return;
	}
	
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		true, false);
}

void UDRGA_RangedWeaponAttack::PlayFireMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo =
		GetCurrentActorInfo();

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

	const UDRProjectileWeaponItemDefinition* WeaponDefinition =
		GetWeaponDefinition(
			GetCurrentAbilitySpecHandle(),
			ActorInfo);

	if (!IsValid(WeaponDefinition)
		|| !IsValid(WeaponDefinition->ItemAnimationSet))
	{
		return;
	}

	UAnimMontage* FireMontage =
		WeaponDefinition->
		ItemAnimationSet->
		PrimaryActionMontage;

	if (!IsValid(FireMontage))
	{
		return;
	}

	ASC->PlayMontage(
		this,
		GetCurrentActivationInfo(),
		FireMontage,
		1.f);
}

void UDRGA_RangedWeaponAttack::PlayLocalResourceEmptyFeedback() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(ActorInfo->PlayerController.Get());
	UDRQuickSlotComponent* QuickSlot = IsValid(PlayerController)
		? PlayerController->GetQuickSlotComponent()
		: nullptr;

	if (IsValid(QuickSlot))
	{
		QuickSlot->PlayWeaponResourceEmptySound();
	}
}

void UDRGA_RangedWeaponAttack::QueueDepletedInstanceAmmoRemoval(
	UDRInventoryComponent* Inventory,
	FGuid InstanceId,
	float RemovalDelay) const
{
	if (!IsValid(Inventory) || !InstanceId.IsValid())
	{
		return;
	}

	UWorld* World = Inventory->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FTimerDelegate RemovalDelegate = FTimerDelegate::CreateWeakLambda(
		Inventory,
		[Inventory, InstanceId]()
		{
			// 예약 이후 아이템이나 탄약 상태가 달라졌다면 제거하지 않는다.
			const FDRItemInstance* CurrentItem = Inventory->FindItemInstance(InstanceId);
			const UDRProjectileWeaponItemDefinition* WeaponDefinition = CurrentItem != nullptr
				? Cast<UDRProjectileWeaponItemDefinition>(CurrentItem->Definition.Get())
				: nullptr;
			const FDRProjectileWeaponRuntimeState* WeaponState = CurrentItem != nullptr
				? CurrentItem->RuntimeState.GetPtr<FDRProjectileWeaponRuntimeState>()
				: nullptr;

			if (!IsValid(WeaponDefinition)
				|| WeaponDefinition->ResourceType != EDRProjectileWeaponResourceType::InstanceAmmo
				|| WeaponState == nullptr
				|| WeaponState->CurrentAmmo > 0)
			{
				return;
			}

			const int32 Quantity = CurrentItem->Quantity;
			const bool bRemoved = Inventory->TryRemoveItemInstance(InstanceId, Quantity);
			ensureMsgf(bRemoved, TEXT("Failed to remove depleted ammo item instance %s"), *InstanceId.ToString());
		});

	if (RemovalDelay <= KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimerForNextTick(RemovalDelegate);
		return;
	}

	FTimerHandle RemovalTimerHandle;
	World->GetTimerManager().SetTimer(RemovalTimerHandle, RemovalDelegate, RemovalDelay, false);
}

float UDRGA_RangedWeaponAttack::GetWeaponStatMultiplier(const FGameplayAttribute& Attribute) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UAbilitySystemComponent* ASC = ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	return IsValid(ASC) && Attribute.IsValid()
		? FMath::Max(0.0f, ASC->GetNumericAttribute(Attribute))
		: 1.0f;
}

bool UDRGA_RangedWeaponAttack::TryConsumeServerFireInterval()
{
	const FGameplayAbilityActorInfo* ActorInfo =
		GetCurrentActorInfo();

	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UWorld* World = IsValid(AvatarActor)
		? AvatarActor->GetWorld()
		: nullptr;

	if (!IsValid(World))
	{
		return false;
	}

	const double Now =
		static_cast<double>(World->GetTimeSeconds());

	const double FireInterval =
		static_cast<double>(GetWeaponFireInterval());

	if (FireInterval <= 0.0)
	{
		return false;
	}

	/*
	 * Client는 정확한 FireInterval cadence로 요청하지만,
	 * Dedicated Server는 별도 tick에서 packet을 처리하므로
	 * 같은 요청도 최대 약 1 server frame 일찍 관측될 수 있다.
	 *
	 * 단, 다음 cadence 기준 자체는 ServerNextAllowedShotTime에서
	 * 계속 전진시키므로 이 tolerance로 연사속도를 올릴 수는 없다.
	 */
	const double ServerFrameTolerance =
		FMath::Min(
			FireInterval * 0.5,
			static_cast<double>(World->GetDeltaSeconds()) + 0.002);

	// 첫 발
	if (ServerNextAllowedShotTime <= 0.0)
	{
		ServerNextAllowedShotTime = Now + FireInterval;
		return true;
	}

	// 서버가 오래 stall된 경우 이전 cadence backlog를 따라잡지 않는다.
	if (Now > ServerNextAllowedShotTime + FireInterval)
	{
		ServerNextAllowedShotTime = Now + FireInterval;
		return true;
	}

	// 정상적인 client request보다 너무 이른 요청
	if (Now + ServerFrameTolerance < ServerNextAllowedShotTime)
	{
		return false;
	}

	/*
	 * 한 frame 일찍 도착한 요청은 기존 cadence를 유지하고,
	 * 늦게 도착한 요청은 현재 시각부터 새 cadence를 시작한다.
	 *
	 * 늦어진 cadence backlog를 따라잡기 위해 같은 frame의
	 * 후속 요청을 연속 승인하지 않는다.
	 */
	ServerNextAllowedShotTime = FMath::Max(
		ServerNextAllowedShotTime + FireInterval,
		Now + FireInterval);

	return true;
}

float UDRGA_RangedWeaponAttack::GetMaxAttackDistance() const
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();

	return IsValid(WeaponDefinition) ? WeaponDefinition->MaxAttackDistance : 0.f;
}

#pragma endregion

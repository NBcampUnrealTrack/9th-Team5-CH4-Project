
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

#include "Kismet/GameplayStatics.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameplayPrediction.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "Components/StaticMeshComponent.h"

UDRGA_RangedWeaponAttack::UDRGA_RangedWeaponAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 모든 원거리 공격 Ability 식별용
	FGameplayTagContainer InitialTags;
	InitialTags.AddTag(DRGameplayTags::Ability_Attack_Ranged);

	SetAssetTags(InitialTags);

	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Snow_Absorb);
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

		return CurrentSnow + KINDA_SMALL_NUMBER >= SnowCost;
	}
	case EDRProjectileWeaponResourceType::InstanceAmmo:
	{
		const FDRProjectileWeaponRuntimeState* WeaponState =
			ItemInstance->RuntimeState.GetPtr<FDRProjectileWeaponRuntimeState>();

		return WeaponState != nullptr && WeaponState->CurrentAmmo > 0;
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

		const bool bConsumed = Inventory->ModifyItemInstance(InstanceId,
			[](FDRItemInstance& Candidate)
			{
				FDRProjectileWeaponRuntimeState* WeaponState =Candidate.RuntimeState.GetMutablePtr<FDRProjectileWeaponRuntimeState>();

				if (WeaponState == nullptr 
					|| WeaponState->CurrentAmmo <= 0)
				{
					return false;
				}

				--WeaponState->CurrentAmmo;
				return true;
			});

		ensureMsgf(bConsumed,TEXT("Failed to consume ammo from item instance %s"),*InstanceId.ToString());

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
	
	if (!CheckCost(Handle, ActorInfo, nullptr))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	if (ActorInfo->IsNetAuthority())
	{
		SendLocalShotRequest();
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
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo, nullptr))
	{
		if (!CheckCost(Handle, ActorInfo, nullptr))
		{
			// 연발 도중 Commit 실패 시 실패 사유 확인을 위한 코드
			EndAbility(Handle, ActorInfo, ActivationInfo,true, false);
		}

		return false;
	}

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
	
	const FVector TraceEnd = ViewLocation + SafeDirection * GetMaxAttackDistance();
	
	FCollisionQueryParams QueryParams;
	BuildWeaponTraceQueryParams(QueryParams);
	
	const bool bBlockingHit = World->LineTraceSingleByChannel(OutHitResult, ViewLocation, TraceEnd,
		DRCollisionChannels::Projectile, QueryParams);
	
	// 충돌하지 않은 경우 시선의 끝을 반환
	if (!bBlockingHit)
	{
		OutHitResult = FHitResult(ViewLocation, TraceEnd);
		OutHitResult.Location = TraceEnd;
		OutHitResult.ImpactPoint = TraceEnd;
	}
	
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
	
	for (const FGameplayEffectSpecHandle& SpecHandle : ImpactEffectSpecs)
	{
		if (!SpecHandle.IsValid())
		{
			continue;
		}
		
		FGameplayEffectSpec ImpactSpec(*SpecHandle.Data.Get());
		ImpactSpec.GetContext().AddHitResult(HitResult, true);
		
		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(ImpactSpec, TargetAbilitySystem);		
	}	
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

	PlayFireMontage();
	ExecuteFireGameplayCue(FireOrigin);
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

	PlayFireMontage();
	ExecuteFireGameplayCue(FireOrigin);
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

void UDRGA_RangedWeaponAttack::ExecuteFireGameplayCue(const FVector& FireOrigin) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UObject* PresentationSourceObject = GetSourceObject(GetCurrentAbilitySpecHandle(), ActorInfo);
	if (!IsValid(ASC) || !IsValid(AvatarActor) || !IsValid(PresentationSourceObject))
	{
		return;
	}

	FGameplayCueParameters Parameters;

	Parameters.Location = FireOrigin;
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;

	// DA_Rifle / DA_Shotgun / DA_Cannon
	Parameters.SourceObject = PresentationSourceObject;

	ASC->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Weapon_Projectile_Fire, Parameters);
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

float UDRGA_RangedWeaponAttack::GetWeaponStatMultiplier(const FGameplayAttribute& Attribute) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UAbilitySystemComponent* ASC = ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	return IsValid(ASC) && Attribute.IsValid()
		? FMath::Max(0.0f, ASC->GetNumericAttribute(Attribute))
		: 1.0f;
}

float UDRGA_RangedWeaponAttack::GetMaxAttackDistance() const
{
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();

	return IsValid(WeaponDefinition) ? WeaponDefinition->MaxAttackDistance : 0.f;
}

#pragma endregion

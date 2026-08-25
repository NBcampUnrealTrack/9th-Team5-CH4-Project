
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
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameplayPrediction.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"

UDRGA_RangedWeaponAttack::UDRGA_RangedWeaponAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;	
}

bool UDRGA_RangedWeaponAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	return IsAttackConfigurationValid();
}

void UDRGA_RangedWeaponAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = GetWeaponDefinition(Handle, ActorInfo);
	
	UDRInventoryComponent* Inventory = nullptr;
	const FDRItemInstance* ItemInstance = nullptr;
	
	if (!IsAttackConfigurationValid()
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
	
	if (bAutomaticFire
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
	if (ActorInfo == nullptr 
		|| !CooldownGameplayEffectClass)
	{
		return;
	}
	
	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	
	if (!CooldownSpec.IsValid())
	{
		return;
	}
	
	CooldownSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Cooldown_Duration, BaseFireInterval);
	
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
		if (WeaponDefinition->SnowCostPerShot <= 0.0f)
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

		return CurrentSnow + KINDA_SMALL_NUMBER >=
			WeaponDefinition->SnowCostPerShot;
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
		if (WeaponDefinition->SnowCostPerShot <= 0.0f 
			|| !WeaponDefinition->SnowCostEffectClass)
		{
			return;
		}

		FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(Handle,ActorInfo,ActivationInfo,
			WeaponDefinition->SnowCostEffectClass, GetAbilityLevel(Handle, ActorInfo));

		if (!CostSpec.IsValid())
		{
			return;
		}

		CostSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Snow_Amount, -WeaponDefinition->SnowCostPerShot);

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

bool UDRGA_RangedWeaponAttack::IsAttackConfigurationValid() const
{
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	
	// Cooldown GE가 없으면 발사가 불가능
	return BaseFireInterval > 0.0f && MaxAttackDistance > 0.0f && CooldownGameplayEffectClass != nullptr
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
	
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();	

	if (!CheckCooldown(Handle, ActorInfo, nullptr))
	{
		return;
	}
	
	if (!CheckCost(Handle, ActorInfo, nullptr))
	{
		EndAbility(Handle, ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}
	
	if (ActorInfo->IsNetAuthority())
	{
		SendLocalShotRequest();
		return;
	}
	
	// 원격 클라이언트가 발사 요청을 전송하면 ScopedPredictionKey로 Cooldown GE를 예측 적용
	FScopedPredictionWindow PredictionWindow(AbilitySystem, true);
	if (SendLocalShotRequest())
	{
		ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
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
	
	const FVector TraceEnd = ViewLocation + SafeDirection * MaxAttackDistance;
	
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

bool UDRGA_RangedWeaponAttack::ResolveMuzzleLocation(const FVector& ViewDirection, FVector& OutMuzzleLocation) const
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
	
	const ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(AvatarActor);
	if (IsValid(Character))
	{
		const UStaticMeshComponent* EquipmentMesh = Character->GetWorldHandEquipmentMesh();
		
		if (IsValid(EquipmentMesh)
			&& EquipmentMesh->DoesSocketExist(MuzzleSocketName))
		{
			OutMuzzleLocation = EquipmentMesh->GetSocketLocation(MuzzleSocketName);
			
			return true;
		}
	}
	
	// MuzzlePoint 소켓이 없는 경우
	//
	FVector SafeDirection = AvatarActor->GetActorForwardVector();
	
	OutMuzzleLocation = AvatarActor->GetActorLocation() + FVector::UpVector * MuzzleHeightOffset + 
		SafeDirection * MuzzleForwardOffset;
	
	return true;	
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
	
	for (const FDRGameplayEffectData& EffectData : ImpactEffects)
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
				EffectSpec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
			}
		}
		
		OutEffectSpecs.Add(EffectSpec);
	}
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

void UDRGA_RangedWeaponAttack::ExecuteImpactGameplayCue(const FHitResult& HitResult) const
{
	if (!ImpactGameplayCueTag.IsValid())
	{
		return;
	}
	
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr 
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}
	
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(AbilitySystem))
	{
		return;
	}
	
	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(GetWeaponDefinition(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo()));
	EffectContext.AddHitResult(HitResult, true);
	
	FGameplayCueParameters CueParameters(EffectContext);
	CueParameters.Location = HitResult.Location;
	CueParameters.Normal = HitResult.Normal;
	
	AbilitySystem->ExecuteGameplayCue(ImpactGameplayCueTag, CueParameters);	
}

void UDRGA_RangedWeaponAttack::PlayLocalFirePresentation(
	const FVector& MuzzleLocation,
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
	ExecuteFireSoundCue(MuzzleLocation);
}

void UDRGA_RangedWeaponAttack::PlayServerFirePresentation(
	const FVector& MuzzleLocation,
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
	ExecuteFireSoundCue(MuzzleLocation);
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

void UDRGA_RangedWeaponAttack::ExecuteFireSoundCue(
		const FVector& MuzzleLocation) const
{
	const FGameplayAbilityActorInfo* ActorInfo =
		GetCurrentActorInfo();

	if (ActorInfo == nullptr
		|| !FireGameplayCueTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		ActorInfo->AbilitySystemComponent.Get();

	AActor* AvatarActor =
		ActorInfo->AvatarActor.Get();

	if (!IsValid(ASC)
		|| !IsValid(AvatarActor))
	{
		return;
	}

	FGameplayCueParameters Parameters;

	Parameters.Location =
		MuzzleLocation;

	Parameters.Instigator =
		AvatarActor;

	Parameters.EffectCauser =
		AvatarActor;

	ASC->ExecuteGameplayCue(
		FireGameplayCueTag,
		Parameters);
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

#pragma endregion

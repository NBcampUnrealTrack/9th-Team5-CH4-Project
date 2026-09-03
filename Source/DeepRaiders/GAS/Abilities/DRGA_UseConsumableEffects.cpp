
#include "DRGA_UseConsumableEffects.h"

#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRConsumableItemDefinition.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/DeepRaiders.h"

UDRGA_UseConsumableEffects::UDRGA_UseConsumableEffects()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);
}

bool UDRGA_UseConsumableEffects::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	const UDRConsumableItemDefinition* ConsumableDefinition = 
		Cast<UDRConsumableItemDefinition>(GetSourceObject(Handle, ActorInfo));
	
	if (!IsValid(ConsumableDefinition)
		|| !HasValidUseEffect(ConsumableDefinition))
	{
		return false;
	}
	
	UDRInventoryComponent* InventoryComponent = nullptr;
	FGuid InstanceId; 
	
	return ResolveSelectedConsumable(ActorInfo, ConsumableDefinition, InventoryComponent, InstanceId);
}

void UDRGA_UseConsumableEffects::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	const UDRConsumableItemDefinition* ConsumableDefinition =
		Cast<UDRConsumableItemDefinition>(GetSourceObject(Handle, ActorInfo));
	
	UDRInventoryComponent* InventoryComponent = nullptr;
	FGuid InstanceId;
	
	if (!IsValid(ConsumableDefinition)
		|| !HasValidUseEffect(ConsumableDefinition)
		|| !ResolveSelectedConsumable(ActorInfo, ConsumableDefinition, InventoryComponent, InstanceId))
	{
		DR_WARNING(TEXT("[%s] Invalid consumable source or selected item."), *GetName());
		
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 아이템 소비가 Cost로 처리되지는 않는다. 정상적인 실행 경계를 유지하기 위해 CommitAbility 호출
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const int32 AppliedEffectCount = ApplyUseEffects(Handle, ActorInfo, ActivationInfo, ConsumableDefinition);
	
	// 적용 가능한 이펙트가 1개 이상이어야 소비된다.
	const bool bShouldConsume = ActorInfo != nullptr && ActorInfo->IsNetAuthority() && AppliedEffectCount > 0;
	
	/*
	 * 마지막 스택을 제거하면 QuickSlotComponent가 현재 ItemAbilitySet을 회수한다.
	 * 사용 GA를 먼저 정상 종료해 자기 자신이 취소되는 재진입을 피한다.
	 */
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	
	if (!bShouldConsume)
	{
		return;
	}
	
	const bool bConsumed = IsValid(InventoryComponent) && InventoryComponent->TryRemoveItemInstance(InstanceId, 1);
	
	ensureMsgf(bConsumed, TEXT("[%s] GE was applied, but ItemInstance %s could not be consumed."),
		*GetName(), *InstanceId.ToString());	
}

bool UDRGA_UseConsumableEffects::HasValidUseEffect(const UDRConsumableItemDefinition* ConsumableDefinition) const
{
	if (!IsValid(ConsumableDefinition))
	{
		return false;
	}
	
	return ConsumableDefinition->UseEffects.ContainsByPredicate(
		[](const FDRGameplayEffectData& EffectData)
		{
			return EffectData.EffectClass != nullptr;
		});
}

bool UDRGA_UseConsumableEffects::ResolveSelectedConsumable(const FGameplayAbilityActorInfo* ActorInfo,
	const UDRConsumableItemDefinition* ExpectedDefinition, UDRInventoryComponent*& OutInventoryComponent,
	FGuid& OutInstanceId) const
{
	OutInventoryComponent = nullptr;
	OutInstanceId.Invalidate();
	
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
	
	if (!IsValid(Inventory)
		|| !IsValid(QuickSlot))
	{
		return false;
	}
	
	const FGuid SelectedInstanceId = QuickSlot->GetSelectedInstanceId();
	const FDRItemInstance* ItemInstance = Inventory->GetItemInstance(SelectedInstanceId);
	
	if (ItemInstance == nullptr
		|| ItemInstance->Definition.Get() != ExpectedDefinition
		|| ItemInstance->Quantity <= 0)
	{
		return false;
	}
	
	OutInventoryComponent = Inventory;
	OutInstanceId = SelectedInstanceId;
	
	return true;
}

int32 UDRGA_UseConsumableEffects::ApplyUseEffects(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const UDRConsumableItemDefinition* ConsumableDefinition) const
{
	if (ActorInfo == nullptr
		|| !IsValid(ConsumableDefinition)
		|| !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return 0;
	}
	
	int32 AppliedEffectCount = 0;
	
	for (const FDRGameplayEffectData& EffectData : ConsumableDefinition->UseEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}
		
		FGameplayEffectSpecHandle EffectSpec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
			EffectData.EffectClass, EffectData.EffectLevel);
		
		if (!EffectSpec.IsValid())
		{
			continue;
		}
		
		for (const TPair<FGameplayTag, float>& Magnitude : EffectData.SetByCallerMagnitudes)
		{
			if (Magnitude.Key.IsValid())
			{
				EffectSpec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
			}
		}
		
		const FActiveGameplayEffectHandle AppliedHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo,
			ActivationInfo,	EffectSpec);
		
		
		if (AppliedHandle.WasSuccessfullyApplied())
		{
			++AppliedEffectCount;
		}
	}
	
	return AppliedEffectCount;
}

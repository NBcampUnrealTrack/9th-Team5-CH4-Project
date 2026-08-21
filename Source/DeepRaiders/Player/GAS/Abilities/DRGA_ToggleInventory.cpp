// Fill out your copyright notice in the Description page of Project Settings.


#include "DRGA_ToggleInventory.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/Inventory/DRInventoryUIComponent.h"

UDRGA_ToggleInventory::UDRGA_ToggleInventory()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UDRGA_ToggleInventory::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	ADRPlayerController* PlayerController = ActorInfo ? Cast<ADRPlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
	
	if (IsValid(PlayerController))
	{
		if (UDRInventoryUIComponent* InventoryUI = PlayerController->GetInventoryUIComponent())
		{
			InventoryUI->TogglePlayerInventory();
		}
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

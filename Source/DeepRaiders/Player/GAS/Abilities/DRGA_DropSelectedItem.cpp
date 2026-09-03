// Fill out your copyright notice in the Description page of Project Settings.


#include "DRGA_DropSelectedItem.h"

#include "DeepRaiders/Core/Subsystem/DRWorldItemSubsystem.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Item/DRWorldItemTypes.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UDRGA_DropSelectedItem::UDRGA_DropSelectedItem()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;	
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
}

void UDRGA_DropSelectedItem::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	ADRPlayerController* Controller = Cast<ADRPlayerController>(ActorInfo->PlayerController.Get());
	APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	
	if (!IsValid(Controller)
		|| !IsValid(AvatarPawn))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	UDRInventoryComponent* Inventory = Controller->GetInventoryComponent();
	UDRQuickSlotComponent* QuickSlot = Controller->GetQuickSlotComponent();
	
	if (!IsValid(Inventory)
		|| !IsValid(QuickSlot))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const FGuid SelectedInstanceId = QuickSlot->GetSelectedInstanceId();
	const FDRItemInstance* SelectedItem = Inventory->FindItemInstance(SelectedInstanceId);
	
	if (SelectedItem == nullptr
		|| !SelectedItem->IsValid()
		|| !IsValid(SelectedItem->Definition)
		|| !SelectedItem->Definition->bCanBeDropped)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	/*
	 * 인벤토리 제거 후 포인터가 무효화 되므로
	 * InstanceId, Quantity, RuntimeState를 포함한 전체 값을 먼저 복사
	 */
	const FDRItemInstance DroppedItem = *SelectedItem;
	
	UWorld* World = AvatarPawn->GetWorld();
	UDRWorldItemSubsystem* WorldItemSubsystem = IsValid(World) ? World->GetSubsystem<UDRWorldItemSubsystem>() : nullptr;
	
	if (!IsValid(WorldItemSubsystem))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const FRotator ControlRotaion = Controller->GetControlRotation();
	const FRotator DropRotation(0.f, ControlRotaion.Yaw, 0.f);
	const FVector DropForward = DropRotation.Vector().GetSafeNormal();
	
	const FVector SourceLocation = AvatarPawn->GetActorLocation() + FVector::UpVector * DropHeightOffset;
	const FVector DesiredTargetLocation = SourceLocation + DropForward * DropForwardDistance;
	
	FDRWorldItemSpawnParams SpawnParams;
	SpawnParams.SourceTransform = FTransform(DropRotation, SourceLocation);
	SpawnParams.TargetTransform = FTransform(DropRotation, DesiredTargetLocation);
	SpawnParams.IgnoredActor = AvatarPawn;
	SpawnParams.bPlayEmergence = true;
	
	ADRWorldItemActor* WorldItem = WorldItemSubsystem->SpawnWorldItem(DroppedItem, SpawnParams);
	
	if (!IsValid(WorldItem))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	/* 
	 * 월드 생성 후 인벤토리에서 아이템 제거
	 * 제거 실패 시 월드 액터 제거하여 롤백 
	 */
	if (!Inventory->TryRemoveItemInstance(DroppedItem.InstanceId, DroppedItem.Quantity))
	{
		WorldItem->Destroy();
		
		EndAbility(Handle, ActorInfo, ActivationInfo, true , true);
		return;
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);	
}

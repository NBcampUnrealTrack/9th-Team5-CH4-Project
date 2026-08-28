#include "DRHeldItemComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"

#include "DeepRaiders/Player/Components/DRMiningComponent.h"
#include "DeepRaiders/Snow/Components/DRSnowRemoveComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"


UDRHeldItemComponent::UDRHeldItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}


void UDRHeldItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDRHeldItemComponent, HeldItemDefinition);
}

ADRPlayerCharacter* UDRHeldItemComponent::GetOwnerCharacter() const
{
	return Cast<ADRPlayerCharacter>(GetOwner());
}

void UDRHeldItemComponent::SetHeldItemDefinition(UDRItemDefinition* NewItemDefinition)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority() || HeldItemDefinition == NewItemDefinition)
	{
		return;
	}

	HeldItemDefinition = NewItemDefinition;

	/*
	 * Listen Server에서는 RepNotify가
	 * 자기 자신에게 호출되지 않으므로
	 * 서버에서도 즉시 갱신한다.
	 */
	RefreshHeldItemState();

	Character->ForceNetUpdate();
}

void UDRHeldItemComponent::OnRep_HeldItemDefinition()
{
	RefreshHeldItemState();
}

void UDRHeldItemComponent::RefreshAnimationLayer()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character))
	{
		return;
	}
	
	if (Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!IsValid(Mesh))
	{
		return;
	}

	TSubclassOf<UAnimInstance> NewAnimLayerClass = nullptr;
	if (IsValid(HeldItemDefinition) && IsValid(HeldItemDefinition->ItemAnimationSet))
	{
		NewAnimLayerClass = HeldItemDefinition->ItemAnimationSet->AnimLayerClass;
	}

	if (LinkedAnimLayerClass == NewAnimLayerClass)
	{
		return;
	}

	// 기존 Linked Layer 제거.
	if (LinkedAnimLayerClass)
	{
		Mesh->UnlinkAnimClassLayers(LinkedAnimLayerClass);
	}

	LinkedAnimLayerClass = NewAnimLayerClass;

	// 새 Linked Layer 적용.
	if (LinkedAnimLayerClass)
	{
		Mesh->LinkAnimClassLayers(LinkedAnimLayerClass);
	}
}

void UDRHeldItemComponent::BeginPlay()
{
	Super::BeginPlay();

	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	MiningComponent = Character->FindComponentByClass<UDRMiningComponent>();
	
	if (!MiningComponent.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT( "[HeldItem] MiningComponent missing. " "Character=%s"), *GetNameSafe(Character));
	}
}

void UDRHeldItemComponent::RefreshHeldItemState()
{
	RefreshVisual();
	RefreshMiningSettings();
	RefreshSnowComponents();
	RefreshAnimationLayer();
	PlayEquipSound();
}

void UDRHeldItemComponent::RefreshVisual()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	if (!IsValid(HeldItemDefinition))
	{
		Character->ClearHandEquipmentVisual();
		return;
	}

	Character->ApplyHandEquipmentVisual(
		HeldItemDefinition->WorldMesh,
		HeldItemDefinition->HandAttachSocketName);
}

void UDRHeldItemComponent::RefreshMiningSettings()
{
	if (UDRMiningComponent* Mining = MiningComponent.Get())
	{
		Mining->ApplyItemDefinition(HeldItemDefinition);
	}
}

void UDRHeldItemComponent::RefreshSnowComponents()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	const UDRRangedWeaponDefinition* RangedWeaponDefinition = Cast<UDRRangedWeaponDefinition>(HeldItemDefinition);

	if (!IsValid(Character) || !IsValid(RangedWeaponDefinition))
	{
		if (IsValid(SnowRemoveComponent))
		{
			SnowRemoveComponent->DestroyComponent();
			SnowRemoveComponent = nullptr;
		}

		return;
	}

	if (RangedWeaponDefinition->SnowAbsorbSettings.bEnabled && !IsValid(SnowRemoveComponent))
	{
		SnowRemoveComponent = NewObject<UDRSnowRemoveComponent>(Character);

		SnowRemoveComponent->SetIsReplicated(false);
		SnowRemoveComponent->RegisterComponent();
	}
	else if (!RangedWeaponDefinition->SnowAbsorbSettings.bEnabled && IsValid(SnowRemoveComponent))
	{
		SnowRemoveComponent->DestroyComponent();
		SnowRemoveComponent = nullptr;
	}
}

void UDRHeldItemComponent::PlayEquipSound()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->IsLocallyControlled() || !IsValid(HeldItemDefinition) || !IsValid(EquipSound))
	{
		return;
	}

	UGameplayStatics::PlaySound2D(Character, EquipSound);
}

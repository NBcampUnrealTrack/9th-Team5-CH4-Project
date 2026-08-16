#include "DRHeldItemComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Item/DRItemDefinition.h"

#include "DeepRaiders/Player/Components/DRMiningComponent.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"
#include "DeepRaiders/Player/Components/DRItemActionPresentationComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"


UDRHeldItemComponent::UDRHeldItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}


void UDRHeldItemComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>&
		OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(
		OutLifetimeProps);

	DOREPLIFETIME(
		UDRHeldItemComponent,
		HeldItemDefinition);
}

ADRPlayerCharacter* UDRHeldItemComponent::GetOwnerCharacter() const
{
	return Cast<ADRPlayerCharacter>(
		GetOwner());
}

void UDRHeldItemComponent::SetHeldItemDefinition(
	UDRItemDefinition* NewItemDefinition)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		HeldItemDefinition ==
			NewItemDefinition)
	{
		return;
	}

	HeldItemDefinition =
		NewItemDefinition;

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

void UDRHeldItemComponent::BeginPlay()
{
	Super::BeginPlay();

	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	MiningComponent =
		Character->FindComponentByClass<
			UDRMiningComponent>();

	MeleeCombatComponent =
		Character->FindComponentByClass<
			UDRMeleeCombatComponent>();

	PresentationComponent =
		Character->FindComponentByClass<
			UDRItemActionPresentationComponent>();

	if (!MiningComponent.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[HeldItem] MiningComponent missing. "
				"Character=%s"),
			*GetNameSafe(Character));
	}

	if (!MeleeCombatComponent.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[HeldItem] MeleeCombatComponent missing. "
				"Character=%s"),
			*GetNameSafe(Character));
	}

	if (!PresentationComponent.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[HeldItem] PresentationComponent missing. "
				"Character=%s"),
			*GetNameSafe(Character));
	}
}

void UDRHeldItemComponent::RefreshHeldItemState()
{
	RefreshVisual();

	RefreshMiningSettings();

	PlayEquipSound();
}

void UDRHeldItemComponent::RefreshVisual()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	if (!IsValid(HeldItemDefinition))
	{
		Character->
			ClearHandEquipmentVisual();

		return;
	}

	UStaticMesh* VisualMesh =
		HeldItemDefinition->WorldMesh;

	const FTransform WorldVisualTransform =
		HeldItemDefinition->SpawnOffsetTransform;

	Character->ApplyHandEquipmentVisual(
		VisualMesh,
		WorldVisualTransform);
}

void UDRHeldItemComponent::RefreshMiningSettings()
{
	if (UDRMiningComponent* Mining =
			MiningComponent.Get())
	{
		Mining->ApplyItemDefinition(HeldItemDefinition);
	}
}

void UDRHeldItemComponent::PlayEquipSound()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		!IsValid(HeldItemDefinition) ||
		!IsValid(EquipSound))
	{
		return;
	}

	UGameplayStatics::PlaySound2D(
		Character,
		EquipSound);
}

bool UDRHeldItemComponent::HasAction(
	EDRItemActionType ActionType) const
{
	if (!IsValid(HeldItemDefinition) ||
		ActionType ==
			EDRItemActionType::None)
	{
		return false;
	}

	return
		HeldItemDefinition->
			PrimaryAction == ActionType ||
		HeldItemDefinition->
			SecondaryAction == ActionType;
}

void UDRHeldItemComponent::RequestPrimaryAction(
	EDRItemActionTriggerEvent TriggerEvent)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		Character->IsDead() ||
		!IsValid(HeldItemDefinition))
	{
		return;
	}

	if (HeldItemDefinition->
			PrimaryActionTriggerEvent !=
		TriggerEvent)
	{
		return;
	}

	ExecuteAction(
		HeldItemDefinition->
			PrimaryAction);
}

void UDRHeldItemComponent::RequestSecondaryAction(
	EDRItemActionTriggerEvent TriggerEvent)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		Character->IsDead() ||
		!IsValid(HeldItemDefinition))
	{
		return;
	}

	if (HeldItemDefinition->
			SecondaryActionTriggerEvent !=
		TriggerEvent)
	{
		return;
	}

	ExecuteAction(
		HeldItemDefinition->
			SecondaryAction);
}

bool UDRHeldItemComponent::CanStartLocalAction() const
{
	const UWorld* World =
		GetWorld();

	return IsValid(World) &&
		World->GetTimeSeconds() >=
			NextLocalActionTime;
}

float UDRHeldItemComponent::GetActionCooldown(
	EDRItemActionType ActionType) const
{
	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		return DigActionCooldown;

	case EDRItemActionType::MeleeAttack:
		{
			const UDRMeleeCombatComponent* Melee =
				MeleeCombatComponent.Get();

			return IsValid(Melee)
				? Melee->GetAttackDuration()
				: 0.f;
		}

	default:
		return 0.f;
	}
}

void UDRHeldItemComponent::ExecuteAction(
    EDRItemActionType ActionType)
{
    ADRPlayerCharacter* Character =
        GetOwnerCharacter();

    UWorld* World =
        GetWorld();

    if (!IsValid(Character) ||
        !IsValid(World) ||
        !CanStartLocalAction())
    {
        return;
    }

    switch (ActionType)
    {
    case EDRItemActionType::Dig:
        {
            UDRMiningComponent* Mining =
                MiningComponent.Get();

            if (!IsValid(Mining) ||
                !Mining->TryMine())
            {
                return;
            }

            NextLocalActionTime =
                World->GetTimeSeconds() +
                GetActionCooldown(
                    EDRItemActionType::Dig);

            break;
        }

    case EDRItemActionType::MeleeAttack:
        {
            UDRMeleeCombatComponent* Melee =
                MeleeCombatComponent.Get();

            if (!IsValid(Melee))
            {
                return;
            }

            NextLocalActionTime =
                World->GetTimeSeconds() +
                GetActionCooldown(
                    EDRItemActionType::
                        MeleeAttack);
    		
            Melee->RequestAttack();

            break;
        }

    case EDRItemActionType::Throw:
        /*
         * Throw는 아직 실제 구현이
         * PlayerController에 있으므로
         * Character Facade 유지.
         */
        Character->RequestThrowHeldItem();
        break;

    case EDRItemActionType::None:
    default:
        break;
    }
}
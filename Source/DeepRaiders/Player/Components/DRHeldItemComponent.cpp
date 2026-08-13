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

	const FTransform
		FirstPersonVisualTransform =
			HeldItemDefinition->
				SpawnOffsetTransform *
			HeldItemDefinition->
				FirstPersonVisualOffsetTransform;

	const FTransform
		ThirdPersonVisualTransform =
			HeldItemDefinition->
				SpawnOffsetTransform;

	Character->
		ApplyHandEquipmentVisual(
			VisualMesh,
			VisualMesh,
			FirstPersonVisualTransform,
			ThirdPersonVisualTransform);
}

void UDRHeldItemComponent::RefreshMiningSettings()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	UDRMiningComponent* MiningComponent =
		Character->
			FindComponentByClass<
				UDRMiningComponent>();

	if (IsValid(MiningComponent))
	{
		MiningComponent->
			ApplyItemDefinition(
				HeldItemDefinition);
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
			const ADRPlayerCharacter*
				Character =
					GetOwnerCharacter();

			if (!IsValid(Character))
			{
				return 0.f;
			}

			const UDRMeleeCombatComponent*
				MeleeCombat =
					Character->
						FindComponentByClass<
							UDRMeleeCombatComponent>();

			return IsValid(MeleeCombat)
				? MeleeCombat->
					GetAttackDuration()
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

	if (!IsValid(Character) ||
		!CanStartLocalAction())
	{
		return;
	}

	UDRItemActionPresentationComponent*
		Presentation =
			Character->
				FindComponentByClass<
					UDRItemActionPresentationComponent>();

	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		{
			/*
			 * 실제 채굴 요청 실패 시
			 * Swing/Cooldown도 적용하지 않는다.
			 */
			if (!Character->RequestMine())
			{
				return;
			}

			NextLocalActionTime =
				GetWorld()->GetTimeSeconds() +
				GetActionCooldown(
					EDRItemActionType::Dig);

			if (IsValid(Presentation))
			{
				Presentation->
					PlayFirstPersonAction(
						EDRItemActionType::Dig);
			}

			break;
		}

	case EDRItemActionType::MeleeAttack:
		{
			NextLocalActionTime =
				GetWorld()->GetTimeSeconds() +
				GetActionCooldown(
					EDRItemActionType::
						MeleeAttack);

			if (IsValid(Presentation))
			{
				Presentation->
					PlayFirstPersonAction(
						EDRItemActionType::
							MeleeAttack);
			}

			Character->
				RequestMeleeAttack();

			break;
		}

	case EDRItemActionType::Throw:
		Character->
			RequestThrowHeldItem();
		break;

	case EDRItemActionType::None:
	default:
		break;
	}
}
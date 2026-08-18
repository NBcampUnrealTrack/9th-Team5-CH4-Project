#include "DRGA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRMeleeWeaponDefinition.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"

UDRGA_MeleeAttack::UDRGA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UDRGA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UDRMeleeWeaponItemDefinition* WeaponDefinition = Cast<UDRMeleeWeaponItemDefinition>(GetSourceObject(Handle, ActorInfo));
	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(WeaponDefinition) || !IsValid(Character) || !WeaponDefinition->DamageEffectClass || !IsValid(WeaponDefinition->ItemAnimationSet) || !IsValid(WeaponDefinition->ItemAnimationSet->PrimaryActionMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UDRMeleeCombatComponent* Melee = Character->FindComponentByClass<UDRMeleeCombatComponent>();
	if (!IsValid(Melee))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	/*
	 * 서버는 실제 판정 가능 여부를
	 * Commit 전에 검증.
	 */
	if (ActorInfo->IsNetAuthority() && !Melee->CanStartAttackFromAbility(WeaponDefinition))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	ActiveWeaponDefinition = WeaponDefinition;

	/*
	 * 실제 Hit 판정은 서버에서만.
	 */
	if (ActorInfo->IsNetAuthority())
	{
		ActiveMeleeComponent = Melee;

		MeleeHitDelegateHandle = Melee->OnMeleeHitDetected.AddUObject(this, &ThisClass::HandleMeleeHit);

		if (!Melee->StartAttackFromAbility(WeaponDefinition))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

			return;
		}
	}
	UAnimMontage* AttackMontage = WeaponDefinition->ItemAnimationSet->PrimaryActionMontage;

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, 1.f, NAME_None, true);

	if (!IsValid(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);

	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);

	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);

	MontageTask->ReadyForActivation();
}

void UDRGA_MeleeAttack::HandleMeleeHit(const FHitResult& HitResult)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	ADRPlayerCharacter* Attacker = Cast<ADRPlayerCharacter>(ActorInfo->AvatarActor.Get());
	ADRPlayerCharacter* Target = Cast<ADRPlayerCharacter>(HitResult.GetActor());
	UDRMeleeWeaponItemDefinition* WeaponDefinition = ActiveWeaponDefinition.Get();
	if (!IsValid(Attacker) || !IsValid(Target) || !IsValid(WeaponDefinition) || Target == Attacker || Target->IsDead())
	{
		return;
	}

	ADRPlayerState* AttackerPS = Attacker->GetPlayerState<ADRPlayerState>();
	ADRPlayerState* TargetPS = Target->GetPlayerState<ADRPlayerState>();
	if (!IsValid(AttackerPS) || !IsValid(TargetPS))
	{
		return;
	}

	const bool bSameTeam = AttackerPS->GetTeamId() == TargetPS->GetTeamId();
	const bool bTargetFrozen = TargetPS->IsFrozen();

	// ============================
	// Ally
	// ============================

	if (bSameTeam)
	{
		if (!bTargetFrozen)
		{
			return;
		}

		// Frozen Ally = Rescue
		TargetPS->ClearFrozenState();

		Attacker->PlayMeleeHitPresentationFromServer(Target, false, HitResult.ImpactPoint);

		return;
	}

	// ============================
	// Enemy
	// ============================

	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		return;
	}

	const float DamageAmount = bTargetFrozen
		                           // Frozen Enemy = Execute
		                           ? Target->GetCurrentHealth()

		                           // Normal Enemy
		                           : WeaponDefinition->BaseDamage;

	if (DamageAmount <= 0.f)
	{
		return;
	}

	const float HealthBefore = Target->GetCurrentHealth();
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(WeaponDefinition);

	FGameplayEffectSpecHandle DamageSpec = SourceASC->MakeOutgoingSpec(WeaponDefinition->DamageEffectClass, GetAbilityLevel(), Context);
	if (!DamageSpec.IsValid())
	{
		return;
	}

	DamageSpec.Data->SetSetByCallerMagnitude(DRGameplayTags::Data_Damage, DamageAmount);
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
	const float AppliedDamage = FMath::Max(0.f, HealthBefore - Target->GetCurrentHealth());
	if (AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const bool bKilled = Target->IsDead();
	Attacker->PlayMeleeHitPresentationFromServer(Target, bKilled, HitResult.ImpactPoint);
}

void UDRGA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo != nullptr && ActorInfo->IsNetAuthority())
	{
		if (UDRMeleeCombatComponent* Melee = ActiveMeleeComponent.Get())
		{
			if (MeleeHitDelegateHandle.IsValid())
			{
				Melee->OnMeleeHitDetected.Remove(MeleeHitDelegateHandle);

				MeleeHitDelegateHandle.Reset();
			}

			Melee->EndAttackFromAbility();
		}
	}

	ActiveMeleeComponent.Reset();
	ActiveWeaponDefinition.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UDRGA_MeleeAttack::HandleMontageCompleted()
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UDRGA_MeleeAttack::HandleMontageInterrupted()
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

#include "DRGA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"
#include "DeepRaiders/Item/DRMeleeWeaponDefinition.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "Kismet/GameplayStatics.h"

UDRGA_MeleeAttack::UDRGA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer InitialTags;
	InitialTags.AddTag(DRGameplayTags::Ability_Attack_Melee);
	SetAssetTags(InitialTags);

	ActivationBlockedTags.AddTag(DRGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_Frozen);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_VoxelContained);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_BlinkRecovery);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_MovementAction_Zipline);
	ActivationBlockedTags.AddTag(DRGameplayTags::State_QuickSlot_ActivationInterval);
}

void UDRGA_MeleeAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	const UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();

	if (IsValid(AbilitySystem)
		&& AbilitySystem->HasMatchingGameplayTag(
			DRGameplayTags::State_MovementAction_Zipline))
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

	UDRMeleeCombatComponent* Melee = Character->GetMeleeCombatComponent();
	if (!IsValid(Melee))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

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
	 * Swing Sound.
	 *
	 * LocalPredicted GA의 GameplayCue 경로를 사용.
	 * 별도 PresentationComponent / Multicast 없음.
	 */
	ExecuteSoundCue(DRGameplayTags::GameplayCue_Sound_Melee_Attack_Swing, Character, Character->GetActorLocation());

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
	AActor* HitActor = HitResult.GetActor();
	UDRMeleeWeaponItemDefinition* WeaponDefinition = ActiveWeaponDefinition.Get();
	if (!IsValid(Attacker) || !IsValid(HitActor) || !IsValid(WeaponDefinition) || HitActor == Attacker)
	{
		return;
	}

	if (ADRBreakableActor* BreakableTarget = Cast<ADRBreakableActor>(HitActor))
	{
		if (BreakableTarget->IsBroken()
			|| WeaponDefinition->BaseDamage <= 0.f)
		{
			return;
		}
		
		FVector DamageDirection = HitResult.TraceEnd - HitResult.TraceStart;
		
		if (!DamageDirection.Normalize())
		{
			DamageDirection = Attacker->GetActorForwardVector();
		}
		
		const float AppliedDamage = UGameplayStatics::ApplyPointDamage(BreakableTarget, WeaponDefinition->BaseDamage,
			DamageDirection, HitResult, Attacker->GetController(), Attacker, UDamageType::StaticClass());
		
		if (AppliedDamage > KINDA_SMALL_NUMBER)
		{
			// 오브젝트 피격 사운드 
			//ExecuteSoundCue(DRGameplayTags::GameplayCue_Sound_Attack_Hit, Attacker, HitResult.ImpactPoint);
		}
		
		return;
	}
	
	ADRPlayerCharacter* Target = Cast<ADRPlayerCharacter>(HitActor);
	
	if (!IsValid(Target) 
		|| Target->IsDead())
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
		
		ExecuteSoundCue(
			DRGameplayTags::
				GameplayCue_Sound_Melee_Attack_Hit,
			Attacker,
			HitResult.ImpactPoint);

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
	const FGameplayTag ImpactSoundTag =
		bKilled
			? DRGameplayTags::
				GameplayCue_Sound_Melee_Attack_Kill
			: DRGameplayTags::
				GameplayCue_Sound_Melee_Attack_Hit;

	ExecuteSoundCue(
		ImpactSoundTag,
		Attacker,
		HitResult.ImpactPoint);
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

void UDRGA_MeleeAttack::ExecuteSoundCue(
	const FGameplayTag& SoundCueTag,
	AActor* SourceActor,
	const FVector& Location) const
{
	UAbilitySystemComponent* ASC =
		GetAbilitySystemComponentFromActorInfo();

	if (!IsValid(ASC)
		|| !IsValid(SourceActor)
		|| !SoundCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters Parameters;

	Parameters.Location = Location;
	Parameters.Instigator = SourceActor;
	Parameters.EffectCauser = SourceActor;

	ASC->ExecuteGameplayCue(
		SoundCueTag,
		Parameters);
}

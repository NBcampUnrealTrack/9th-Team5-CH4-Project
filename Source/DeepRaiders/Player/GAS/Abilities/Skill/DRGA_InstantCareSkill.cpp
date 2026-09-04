#include "DRGA_InstantCareSkill.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

#include "DeepRaiders/Combat/Projectile/DRInstantCareProjectile.h"
#include "DeepRaiders/Combat/Throw/DRThrowActionTypes.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"

void UDRGA_InstantCareSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	const ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(Character) || !IsValid(PlayerState) || !IsValid(AbilitySystemComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[InstantCare][ActivateFailed] Reason=InvalidActorInfo"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ProjectileClass || !RecoveryEffectClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[InstantCare][ActivateFailed] Player=%s Reason=MissingClass Projectile=%s RecoveryEffect=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(ProjectileClass),
			*GetNameSafe(RecoveryEffectClass));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[InstantCare][ActivateFailed] Player=%s Reason=CommitFailed"),
			*GetNameSafe(Character));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
		const UDRPerkComponent* PerkComponent = PlayerState->GetPerkComponent();
		const FGameplayTag SkillId = IsValid(SkillDefinition)
			? SkillDefinition->SkillId
			: FGameplayTag();
		const float RadiusBonus = IsValid(PerkComponent)
			? PerkComponent->GetSkillEffectValue(
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				DRGameplayTags::Data_Perk_HotPack_AreaRadius)
			: 0.0f;
		const float RecoveryBonus = IsValid(PerkComponent)
			? PerkComponent->GetSkillEffectValue(
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				DRGameplayTags::Data_Perk_HotPack_HealthRecovery)
			: 0.0f;
		FGameplayEffectSpecHandle RecoveryEffectSpec = MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			RecoveryEffectClass,
			1.0f);

		if (RecoveryEffectSpec.IsValid())
		{
			RecoveryEffectSpec.Data->SetSetByCallerMagnitude(
				DRGameplayTags::Data_Health_Heal,
				HealthRecoveryAmount + RecoveryBonus);

			FVector LaunchDirection = Character->GetActorForwardVector();
			if (const AController* Controller = Character->GetController())
			{
				LaunchDirection = Controller->GetControlRotation().Vector();
			}

			const FVector LaunchLocation = DRThrow::ResolveLaunchLocation(
				Character,
				ActionSettings,
				LaunchDirection);
			const FTransform SpawnTransform(LaunchDirection.Rotation(), LaunchLocation);
			ADRInstantCareProjectile* Projectile = Character->GetWorld()->SpawnActorDeferred<ADRInstantCareProjectile>(
				ProjectileClass,
				SpawnTransform,
				Character,
				Character,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

			if (IsValid(Projectile))
			{
				FDRThrowableItemSettings ModifiedProjectileSettings;
				ModifiedProjectileSettings.InitialSpeed = InitialSpeed;
				ModifiedProjectileSettings.GravityScale = GravityScale;
				ModifiedProjectileSettings.ExplosionRadius = FMath::Max(0.0f, RecoveryRadius + RadiusBonus);

				TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
				ImpactEffectSpecs.Add(RecoveryEffectSpec);
				Projectile->InitializeThrowable(
					AbilitySystemComponent,
					ImpactEffectSpecs,
					ModifiedProjectileSettings,
					ActionSettings,
					PlayerState->GetTeamId(),
					SkillDefinition);
				Projectile->FinishSpawning(SpawnTransform);
				UE_LOG(
					LogTemp,
					Log,
					TEXT("[InstantCare][SpawnSucceeded] Player=%s Projectile=%s Location=%s Direction=%s Radius=%.1f Heal=%.1f Speed=%.1f Gravity=%.2f"),
					*GetNameSafe(Character),
					*GetNameSafe(Projectile),
					*LaunchLocation.ToCompactString(),
					*LaunchDirection.ToCompactString(),
					ModifiedProjectileSettings.ExplosionRadius,
					HealthRecoveryAmount + RecoveryBonus,
					InitialSpeed,
					GravityScale);
			}
			else
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[InstantCare][SpawnFailed] Player=%s Class=%s Location=%s"),
					*GetNameSafe(Character),
					*GetNameSafe(ProjectileClass),
					*LaunchLocation.ToCompactString());
			}
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[InstantCare][SpawnFailed] Player=%s Reason=InvalidRecoveryEffectSpec Effect=%s"),
				*GetNameSafe(Character),
				*GetNameSafe(RecoveryEffectClass));
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

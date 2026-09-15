#include "DRGA_SlowProjectileSkill.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Combat/Projectile/DRSlowProjectile.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRThrowableItemDefinition.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

bool UDRGA_SlowProjectileSkill::SpawnServerProjectile(
	const FVector& LaunchLocation,
	const FVector& LaunchDirection)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UDRThrowableItemDefinition* ThrowableDefinition = GetActiveDefinition();
	const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
	ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	UAbilitySystemComponent* AbilitySystem = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	UWorld* World = GetWorld();
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;

	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(ThrowableDefinition)
		|| !IsValid(SkillDefinition)
		|| SkillDefinition->EffectDuration <= 0.f
		|| !IsValid(PlayerState)
		|| !IsValid(AbilitySystem)
		|| !IsValid(World)
		|| !IsValid(AvatarActor)
		|| !ProjectileClass
		|| !SlowEffectClass)
	{
		return false;
	}

	const UDRPerkComponent* PerkComponent = PlayerState->GetPerkComponent();
	FGameplayEffectSpecHandle SlowSpec;
	FGameplayEffectSpecHandle AllySpec;
	if (!BuildEffectSpecs(SkillDefinition, PerkComponent, SlowSpec, AllySpec))
	{
		return false;
	}

	const FTransform SpawnTransform(LaunchDirection.Rotation(), LaunchLocation);
	ADRSlowProjectile* Projectile = World->SpawnActorDeferred<ADRSlowProjectile>(
		ProjectileClass,
		SpawnTransform,
		AvatarActor,
		Cast<APawn>(AvatarActor),
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(Projectile))
	{
		return false;
	}

	if (!CommitAbility(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo()))
	{
		Projectile->Destroy();
		return false;
	}

	FDRThrowableItemSettings ThrowSettings = ThrowableDefinition->ThrowSettings;
	ThrowSettings.ExplosionRadius = EffectRadius;
	Projectile->InitializeSlowProjectile(
		AbilitySystem,
		SlowSpec,
		AllySpec,
		ThrowSettings,
		GetThrowActionSettings(),
		GetSourceTeamId(),
		ThrowableDefinition);
	Projectile->FinishSpawning(SpawnTransform);
	ExecuteThrowGameplayCue(LaunchLocation, LaunchDirection);
	return true;
}

bool UDRGA_SlowProjectileSkill::BuildEffectSpecs(
	const UDRSkillDefinition* SkillDefinition,
	const UDRPerkComponent* PerkComponent,
	FGameplayEffectSpecHandle& OutSlowSpec,
	FGameplayEffectSpecHandle& OutAllySpec) const
{
	const FGameplayTag SkillId = SkillDefinition->SkillId;
	const bool IsExtremeSlow = IsValid(PerkComponent)
		&& PerkComponent->HasSkillPerk(
			SkillId, DRGameplayTags::Perk_Skill_SlowProjectile_ExtremeSlow);
	const bool IsAllySpeed = IsValid(PerkComponent)
		&& PerkComponent->HasSkillPerk(
			SkillId, DRGameplayTags::Perk_Skill_SlowProjectile_AllySpeed);
	if ((IsExtremeSlow && !ExtremeSlowEffectClass)
		|| (IsAllySpeed && !AllySpeedEffectClass))
	{
		return false;
	}

	OutSlowSpec = MakeOutgoingGameplayEffectSpec(
		IsExtremeSlow ? ExtremeSlowEffectClass : SlowEffectClass,
		GetAbilityLevel());
	OutAllySpec = IsAllySpeed
		? MakeOutgoingGameplayEffectSpec(AllySpeedEffectClass, GetAbilityLevel())
		: FGameplayEffectSpecHandle();
	if (!OutSlowSpec.IsValid() || (IsAllySpeed && !OutAllySpec.IsValid()))
	{
		return false;
	}

	const float SlowMagnitude = IsExtremeSlow
		? PerkComponent->GetSkillPerkEffectValue(
			SkillId,
			DRGameplayTags::Perk_Skill_SlowProjectile_ExtremeSlow,
			EDRSkillEffectTrigger::OnSkillCommitted,
			DRGameplayTags::Data_Effect_MoveSpeed)
		: BaseSlowMagnitude;
	OutSlowSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Effect_MoveSpeed, SlowMagnitude);
	OutSlowSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Effect_Duration, SkillDefinition->EffectDuration);

	if (OutAllySpec.IsValid())
	{
		const float AllySpeedMagnitude = PerkComponent->GetSkillPerkEffectValue(
			SkillId,
			DRGameplayTags::Perk_Skill_SlowProjectile_AllySpeed,
			EDRSkillEffectTrigger::OnSkillCommitted,
			DRGameplayTags::Data_Effect_MoveSpeed);
		OutAllySpec.Data->SetSetByCallerMagnitude(
			DRGameplayTags::Data_Effect_MoveSpeed, AllySpeedMagnitude);
		OutAllySpec.Data->SetSetByCallerMagnitude(
			DRGameplayTags::Data_Effect_Duration, SkillDefinition->EffectDuration);
	}
	return true;
}

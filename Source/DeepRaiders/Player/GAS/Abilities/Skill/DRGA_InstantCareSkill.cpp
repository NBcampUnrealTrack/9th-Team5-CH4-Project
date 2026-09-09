#include "DRGA_InstantCareSkill.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Combat/Projectile/DRInstantCareProjectile.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRThrowableItemDefinition.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

UDRGA_InstantCareSkill::UDRGA_InstantCareSkill()
{
	static ConstructorHelpers::FObjectFinder<UDRThrowableItemDefinition> ThrowableDefinitionFinder(
		TEXT("/Game/DeepRaiders/Item/Data/DataAssets/Throwable/DA_DRInstantCareThrowable.DA_DRInstantCareThrowable"));
	ThrowableDefinition = ThrowableDefinitionFinder.Object;
}

UDRThrowableItemDefinition* UDRGA_InstantCareSkill::ResolveThrowableDefinition(
	const FGameplayAbilitySpecHandle /*Handle*/,
	const FGameplayAbilityActorInfo* /*ActorInfo*/) const
{
	return ThrowableDefinition;
}

bool UDRGA_InstantCareSkill::SpawnServerProjectile(
	const FVector& LaunchLocation,
	const FVector& LaunchDirection)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UDRThrowableItemDefinition* ActiveThrowableDefinition = GetActiveDefinition();
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
		|| !IsValid(ActiveThrowableDefinition)
		|| !IsValid(SkillDefinition)
		|| !IsValid(PlayerState)
		|| !IsValid(AbilitySystem)
		|| !IsValid(World)
		|| !IsValid(AvatarActor)
		|| !ProjectileClass
		|| !RecoveryEffectClass)
	{
		return false;
	}

	const UDRPerkComponent* PerkComponent = PlayerState->GetPerkComponent();
	const FGameplayTag SkillId = SkillDefinition->SkillId;
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
		RecoveryEffectClass,
		GetAbilityLevel());
	if (!RecoveryEffectSpec.IsValid())
	{
		return false;
	}

	RecoveryEffectSpec.Data->SetSetByCallerMagnitude(
		DRGameplayTags::Data_Health_Heal,
		HealthRecoveryAmount + RecoveryBonus);

	const FTransform SpawnTransform(LaunchDirection.Rotation(), LaunchLocation);
	ADRInstantCareProjectile* Projectile = World->SpawnActorDeferred<ADRInstantCareProjectile>(
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

	FDRThrowableItemSettings ThrowSettings = ActiveThrowableDefinition->ThrowSettings;
	ThrowSettings.ExplosionRadius = FMath::Max(0.0f, RecoveryRadius + RadiusBonus);

	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	ImpactEffectSpecs.Add(RecoveryEffectSpec);
	Projectile->InitializeThrowable(
		AbilitySystem,
		ImpactEffectSpecs,
		ThrowSettings,
		GetThrowActionSettings(),
		GetSourceTeamId(),
		ActiveThrowableDefinition);
	Projectile->FinishSpawning(SpawnTransform);
	ExecuteThrowGameplayCue(LaunchLocation, LaunchDirection);
	return true;
}

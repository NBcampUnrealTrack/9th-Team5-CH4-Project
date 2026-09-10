#include "DRGA_TurretSkill.h"

#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "DeepRaiders/Combat/Placement/DRPlacementPreviewActor.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTargetActor.h"
#include "DeepRaiders/Combat/Projectile/DRCannonProjectile.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Combat/Projectile/DRSnowProjectile.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/Turret/DRTurret.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

UDRGA_TurretSkill::UDRGA_TurretSkill()
{
	FGameplayTagContainer TurretAbilityTags;
	TurretAbilityTags.AddTag(DRGameplayTags::Ability_Action);
	TurretAbilityTags.AddTag(DRGameplayTags::Ability_Skill);
	TurretAbilityTags.AddTag(DRGameplayTags::Ability_Skill_Turret);
	SetAssetTags(TurretAbilityTags);
	TurretCooldownTag = DRGameplayTags::Cooldown_Skill_Turret;

	TargetActorClass = ADRPlacementTargetActor::StaticClass();
	TurretClass = ADRTurret::StaticClass();

	TurretWeaponSettings.ProjectileClass = ADRSnowProjectile::StaticClass();
	static ConstructorHelpers::FObjectFinder<UDRProjectileWeaponItemDefinition> SnowPresentationFinder(
		TEXT("/Game/DeepRaiders/Item/Data/DataAssets/RangeWeapon/DA_DRSnowPistol.DA_DRSnowPistol"));
	if (SnowPresentationFinder.Succeeded())
	{
		TurretWeaponSettings.ProjectilePresentationDefinition = SnowPresentationFinder.Object;
	}

	static ConstructorHelpers::FClassFinder<ADRProjectile> CannonProjectileFinder(
		TEXT("/Game/DeepRaiders/Item/Weapons/Projectiles/BP_DRSnowProjectile_Cannon"));
	CannonProjectileClass = ADRCannonProjectile::StaticClass();
	if (CannonProjectileFinder.Succeeded())
	{
		CannonProjectileClass = CannonProjectileFinder.Class;
	}

	static ConstructorHelpers::FObjectFinder<UDRProjectileWeaponItemDefinition> CannonPresentationFinder(
		TEXT("/Game/DeepRaiders/Item/Data/DataAssets/RangeWeapon/DA_DRCannon.DA_DRCannon"));
	if (CannonPresentationFinder.Succeeded())
	{
		CannonProjectilePresentationDefinition = CannonPresentationFinder.Object;
	}
}

void UDRGA_TurretSkill::ResolveCooldownSettings(
	FGameplayTag& OutCooldownTag,
	float& OutCooldownDuration) const
{
	const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
	OutCooldownTag = IsValid(SkillDefinition)
		&& SkillDefinition->CooldownTag.IsValid()
		? SkillDefinition->CooldownTag
		: TurretCooldownTag;
	OutCooldownDuration = IsValid(SkillDefinition) && SkillDefinition->CooldownDuration > 0.f
		? SkillDefinition->CooldownDuration
		: TurretCooldownDuration;
}

bool UDRGA_TurretSkill::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	const ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	UWorld* World = IsValid(AvatarActor) ? AvatarActor->GetWorld() : nullptr;
	if (!IsValid(World) || !IsValid(PlayerState))
	{
		return false;
	}

	for (TActorIterator<ADRTurret> It(World); It; ++It)
	{
		if (IsValid(*It) && It->IsInstalledBy(PlayerState))
		{
			return false;
		}
	}

	return true;
}

FDRTurretWeaponSettings UDRGA_TurretSkill::ResolveWeaponSettings() const
{
	FDRTurretWeaponSettings WeaponSettings = TurretWeaponSettings;
	const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const ADRPlayerState* PlayerState = ActorInfo != nullptr
		? Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	const UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;
	if (!IsValid(SkillDefinition) || !IsValid(PerkComponent))
	{
		return WeaponSettings;
	}

	const FGameplayTag SkillId = SkillDefinition->SkillId;
	const FGameplayTag StatBoostPerkTags[] =
	{
		DRGameplayTags::Perk_Skill_Turret_StatBoost,
		DRGameplayTags::Perk_Skill_Turret_CannonProjectile
	};
	for (const FGameplayTag PerkTag : StatBoostPerkTags)
	{
		if (!PerkComponent->HasSkillPerk(SkillId, PerkTag))
		{
			continue;
		}

		const float DamageScale = 1.f + PerkComponent->GetSkillPerkEffectValue(
			SkillId,
			PerkTag,
			EDRSkillEffectTrigger::OnSkillCommitted,
			DRGameplayTags::Data_Perk_Turret_DamageBonusRatio);
		const float FireRateScale = 1.f + PerkComponent->GetSkillPerkEffectValue(
			SkillId,
			PerkTag,
			EDRSkillEffectTrigger::OnSkillCommitted,
			DRGameplayTags::Data_Perk_Turret_FireRateBonusRatio);
		const float RangeScale = 1.f + PerkComponent->GetSkillPerkEffectValue(
			SkillId,
			PerkTag,
			EDRSkillEffectTrigger::OnSkillCommitted,
			DRGameplayTags::Data_Perk_Turret_RangeBonusRatio);

		WeaponSettings.BreakableDamage *= FMath::Max(0.f, DamageScale);
		WeaponSettings.FireInterval /= FMath::Max(KINDA_SMALL_NUMBER, FireRateScale);
		WeaponSettings.MaxAttackDistance *= FMath::Max(0.f, RangeScale);
		for (FDRGameplayEffectData& ImpactEffect : WeaponSettings.ImpactEffects)
		{
			if (float* Damage = ImpactEffect.SetByCallerMagnitudes.Find(DRGameplayTags::Data_Damage))
			{
				*Damage *= FMath::Max(0.f, DamageScale);
			}
		}
	}

	if (CannonProjectileClass
		&& PerkComponent->HasSkillPerk(
			SkillId,
			DRGameplayTags::Perk_Skill_Turret_CannonProjectile))
	{
		WeaponSettings.ProjectileClass = CannonProjectileClass;
		WeaponSettings.ProjectilePresentationDefinition = CannonProjectilePresentationDefinition;
	}

	return WeaponSettings;
}

void UDRGA_TurretSkill::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 설치 시점에는 쿨다운을 걸지 않는다. 터렛이 사라질 때 ADRTurret이 쿨다운을 시작한다.
	NotifySkillCommitted(Handle, ActorInfo);
	NotifySkillActivated(Handle, ActorInfo);
}

void UDRGA_TurretSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsValid(GetPlayerCharacter(ActorInfo)) || !TargetActorClass || !PreviewActorClass || !TurretClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartTargeting();
}

void UDRGA_TurretSkill::StartTargeting()
{
	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(this, TEXT("TurretTargetData"),
		EGameplayTargetingConfirmation::UserConfirmed, TargetActorClass);
	if (!IsValid(TargetDataTask))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCanceled);
	TargetDataTask->ReadyForActivation();

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (!TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		// 서버는 클라이언트가 전송하는 TargetData를 기다린다.
		return;
	}

	ADRPlacementTargetActor* PlacementTargetActor = Cast<ADRPlacementTargetActor>(SpawnedTargetActor);
	if (!IsValid(PlacementTargetActor))
	{
		if (IsValid(SpawnedTargetActor))
		{
			SpawnedTargetActor->Destroy();
		}
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	PlacementTargetActor->Configure(PlacementSettings, PreviewActorClass, TurretDimensions);
	TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
}

void UDRGA_TurretSkill::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive() || TargetData.Num() != 1)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		FTransform TurretTransform;
		if (!ValidateServerTargetData(TargetData, TurretTransform)
			|| !CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
		ADRPlayerState* PlayerState = IsValid(AvatarPawn)
			? AvatarPawn->GetPlayerState<ADRPlayerState>()
			: nullptr;
		UWorld* World = GetWorld();
		ADRTurret* Turret = IsValid(World) && IsValid(PlayerState)
			? World->SpawnActorDeferred<ADRTurret>(TurretClass, TurretTransform,
				ActorInfo->AvatarActor.Get(), Cast<APawn>(ActorInfo->AvatarActor.Get()),
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn)
			: nullptr;
		if (!IsValid(Turret))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		FGameplayTag CooldownTag;
		float CooldownDuration = 0.f;
		ResolveCooldownSettings(CooldownTag, CooldownDuration);
		const FDRTurretWeaponSettings WeaponSettings = ResolveWeaponSettings();
		Turret->InitializeTurret(
			PlayerState,
			PlayerState->GetTeamId(),
			TurretLifeSpan,
			ActorInfo->AbilitySystemComponent.Get(),
			CooldownTag,
			CooldownDuration,
			WeaponSettings);
		Turret->FinishSpawning(TurretTransform);
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}

	if (ActorInfo->IsLocallyControlled()
		&& !CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
	}
}

void UDRGA_TurretSkill::HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
	}
}

bool UDRGA_TurretSkill::ValidateServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData,
	FTransform& OutTurretTransform) const
{
	OutTurretTransform = FTransform::Identity;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* ClientHit = Data != nullptr ? Data->GetHitResult() : nullptr;
	APlayerController* PlayerController = ActorInfo != nullptr ? ActorInfo->PlayerController.Get() : nullptr;
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = GetWorld();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority() || ClientHit == nullptr
		|| !IsValid(PlayerController) || !IsValid(AvatarActor) || !IsValid(World))
	{
		return false;
	}

	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	PlayerController->GetPlayerViewPoint(ServerViewLocation, ServerViewRotation);
	if (FVector::Dist(ServerViewLocation, ClientHit->TraceStart) > PlacementSettings.ServerViewOriginTolerance)
	{
		return false;
	}

	const FVector ClientAimDirection = (ClientHit->TraceEnd - ClientHit->TraceStart).GetSafeNormal();
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(PlacementSettings.ServerAimAngleTolerance, 0.f, 90.f)));
	if (ClientAimDirection.IsNearlyZero()
		|| FVector::DotProduct(ClientAimDirection, ServerViewRotation.Vector()) < MinimumDot)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRTurretServerAim), false);
	QueryParams.AddIgnoredActor(AvatarActor);
	FHitResult ServerHit;
	const FVector TraceEnd = ServerViewLocation + ClientAimDirection * PlacementSettings.MaxDistance;
	if (!World->LineTraceSingleByChannel(ServerHit, ServerViewLocation, TraceEnd,
		PlacementSettings.AimTraceChannel, QueryParams)
		|| !ADRPlacementTargetActor::IsValidPlacementSurface(ServerHit, PlacementSettings))
	{
		return false;
	}

	OutTurretTransform = MakeTurretTransform(ServerHit.ImpactPoint, ServerViewRotation);
	return true;
}

FTransform UDRGA_TurretSkill::MakeTurretTransform(const FVector& ImpactPoint,
	const FRotator& ViewRotation) const
{
	FVector FacingDirection = ViewRotation.Vector().GetSafeNormal2D();
	if (FacingDirection.IsNearlyZero())
	{
		FacingDirection = FVector::ForwardVector;
	}

	return FTransform(FacingDirection.Rotation(),
		ImpactPoint + FVector::UpVector * TurretGroundOffset);
}

void UDRGA_TurretSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (IsValid(TargetDataTask))
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

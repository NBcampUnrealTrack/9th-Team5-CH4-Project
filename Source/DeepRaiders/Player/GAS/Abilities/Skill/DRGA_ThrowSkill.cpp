#include "DRGA_ThrowSkill.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "DeepRaiders/Combat/Projectile/DRThrowableProjectile.h"
#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/Combat/Throw/DRThrowTargetActor.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/Animation/DRItemAnimationSet.h"
#include "DeepRaiders/Item/DRThrowableItemDefinition.h"
#include "DeepRaiders/Skill/DRThrowSkillDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDRGA_ThrowSkill::UDRGA_ThrowSkill()
{
	FGameplayTagContainer DefaultTags;
	DefaultTags.AddTag(DRGameplayTags::Ability_Skill);
	DefaultTags.AddTag(DRGameplayTags::Ability_Attack);
	DefaultTags.AddTag(DRGameplayTags::Ability_Throw);
	SetAssetTags(DefaultTags);

	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Throw);

	TargetActorClass = ADRThrowTargetActor::StaticClass();
}

bool UDRGA_ThrowSkill::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UDRThrowSkillDefinition* SkillDefinition = 
		Cast<UDRThrowSkillDefinition>(GetSourceObject(Handle, ActorInfo));
	const UDRThrowableItemDefinition* ThrowableDefinition = IsValid(SkillDefinition)
		? SkillDefinition->ThrowableDefinition
		: nullptr;

	return IsValid(ThrowableDefinition)
		&& ThrowableDefinition->ProjectileClass
		&& IsValid(ThrowableDefinition->ItemAnimationSet)
		&& IsValid(ThrowableDefinition->ItemAnimationSet->PrimaryActionMontage)
		&& TargetActorClass;
}

void UDRGA_ThrowSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const UDRThrowSkillDefinition* SkillDefinition =
		Cast<UDRThrowSkillDefinition>(GetSourceObject(Handle, ActorInfo));
	ActiveDefinition = IsValid(SkillDefinition) ? SkillDefinition->ThrowableDefinition : nullptr;
	if (!IsValid(ActiveDefinition)
		|| !ActiveDefinition->ProjectileClass
		|| !TargetActorClass)
	{
		CancelThrow();
		return;
	}

	bReleaseEventReceived = false;
	bEndingThrow = false;
	ValidatedAimDirection = FVector::ZeroVector;

	StartBlockingStateTasks();
	SetThrowAimState(true);
	StartTargeting();
}

void UDRGA_ThrowSkill::StartTargeting()
{
	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		TEXT("ThrowSkillTargetData"),
		EGameplayTargetingConfirmation::UserConfirmed,
		TargetActorClass);
	if (!IsValid(TargetDataTask))
	{
		CancelThrow();
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCanceled);
	TargetDataTask->ReadyForActivation();

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (!TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		// 서버는 클라이언트가 복제한 TargetData를 기다리므로 TargetActor를 생성하지 않는다.
		return;
	}

	ADRThrowTargetActor* ThrowTargetActor = Cast<ADRThrowTargetActor>(SpawnedTargetActor);
	if (!IsValid(ThrowTargetActor))
	{
		if (IsValid(SpawnedTargetActor))
		{
			SpawnedTargetActor->Destroy();
		}
		CancelThrow();
		return;
	}

	ThrowTargetActor->Configure(ActiveDefinition->ThrowSettings, ActionSettings, true);
	TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
}

void UDRGA_ThrowSkill::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive() || TargetData.Num() != 1)
	{
		CancelThrow();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		CancelThrow();
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		FVector ServerValidatedAimDirection;
		if (!ValidateServerTargetData(TargetData, ServerValidatedAimDirection))
		{
			CancelThrow();
			return;
		}

		ValidatedAimDirection = ServerValidatedAimDirection;
	}

	StartThrowMontage();
}

void UDRGA_ThrowSkill::HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	CancelThrow();
}

bool UDRGA_ThrowSkill::ValidateServerTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutAimDirection) const
{
	OutAimDirection = FVector::ZeroVector;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsValid(ActiveDefinition)
		|| TargetData.Num() != 1
		|| ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* ClientAimHit = Data != nullptr ? Data->GetHitResult() : nullptr;
	APlayerController* PlayerController = ActorInfo->PlayerController.Get();
	if (ClientAimHit == nullptr || !IsValid(PlayerController))
	{
		return false;
	}

	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	PlayerController->GetPlayerViewPoint(ServerViewLocation, ServerViewRotation);
	if (FVector::Dist(ServerViewLocation, ClientAimHit->TraceStart) > ActionSettings.ServerViewOriginTolerance)
	{
		return false;
	}

	const FVector ClientAimDirection = (ClientAimHit->TraceEnd - ClientAimHit->TraceStart).GetSafeNormal();
	if (ClientAimDirection.IsNearlyZero())
	{
		return false;
	}

	const float MaximumAimAngle = FMath::Clamp(ActionSettings.ServerAimAngleTolerance, 0.f, 90.f);
	const float MinimumAimDot = FMath::Cos(FMath::DegreesToRadians(MaximumAimAngle));
	if (FVector::DotProduct(ClientAimDirection, ServerViewRotation.Vector()) < MinimumAimDot)
	{
		return false;
	}

	OutAimDirection = ClientAimDirection;
	return true;
}

bool UDRGA_ThrowSkill::ResolveServerLaunchData(
	FVector& OutLaunchLocation,
	FVector& OutLaunchDirection) const
{
	OutLaunchLocation = FVector::ZeroVector;
	OutLaunchDirection = FVector::ZeroVector;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsValid(ActiveDefinition)
		|| ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	const FVector LockedAimDirection = ValidatedAimDirection.GetSafeNormal();
	APlayerController* PlayerController = ActorInfo->PlayerController.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (LockedAimDirection.IsNearlyZero()
		|| !IsValid(PlayerController)
		|| !IsValid(AvatarActor)
		|| !IsValid(World))
	{
		return false;
	}

	FVector CurrentViewLocation;
	FRotator CurrentViewRotation;
	PlayerController->GetPlayerViewPoint(CurrentViewLocation, CurrentViewRotation);

	const float MaximumAimDistance = FMath::Max(ActiveDefinition->ThrowSettings.MaxAimDistance, 1.f);
	const FVector ServerTraceEnd = CurrentViewLocation + LockedAimDirection * MaximumAimDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRServerThrowSkillAim), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	FHitResult ServerAimHit;
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		ServerAimHit,
		CurrentViewLocation,
		ServerTraceEnd,
		ActionSettings.AimTraceChannel,
		QueryParams);
	const FVector AimPoint = bBlockingHit ? ServerAimHit.ImpactPoint : ServerTraceEnd;

	OutLaunchLocation = DRThrow::ResolveLaunchLocation(AvatarActor, ActionSettings, LockedAimDirection);
	OutLaunchDirection = (AimPoint - OutLaunchLocation).GetSafeNormal();
	return !OutLaunchDirection.IsNearlyZero();
}

void UDRGA_ThrowSkill::StartThrowMontage()
{
	if (!IsValid(ActiveDefinition)
		|| !IsValid(ActiveDefinition->ItemAnimationSet)
		|| !IsValid(ActiveDefinition->ItemAnimationSet->PrimaryActionMontage))
	{
		CancelThrow();
		return;
	}

	ReleaseEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		DRGameplayTags::Event_Ability_Throw_Release,
		nullptr,
		true,
		true);
	if (!IsValid(ReleaseEventTask))
	{
		CancelThrow();
		return;
	}

	ReleaseEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleThrowReleaseEvent);
	ReleaseEventTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("ThrowSkillMontage"),
			ActiveDefinition->ItemAnimationSet->PrimaryActionMontage,
			1.f,
			NAME_None,
			false);
	if (!IsValid(MontageTask))
	{
		CancelThrow();
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UDRGA_ThrowSkill::HandleThrowReleaseEvent(FGameplayEventData Payload)
{
	if (!IsActive() || bReleaseEventReceived)
	{
		return;
	}

	bReleaseEventReceived = true;
	SetThrowAimState(false);

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		CancelThrow();
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		ExecuteConfirmedThrow();
		return;
	}

	if (ActorInfo->IsLocallyControlled()
		&& !CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		CancelThrow();
	}
}

void UDRGA_ThrowSkill::ExecuteConfirmedThrow()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	FVector LaunchLocation;
	FVector LaunchDirection;
	if (!ResolveServerLaunchData(LaunchLocation, LaunchDirection)
		|| !SpawnServerProjectile(LaunchLocation, LaunchDirection))
	{
		CancelThrow();
		return;
	}

	EndAbility(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		true,
		false);
}

bool UDRGA_ThrowSkill::SpawnServerProjectile(
	const FVector& LaunchLocation,
	const FVector& LaunchDirection)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(ActiveDefinition)
		|| !ActiveDefinition->ProjectileClass)
	{
		return false;
	}

	UWorld* World = GetWorld();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(World)
		|| !IsValid(AvatarActor)
		|| !IsValid(AbilitySystem))
	{
		return false;
	}

	const FTransform SpawnTransform(LaunchDirection.Rotation(), LaunchLocation);
	ADRThrowableProjectile* Projectile = World->SpawnActorDeferred<ADRThrowableProjectile>(
		ActiveDefinition->ProjectileClass,
		SpawnTransform,
		AvatarActor,
		AvatarPawn,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(Projectile))
	{
		return false;
	}

	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		Projectile->Destroy();
		return false;
	}

	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	BuildImpactEffectSpecs(ImpactEffectSpecs);
	Projectile->InitializeThrowable(
		AbilitySystem,
		ImpactEffectSpecs,
		ActiveDefinition->ThrowSettings,
		ActionSettings,
		GetSourceTeamId(),
		ActiveDefinition);
	Projectile->FinishSpawning(SpawnTransform);

	ExecuteThrowGameplayCue(LaunchLocation, LaunchDirection);
	return true;
}

void UDRGA_ThrowSkill::ExecuteThrowGameplayCue(
	const FVector& LaunchLocation,
	const FVector& LaunchDirection)
{
	if (!IsValid(ActiveDefinition) || !ActiveDefinition->ThrowGameplayCueTag.IsValid())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AbilitySystem) || !IsValid(AvatarActor))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = LaunchLocation;
	Parameters.Normal = LaunchDirection;
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;
	Parameters.SourceObject = ActiveDefinition;
	AbilitySystem->ExecuteGameplayCue(ActiveDefinition->ThrowGameplayCueTag, Parameters);
}

void UDRGA_ThrowSkill::BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const
{
	OutEffectSpecs.Reset();
	if (!IsValid(ActiveDefinition))
	{
		return;
	}

	for (const FDRGameplayEffectData& EffectData : ActiveDefinition->ThrowSettings.ImpactEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}

		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActorInfo(),
			GetCurrentActivationInfo(),
			EffectData.EffectClass,
			EffectData.EffectLevel);
		if (!Spec.IsValid())
		{
			continue;
		}

		for (const TPair<FGameplayTag, float>& Magnitude : EffectData.SetByCallerMagnitudes)
		{
			if (Magnitude.Key.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
			}
		}

		OutEffectSpecs.Add(Spec);
	}
}

int32 UDRGA_ThrowSkill::GetSourceTeamId() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		return INDEX_NONE;
	}

	const int32 OwnerTeamId = DRCombatTeam::GetActorTeamId(ActorInfo->OwnerActor.Get());
	return OwnerTeamId != INDEX_NONE
		? OwnerTeamId
		: DRCombatTeam::GetActorTeamId(ActorInfo->AvatarActor.Get());
}

void UDRGA_ThrowSkill::StartBlockingStateTasks()
{
	UAbilityTask_WaitGameplayTagAdded* DeadTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this,
		DRGameplayTags::State_Dead,
		nullptr,
		true);
	DeadTask->Added.AddDynamic(this, &ThisClass::HandleBlockingStateAdded);
	DeadTask->ReadyForActivation();

	UAbilityTask_WaitGameplayTagAdded* FrozenTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this,
		DRGameplayTags::State_Frozen,
		nullptr,
		true);
	FrozenTask->Added.AddDynamic(this, &ThisClass::HandleBlockingStateAdded);
	FrozenTask->ReadyForActivation();

	UAbilityTask_WaitGameplayTagAdded* VoxelContainedTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this,
		DRGameplayTags::State_VoxelContained,
		nullptr,
		true);
	VoxelContainedTask->Added.AddDynamic(this, &ThisClass::HandleBlockingStateAdded);
	VoxelContainedTask->ReadyForActivation();
}

void UDRGA_ThrowSkill::HandleBlockingStateAdded()
{
	CancelThrow();
}

void UDRGA_ThrowSkill::SetThrowAimState(bool bEnable)
{
	if (bUsingThrowAimState == bEnable)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* AbilitySystem = ActorInfo != nullptr
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		bUsingThrowAimState = false;
		return;
	}

	if (bEnable)
	{
		AbilitySystem->AddLooseGameplayTag(DRGameplayTags::State_Aiming_Throw);
	}
	else
	{
		AbilitySystem->RemoveLooseGameplayTag(DRGameplayTags::State_Aiming_Throw);
	}

	bUsingThrowAimState = bEnable;
}

void UDRGA_ThrowSkill::HandleMontageCompleted()
{
	if (!bReleaseEventReceived)
	{
		CancelThrow();
	}
}

void UDRGA_ThrowSkill::HandleMontageInterrupted()
{
	CancelThrow();
}

void UDRGA_ThrowSkill::CancelThrow()
{
	if (IsActive())
	{
		EndAbility(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActorInfo(),
			GetCurrentActivationInfo(),
			true,
			true);
	}
}

void UDRGA_ThrowSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bEndingThrow)
	{
		return;
	}
	bEndingThrow = true;

	SetThrowAimState(false);

	if (bWasCancelled)
	{
		MontageStop();
	}

	if (IsValid(TargetDataTask))
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}

	if (IsValid(ReleaseEventTask))
	{
		ReleaseEventTask->EndTask();
		ReleaseEventTask = nullptr;
	}

	ActiveDefinition = nullptr;
	ValidatedAimDirection = FVector::ZeroVector;
	bReleaseEventReceived = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bEndingThrow = false;
}

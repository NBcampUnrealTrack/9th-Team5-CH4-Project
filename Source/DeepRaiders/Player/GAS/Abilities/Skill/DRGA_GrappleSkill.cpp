#include "DRGA_GrappleSkill.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "DeepRaiders/Combat/Grapple/DRGrappleTargetActor.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UDRGA_GrappleSkill::UDRGA_GrappleSkill()
{
	FGameplayTagContainer InitialAbilityTags;
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Action);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Skill);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Skill_Grapple);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_MovementAction);
	InitialAbilityTags.AddTag(DRGameplayTags::Ability_Input_SecondaryCancel);
	SetAssetTags(InitialAbilityTags);

	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_MovementAction);
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Skill);
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Attack);
	BlockAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Snow_Absorb);

	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Skill);
	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Attack);
	CancelAbilitiesWithTag.AddTag(DRGameplayTags::Ability_Snow_Absorb);

	TargetActorClass = ADRGrappleTargetActor::StaticClass();
}

bool UDRGA_GrappleSkill::CanActivateAbility(
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

	const UDRSkillDefinition* SkillDefinition = ActorInfo != nullptr
		? Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo))
		: nullptr;
	const ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	const UDRMovementActionComponent* MovementAction = IsValid(Character)
		? Character->GetMovementActionComponent()
		: nullptr;

	return IsValid(SkillDefinition)
		&& TargetActorClass
		&& IsValid(MovementAction)
		&& !MovementAction->IsMovementActionActive();
}

void UDRGA_GrappleSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ActiveSkillDefinition = Cast<UDRSkillDefinition>(GetSourceObject(Handle, ActorInfo));
	if (!IsValid(ActiveSkillDefinition))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	GrapplePhase = EDRGrappleSkillPhase::Targeting;
	StartTargeting();
}

void UDRGA_GrappleSkill::StartTargeting()
{
	if (GetCurrentActorInfo() == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		TEXT("GrappleSkillTargetData"),
		EGameplayTargetingConfirmation::Instant,
		TargetActorClass);

	if (!IsValid(TargetDataTask))
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCanceled);

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		ADRGrappleTargetActor* GrappleTargetActor = Cast<ADRGrappleTargetActor>(SpawnedTargetActor);
		if (!IsValid(GrappleTargetActor))
		{
			SpawnedTargetActor->Destroy();
			QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
			return;
		}

		GrappleTargetActor->Configure(GrappleSettings);
		TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
}

void UDRGA_GrappleSkill::StartCancelEventTask()
{
	if (IsValid(CancelEventTask))
	{
		return;
	}

	CancelEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		DRGameplayTags::Event_MovementAction_Cancel,
		nullptr,
		true,
		true);

	if (!IsValid(CancelEventTask))
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	CancelEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleCancelEventReceived);
	CancelEventTask->ReadyForActivation();
}

bool UDRGA_GrappleSkill::BeginHookFlight(const FVector& InHookLocation, const FVector& InHookNormal)
{
	if (!IsActive()
		|| GrapplePhase != EDRGrappleSkillPhase::Targeting
		|| InHookLocation.ContainsNaN()
		|| InHookNormal.ContainsNaN())
	{
		return false;
	}

	const FVector SafeHookNormal = InHookNormal.GetSafeNormal();
	UWorld* World = GetWorld();
	if (SafeHookNormal.IsNearlyZero() || !IsValid(World))
	{
		return false;
	}

	HookLocation = InHookLocation;
	HookSurfaceNormal = SafeHookNormal;
	HookFlightDuration = CalculateHookFlightDuration(HookLocation);
	GrapplePhase = EDRGrappleSkillPhase::HookFlying;

	ApplyMovementActionTag();
	StartGrappleGameplayCue(HookLocation, HookSurfaceNormal);

	if (HookFlightDuration <= KINDA_SMALL_NUMBER)
	{
		HandleHookFlightFinished();
		return true;
	}

	World->GetTimerManager().SetTimer(
		HookFlightTimerHandle,
		this,
		&ThisClass::HandleHookFlightFinished,
		HookFlightDuration,
		false);
	return true;
}

float UDRGA_GrappleSkill::CalculateHookFlightDuration(const FVector& InHookLocation) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	const float MinimumDuration = FMath::Max(GrappleSettings.MinimumHookFlightDuration, 0.f);

	if (!IsValid(AvatarActor) || InHookLocation.ContainsNaN())
	{
		return MinimumDuration;
	}

	const float TravelSpeed = FMath::Max(GrappleSettings.HookTravelSpeed, 0.f);
	if (TravelSpeed <= KINDA_SMALL_NUMBER)
	{
		return MinimumDuration;
	}

	return FMath::Max(FVector::Distance(AvatarActor->GetActorLocation(), InHookLocation) / TravelSpeed, MinimumDuration);
}

void UDRGA_GrappleSkill::HandleHookFlightFinished()
{
	HookFlightTimerHandle.Invalidate();

	if (!IsActive() || GrapplePhase != EDRGrappleSkillPhase::HookFlying)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	bool bMovementStartedSuccessfully = false;
	if (ActorInfo->IsNetAuthority())
	{
		bMovementStartedSuccessfully = StartAuthoritativeMovement();
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		bMovementStartedSuccessfully = StartPredictedMovement();
	}

	if (!bMovementStartedSuccessfully)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
	}
}

void UDRGA_GrappleSkill::HandleTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (!IsActive() || GrapplePhase != EDRGrappleSkillPhase::Targeting || TargetData.Num() != 1)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		FVector ServerTargetLocation;
		FVector ServerTargetNormal;
		const EDRTargetValidationResult ValidationResult =
			ValidateServerTargetData(TargetData, ServerTargetLocation, ServerTargetNormal);

		if (ValidationResult == EDRTargetValidationResult::Succeeded)
		{
			if (!BeginHookFlight(ServerTargetLocation, ServerTargetNormal))
			{
				QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
			}
			return;
		}

		if (ValidationResult == EDRTargetValidationResult::Failed)
		{
			PlayFailedGrappleGameplayCue(ServerTargetLocation);
		}

		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	if (!ActorInfo->IsLocallyControlled())
	{
		return;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* ClientHit = Data != nullptr ? Data->GetHitResult() : nullptr;
	if (ClientHit == nullptr)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	if (ADRGrappleTargetActor::IsValidGrappleSurface(*ClientHit))
	{
		if (!BeginHookFlight(ClientHit->ImpactPoint, ClientHit->ImpactNormal))
		{
			QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		}
		return;
	}

	const FVector FailedLocation = ClientHit->IsValidBlockingHit() ? ClientHit->ImpactPoint : ClientHit->TraceEnd;
	PlayFailedGrappleGameplayCue(FailedLocation);
	QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
}

void UDRGA_GrappleSkill::HandleTargetDataCanceled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	QueueEndGrapple(EDRMovementActionEndReason::Cancelled);
}

void UDRGA_GrappleSkill::HandleCancelEventReceived(FGameplayEventData Payload)
{
	if (GrapplePhase == EDRGrappleSkillPhase::Grappling && bMovementStarted)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Cancelled);
	}
}

bool UDRGA_GrappleSkill::StartPredictedMovement()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = GetWorld();
	if (ActorInfo == nullptr
		|| ActorInfo->IsNetAuthority()
		|| !ActorInfo->IsLocallyControlled()
		|| !IsValid(World)
		|| GrapplePhase != EDRGrappleSkillPhase::HookFlying
		|| HookLocation.ContainsNaN()
		|| HookSurfaceNormal.IsNearlyZero())
	{
		return false;
	}

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	UDRMovementActionComponent* MovementAction = IsValid(Character) ? Character->GetMovementActionComponent() : nullptr;
	UDRCharacterMovementComponent* Movement = IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	if (!IsValid(MovementAction) || !IsValid(Movement))
	{
		return false;
	}

	if (!MovementAction->StartPredictedMovementAction(BuildMovementActionState(HookLocation)))
	{
		return false;
	}

	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		MovementAction->EndMovementAction(EDRMovementActionEndReason::Invalidated);
		return false;
	}

	MovementAction->OnMovementActionEnded.RemoveAll(this);
	MovementAction->OnMovementActionSimulated.RemoveAll(this);
	MovementAction->OnMovementActionEnded.AddUObject(this, &ThisClass::HandleMovementActionEnded);
	MovementAction->OnMovementActionSimulated.AddUObject(this, &ThisClass::HandleMovementActionSimulated);
	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);

	MovementStartTimeSeconds = World->GetTimeSeconds();
	bMovementStarted = true;
	GrapplePhase = EDRGrappleSkillPhase::Grappling;
	StartCancelEventTask();
	return true;
}

bool UDRGA_GrappleSkill::StartAuthoritativeMovement()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = GetWorld();
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(World)
		|| !IsValid(ActiveSkillDefinition)
		|| GrapplePhase != EDRGrappleSkillPhase::HookFlying
		|| HookLocation.ContainsNaN()
		|| HookSurfaceNormal.IsNearlyZero())
	{
		return false;
	}

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	UDRMovementActionComponent* MovementAction = IsValid(Character) ? Character->GetMovementActionComponent() : nullptr;
	UDRCharacterMovementComponent* Movement = IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	if (!IsValid(MovementAction) || !IsValid(Movement))
	{
		return false;
	}

	if (!MovementAction->StartAuthoritativeMovementAction(BuildMovementActionState(HookLocation)))
	{
		return false;
	}

	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		MovementAction->EndMovementAction(EDRMovementActionEndReason::Invalidated);
		return false;
	}

	MovementAction->OnMovementActionEnded.RemoveAll(this);
	MovementAction->OnMovementActionSimulated.RemoveAll(this);
	MovementAction->OnMovementActionEnded.AddUObject(this, &ThisClass::HandleMovementActionEnded);
	MovementAction->OnMovementActionSimulated.AddUObject(this, &ThisClass::HandleMovementActionSimulated);
	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);

	MovementStartTimeSeconds = World->GetTimeSeconds();
	bMovementStarted = true;
	GrapplePhase = EDRGrappleSkillPhase::Grappling;
	StartCancelEventTask();
	return true;
}

FDRMovementActionState UDRGA_GrappleSkill::BuildMovementActionState(const FVector& InHookLocation) const
{
	FDRMovementActionState State;
	State.bActive = true;
	State.ActionType = EDRMovementActionType::Grapple;
	State.SessionId = ResolveSessionId();
	State.ReferenceLocation = InHookLocation;
	State.ActionAcceleration = GrappleSettings.PullAcceleration;
	State.MaxSpeed = GrappleSettings.MaxSpeed;
	State.ControlScale = GrappleSettings.ControlScale;
	State.ViewDirectionWeight = GrappleSettings.ViewPullWeight;
	State.MinimumReferenceClosingSpeed = FMath::Max(GrappleSettings.MinimumClosingSpeed, 0.f);
	return State;
}

void UDRGA_GrappleSkill::ApplyMovementActionTag()
{
	if (bMovementActionTagApplied)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsValid(ASC) || ActorInfo == nullptr)
	{
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		ASC->AddLooseGameplayTag(
			DRGameplayTags::State_MovementAction_Active,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		ASC->AddLooseGameplayTag(DRGameplayTags::State_MovementAction_Active);
	}

	bMovementActionTagApplied = true;
}

void UDRGA_GrappleSkill::RemoveMovementActionTag()
{
	if (!bMovementActionTagApplied)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!IsValid(ASC) || ActorInfo == nullptr)
	{
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		ASC->RemoveLooseGameplayTag(
			DRGameplayTags::State_MovementAction_Active,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		ASC->RemoveLooseGameplayTag(DRGameplayTags::State_MovementAction_Active);
	}

	bMovementActionTagApplied = false;
}

UDRGA_GrappleSkill::EDRTargetValidationResult UDRGA_GrappleSkill::ValidateServerTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutTargetLocation,
	FVector& OutTargetNormal) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutTargetNormal = FVector::ZeroVector;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority()
		|| TargetData.Num() != 1
		|| !IsValid(ActiveSkillDefinition))
	{
		return EDRTargetValidationResult::InvalidRequest;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* ClientHit = Data != nullptr ? Data->GetHitResult() : nullptr;
	APlayerController* PlayerController = ActorInfo->PlayerController.Get();
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (ClientHit == nullptr
		|| !IsValid(PlayerController)
		|| !IsValid(AvatarActor)
		|| !IsValid(World))
	{
		return EDRTargetValidationResult::InvalidRequest;
	}

	FVector ServerViewLocation;
	FRotator ServerViewRotation;
	PlayerController->GetPlayerViewPoint(ServerViewLocation, ServerViewRotation);

	if (FVector::Dist(ServerViewLocation, ClientHit->TraceStart) > GrappleSettings.ServerViewOriginTolerance)
	{
		return EDRTargetValidationResult::InvalidRequest;
	}

	const FVector ClientAimDirection = (ClientHit->TraceEnd - ClientHit->TraceStart).GetSafeNormal();
	if (ClientAimDirection.IsNearlyZero())
	{
		return EDRTargetValidationResult::InvalidRequest;
	}

	const float MaxAngle = FMath::Clamp(GrappleSettings.ServerAimAngleTolerance, 0.f, 90.f);
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(MaxAngle));
	if (FVector::DotProduct(ClientAimDirection, ServerViewRotation.Vector()) < MinimumDot)
	{
		return EDRTargetValidationResult::InvalidRequest;
	}

	const FVector TraceEnd = ServerViewLocation + ClientAimDirection * GrappleSettings.MaxDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRGrappleSkillServerAim), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	FHitResult ServerHit;
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		ServerHit,
		ServerViewLocation,
		TraceEnd,
		GrappleSettings.AimTraceChannel,
		QueryParams);

	if (!bBlockingHit || !ADRGrappleTargetActor::IsValidGrappleSurface(ServerHit))
	{
		OutTargetLocation = bBlockingHit ? ServerHit.ImpactPoint : TraceEnd;
		OutTargetNormal = bBlockingHit ? ServerHit.ImpactNormal.GetSafeNormal() : FVector::ZeroVector;
		return EDRTargetValidationResult::Failed;
	}

	OutTargetLocation = ServerHit.ImpactPoint;
	OutTargetNormal = ServerHit.ImpactNormal.GetSafeNormal();
	return EDRTargetValidationResult::Succeeded;
}

int32 UDRGA_GrappleSkill::ResolveSessionId() const
{
	const int32 PredictionKey = static_cast<int32>(GetCurrentActivationInfo().GetActivationPredictionKey().Current);
	return FMath::Max(PredictionKey, 1);
}

FVector UDRGA_GrappleSkill::ResolveViewDirection() const
{
	const ADRPlayerCharacter* Character = GetPlayerCharacter(GetCurrentActorInfo());
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const FVector ViewDirection = Character->GetBaseAimRotation().Vector().GetSafeNormal();
	return !ViewDirection.IsNearlyZero() ? ViewDirection : Character->GetActorForwardVector().GetSafeNormal();
}

void UDRGA_GrappleSkill::HandleMovementActionSimulated(const FDRMovementActionSimulationResult& Result)
{
	if (GrapplePhase != EDRGrappleSkillPhase::Grappling
		|| !bMovementStarted
		|| !IsValid(ActiveSkillDefinition))
	{
		return;
	}

	const float ElapsedTime = GetMovementElapsedTime();
	if (GrappleSettings.MaxDuration > 0.f && ElapsedTime >= GrappleSettings.MaxDuration)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
		return;
	}

	const FVector ToHook = HookLocation - Result.Location;
	const float ArrivalDistance = FMath::Max(GrappleSettings.ArrivalDistance, 0.f);
	if (ToHook.SizeSquared() <= FMath::Square(ArrivalDistance))
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
		return;
	}

	if (ElapsedTime < GrappleSettings.ViewDetachProtectionDuration)
	{
		return;
	}

	if (HookSurfaceNormal.IsNearlyZero())
	{
		QueueEndGrapple(EDRMovementActionEndReason::Invalidated);
		return;
	}

	const FVector HookToCharacterDirection = (Result.Location - HookLocation).GetSafeNormal();
	if (HookToCharacterDirection.IsNearlyZero())
	{
		return;
	}

	if (FVector::DotProduct(HookSurfaceNormal, HookToCharacterDirection) <= 0.f)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
		return;
	}

	const FVector ToHookDirection = ToHook.GetSafeNormal();
	const FVector ViewDirection = ResolveViewDirection();
	if (!ToHookDirection.IsNearlyZero()
		&& !ViewDirection.IsNearlyZero()
		&& FVector::DotProduct(ViewDirection, ToHookDirection) <= 0.f)
	{
		QueueEndGrapple(EDRMovementActionEndReason::Completed);
	}
}

void UDRGA_GrappleSkill::HandleMovementActionEnded(EDRMovementActionEndReason EndReason)
{
	if (!bEndingGrapple)
	{
		QueueEndGrapple(EndReason);
	}
}

void UDRGA_GrappleSkill::QueueEndGrapple(EDRMovementActionEndReason EndReason)
{
	if (bEndQueued || bEndingGrapple)
	{
		return;
	}

	bEndQueued = true;
	PendingEndReason = EndReason;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		ApplyQueuedGrappleEnd();
		return;
	}

	EndGrappleTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::ApplyQueuedGrappleEnd));
}

void UDRGA_GrappleSkill::ApplyQueuedGrappleEnd()
{
	EndGrappleTimerHandle.Invalidate();

	if (!IsActive() || bEndingGrapple)
	{
		bEndQueued = false;
		PendingEndReason = EDRMovementActionEndReason::Invalidated;
		return;
	}

	EndAbility(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		true,
		PendingEndReason != EDRMovementActionEndReason::Completed);
}

void UDRGA_GrappleSkill::StopMovementAction(EDRMovementActionEndReason EndReason)
{
	bEndingGrapple = true;

	ADRPlayerCharacter* Character = GetPlayerCharacter(GetCurrentActorInfo());
	if (IsValid(Character))
	{
		UDRMovementActionComponent* MovementAction = Character->GetMovementActionComponent();
		UDRCharacterMovementComponent* Movement =
			Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());
		const bool bShouldPreserveMomentum = bMovementStarted
			&& (EndReason == EDRMovementActionEndReason::Completed
				|| EndReason == EDRMovementActionEndReason::Cancelled);

		if (IsValid(Movement) && bShouldPreserveMomentum)
		{
			Movement->BeginAirborneMomentumPreservation();
		}

		if (IsValid(MovementAction) && MovementAction->IsMovementActionActive())
		{
			MovementAction->EndMovementAction(EndReason);
		}

		if (IsValid(MovementAction))
		{
			MovementAction->OnMovementActionEnded.RemoveAll(this);
			MovementAction->OnMovementActionSimulated.RemoveAll(this);
		}

		if (IsValid(Movement) && Movement->IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
		{
			Movement->ExitCustomMovementMode();
		}
	}

	bMovementStarted = false;
	bEndingGrapple = false;
}

float UDRGA_GrappleSkill::GetMovementElapsedTime() const
{
	const UWorld* World = GetWorld();
	if (!bMovementStarted || !IsValid(World) || MovementStartTimeSeconds < 0.f)
	{
		return 0.f;
	}

	return FMath::Max(World->GetTimeSeconds() - MovementStartTimeSeconds, 0.f);
}

void UDRGA_GrappleSkill::StartGrappleGameplayCue(
	const FVector& InHookLocation,
	const FVector& InHookNormal)
{
	if (bGrappleGameplayCueActive)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(ASC) || !IsValid(AvatarActor) || !IsValid(ActiveSkillDefinition))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = InHookLocation;
	Parameters.Normal = InHookNormal.ContainsNaN() ? FVector::ZeroVector : InHookNormal.GetSafeNormal();
	Parameters.RawMagnitude = HookFlightDuration;
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;
	Parameters.SourceObject = ActiveSkillDefinition;
	ASC->AddGameplayCue(DRGameplayTags::GameplayCue_MovementAction_Grapple_Active, Parameters);
	bGrappleGameplayCueActive = true;
}

void UDRGA_GrappleSkill::PlayFailedGrappleGameplayCue(const FVector& FailedLocation)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(ASC) || !IsValid(AvatarActor) || FailedLocation.ContainsNaN())
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = FailedLocation;
	Parameters.RawMagnitude = CalculateHookFlightDuration(FailedLocation);
	Parameters.Instigator = AvatarActor;
	Parameters.EffectCauser = AvatarActor;
	Parameters.SourceObject = ActiveSkillDefinition;
	ASC->ExecuteGameplayCue(DRGameplayTags::GameplayCue_MovementAction_Grapple_Failed, Parameters);
}

void UDRGA_GrappleSkill::StopGrappleGameplayCue()
{
	if (!bGrappleGameplayCueActive)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveGameplayCue(DRGameplayTags::GameplayCue_MovementAction_Grapple_Active);
	}

	bGrappleGameplayCueActive = false;
}

void UDRGA_GrappleSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	GrapplePhase = EDRGrappleSkillPhase::Ending;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HookFlightTimerHandle);
		World->GetTimerManager().ClearTimer(EndGrappleTimerHandle);
	}

	HookFlightTimerHandle.Invalidate();
	EndGrappleTimerHandle.Invalidate();

	const EDRMovementActionEndReason EndReason = bEndQueued
		? PendingEndReason
		: bWasCancelled
			? EDRMovementActionEndReason::Cancelled
			: EDRMovementActionEndReason::Completed;

	bEndQueued = false;
	PendingEndReason = EDRMovementActionEndReason::Invalidated;

	if (IsValid(TargetDataTask))
	{
		TargetDataTask->EndTask();
		TargetDataTask = nullptr;
	}

	if (IsValid(CancelEventTask))
	{
		CancelEventTask->EndTask();
		CancelEventTask = nullptr;
	}

	StopMovementAction(EndReason);
	RemoveMovementActionTag();
	StopGrappleGameplayCue();

	ActiveSkillDefinition = nullptr;
	HookLocation = FVector::ZeroVector;
	HookSurfaceNormal = FVector::ZeroVector;
	HookFlightDuration = 0.f;
	MovementStartTimeSeconds = -1.f;
	GrapplePhase = EDRGrappleSkillPhase::Inactive;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

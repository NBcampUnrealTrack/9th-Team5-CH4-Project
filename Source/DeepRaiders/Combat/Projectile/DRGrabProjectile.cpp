#include "DRGrabProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "Components/SphereComponent.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

ADRGrabProjectile::ADRGrabProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ADRGrabProjectile::InitializeGrabProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	int32 InSourceTeamId,
	float InMaxDistance,
	float InPullSpeed,
	float InPullDestinationDistance,
	const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
	float InHitScale,
	const FGameplayEffectSpecHandle& InArrivalSlowSpec,
	float InMaxPullDuration)
{
	ArrivalSlowSpec = InArrivalSlowSpec;
	LaunchLocation = GetActorLocation();
	MaxDistance = FMath::Max(InMaxDistance, 0.f);
	PullSpeed = FMath::Max(InPullSpeed, 0.f);
	MaxPullDuration = FMath::Max(InMaxPullDuration, 0.01f);
	PullDestinationDistance = FMath::Max(InPullDestinationDistance, 0.f);
	if (USphereComponent* Sphere = Cast<USphereComponent>(CollisionComponent))
	{
		Sphere->SetSphereRadius(Sphere->GetUnscaledSphereRadius() * FMath::Max(InHitScale, 1.f));
	}

	InitializeProjectile(
		InSourceAbilitySystem,
		InImpactEffectSpecs,
		0.f,
		FDRProjectileWorldImpactData(),
		InSourceTeamId,
		nullptr);
	SetActorTickEnabled(MaxDistance > 0.f);
}

void ADRGrabProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		SetLifeSpan(0.f);
	}
	StartGrabGameplayCue();
}

void ADRGrabProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopGrabGameplayCue();
	ReleasePullTarget();
	Super::EndPlay(EndPlayReason);
}

void ADRGrabProjectile::ReleasePullTarget()
{
	ADRPlayerCharacter* Character = PulledCharacter.Get();
	UDRMovementActionComponent* Action = IsValid(Character) ? Character->GetMovementActionComponent() : nullptr;
	if (IsValid(Action))
	{
		Action->OnMovementActionEnded.Remove(PullEndedHandle);
		const FDRMovementActionState& State = Action->GetSimulationActionState();
		if (HasAuthority() && State.IsActive() && State.SessionId == PullSessionId)
		{
			Action->EndMovementAction(EDRMovementActionEndReason::Cancelled);
			UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());
			if (IsValid(Movement) && Movement->IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
			{
				Movement->StopMovementImmediately();
				Movement->ExitCustomMovementMode();
			}
		}
	}
}

void ADRGrabProjectile::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	if (HasActorBegunPlay())
	{
		StartGrabGameplayCue();
	}
}

void ADRGrabProjectile::OnRep_AttachmentReplication()
{
	Super::OnRep_AttachmentReplication();
	if (IsValid(GetAttachParentActor()))
	{
		StopProjectileMotion();
	}
}

void ADRGrabProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}
	if (IsPulling)
	{
		UpdatePullState();
		return;
	}

	if (FVector::DistSquared(LaunchLocation, GetActorLocation()) >= FMath::Square(MaxDistance))
	{
		Destroy();
	}
}

void ADRGrabProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	AActor* TargetActor = ImpactResult.GetActor();
	if (HasAuthority() && IsValid(TargetActor)
		&& TargetActor != GetInstigator() && !IsFriendlyTarget(TargetActor)
		&& TargetActor != GetOwner()
		&& Cast<ADRPlayerCharacter>(TargetActor) != nullptr)
	{
		ApplyImpactEffect(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor), ImpactResult);
	}
	PullTarget(ImpactResult);
	if (!IsPulling)
	{
		Destroy();
	}
}

void ADRGrabProjectile::StartGrabGameplayCue()
{
	AActor* SourceActor = GetInstigator();

	if (GetNetMode() == NM_DedicatedServer
		|| !IsValid(SourceActor)
		|| IsGrabGameplayCueActive)
	{
		return;
	}

	// 투사체가 각 화면에 생성된 뒤 로컬 참조로 줄을 연결한다.
	HandleGrabGameplayCue(EGameplayCueEvent::OnActive);
	IsGrabGameplayCueActive = true;
}

void ADRGrabProjectile::StopGrabGameplayCue()
{
	if (!IsGrabGameplayCueActive)
	{
		return;
	}

	HandleGrabGameplayCue(EGameplayCueEvent::Removed);
	IsGrabGameplayCueActive = false;
}

void ADRGrabProjectile::HandleGrabGameplayCue(EGameplayCueEvent::Type EventType)
{
	FGameplayCueParameters Parameters;
	Parameters.Location = GetActorLocation();
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = this;
	Parameters.SourceObject = this;
	if (EventType == EGameplayCueEvent::OnActive)
	{
		UDRGameplayCuePresentationLibrary::ActivateLocalSoundCue(
			GetInstigator(), DRGameplayTags::GameplayCue_Skill_Grab_Active, Parameters);
	}
	else
	{
		UDRGameplayCuePresentationLibrary::RemoveLocalSoundCue(
			GetInstigator(), DRGameplayTags::GameplayCue_Skill_Grab_Active, Parameters);
	}
}

void ADRGrabProjectile::PullTarget(const FHitResult& ImpactResult)
{
	ADRPlayerCharacter* SourceCharacter = Cast<ADRPlayerCharacter>(GetInstigator());
	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(ImpactResult.GetActor());

	if (!HasAuthority()
		|| !IsValid(SourceCharacter)
		|| !IsValid(TargetCharacter)
		|| IsFriendlyTarget(TargetCharacter)
		|| PullSpeed <= 0.f)
	{
		return;
	}

	UDRMovementActionComponent* Action = TargetCharacter->GetMovementActionComponent();
	UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(TargetCharacter->GetCharacterMovement());
	if (!IsValid(Action) || !IsValid(Movement))
	{
		return;
	}

	const FVector PullDestination =
		SourceCharacter->GetActorLocation()
		+ SourceCharacter->GetActorForwardVector().GetSafeNormal2D() * PullDestinationDistance;
	if (TargetCharacter->GetActorLocation().Equals(PullDestination, KINDA_SMALL_NUMBER))
	{
		ApplyArrivalSlow(TargetCharacter);
		return;
	}

	Action->EndMovementAction(EDRMovementActionEndReason::Cancelled);
	FDRMovementActionState State;
	State.bActive = true;
	State.ActionType = EDRMovementActionType::Grab;
	State.SessionId = static_cast<int32>(GetUniqueID());
	State.ReferenceLocation = PullDestination;
	State.MaxSpeed = PullSpeed;
	State.ControlScale = 0.f;
	if (!Action->StartAuthoritativeMovementAction(State))
	{
		return;
	}

	if (ArrivalSlowSpec.IsValid())
	{
		ArrivalSlowSpec.Data->GetContext().AddHitResult(ImpactResult, true);
	}
	PulledCharacter = TargetCharacter;
	PullSessionId = State.SessionId;
	IsPulling = true;
	PullEndedHandle = Action->OnMovementActionEnded.AddUObject(this, &ThisClass::HandlePullEnded);
	Movement->ClearAccumulatedForces();
	Movement->StopMovementImmediately();
	Movement->ClearAirborneMomentumPreservation();
	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(MaxPullDuration);
	StopProjectileMotion();
	AttachToActor(TargetCharacter, FAttachmentTransformRules::KeepWorldTransform);
	ForceNetUpdate();
}

void ADRGrabProjectile::StopProjectileMotion()
{
	if (UProjectileMovementComponent* ProjectileMotion = FindComponentByClass<UProjectileMovementComponent>())
	{
		ProjectileMotion->StopMovementImmediately();
		ProjectileMotion->Deactivate();
	}
}

void ADRGrabProjectile::UpdatePullState()
{
	ADRPlayerCharacter* TargetCharacter = PulledCharacter.Get();
	UDRMovementActionComponent* Action = IsValid(TargetCharacter) ? TargetCharacter->GetMovementActionComponent() : nullptr;
	UAbilitySystemComponent* TargetSystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetCharacter);
	if (!IsValid(Action) || !Action->IsMovementActionActive()
		|| Action->GetSimulationActionState().SessionId != PullSessionId
		|| (IsValid(TargetSystem) && TargetSystem->HasMatchingGameplayTag(DRGameplayTags::State_Dead)))
	{
		Destroy();
	}
}

void ADRGrabProjectile::HandlePullEnded(EDRMovementActionEndReason Reason)
{
	if (Reason == EDRMovementActionEndReason::Completed && PulledCharacter.IsValid())
	{
		ApplyArrivalSlow(PulledCharacter.Get());
	}
	Destroy();
}

void ADRGrabProjectile::ApplyArrivalSlow(ADRPlayerCharacter* TargetCharacter) const
{
	UAbilitySystemComponent* SourceSystem = GetSourceAbilitySystem();
	UAbilitySystemComponent* TargetAbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetCharacter);
	if (HasAuthority() && ArrivalSlowSpec.IsValid()
		&& IsValid(SourceSystem) && IsValid(TargetAbilitySystem))
	{
		SourceSystem->ApplyGameplayEffectSpecToTarget(
			*ArrivalSlowSpec.Data.Get(), TargetAbilitySystem);
	}
}

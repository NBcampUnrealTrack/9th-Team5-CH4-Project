
#include "DRGameplayCueGrapple.h"

#include "CableComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

ADRGameplayCueGrapple::ADRGameplayCueGrapple(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	
	SetReplicates(false);
	
	bAutoDestroyOnRemove = false;
	bAutoAttachToOwner = false;
	bUniqueInstancePerInstigator = true;
	bUniqueInstancePerSourceObject = true;
	bAllowMultipleOnActiveEvents = false;
	bAllowMultipleWhileActiveEvents = false;
	
	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	SetRootComponent(PresentationRoot);
	
	HookRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HookRoot"));
	HookRoot->SetupAttachment(PresentationRoot);
	// 위치와 회전이 플레이어 손의 움직임을 따라가지 않도록 월드 공간에 고정한다.
	HookRoot->SetAbsolute(true, true, false);

	CableComponent = CreateDefaultSubobject<UCableComponent>(TEXT("CableComponent"));
	CableComponent->SetupAttachment(PresentationRoot);
	CableComponent->PrimaryComponentTick.TickGroup = TG_PostPhysics;
	CableComponent->AddTickPrerequisiteActor(this);
	
	CableComponent->bAttachStart = true;
	CableComponent->bAttachEnd = true;
	CableComponent ->SetAttachEndToComponent(HookRoot);
	
	CableComponent->EndLocation = FVector::ZeroVector;
	CableComponent->CableLength = 100.f;
	CableComponent->NumSegments = 8;
	CableComponent->SolverIterations = 16;
	CableComponent->SubstepTime = 1.f / 120.f;
	CableComponent->bUseSubstepping = true;
	CableComponent->bEnableStiffness = true;
	CableComponent->bEnableCollision = false;
	CableComponent->CableGravityScale = 0.f;
	CableComponent->CableWidth = 2.f;
	CableComponent->NumSides = 6;
	CableComponent->bResetAfterTeleport = true;
	CableComponent->bTeleportAfterReattach = true;
	CableComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CableComponent->SetCastShadow(false);
	CableComponent->SetVisibility(false, true);
	
	HookMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMeshComponent"));
	HookMeshComponent->SetupAttachment(HookRoot);
	HookMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HookMeshComponent->SetGenerateOverlapEvents(false);
	HookMeshComponent->SetCastShadow(false);
	HookMeshComponent->SetVisibility(false, true);
	
	HookNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("HookNiagaraComponent"));
	HookNiagaraComponent->SetupAttachment(HookRoot);
	HookNiagaraComponent->SetAutoActivate(false);
	HookNiagaraComponent->SetVisibility(false, true);	
}

void ADRGameplayCueGrapple::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	switch (PresentationPhase)
	{
	case EPresentationPhase::Extending:
	{
		PhaseElapsedTime += DeltaSeconds;
		
		const float Alpha = CurrentPhaseDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(PhaseElapsedTime / CurrentPhaseDuration, 0.f, 1.f)
			: 1.f;

		UpdateHookLocation(FMath::Lerp(LaunchLocation, TargetLocation, Alpha));
		
		if (Alpha >= 1.f)
		{
			PhaseElapsedTime = 0.f;
			UpdateHookLocation(TargetLocation);
			
			if (bRetractAfterExtension)
			{
				BeginRetraction();
			}
			else
			{
				EnterAttachedPhase();
			}
		}
		
		break;
	}
		
	case EPresentationPhase::Attached:
		if (AActor* FollowTarget = FollowTargetActor.Get(); IsValid(FollowTarget))
		{
			TargetLocation = FollowTarget->GetActorLocation();
		}
		UpdateHookLocation(TargetLocation);
		break;
		
	case EPresentationPhase::Retracting:
	{
		PhaseElapsedTime += DeltaSeconds;
		
		const float Alpha = CurrentPhaseDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(PhaseElapsedTime / CurrentPhaseDuration, 0.f, 1.f)
			: 1.f;
		const FVector RetractTarget = GetCurrentStartLocation();
		
		UpdateHookLocation(FMath::Lerp(RetractStartLocation, RetractTarget, Alpha));
		
		if (Alpha >= 1.f)
		{
			FinishPresentation();
		}
		
		break;
	}
		
	default:
		SetActorTickEnabled(false);
		break;
	}
}

bool ADRGameplayCueGrapple::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	bRetractAfterExtension = true;
	
	if (!BeginPresentation(MyTarget, Parameters))
	{
		GameplayCueFinishedCallback();
		return false;
	}
	
	if (PresentationPhase == EPresentationPhase::Attached)
	{
		BeginRetraction();
	}
	
	return true;
}

bool ADRGameplayCueGrapple::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	bRetractAfterExtension = false;
	return BeginPresentation(MyTarget, Parameters);
}

bool ADRGameplayCueGrapple::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	bRetractAfterExtension = false;
	return BeginPresentation(MyTarget, Parameters);
}

bool ADRGameplayCueGrapple::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (PresentationPhase == EPresentationPhase::Inactive)
	{
		GameplayCueFinishedCallback();
		return true;
	}
	
	BeginRetraction();
	return true;
}

bool ADRGameplayCueGrapple::BeginPresentation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (PresentationPhase != EPresentationPhase::Inactive)
	{
		return true;
	}
	
	if (!IsValid(Target)
		|| Parameters.Location.ContainsNaN())
	{
		return false;
	}
	
	USceneComponent* ResolvedStartComponent = nullptr;
	FName ResolvedSocketName = NAME_None;
	
	if (!ResolveStartAttachment(Target, Parameters.SourceObject.Get(), ResolvedStartComponent, ResolvedSocketName))
	{
		return false;
	}
	
	const FAttachmentTransformRules AttachmentRules(
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::KeepWorld, false);
	
	AttachToComponent(ResolvedStartComponent, AttachmentRules, ResolvedSocketName);
	
	SetActorHiddenInGame(false);
	
	StartComponent = ResolvedStartComponent;
	StartSocketName = ResolvedSocketName;
	SoundTargetActor = Target;
	
	LaunchLocation = GetCurrentStartLocation();
	TargetLocation = Parameters.Location;

	FGameplayCueParameters LaunchSoundParameters;
	LaunchSoundParameters.Location = LaunchLocation;
	LaunchSoundParameters.Instigator = Target;
	LaunchSoundParameters.EffectCauser = Target;
	UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(
		Target,
		DRGameplayTags::GameplayCue_Sound_MovementAction_Grapple_Launch,
		LaunchSoundParameters);
	
	const FVector CueNormal = Parameters.Normal;
	TargetNormal = CueNormal.ContainsNaN() ? FVector::ZeroVector : CueNormal.GetSafeNormal();
	
	if (TargetNormal.IsNearlyZero())
	{
		TargetNormal = (LaunchLocation - TargetLocation).GetSafeNormal();
	}
	
	FollowTargetActor = IsEffectCauserTrackingEnabled
		? Parameters.EffectCauser
		: nullptr;

	if (!FollowTargetActor.IsValid()
		|| FollowTargetActor.Get() == Target)
	{
		FollowTargetActor.Reset();
	}

	RetractStartLocation = FVector::ZeroVector;
	PhaseElapsedTime = 0.f;
	
	/*
 	* GA가 RawMagnitude로 전달한 시간이 있으면 이를 우선한다.
 	* 값이 없는 기존 호출 경로에서는 Cue의 HookTravelSpeed를 fallback으로 사용한다.
 	*/
	const float RequestedDuration =	FMath::IsFinite(Parameters.RawMagnitude) ? Parameters.RawMagnitude : 0.f;

	CurrentPhaseDuration = RequestedDuration > 0.f ?
		RequestedDuration : CalculatePhaseDuration(LaunchLocation, TargetLocation, HookTravelSpeed);

	CableComponent->SetAttachEndToComponent(HookRoot);
	CableComponent->EndLocation = FVector::ZeroVector;
	CableComponent->SetVisibility(true, true);
	
	const bool bHasHookMesh = IsValid(HookMeshComponent->GetStaticMesh());
	HookMeshComponent->SetVisibility(bHasHookMesh, true);
	FVector LaunchDirection = (TargetLocation - LaunchLocation).GetSafeNormal();
	HookMeshComponent->SetRelativeRotation(LaunchDirection.Rotation());
	
	const bool bHasHookNiagara = IsValid(HookNiagaraComponent->GetAsset());
	HookNiagaraComponent->SetVisibility(bHasHookNiagara, true);
	
	if (bHasHookNiagara)
	{
		HookNiagaraComponent->Activate(true);
	}
	
	UpdateHookLocation(LaunchLocation);
	
	if (FollowTargetActor.IsValid())
	{
		TargetLocation = FollowTargetActor->GetActorLocation();
		EnterAttachedPhase();
	}
	else if (CurrentPhaseDuration <= KINDA_SMALL_NUMBER)
	{
		EnterAttachedPhase();
	}
	else
	{
		PresentationPhase = EPresentationPhase::Extending;
	}
	
	SetActorTickEnabled(true);
	
	return true;
}

bool ADRGameplayCueGrapple::ResolveStartAttachment(
	AActor* Target,
	const UObject* SourceObject,
	USceneComponent*& OutComponent,
	FName& OutSocketName) const
{
	OutComponent = nullptr;
	OutSocketName = NAME_None;
	
	if (!IsValid(Target))
	{
		return false;
	}
	
	const ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(Target);
	if (IsValid(Character))
	{
		/*
		 * Skill Grapple
		 * Character SkeletalMesh의 전용 발사 Socket을 사용한다.
		 */
		if (IsValid(Cast<UDRSkillDefinition>(SourceObject)))
		{
			USkeletalMeshComponent* CharacterMesh =
				Character->GetMesh();

			if (!IsValid(CharacterMesh)
				|| SkillLaunchSocketName.IsNone()
				|| !CharacterMesh->DoesSocketExist(
					SkillLaunchSocketName))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"[GrappleCue] Skill launch socket missing. "
						"Character=%s Socket=%s"),
					*GetNameSafe(Character),
					*SkillLaunchSocketName.ToString());

				return false;
			}

			OutComponent = CharacterMesh;
			OutSocketName = SkillLaunchSocketName;

			return true;
		}

		/*
		 * Item Grapple
		 * 손에 장착된 StaticMesh의 전용 총구 Socket을 사용한다.
		 *
		 * Socket이 없을 때 Component Origin으로 fallback하지 않는다.
		 * 잘못된 위치에서 Cable이 시작하는 것보다 연출을 실패시키는 편이 안전하다.
		 */
		UStaticMeshComponent* EquipmentMesh =
			Character->GetWorldHandEquipmentMesh();

		if (!IsValid(EquipmentMesh)
			|| !IsValid(EquipmentMesh->GetStaticMesh())
			|| LaunchSocketName.IsNone()
			|| !EquipmentMesh->DoesSocketExist(
				LaunchSocketName))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[GrappleCue] Item launch socket missing. "
					"Character=%s Mesh=%s Socket=%s"),
				*GetNameSafe(Character),
				*GetNameSafe(
					IsValid(EquipmentMesh)
						? EquipmentMesh->GetStaticMesh()
						: nullptr),
				*LaunchSocketName.ToString());

			return false;
		}

		OutComponent = EquipmentMesh;
		OutSocketName = LaunchSocketName;

		return true;
	}
	
	USceneComponent* TargetRoot = Target->GetRootComponent();
	if (!IsValid(TargetRoot))
	{
		return false;
	}
	
	OutComponent = TargetRoot;
	
	return true;
}

FVector ADRGameplayCueGrapple::GetCurrentStartLocation() const
{
	const USceneComponent* ResolvedStartComponent = StartComponent.Get();
	
	if (!IsValid(ResolvedStartComponent))
	{
		return GetActorLocation();
	}
	
	return ResolvedStartComponent->GetSocketLocation(StartSocketName);
}

float ADRGameplayCueGrapple::CalculatePhaseDuration(const FVector& StartLocation, const FVector& EndLocation, float Speed) const
{
	const float Distance = FVector::Distance(StartLocation, EndLocation);

	if (Distance <= KINDA_SMALL_NUMBER || Speed <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::Max(Distance / Speed, MinimumPhaseDuration);
}

void ADRGameplayCueGrapple::UpdateHookLocation(const FVector& NewLocation)
{
	if (!IsValid(HookRoot)
		|| !IsValid(CableComponent))
	{
		return;
	}

	HookRoot->SetWorldLocation(NewLocation);

	const float CurrentDistance = FVector::Distance(CableComponent->GetComponentLocation(), NewLocation);
	CableComponent->CableLength = FMath::Max(CurrentDistance * CableLengthScale, 1.f);

	// 빠른 VFX 이동은 이전 프레임 Transform을 사용한 객체 모션 블러가 과하게 보일 수 있다.
	if (IsValid(HookMeshComponent))
	{
		HookMeshComponent->ResetSceneVelocity();
	}

	CableComponent->ResetSceneVelocity();
}

void ADRGameplayCueGrapple::EnterAttachedPhase()
{
	PresentationPhase = EPresentationPhase::Attached;
	PhaseElapsedTime = 0.f;

	UpdateHookLocation(TargetLocation);

	if (bRetractAfterExtension || bAttachmentFeedbackPlayed)
	{
		return;
	}

	bAttachmentFeedbackPlayed = true;

	if (AActor* SoundTarget = SoundTargetActor.Get(); IsValid(SoundTarget))
	{
		FGameplayCueParameters AttachSoundParameters;
		AttachSoundParameters.Location = TargetLocation;
		AttachSoundParameters.Instigator = SoundTarget;
		AttachSoundParameters.EffectCauser = SoundTarget;
		UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(
			SoundTarget,
			DRGameplayTags::GameplayCue_Sound_MovementAction_Grapple_Attach,
			AttachSoundParameters);
	}

	ReceiveHookAttached(TargetLocation, TargetNormal);
}

void ADRGameplayCueGrapple::BeginRetraction()
{
	if (PresentationPhase == EPresentationPhase::Inactive
		|| PresentationPhase == EPresentationPhase::Retracting)
	{
		return;
	}
	
	if (!IsValid(HookRoot))
	{
		FinishPresentation();
		return;
	}
	
	bRetractAfterExtension = false;
	RetractStartLocation = HookRoot->GetComponentLocation();
	
	const FVector RetractTarget = GetCurrentStartLocation();
	CurrentPhaseDuration = CalculatePhaseDuration(RetractStartLocation, RetractTarget, HookRetractSpeed);
	
	PhaseElapsedTime = 0.f;
	PresentationPhase = EPresentationPhase::Retracting;
	
	if (CurrentPhaseDuration <= KINDA_SMALL_NUMBER)
	{
		FinishPresentation();
	}
	else
	{
		SetActorTickEnabled(true);
	}	
}

void ADRGameplayCueGrapple::FinishPresentation()
{
	ResetPresentationState();
	GameplayCueFinishedCallback();
}

void ADRGameplayCueGrapple::ResetPresentationState()
{
	PresentationPhase = EPresentationPhase::Inactive;
	PhaseElapsedTime = 0.f;
	CurrentPhaseDuration = 0.f;
	bRetractAfterExtension = false;
	bAttachmentFeedbackPlayed = false;

	StartComponent.Reset();
	FollowTargetActor.Reset();
	SoundTargetActor.Reset();
	StartSocketName = NAME_None;

	LaunchLocation = FVector::ZeroVector;
	TargetLocation = FVector::ZeroVector;
	TargetNormal = FVector::ZeroVector;
	RetractStartLocation = FVector::ZeroVector;

	SetActorTickEnabled(false);

	if (IsValid(CableComponent))
	{
		CableComponent->SetVisibility(false, true);
	}

	if (IsValid(HookMeshComponent))
	{
		HookMeshComponent->SetVisibility(false, true);
	}

	if (IsValid(HookNiagaraComponent))
	{
		HookNiagaraComponent->DeactivateImmediate();
		HookNiagaraComponent->SetVisibility(false, true);
	}
}

bool ADRGameplayCueGrapple::Recycle()
{
	ResetPresentationState();

	return Super::Recycle();
}

void ADRGameplayCueGrapple::ReuseAfterRecycle()
{
	Super::ReuseAfterRecycle();

	ResetPresentationState();
}

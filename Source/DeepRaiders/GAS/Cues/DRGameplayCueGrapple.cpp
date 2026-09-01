
#include "DRGameplayCueGrapple.h"

#include "CableComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

ADRGameplayCueGrapple::ADRGameplayCueGrapple(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	SetReplicates(false);
	
	GameplayCueTag = DRGameplayTags::GameplayCue_MovementAction_Grapple_Active;
	
	bAutoDestroyOnRemove = false;
	bAutoAttachToOwner = false;
	bUniqueInstancePerInstigator = true;
	bUniqueInstancePerSourceObject = true;
	bAllowMultipleOnActiveEvents = false;
	bAllowMultipleWhileActiveEvents = false;
	
	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	
	HookRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HookRoot"));
	HookRoot->SetupAttachment(PresentationRoot);

	CableComponent = CreateDefaultSubobject<UCableComponent>(TEXT("CableComponent"));
	CableComponent->SetupAttachment(PresentationRoot);
	
	CableComponent->bAttachStart = true;
	CableComponent->bAttachEnd = true;
	CableComponent ->SetAttachEndToComponent(HookRoot);
	CableComponent->CableLength = 100.f;
	CableComponent->NumSegments = 10;
	CableComponent->SolverIterations = 8;
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
		
		const float Alpha = HookTravelDuration > KINDA_SMALL_NUMBER ?
		 FMath::Clamp(PhaseElapsedTime / HookTravelDuration, 0.f, 1.f) : 1.f;
		const float EasedAlpha = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
		
		UpdateHookLocation(FMath::Lerp(LaunchLocation, TargetLocation, EasedAlpha));
		
		if (Alpha >= 1.f)
		{
			PresentationPhase = EPresentationPhase::Attached;
			PhaseElapsedTime = 0.f;
			
			UpdateHookLocation(TargetLocation);
		}
		
		break;
	}
		
	case EPresentationPhase::Attached:
		UpdateHookLocation(TargetLocation);
		break;
		
	case EPresentationPhase::Retracting:
	{
		PhaseElapsedTime += DeltaSeconds;
		
		const float Alpha = HookRetractDuration > KINDA_SMALL_NUMBER ?
			FMath::Clamp(PhaseElapsedTime / HookRetractDuration, 0.f, 1.f) : 1.f;
		const float EasedAlpha = FMath::InterpEaseIn(0.f, 1.f, Alpha, 2.f);
		
		const FVector RetractTarget = GetCurrentStartLocation();
		
		UpdateHookLocation(FMath::Lerp(RetractStartLocation, RetractTarget, EasedAlpha));
		
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

bool ADRGameplayCueGrapple::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return BeginPresentation(MyTarget, Parameters);
}

bool ADRGameplayCueGrapple::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return BeginPresentation(MyTarget, Parameters);
}

bool ADRGameplayCueGrapple::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (PresentationPhase == EPresentationPhase::Inactive)
	{
		GameplayCueFinishedCallback();
		return true;
	}
	
	if (PresentationPhase == EPresentationPhase::Retracting)
	{
		return true;
	}
	
	RetractStartLocation = HookRoot->GetComponentLocation();
	
	PhaseElapsedTime = 0.f;
	PresentationPhase = EPresentationPhase::Retracting;
	
	if (HookRetractDuration <= KINDA_SMALL_NUMBER)
	{
		FinishPresentation();
	}
	else
	{
		SetActorTickEnabled(true);
	}
	
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
	
	if (!ResolveStartAttachment(Target, ResolvedStartComponent, ResolvedSocketName))
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
	
	LaunchLocation = GetCurrentStartLocation();
	TargetLocation = Parameters.Location;
	RetractStartLocation = FVector::ZeroVector;
	PhaseElapsedTime = 0.f;
	
	CableComponent->SetAttachEndToComponent(HookRoot);
	CableComponent->SetVisibility(true, true);
	
	const bool bHasHookMesh = IsValid(HookMeshComponent->GetStaticMesh());
	HookMeshComponent->SetVisibility(bHasHookMesh, true);
	
	const bool bHasHookNiagara = IsValid(HookNiagaraComponent->GetAsset());
	HookNiagaraComponent->SetVisibility(bHasHookNiagara, true);
	
	if (bHasHookNiagara)
	{
		HookNiagaraComponent->Activate(true);
	}
	
	UpdateHookLocation(LaunchLocation);
	
	if (HookTravelDuration <= KINDA_SMALL_NUMBER
		|| FVector::PointsAreNear(LaunchLocation, TargetLocation, KINDA_SMALL_NUMBER))
	{
		PresentationPhase = EPresentationPhase::Attached;
		
		UpdateHookLocation(TargetLocation);
	}
	else
	{
		PresentationPhase = EPresentationPhase::Extending;
	}
	
	SetActorTickEnabled(true);
	
	return true;
}

bool ADRGameplayCueGrapple::ResolveStartAttachment(AActor* Target, USceneComponent*& OutComponent,
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
		UStaticMeshComponent* EquipmentMesh = Character->GetWorldHandEquipmentMesh();
		
		if (IsValid(EquipmentMesh)
			&& IsValid(EquipmentMesh->GetStaticMesh()))
		{
			OutComponent = EquipmentMesh;
			
			if (!LaunchSocketName.IsNone()
				&& EquipmentMesh->DoesSocketExist(LaunchSocketName))
			{
				OutSocketName = LaunchSocketName;
			}
			
			return true;
		}
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

	StartComponent.Reset();
	StartSocketName = NAME_None;

	LaunchLocation = FVector::ZeroVector;
	TargetLocation = FVector::ZeroVector;
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



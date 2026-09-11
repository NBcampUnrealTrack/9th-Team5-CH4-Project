#include "DRGameplayCueAbsorb.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

ADRGameplayCueAbsorb::ADRGameplayCueAbsorb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	SetReplicates(false);

	bAutoDestroyOnRemove = true;
	AutoDestroyDelay = 0.0f;
	bAutoAttachToOwner = false;
	bUniqueInstancePerInstigator = true;
	bUniqueInstancePerSourceObject = true;
	bAllowMultipleOnActiveEvents = false;
	bAllowMultipleWhileActiveEvents = false;
	GameplayCueTag = DRGameplayTags::GameplayCue_Weapon_Absorb_Active;
	GameplayCueName = GameplayCueTag.GetTagName();

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	SetRootComponent(PresentationRoot);

	BeamNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BeamNiagaraComponent"));
	BeamNiagaraComponent->SetupAttachment(PresentationRoot);
	BeamNiagaraComponent->SetAutoActivate(false);
	BeamNiagaraComponent->SetVisibility(false, true);
}

void ADRGameplayCueAbsorb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPresentationActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	if (!TargetCharacter.IsValid() || !StartComponent.IsValid() || !WeaponDefinition.IsValid())
	{
		StopPresentation(false);
		return;
	}

	UpdateBeamEndpoints();
}

bool ADRGameplayCueAbsorb::Recycle()
{
	StopPresentation(false);
	return Super::Recycle();
}

void ADRGameplayCueAbsorb::ReuseAfterRecycle()
{
	Super::ReuseAfterRecycle();
	StopPresentation(false);
}

bool ADRGameplayCueAbsorb::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return BeginPresentation(MyTarget, Parameters);
}

bool ADRGameplayCueAbsorb::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return BeginPresentation(MyTarget, Parameters);
}

bool ADRGameplayCueAbsorb::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	StopPresentation(true);
	return true;
}

bool ADRGameplayCueAbsorb::BeginPresentation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (bPresentationActive)
	{
		return true;
	}

	StopPresentation(false);

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(MyTarget);
	const UDRRangedWeaponDefinition* RangedWeaponDefinition =
		Cast<UDRRangedWeaponDefinition>(Parameters.SourceObject.Get());
	UStaticMeshComponent* EquipmentMesh = Character ? Character->GetWorldHandEquipmentMesh() : nullptr;
	if (!IsValid(Character) || !IsValid(RangedWeaponDefinition) || !IsValid(EquipmentMesh))
	{
		return false;
	}

	const FDRSnowAbsorbPresentationData& Presentation = RangedWeaponDefinition->SnowAbsorbPresentation;
	const bool bHasSound = Presentation.StartSoundCueTag.IsValid() || Presentation.LoopSoundCueTag.IsValid()
		|| Presentation.EndSoundCueTag.IsValid();
	if (!IsValid(Presentation.BeamVFX) && !bHasSound)
	{
		return false;
	}

	TargetCharacter = Character;
	StartComponent = EquipmentMesh;
	WeaponDefinition = RangedWeaponDefinition;
	ActiveStartSoundCueTag = Presentation.StartSoundCueTag;
	ActiveLoopSoundCueTag = Presentation.LoopSoundCueTag;
	ActiveEndSoundCueTag = Presentation.EndSoundCueTag;
	StartSocketName = Presentation.AttachSocketName;
	BeamStartParameterName = Presentation.BeamStartParameterName;
	BeamEndParameterName = Presentation.BeamEndParameterName;
	bPresentationActive = true;

	AttachToComponent(EquipmentMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, StartSocketName);
	SetActorHiddenInGame(false);

	if (IsValid(Presentation.BeamVFX))
	{
		BeamNiagaraComponent->SetAsset(Presentation.BeamVFX);
		BeamNiagaraComponent->SetVisibility(true, true);
		UpdateBeamEndpoints();
		BeamNiagaraComponent->Activate(true);
		SetActorTickEnabled(true);
	}

	PlayStartAndLoopSounds();
	return true;
}

void ADRGameplayCueAbsorb::StopPresentation(bool bPlayEndSound)
{
	const bool bWasPresentationActive = bPresentationActive;
	StopLoopAndPlayEndSound(bPlayEndSound && bWasPresentationActive);

	bPresentationActive = false;
	SetActorTickEnabled(false);

	if (IsValid(BeamNiagaraComponent))
	{
		BeamNiagaraComponent->DeactivateImmediate();
		BeamNiagaraComponent->SetVisibility(false, true);
		BeamNiagaraComponent->SetAsset(nullptr);
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	ResetPresentationState();
}

void ADRGameplayCueAbsorb::ResetPresentationState()
{
	bPresentationActive = false;
	TargetCharacter.Reset();
	StartComponent.Reset();
	WeaponDefinition.Reset();
	ActiveStartSoundCueTag = FGameplayTag();
	ActiveLoopSoundCueTag = FGameplayTag();
	ActiveEndSoundCueTag = FGameplayTag();
	StartSocketName = NAME_None;
	BeamStartParameterName = NAME_None;
	BeamEndParameterName = NAME_None;
}

bool ADRGameplayCueAbsorb::ResolveBeamEndpoints(FVector& OutBeamStart, FVector& OutBeamEnd) const
{
	const ADRPlayerCharacter* Character = TargetCharacter.Get();
	const UStaticMeshComponent* EquipmentMesh = StartComponent.Get();
	const UDRRangedWeaponDefinition* RangedWeaponDefinition = WeaponDefinition.Get();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !IsValid(EquipmentMesh) || !IsValid(RangedWeaponDefinition) || !IsValid(World))
	{
		return false;
	}

	OutBeamStart = EquipmentMesh->DoesSocketExist(StartSocketName)
		? EquipmentMesh->GetSocketLocation(StartSocketName)
		: EquipmentMesh->GetComponentLocation();

	FVector ViewLocation;
	FRotator ViewRotation;
	if (const AController* Controller = Character->GetController())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		ViewLocation = Character->GetPawnViewLocation();
		ViewRotation = Character->GetBaseAimRotation();
	}

	FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		ViewDirection = Character->GetActorForwardVector();
	}

	FVector AbsorbOrigin = Character->GetActorLocation();
	Character->CalculateGameplayFireOrigin(Character->GetActorForwardVector(), AbsorbOrigin);
	const FVector FrustumOrigin = AbsorbOrigin
		+ Character->GetActorTransform().TransformVectorNoScale(RangedWeaponDefinition->StartOffset);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AbsorbCueCameraAim), false, Character);
	constexpr float CameraTraceDistance = 10000.0f;
	const FVector CameraTraceEnd = ViewLocation + ViewDirection * CameraTraceDistance;
	FHitResult CameraHit;
	const bool bCameraHit = World->LineTraceSingleByChannel(
		CameraHit,
		ViewLocation,
		CameraTraceEnd,
		DRCollisionChannels::Projectile,
		QueryParams);
	const FVector AimPoint = bCameraHit ? CameraHit.ImpactPoint : CameraTraceEnd;
	const FVector AimDirection = RangedWeaponDefinition->ResolveCameraAimDirection(
		ViewDirection,
		FrustumOrigin,
		AimPoint);

	const float AbsorbRange = FMath::Max(0.0f, RangedWeaponDefinition->SnowAbsorbSettings.Range);
	if (AbsorbRange <= KINDA_SMALL_NUMBER)
	{
		OutBeamEnd = FrustumOrigin;
		return true;
	}

	const FVector TraceEnd = FrustumOrigin + AimDirection * AbsorbRange;
	FHitResult AbsorbHit;
	const bool bAbsorbHit = World->LineTraceSingleByChannel(
		AbsorbHit,
		FrustumOrigin,
		TraceEnd,
		DRCollisionChannels::Projectile,
		QueryParams);
	OutBeamEnd = bAbsorbHit ? AbsorbHit.ImpactPoint : TraceEnd;
	return true;
}

void ADRGameplayCueAbsorb::UpdateBeamEndpoints()
{
	if (!IsValid(BeamNiagaraComponent))
	{
		return;
	}

	FVector BeamStart;
	FVector BeamEnd;
	if (!ResolveBeamEndpoints(BeamStart, BeamEnd))
	{
		return;
	}

	if (!BeamStartParameterName.IsNone())
	{
		BeamNiagaraComponent->SetVariablePosition(BeamStartParameterName, BeamStart);
	}

	if (!BeamEndParameterName.IsNone())
	{
		BeamNiagaraComponent->SetVariablePosition(BeamEndParameterName, BeamEnd);
	}
}

FGameplayCueParameters ADRGameplayCueAbsorb::BuildSoundParameters() const
{
	FGameplayCueParameters Parameters;
	if (ADRPlayerCharacter* Character = TargetCharacter.Get())
	{
		Parameters.Location = StartComponent.IsValid()
			? StartComponent->GetSocketLocation(StartSocketName)
			: Character->GetActorLocation();
		Parameters.Normal = Character->GetActorForwardVector();
		Parameters.Instigator = Character;
		Parameters.EffectCauser = Character;
		Parameters.SourceObject = WeaponDefinition.Get();
	}

	return Parameters;
}

void ADRGameplayCueAbsorb::PlayStartAndLoopSounds()
{
	AActor* Character = TargetCharacter.Get();
	if (!IsValid(Character))
	{
		return;
	}

	const FGameplayCueParameters Parameters = BuildSoundParameters();
	if (ActiveStartSoundCueTag.IsValid())
	{
		UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(Character, ActiveStartSoundCueTag, Parameters);
	}

	if (ActiveLoopSoundCueTag.IsValid())
	{
		UDRGameplayCuePresentationLibrary::ActivateLocalSoundCue(Character, ActiveLoopSoundCueTag, Parameters);
	}
}

void ADRGameplayCueAbsorb::StopLoopAndPlayEndSound(bool bPlayEndSound)
{
	AActor* Character = TargetCharacter.Get();
	if (!IsValid(Character))
	{
		return;
	}

	const FGameplayCueParameters Parameters = BuildSoundParameters();
	if (ActiveLoopSoundCueTag.IsValid())
	{
		UDRGameplayCuePresentationLibrary::RemoveLocalSoundCue(Character, ActiveLoopSoundCueTag, Parameters);
	}

	if (bPlayEndSound && ActiveEndSoundCueTag.IsValid())
	{
		UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(Character, ActiveEndSoundCueTag, Parameters);
	}
}

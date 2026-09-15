#include "DRGameplayCueAbsorb.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

ADRGameplayCueAbsorb::ADRGameplayCueAbsorb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetReplicates(false);

	bAutoDestroyOnRemove = true;
	AutoDestroyDelay = 0.0f;
	bAutoAttachToOwner = false;
	// 예측 Cue의 parameterless Removed도 현재 Target의 흡수 CueActor를 찾을 수 있어야 한다.
	// 흡수 Presentation은 무기별 인스턴스가 아니라 Target별 하나만 유지한다.
	bUniqueInstancePerInstigator = false;
	bUniqueInstancePerSourceObject = false;
	bAllowMultipleOnActiveEvents = true;
	bAllowMultipleWhileActiveEvents = true;
	GameplayCueName = GameplayCueTag.GetTagName();

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	SetRootComponent(PresentationRoot);
	PresentationRoot->SetAbsolute(false, false, true);

	BeamNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BeamNiagaraComponent"));
	BeamNiagaraComponent->SetupAttachment(PresentationRoot);
	BeamNiagaraComponent->SetAutoActivate(false);
	BeamNiagaraComponent->SetVisibility(false, true);
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
	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(MyTarget);
	const UDRRangedWeaponDefinition* RangedWeaponDefinition =
		Cast<UDRRangedWeaponDefinition>(Parameters.SourceObject.Get());
	UStaticMeshComponent* EquipmentMesh = Character ? Character->GetWorldHandEquipmentMesh() : nullptr;
	if (!IsValid(Character) || !IsValid(RangedWeaponDefinition) || !IsValid(EquipmentMesh))
	{
		StopPresentation(false);
		return false;
	}

	// OnActive와 WhileActive가 연속으로 들어오거나 같은 Cue가 재전달된 경우에는 다시 시작하지 않는다.
	if (bPresentationActive
		&& TargetCharacter.Get() == Character
		&& WeaponDefinition.Get() == RangedWeaponDefinition)
	{
		return true;
	}

	// 무기 교체 중 새 SourceObject가 들어오면 남아 있는 이전 무기 Presentation을 같은 Actor에서 교체한다.
	StopPresentation(false);

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
	ScaleParameterName = Presentation.ScaleParameterName;
	bPresentationActive = true;

	AttachToComponent(EquipmentMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, StartSocketName);
	SetActorHiddenInGame(false);

	if (IsValid(Presentation.BeamVFX))
	{
		const float Radius = WeaponDefinition->SnowAbsorbSettings.Radius;
		const float Range = WeaponDefinition->SnowAbsorbSettings.Range;
		FVector VFXScale = FVector::OneVector;
		constexpr float VFXMeshSize = 100.f;

		VFXScale.X = FMath::Max(1.f, Radius / VFXMeshSize * 2);
		VFXScale.Y = FMath::Max(1.f, Radius / VFXMeshSize * 2);
		VFXScale.Z = FMath::Max(1.f, Range / VFXMeshSize);

		// SetAsset의 기본 동작은 기존 override parameter를 초기화하므로 에셋을 먼저 지정한다.
		BeamNiagaraComponent->SetAsset(Presentation.BeamVFX);

		if (!ScaleParameterName.IsNone())
		{
			BeamNiagaraComponent->SetVariableVec3(ScaleParameterName, VFXScale);
		}

		BeamNiagaraComponent->SetVisibility(true, true);
		BeamNiagaraComponent->Activate(true);
	}

	PlayStartAndLoopSounds();
	return true;
}

void ADRGameplayCueAbsorb::StopPresentation(bool bPlayEndSound)
{
	const bool bWasPresentationActive = bPresentationActive;
	StopLoopAndPlayEndSound(bPlayEndSound && bWasPresentationActive);

	bPresentationActive = false;

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

	// 무기 외형 복제가 Cue 제거보다 먼저 도착하면 부착 중인 Cue가 새 무기 Scale을 상속할 수 있다.
	// 재활용된 Actor가 그 World Scale을 다음 활성화로 가져가지 않도록 Presentation Transform을 복구한다.
	SetActorScale3D(FVector::OneVector);
	if (IsValid(BeamNiagaraComponent))
	{
		BeamNiagaraComponent->SetRelativeTransform(FTransform::Identity);
	}

	TargetCharacter.Reset();
	StartComponent.Reset();
	WeaponDefinition.Reset();
	ActiveStartSoundCueTag = FGameplayTag();
	ActiveLoopSoundCueTag = FGameplayTag();
	ActiveEndSoundCueTag = FGameplayTag();
	StartSocketName = NAME_None;
	ScaleParameterName = NAME_None;
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

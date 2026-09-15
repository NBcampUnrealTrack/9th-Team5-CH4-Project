#include "DRFreezeVisualComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/Data/DRFreezeVisualProfile.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Audio/DRSoundLibrary.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"

namespace
{
	float GetNormalizedRangeAlpha(float Value, float Start, float End)
	{
		Start = FMath::Clamp(Start, 0.f, 1.f);
		End = FMath::Clamp(End, 0.f, 1.f);

		if (End <= Start + KINDA_SMALL_NUMBER)
		{
			return Value >= Start ? 1.f : 0.f;
		}

		return FMath::GetMappedRangeValueClamped(FVector2D(Start, End), FVector2D(0.f, 1.f), Value);
	}

	float Smooth01(float Alpha)
	{
		Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

		return Alpha * Alpha * (3.f - 2.f * Alpha);
	}
}

UDRFreezeVisualComponent::UDRFreezeVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// 이 Component는 Presentation 전용.
	SetIsReplicatedByDefault(false);
}

void UDRFreezeVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!ShouldCreateVisuals())
	{
		SetComponentTickEnabled(false);
		return;
	}

	CreateAttachmentVisuals();
	CreateSurfaceFrostVisual();
	CreateNiagaraVisual();
	CreateFrozenShellVisual();

	ApplyVisualFreezeAmount(VisualFreezeAmount);

	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
	{
		SetFrozenStateVisual(ASC->HasMatchingGameplayTag(DRGameplayTags::State_Frozen));
	}
}

void UDRFreezeVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystem();

	DestroyFrozenShellVisual();
	ClearNiagaraVisual();
	ClearSurfaceFrostVisual();
	DestroyAttachmentVisuals();

	Super::EndPlay(EndPlayReason);
}

// =====================================================
// Ability System
// =====================================================

void UDRFreezeVisualComponent::BindAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!ShouldCreateVisuals() || !IsValid(InASC))
	{
		return;
	}

	/*
	 * 같은 ASC로 Character 초기화 함수가
	 * 다시 호출되는 경우 Delegate 중복 등록 방지.
	 */
	if (BoundAbilitySystem.Get() == InASC
		&& FreezeGaugeChangedHandle.IsValid()
		&& FrozenTagChangedHandle.IsValid())
	{
		RefreshTargetFreezeAmount(true);
		return;
	}

	UnbindAbilitySystem();

	BoundAbilitySystem = InASC;

	FreezeGaugeChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(
		UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).AddUObject(this, &ThisClass::HandleFreezeGaugeChanged);

	FrozenTagChangedHandle = InASC->RegisterGameplayTagEvent(
		DRGameplayTags::State_Frozen, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleFrozenTagChanged);
	
	SetFrozenStateVisual(InASC->HasMatchingGameplayTag(DRGameplayTags::State_Frozen));
	
	/*
	 * Delegate 등록 전에 이미 Gauge 값이 존재할 수 있으므로
	 * 현재 값을 한 번 즉시 읽는다.
	 */
	RefreshTargetFreezeAmount(true);
}

void UDRFreezeVisualComponent::UnbindAbilitySystem()
{
	UAbilitySystemComponent* ASC = BoundAbilitySystem.Get();

	if (IsValid(ASC))
	{
		if (FreezeGaugeChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).Remove(FreezeGaugeChangedHandle);
		}

		if (FrozenTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(
				DRGameplayTags::State_Frozen, EGameplayTagEventType::NewOrRemoved).Remove(FrozenTagChangedHandle);
		}
	}

	FreezeGaugeChangedHandle.Reset();
	FrozenTagChangedHandle.Reset();

	BoundAbilitySystem.Reset();
}

void UDRFreezeVisualComponent::HandleFreezeGaugeChanged(const FOnAttributeChangeData& Data)
{
	RefreshTargetFreezeAmount(false);
}

void UDRFreezeVisualComponent::HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& Data)
{
	RefreshTargetFreezeAmount(false);
}

void UDRFreezeVisualComponent::RefreshTargetFreezeAmount(bool bSnapImmediately)
{
	const UAbilitySystemComponent* ASC = BoundAbilitySystem.Get();

	if (!IsValid(ASC))
	{
		return;
	}

	const float FreezeGauge = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetFreezeGaugeAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetMaxHealthAttribute());
	TargetFreezeAmount = MaxHealth > KINDA_SMALL_NUMBER ? FMath::Clamp(FreezeGauge / MaxHealth, 0.f, 1.f) : 0.f;
	
	if (bSnapImmediately)
	{
		VisualFreezeAmount = TargetFreezeAmount;
		ApplyVisualFreezeAmount(VisualFreezeAmount);
		SetComponentTickEnabled(false);

		return;
	}

	if (!FMath::IsNearlyEqual(VisualFreezeAmount, TargetFreezeAmount, 0.001f))
	{
		SetComponentTickEnabled(true);
	}
}

// =====================================================
// Tick
// =====================================================

void UDRFreezeVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(VisualProfile))
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (FMath::IsNearlyEqual(VisualFreezeAmount, TargetFreezeAmount, 0.001f))
	{
		VisualFreezeAmount = TargetFreezeAmount;
		ApplyVisualFreezeAmount(VisualFreezeAmount);
		SetComponentTickEnabled(false);

		return;
	}

	const bool bGrowing = TargetFreezeAmount > VisualFreezeAmount;
	const float InterpSpeed = bGrowing ? VisualProfile->GrowInterpSpeed : VisualProfile->DecayInterpSpeed;
	VisualFreezeAmount = FMath::FInterpTo(VisualFreezeAmount, TargetFreezeAmount, DeltaTime, FMath::Max(InterpSpeed, 0.01f));
	ApplyVisualFreezeAmount(VisualFreezeAmount);
}

// =====================================================
// Visual Root
// =====================================================

bool UDRFreezeVisualComponent::ShouldCreateVisuals() const
{
	const AActor* Owner = GetOwner();

	return IsValid(Owner) && Owner->GetNetMode() != NM_DedicatedServer;
}


void UDRFreezeVisualComponent::ApplyVisualFreezeAmount(float FreezeAmount)
{
	if (!IsValid(VisualProfile))
	{
		return;
	}

	const float Amount = FMath::Clamp(FreezeAmount, 0.f, 1.f);

	if (VisualProfile->bEnableAttachments)
	{
		ApplyAttachmentVisuals(Amount);
	}

	if (VisualProfile->bEnableSurfaceFrost)
	{
		ApplySurfaceFrostVisual(Amount);
	}

	if (VisualProfile->bEnableNiagara)
	{
		ApplyNiagaraVisual(Amount);
	}
}

// =====================================================
// Attachments
// =====================================================

void UDRFreezeVisualComponent::CreateAttachmentVisuals()
{
	if (!IsValid(VisualProfile) || !VisualProfile->bEnableAttachments)
	{
		return;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(GetOwner());

	if (!IsValid(Character) || !IsValid(Character->GetMesh()))
	{
		return;
	}

	const TArray<FDRFreezeVisualPart>& Parts = VisualProfile->Parts;

	RuntimePartComponents.SetNum(Parts.Num());

	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const FDRFreezeVisualPart& Part = Parts[Index];

		if (!IsValid(Part.Mesh) || Part.BoneName.IsNone())
		{
			continue;
		}

		if (Character->GetMesh()->GetBoneIndex(Part.BoneName) == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT( "[FreezeVisual] Invalid Bone. " "Character=%s Bone=%s"), *GetNameSafe(Character), *Part.BoneName.ToString());

			continue;
		}

		ensureMsgf(Part.EndThreshold >= Part.StartThreshold, TEXT( "[FreezeVisual] Invalid Threshold. " "Bone=%s Start=%.2f End=%.2f"), *Part.BoneName.ToString(), Part.StartThreshold, Part.EndThreshold);

		const FName ComponentName(*FString::Printf(TEXT("FreezeVisualPart_%d"), Index));

		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(Character, ComponentName);

		if (!IsValid(MeshComponent))
		{
			continue;
		}

		Character->AddInstanceComponent(MeshComponent);

		MeshComponent->SetupAttachment(Character->GetMesh(), Part.BoneName);
		MeshComponent->SetStaticMesh(Part.Mesh);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetCanEverAffectNavigation(false);
		MeshComponent->SetIsReplicated(false);
		MeshComponent->SetRelativeTransform(Part.AttachTransform);
		MeshComponent->SetVisibility(false, true);
		MeshComponent->RegisterComponent();

		RuntimePartComponents[Index] = MeshComponent;
	}
}

void UDRFreezeVisualComponent::DestroyAttachmentVisuals()
{
	for (UStaticMeshComponent* MeshComponent : RuntimePartComponents)
	{
		if (IsValid(MeshComponent))
		{
			MeshComponent->DestroyComponent();
		}
	}

	RuntimePartComponents.Empty();
}

void UDRFreezeVisualComponent::ApplyAttachmentVisuals(float FreezeAmount)
{
	if (!IsValid(VisualProfile))
	{
		return;
	}

	const TArray<FDRFreezeVisualPart>& Parts = VisualProfile->Parts;

	const int32 Count = FMath::Min(Parts.Num(), RuntimePartComponents.Num());

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FDRFreezeVisualPart& Part = Parts[Index];

		UStaticMeshComponent* MeshComponent = RuntimePartComponents[Index];

		if (!IsValid(MeshComponent))
		{
			continue;
		}

		const FVector BaseScale = Part.AttachTransform.GetScale3D();

		const FVector BaseLocation = Part.AttachTransform.GetLocation();

		/*
		 * 등장 Threshold 이전이면 숨기고
		 * 초기 Transform으로 되돌린다.
		 */
		if (FreezeAmount < Part.StartThreshold)
		{
			MeshComponent->SetVisibility(false, true);

			MeshComponent->SetRelativeScale3D(BaseScale);

			MeshComponent->SetRelativeLocation(BaseLocation);

			continue;
		}

		MeshComponent->SetVisibility(true, true);

		const float LocalAlpha = GetNormalizedRangeAlpha(FreezeAmount, Part.StartThreshold, Part.EndThreshold);

		const float SmoothAlpha = Smooth01(LocalAlpha);

		const FVector TargetScale = BaseScale * Part.GrowthScale;

		const FVector CurrentScale = FMath::Lerp(BaseScale, TargetScale, SmoothAlpha);
		const FVector CurrentLocation = BaseLocation + Part.GrowthOffset * SmoothAlpha;

		MeshComponent->SetRelativeScale3D(CurrentScale);
		MeshComponent->SetRelativeLocation(CurrentLocation);
	}
}

// =====================================================
// Surface Frost
// =====================================================

void UDRFreezeVisualComponent::CreateSurfaceFrostVisual()
{
	if (!IsValid(VisualProfile) || !VisualProfile->bEnableSurfaceFrost || !IsValid(VisualProfile->FrostOverlayMaterial))
	{
		return;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(GetOwner());

	if (!IsValid(Character) || !IsValid(Character->GetMesh()))
	{
		return;
	}

	SurfaceFrostMID = UMaterialInstanceDynamic::Create(VisualProfile->FrostOverlayMaterial, this);

	if (!IsValid(SurfaceFrostMID))
	{
		return;
	}

	SurfaceFrostMID->SetScalarParameterValue(TEXT("FreezeAmount"), 0.f);

	Character->GetMesh()->SetOverlayMaterial(SurfaceFrostMID);
}


void UDRFreezeVisualComponent::ClearSurfaceFrostVisual()
{
	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(GetOwner());

	if (IsValid(Character) && IsValid(Character->GetMesh()))
	{
		Character->GetMesh()->SetOverlayMaterial(nullptr);
	}

	SurfaceFrostMID = nullptr;
}

void UDRFreezeVisualComponent::ApplySurfaceFrostVisual(float FreezeAmount)
{
	if (!IsValid(VisualProfile) || !IsValid(SurfaceFrostMID))
	{
		return;
	}

	const float FrostAlpha = GetNormalizedRangeAlpha(FreezeAmount, VisualProfile->SurfaceFrostStartThreshold, VisualProfile->SurfaceFrostFullThreshold);

	SurfaceFrostMID->SetScalarParameterValue(TEXT("FreezeAmount"), FrostAlpha);
}

// =====================================================
// Niagara
// =====================================================

void UDRFreezeVisualComponent::CreateNiagaraVisual()
{
	if (!IsValid(VisualProfile) || !VisualProfile->bEnableNiagara || !IsValid(VisualProfile->FreezeNiagaraSystem))
	{
		return;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(GetOwner());

	if (!IsValid(Character) || !IsValid(Character->GetMesh()))
	{
		return;
	}

	FreezeNiagaraComponent = NewObject<UNiagaraComponent>(Character, TEXT("FreezeNiagaraComponent"));

	if (!IsValid(FreezeNiagaraComponent))
	{
		return;
	}

	Character->AddInstanceComponent(FreezeNiagaraComponent);

	FreezeNiagaraComponent->SetupAttachment(Character->GetMesh());
	FreezeNiagaraComponent->SetAsset(VisualProfile->FreezeNiagaraSystem);
	FreezeNiagaraComponent->SetAutoActivate(false);
	FreezeNiagaraComponent->SetIsReplicated(false);
	FreezeNiagaraComponent->RegisterComponent();
	FreezeNiagaraComponent->SetVariableFloat(TEXT("User.FreezeAmount"), 0.f);
}

void UDRFreezeVisualComponent::ClearNiagaraVisual()
{
	if (!IsValid(FreezeNiagaraComponent))
	{
		return;
	}

	FreezeNiagaraComponent->Deactivate();
	FreezeNiagaraComponent->DestroyComponent();

	FreezeNiagaraComponent = nullptr;
}

void UDRFreezeVisualComponent::ApplyNiagaraVisual(float FreezeAmount)
{
	if (!IsValid(VisualProfile) || !IsValid(FreezeNiagaraComponent))
	{
		return;
	}

	const float NiagaraAlpha = GetNormalizedRangeAlpha(FreezeAmount, VisualProfile->NiagaraStartThreshold, VisualProfile->NiagaraFullThreshold);

	FreezeNiagaraComponent->SetVariableFloat(TEXT("User.FreezeAmount"), NiagaraAlpha);

	if (NiagaraAlpha <= KINDA_SMALL_NUMBER)
	{
		if (FreezeNiagaraComponent->IsActive())
		{
			FreezeNiagaraComponent->Deactivate();
		}

		return;
	}

	if (!FreezeNiagaraComponent->IsActive())
	{
		FreezeNiagaraComponent->Activate(true);
	}
}

void UDRFreezeVisualComponent::CreateFrozenShellVisual()
{
	if (!IsValid(VisualProfile) || !VisualProfile->bEnableFrozenShell || !IsValid(VisualProfile->FrozenShellMesh))
	{
		return;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(GetOwner());

	if (!IsValid(Character) || !IsValid(Character->GetMesh()))
	{
		return;
	}

	FrozenShellComponent = NewObject<UStaticMeshComponent>(Character, TEXT("FrozenShellComponent"));

	if (!IsValid(FrozenShellComponent))
	{
		return;
	}

	Character->AddInstanceComponent(FrozenShellComponent);

	FrozenShellComponent->SetupAttachment(Character->GetMesh());
	FrozenShellComponent->SetStaticMesh(VisualProfile->FrozenShellMesh);
	FrozenShellComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrozenShellComponent->SetGenerateOverlapEvents(false);
	FrozenShellComponent->SetCanEverAffectNavigation(false);
	FrozenShellComponent->SetIsReplicated(false);
	FrozenShellComponent->SetRelativeTransform(VisualProfile->FrozenShellTransform);
	FrozenShellComponent->SetVisibility(false, true);
	FrozenShellComponent->RegisterComponent();
}

void UDRFreezeVisualComponent::DestroyFrozenShellVisual()
{
	if (!IsValid(FrozenShellComponent))
	{
		return;
	}

	FrozenShellComponent->DestroyComponent();
	FrozenShellComponent = nullptr;
}

void UDRFreezeVisualComponent::SetFrozenShellVisible(bool bVisible)
{
	if (!IsValid(FrozenShellComponent))
	{
		return;
	}

	FrozenShellComponent->SetVisibility(bVisible, true);
}

void UDRFreezeVisualComponent::HandleFrozenTagChanged(
	const FGameplayTag /*Tag*/,
	int32 NewCount)
{
	const bool bFrozen = NewCount > 0;

	SetFrozenStateVisual(bFrozen);

	if (!bFrozen)
	{
		return;
	}

	ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Character->GetActorLocation();
	Parameters.Instigator = Character;
	Parameters.EffectCauser = Character;

	UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(
		Character,
		DRGameplayTags::GameplayCue_Sound_Player_Frozen_Enter,
		Parameters);
}

void UDRFreezeVisualComponent::SetFrozenStateVisual(bool bFrozen)
{
	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(GetOwner());

	if (!IsValid(Character) || !IsValid(Character->GetMesh()))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	UStaticMeshComponent* HandEquipmentMesh = Character->GetWorldHandEquipmentMesh();
	UStaticMeshComponent* BackEquipmentMesh = Character->GetWorldBackEquipmentMesh();

	if (bFrozen)
	{
		if (!bFrozenVisualActive)
		{
			/*
			 * Frozen 해제 시 원래 상태로 복구하기 위해
			 * 빙결 직전 Visibility를 저장한다.
			 */
			bCharacterMeshWasVisibleBeforeFrozen = CharacterMesh->IsVisible();

			if (IsValid(HandEquipmentMesh))
			{
				bHandEquipmentWasVisibleBeforeFrozen = HandEquipmentMesh->IsVisible();
			}

			if (IsValid(BackEquipmentMesh))
			{
				bBackEquipmentWasVisibleBeforeFrozen = BackEquipmentMesh->IsVisible();
			}

			bFrozenVisualActive = true;
		}

		/*
		 * 자식 전체에 Visibility를 전파하지 않는다.
		 * FrozenShell / Freeze Visual도 CharacterMesh 하위에 있기 때문.
		 */
		CharacterMesh->SetVisibility(false, false);

		if (IsValid(HandEquipmentMesh))
		{
			HandEquipmentMesh->SetVisibility(false, false);
		}

		if (IsValid(BackEquipmentMesh))
		{
			BackEquipmentMesh->SetVisibility(false, false);
		}

		SetFrozenShellVisible(true);
	}
	else
	{
		if (bFrozenVisualActive)
		{
			CharacterMesh->SetVisibility(bCharacterMeshWasVisibleBeforeFrozen, false);

			if (IsValid(HandEquipmentMesh))
			{
				HandEquipmentMesh->SetVisibility(bHandEquipmentWasVisibleBeforeFrozen, false);
			}

			if (IsValid(BackEquipmentMesh))
			{
				BackEquipmentMesh->SetVisibility(bBackEquipmentWasVisibleBeforeFrozen, false);
			}

			bFrozenVisualActive = false;
		}

		SetFrozenShellVisible(false);
	}
}

#include "DRFreezeVisualComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/Data/DRFreezeVisualProfile.h"

UDRFreezeVisualComponent::UDRFreezeVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 평소에는 Tick하지 않는다.
	// TargetFreezeAmount와 차이가 생겼을 때만 켠다.
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Presentation Component 자체는 복제하지 않는다.
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

	CreateVisualParts();

	/*
	 * BindAbilitySystem이 BeginPlay보다 먼저 호출됐을 수도 있으므로
	 * 현재 Visual 값을 새로 생성된 Mesh들에 한 번 적용한다.
	 */
	ApplyVisualFreezeAmount(VisualFreezeAmount);
}

void UDRFreezeVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystem();

	for (UStaticMeshComponent* MeshComponent : RuntimePartComponents)
	{
		if (IsValid(MeshComponent))
		{
			MeshComponent->DestroyComponent();
		}
	}

	RuntimePartComponents.Empty();

	Super::EndPlay(EndPlayReason);
}

bool UDRFreezeVisualComponent::ShouldCreateVisuals() const
{
	const AActor* Owner = GetOwner();

	return IsValid(Owner) && Owner->GetNetMode() != NM_DedicatedServer;
}

void UDRFreezeVisualComponent::BindAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!ShouldCreateVisuals() || !IsValid(InASC))
	{
		return;
	}

	/*
	 * 같은 ASC로 InitializeAbilitySystem이 다시 호출될 수 있다.
	 * 중복 Delegate 등록을 방지한다.
	 */
	if (BoundAbilitySystem.Get() == InASC && FreezeGaugeChangedHandle.IsValid())
	{
		RefreshTargetFreezeAmount(true);
		return;
	}

	UnbindAbilitySystem();

	BoundAbilitySystem = InASC;
	FreezeGaugeChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).AddUObject(this, &ThisClass::HandleFreezeGaugeChanged);
	MaxFreezeGaugeChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute()).AddUObject(this, &ThisClass::HandleMaxFreezeGaugeChanged);

	/*
	 * Delegate 등록만 하고 끝내면 안 된다.
	 *
	 * 이미 FreezeGauge가 50인 상태에서
	 * 이 Pawn이 새로 Relevant해질 수도 있기 때문에
	 * 현재 값을 즉시 한 번 읽는다.
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
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).Remove(FreezeGaugeChangedHandle);
		}

		if (MaxFreezeGaugeChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute()).Remove(MaxFreezeGaugeChangedHandle);
		}
	}

	FreezeGaugeChangedHandle.Reset();
	MaxFreezeGaugeChangedHandle.Reset();

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
	const float MaxFreezeGauge = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute());
	const float NewTarget = MaxFreezeGauge > KINDA_SMALL_NUMBER ? FMath::Clamp(FreezeGauge / MaxFreezeGauge, 0.f, 1.f) : 0.f;

	TargetFreezeAmount = NewTarget;

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

void UDRFreezeVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FMath::IsNearlyEqual(VisualFreezeAmount, TargetFreezeAmount, 0.001f))
	{
		VisualFreezeAmount = TargetFreezeAmount;

		ApplyVisualFreezeAmount(VisualFreezeAmount);
		SetComponentTickEnabled(false);

		return;
	}

	if (!IsValid(VisualProfile))
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float InterpSpeed = TargetFreezeAmount > VisualFreezeAmount ? VisualProfile->GrowInterpSpeed : VisualProfile->DecayInterpSpeed;

	VisualFreezeAmount = FMath::FInterpTo(VisualFreezeAmount, TargetFreezeAmount, DeltaTime, InterpSpeed);

	ApplyVisualFreezeAmount(VisualFreezeAmount);
}

void UDRFreezeVisualComponent::CreateVisualParts()
{
	if (bVisualPartsCreated || !IsValid(VisualProfile))
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

		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(Character);

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

	bVisualPartsCreated = true;
}

void UDRFreezeVisualComponent::ApplyVisualFreezeAmount(float FreezeAmount)
{
	if (!bVisualPartsCreated || !IsValid(VisualProfile))
	{
		return;
	}

	const float ClampedAmount = FMath::Clamp(FreezeAmount, 0.f, 1.f);

	const int32 Count = FMath::Min(VisualProfile->Parts.Num(), RuntimePartComponents.Num());

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FDRFreezeVisualPart& Part = VisualProfile->Parts[Index];

		UStaticMeshComponent* MeshComponent = RuntimePartComponents[Index];

		if (!IsValid(MeshComponent))
		{
			continue;
		}

		const FVector BaseScale = Part.AttachTransform.GetScale3D();

		const FVector BaseLocation = Part.AttachTransform.GetLocation();

		// 아직 등장 구간 이전
		if (ClampedAmount < Part.StartThreshold)
		{
			MeshComponent->SetVisibility(false, true);

			// 다음에 다시 등장할 때 확실한 초기 상태
			MeshComponent->SetRelativeScale3D(BaseScale);
			MeshComponent->SetRelativeLocation(BaseLocation);

			continue;
		}

		MeshComponent->SetVisibility(true, true);

		float LocalAlpha = 1.f;

		if (Part.EndThreshold > Part.StartThreshold + KINDA_SMALL_NUMBER)
		{
			LocalAlpha = FMath::GetMappedRangeValueClamped(FVector2D(Part.StartThreshold, Part.EndThreshold), FVector2D(0.f, 1.f), ClampedAmount);
		}

		const float SmoothAlpha = LocalAlpha * LocalAlpha * (3.f - 2.f * LocalAlpha);
		const FVector TargetScale = BaseScale * Part.GrowthScale;

		const FVector CurrentScale = FMath::Lerp(BaseScale, TargetScale, SmoothAlpha);
		const FVector CurrentLocation = BaseLocation + Part.GrowthOffset * SmoothAlpha;

		MeshComponent->SetRelativeScale3D(CurrentScale);
		MeshComponent->SetRelativeLocation(CurrentLocation);
	}
}

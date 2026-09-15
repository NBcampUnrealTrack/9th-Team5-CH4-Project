
#include "DRCharacterShadowComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

constexpr float ShadowVisibilityThreshold = 0.001f;

UDRCharacterShadowComponent::UDRCharacterShadowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	
	SetIsReplicatedByDefault(false);
	SetCanEverAffectNavigation(false);
	SetVisibility(false);
	
	// X는 투영 깊이, Y/Z는 원형 그림자의 반경
	DecalSize = FVector(16.f, 45.f, 45.f);
	FadeScreenSize = 0.001f;
	SortOrder = 1;
}

void UDRCharacterShadowComponent::BeginPlay()
{
	Super::BeginPlay();
	
	SetShadowVisible(false);
	
	if (!ShouldCreateVisuals()
		|| !IsValid(GetDecalMaterial()))
	{
		SetComponentTickEnabled(false);
		return;
	}
	
	ShadowMaterialInstance = CreateDynamicMaterialInstance();
	
	if (!IsValid(ShadowMaterialInstance))
	{
		SetComponentTickEnabled(false);
		return;
	}
	
	RefreshTrackingState();
}

void UDRCharacterShadowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystem();
	
	SetComponentTickEnabled(false);
	SetShadowVisible(false);
	ShadowMaterialInstance = nullptr;
	
	Super::EndPlay(EndPlayReason);
}

void UDRCharacterShadowComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateShadow();
}

void UDRCharacterShadowComponent::BindAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!ShouldCreateVisuals()
		|| !IsValid(InASC))
	{
		return;
	}
	
	if (BoundAbilitySystem.Get() == InASC
		&& StealthedTagChangedHandle.IsValid())
	{
		bStealthed = InASC->HasMatchingGameplayTag(DRGameplayTags::State_Stealthed);
		RefreshTrackingState();
		return;
	}
	
	UnbindAbilitySystem();
	
	BoundAbilitySystem = InASC;
	StealthedTagChangedHandle= InASC->RegisterGameplayTagEvent(DRGameplayTags::State_Stealthed,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleStealthedTagChanged);
	
	bStealthed = InASC->HasMatchingGameplayTag(DRGameplayTags::State_Stealthed);
	RefreshTrackingState();
}

void UDRCharacterShadowComponent::UnbindAbilitySystem()
{
	UAbilitySystemComponent* ASC = BoundAbilitySystem.Get();
	
	if (IsValid(ASC)
		&& StealthedTagChangedHandle.IsValid())
	{
		ASC->RegisterGameplayTagEvent(DRGameplayTags::State_Stealthed,
			EGameplayTagEventType::NewOrRemoved).Remove(StealthedTagChangedHandle);
	}
	
	StealthedTagChangedHandle.Reset();
	BoundAbilitySystem.Reset();
	bStealthed = false;
}

void UDRCharacterShadowComponent::HandleStealthedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bStealthed = NewCount > 0;
	RefreshTrackingState();
}

bool UDRCharacterShadowComponent::ShouldCreateVisuals() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->GetNetMode() != NM_DedicatedServer;
}

bool UDRCharacterShadowComponent::TraceGround(FHitResult& OutHit, float& OutGroundDistance) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	const UWorld* World = GetWorld();
	
	if (!IsValid(Character)
		|| !IsValid(World)
		|| !IsValid(Capsule))
	{
		return false;
	}
	
	const FVector UpVector = FVector::UpVector;
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector FeetLocation = Character->GetActorLocation() - UpVector * CapsuleHalfHeight;
	const FVector TraceStart = FeetLocation + UpVector * FMath::Max(TraceStartOffset, 0.f);
	const FVector TraceEnd = FeetLocation - UpVector * FMath::Max(TraceDistance, 0.f);
	
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CharacterShdow), false, Character);
	
	const bool bHitGround = World->LineTraceSingleByObjectType(OutHit, TraceStart, TraceEnd,
		ObjectQueryParams, QueryParams);
	
	if (!bHitGround
		|| !OutHit.bBlockingHit)
	{
		return false;
	}
	
	OutGroundDistance = FMath::Max(FeetLocation.Z - OutHit.ImpactPoint.Z, 0.f);
	
	return true;
}

void UDRCharacterShadowComponent::RefreshTrackingState()
{
	const bool bCanTrack = ShouldCreateVisuals() && IsValid(ShadowMaterialInstance) && !bStealthed;
	
	SetComponentTickEnabled(bCanTrack);
	
	if (!bCanTrack)
	{
		SetShadowVisible(false);
		return;
	}
	
	UpdateShadow();
}

void UDRCharacterShadowComponent::UpdateShadow()
{
	if (bStealthed
		|| !IsValid(ShadowMaterialInstance))
	{
		SetShadowVisible(false);
		return;
	}
	
	FHitResult GroundHit;
	float GroundDistance = 0.f;
	
	if (!TraceGround(GroundHit, GroundDistance))
	{
		SetShadowVisible(false);
		return;
	}
	
	const float Opacity = CalculateOpacity(GroundDistance);
	
	if (Opacity <= ShadowVisibilityThreshold)
	{
		SetShadowVisible(false);
		return;
	}
	
	const FVector SurfaceNormal = GroundHit.ImpactNormal.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SetShadowVisible(false);
		return;
	}
	
	const FVector ShadowLocation = GroundHit.ImpactPoint + SurfaceNormal * FMath::Max(SurfaceOffset, 0.f);
	
	// Decal의 투영 방향이 지면을 향하도록 표면 법선의 반대 방향 사용
	const FRotator ShadowRotation = (-SurfaceNormal).Rotation();
	
	SetWorldLocationAndRotation(ShadowLocation, ShadowRotation);
	ShadowMaterialInstance->SetScalarParameterValue(OpacityParameterName, Opacity);
	SetShadowVisible(true);	
}

void UDRCharacterShadowComponent::SetShadowVisible(bool NewVisible)
{
	if (IsVisible() != NewVisible)
	{
		SetVisibility(NewVisible);
	}
}

float UDRCharacterShadowComponent::CalculateOpacity(float GroundDistance) const
{
	const float FadeStart = FMath::Max(FullOpacityDistance, 0.f);
	const float FadeEnd = FMath::Max(FadeOutDistance, FadeStart + UE_KINDA_SMALL_NUMBER);
	const float FadeAlpha = 1.f - FMath::SmoothStep(FadeStart, FadeEnd, GroundDistance);

	return FMath::Clamp(MaxOpacity, 0.f, 1.f) * FadeAlpha;
}


#include "DRHoveringWorldItemActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Item/Data/DRWorldItemPresentationProfile.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

ADRHoveringWorldItemActor::ADRHoveringWorldItemActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetReplicateMovement(false);

	// 본 클래스는 RootComponent인 StaticMeshComponent의 사용을 권장하지 않습니다.
	// 액터의 외관은 PresentationMeshComponent를 사용해주세요.
	StaticMeshComponent->SetSimulatePhysics(false);
	StaticMeshComponent->SetNotifyRigidBodyCollision(false);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PresentationMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("PresentationMeshComponent"));

	PresentationMeshComponent->SetupAttachment(StaticMeshComponent);
	PresentationMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PresentationMeshComponent->SetGenerateOverlapEvents(false);

	InteractionSphereComponent = CreateDefaultSubobject<USphereComponent>(
		TEXT("InteractionSphereComponent"));

	InteractionSphereComponent->SetupAttachment(StaticMeshComponent);
	InteractionSphereComponent->SetSphereRadius(InteractionRadius);
	InteractionSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionSphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphereComponent->SetCollisionResponseToChannel(
		DRCollisionChannels::Interaction,
		ECR_Overlap);
	InteractionSphereComponent->SetGenerateOverlapEvents(true);
	
	SpawnTrailVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(
		TEXT("SpawnTrailVFXComponent"));

	SpawnTrailVFXComponent->SetupAttachment(PresentationMeshComponent);
	SpawnTrailVFXComponent->SetAutoActivate(false);
	SpawnTrailVFXComponent->SetIsReplicated(false);

	IdleAuraVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(
		TEXT("IdleAuraVFXComponent"));

	IdleAuraVFXComponent->SetupAttachment(StaticMeshComponent);
	IdleAuraVFXComponent->SetAutoActivate(false);
	IdleAuraVFXComponent->SetIsReplicated(false);
}

void ADRHoveringWorldItemActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (InteractionSphereComponent)
	{
		InteractionSphereComponent->SetSphereRadius(InteractionRadius);
	}
	
	SetActorTickEnabled(GetNetMode() != NM_DedicatedServer);
	
	RefreshItemPresentation();
	HandleWorldItemStateChanged();
	
	if (HasAuthority()
		&& WorldItemState == EDRWorldItemState::Emerging)
	{
		ScheduleEmergenceCompletion();
	}
}

void ADRHoveringWorldItemActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, EmergenceData);
}

void ADRHoveringWorldItemActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (!IsValid(PresentationMeshComponent))
	{
		return;
	}

	switch (WorldItemState)
	{
	case EDRWorldItemState::Emerging:
		UpdateEmergenceTransform();
		break;
		
	case EDRWorldItemState::Dropped:
		if (WasRecentlyRendered(0.25f))
		{
			UpdateHoverTransform();
		}
		break;
		
	default:
		break;
	}
}

bool ADRHoveringWorldItemActor::InitializeHoverPresentation(const FVector& SourceWorldLocation, bool bPlayEmergence)
{
	if (!HasAuthority()
		|| HasActorBegunPlay())
	{
		return false;
	}
	
	EmergenceData.SourceWorldLocation = SourceWorldLocation;
	EmergenceData.StartServerTime = GetSynchronizedWorldTime();
	
	SetWorldItemState(bPlayEmergence ? EDRWorldItemState::Emerging : EDRWorldItemState::Dropped);
	
	return true;	
}

void ADRHoveringWorldItemActor::RefreshItemPresentation()
{
	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	
	// 본 클래스는 RootComponent인 StaticMeshComponent의 사용을 권장하지 않습니다.
	// 액터의 외관은 PresentationMeshComponent를 사용해주세요.
	StaticMeshComponent->SetStaticMesh(nullptr);
	StaticMeshComponent->SetSimulatePhysics(false);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	if (!ItemInstance.IsValid()
		|| !IsValid(Definition))
	{
		PresentationMeshComponent->SetStaticMesh(nullptr);
		InteractionSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		SpawnTrailVFXComponent->Deactivate();
		IdleAuraVFXComponent->Deactivate();
		
		SetActorHiddenInGame(true);
		return;
	}
	
	PresentationMeshComponent->SetStaticMesh(Definition->WorldMesh);
	PresentationMeshComponent->SetRelativeTransform((FTransform::Identity));
	
	SetActorHiddenInGame(false);
	
	RefreshRarityPresentation();
	ApplyPresentationState();
	RefreshPresentationTransform();	
}

void ADRHoveringWorldItemActor::HandleWorldItemStateChanged()
{
	Super::HandleWorldItemStateChanged();
	
	StaticMeshComponent->SetSimulatePhysics(false);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ApplyPresentationState();
	RefreshPresentationTransform();
}

void ADRHoveringWorldItemActor::OnRep_EmergenceData()
{
	RefreshPresentationTransform();
}

void ADRHoveringWorldItemActor::ScheduleEmergenceCompletion()
{
	const float RemainingDuration = GetEmergenceDuration() 
		- FMath::Max(0.f, GetSynchronizedWorldTime() - EmergenceData.StartServerTime);
	
	if (RemainingDuration <= KINDA_SMALL_NUMBER)
	{
		CompleteEmergence();
		return;
	}
	
	// 타이머로 등장 종료를 체크
	GetWorldTimerManager().SetTimer(EmergenceTimerHandle, this, &ThisClass::CompleteEmergence,
		RemainingDuration, false);
}

void ADRHoveringWorldItemActor::CompleteEmergence()
{
	if (!HasAuthority()
		|| WorldItemState != EDRWorldItemState::Emerging)
	{
		return;
	}
	
	SetWorldItemState(EDRWorldItemState::Dropped);
	MulticastPlayDroppedSound();
}

void ADRHoveringWorldItemActor::ApplyPresentationState()
{
	const bool bCanRender = GetNetMode() != NM_DedicatedServer;
	const bool bIsEmerging = WorldItemState == EDRWorldItemState::Emerging;
	const bool bIsHovering = WorldItemState == EDRWorldItemState::Dropped;
	
	// 온전히 Drop된 이후 상호작용이 가능
	InteractionSphereComponent->SetCollisionEnabled(bIsHovering ?
		ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	
	if (!bCanRender)
	{
		SpawnTrailVFXComponent->Deactivate();
		IdleAuraVFXComponent->Deactivate();
		return;
	}
	
	if (bIsEmerging 
		&& IsValid(SpawnTrailVFXComponent->GetAsset()))
	{
		SpawnTrailVFXComponent->Activate(true);
	}
	else
	{
		SpawnTrailVFXComponent->Deactivate();
	}
	
	if (bIsHovering 
		&& IsValid(IdleAuraVFXComponent->GetAsset()))
	{
		IdleAuraVFXComponent->Activate(true);
	}
	else
	{
		IdleAuraVFXComponent->Deactivate();
	}
}

void ADRHoveringWorldItemActor::RefreshRarityPresentation()
{
	const UDRItemDefinition* Definition = ItemInstance.Definition;
	const UDRWorldItemPresentationProfile* Profile = GetPresentationProfile();
	
	if (!IsValid(Definition)
		|| !IsValid(Profile))
	{
		SpawnTrailVFXComponent->Deactivate();
		SpawnTrailVFXComponent->SetAsset(nullptr);
		
		IdleAuraVFXComponent->Deactivate();
		IdleAuraVFXComponent->SetAsset(nullptr);
		
		return;
	}
	
	const FDRWorldItemRarityVisual& RarityVisual = Profile->GetRarityVisual(Definition->Rarity);
	
	UNiagaraSystem* SpawnTrailSystem = IsValid(RarityVisual.SpawnTrailOverride) ? 
		RarityVisual.SpawnTrailOverride : Profile->SpawnTrailSystem;
	UNiagaraSystem* IdleAuraSystem = IsValid(RarityVisual.IdleAuraOverride) ? 
		RarityVisual.IdleAuraOverride : Profile->IdleAuraSystem;
	
	SpawnTrailVFXComponent->SetAsset(SpawnTrailSystem);
	IdleAuraVFXComponent->SetAsset(IdleAuraSystem);
	
	Profile->ApplyRarityParameters(SpawnTrailVFXComponent, Definition->Rarity);	
	Profile->ApplyRarityParameters(IdleAuraVFXComponent, Definition->Rarity);
	
	IdleAuraVFXComponent->SetVariableFloat(TEXT("User.InteractionRadius"), InteractionRadius);
}

void ADRHoveringWorldItemActor::RefreshPresentationTransform()
{
	switch (WorldItemState)
	{
	case EDRWorldItemState::Emerging:
		UpdateEmergenceTransform();
		break;
		
	case EDRWorldItemState::Dropped:
		UpdateHoverTransform();
		break;
		
	default:
		PresentationMeshComponent->SetRelativeLocation(FVector::ZeroVector);
		break;
	}
}

void ADRHoveringWorldItemActor::UpdateEmergenceTransform()
{
	if (EmergenceData.StartServerTime < 0.f)
	{
		PresentationMeshComponent->SetRelativeLocation(FVector::ZeroVector);
		return;
	}
	
	const float Duration = GetEmergenceDuration();
	const float ElapsedTime = FMath::Max(0.f, GetSynchronizedWorldTime() - EmergenceData.StartServerTime);
	const float RawAlpha = Duration > KINDA_SMALL_NUMBER ? 
		FMath::Clamp(ElapsedTime/ Duration, 0.f, 1.f) : 1.f;
	
	float MovementAlpha = FMath::InterpEaseOut(0.f, 1.f, RawAlpha,2.f);
	
	const UDRWorldItemPresentationProfile* Profile = GetPresentationProfile();
	
	if (IsValid(Profile)
		&& IsValid(Profile->EmergenceCurve))
	{
		MovementAlpha = FMath::Clamp(Profile->EmergenceCurve->GetFloatValue(RawAlpha),
			0.f, 1.f);
	}
	
	const FVector StartRelativelocation = GetActorTransform().InverseTransformPosition(EmergenceData.SourceWorldLocation);
	FVector RelativeLocation = FMath::Lerp(StartRelativelocation, FVector::ZeroVector, MovementAlpha);
	
	RelativeLocation.Z += 4.f * GetEmergenceArcHeight() * RawAlpha * (1.f - RawAlpha);
	
	PresentationMeshComponent->SetRelativeLocation(RelativeLocation);
}

void ADRHoveringWorldItemActor::UpdateHoverTransform()
{
	const float PhaseRatio = static_cast<float>(GetTypeHash(ItemInstance.InstanceId) % 1024) / 1024.f;
	
	const float PhaseRadians = PhaseRatio * UE_TWO_PI;
	const float TimeRadians = GetSynchronizedWorldTime() * GetHoverFrequency() * UE_TWO_PI;
	const float HoverOffset = FMath::Sin(TimeRadians + PhaseRadians) * GetHoverAmplitude();
	
	PresentationMeshComponent->SetRelativeLocation(FVector::UpVector * HoverOffset);	
}

float ADRHoveringWorldItemActor::GetSynchronizedWorldTime() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0.f;
	}
	
	const AGameStateBase* GameState = World->GetGameState();
	
	return IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float ADRHoveringWorldItemActor::GetEmergenceDuration() const
{
	const UDRWorldItemPresentationProfile* Profile = GetPresentationProfile();
	
	return IsValid(Profile) ? FMath::Max(0.f, Profile->EmergenceDuration) : 0.65f;
}

float ADRHoveringWorldItemActor::GetEmergenceArcHeight() const
{
	const UDRWorldItemPresentationProfile* Profile = GetPresentationProfile();
	
	return IsValid(Profile) ? FMath::Max(0.f, Profile->EmergenceArcHeight) : 80.f;
}

float ADRHoveringWorldItemActor::GetHoverAmplitude() const
{
	const UDRWorldItemPresentationProfile* Profile = GetPresentationProfile();
	
	return IsValid(Profile) ? FMath::Max(0.f, Profile->HoverAmplitude) : 8.f;
}

float ADRHoveringWorldItemActor::GetHoverFrequency() const
{
	const UDRWorldItemPresentationProfile* Profile = GetPresentationProfile();
	
	return IsValid(Profile) ? FMath::Max(0.f, Profile->HoverFrequency) : 0.5f;
}

const UDRWorldItemPresentationProfile* ADRHoveringWorldItemActor::GetPresentationProfile() const
{
	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	return IsValid(Definition) ? Definition->WorldItemPresentationProfile.Get() : nullptr;
	
}

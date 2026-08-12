#include "DRTeleportPoint.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/Subsystem/DRTeleportSubsystem.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ADRTeleportPoint::ADRTeleportPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(Root);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(Root);
	InteractionVolume->SetBoxExtent(FVector(160.f, 160.f, 120.f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(true);
	InteractionVolume->SetHiddenInGame(true);
}

void ADRTeleportPoint::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(InteractionVolume))
	{
		InteractionVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionVolumeBeginOverlap);
		InteractionVolume->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionVolumeEndOverlap);
	}

	if (UWorld* World = GetWorld())
	{
		if (UDRTeleportSubsystem* TeleportSubsystem = World->GetSubsystem<UDRTeleportSubsystem>())
		{
			TeleportSubsystem->RegisterTeleportPoint(this);
		}
	}
}

void ADRTeleportPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(InteractionVolume))
	{
		InteractionVolume->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionVolumeBeginOverlap);
		InteractionVolume->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleInteractionVolumeEndOverlap);
	}

	if (UWorld* World = GetWorld())
	{
		if (UDRTeleportSubsystem* TeleportSubsystem = World->GetSubsystem<UDRTeleportSubsystem>())
		{
			TeleportSubsystem->UnregisterTeleportPoint(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ADRTeleportPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRTeleportPoint, bRegistered);
	DOREPLIFETIME(ADRTeleportPoint, OwnerTeamId);
}

FText ADRTeleportPoint::GetTeleportDisplayName() const
{
	return TeleportDisplayName.IsEmpty() ? FText::FromString(GetName()) : TeleportDisplayName;
}

bool ADRTeleportPoint::IsRegisteredForTeam(int32 TeamId) const
{
	if (!bRegistered || TeamId == INDEX_NONE)
	{
		return false;
	}

	switch (AccessType)
	{
	case EDRTeleportAccessType::Public:
		return true;

	case EDRTeleportAccessType::Claimable:
	case EDRTeleportAccessType::TeamOwned:
		return OwnerTeamId == TeamId;

	default:
		return false;
	}
}

FTransform ADRTeleportPoint::GetTeleportArrivalTransform() const
{
	return FTransform(GetActorRotation(), GetActorLocation() + GetActorRotation().RotateVector(TeleportArrivalOffset));
}

bool ADRTeleportPoint::CanRegisterForTeam(int32 TeamId, APawn* Interactor) const
{
	if (!IsValid(Interactor) || TeamId == INDEX_NONE || !IsInteractorInRange(Interactor))
	{
		return false;
	}

	if (bRegistered && AccessType != EDRTeleportAccessType::Public)
	{
		return false;
	}

	switch (AccessType)
	{
	case EDRTeleportAccessType::Public:
		// 공용 텔레포트는 어떤 팀이든 등록할 수 있다.
		return true;

	case EDRTeleportAccessType::Claimable:
		// 선점형 텔레포트는 아직 소유 팀이 없을 때만 최초 등록할 수 있다.
		return OwnerTeamId == INDEX_NONE;

	case EDRTeleportAccessType::TeamOwned:
		// 팀 고정 텔레포트는 미리 지정된 팀만 등록할 수 있다.
		return OwnerTeamId == TeamId;

	default:
		return false;
	}
}

bool ADRTeleportPoint::TryRegisterForTeam(int32 TeamId, APawn* Interactor)
{
	if (!HasAuthority() || !CanRegisterForTeam(TeamId, Interactor))
	{
		return false;
	}

	if (AccessType == EDRTeleportAccessType::Claimable)
	{
		// 선점형은 등록 순간부터 해당 팀 전용 텔레포트가 된다.
		OwnerTeamId = TeamId;
	}

	bRegistered = true;
	ForceNetUpdate();

	return true;
}

void ADRTeleportPoint::HandleInteractionVolumeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool IsFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsValid(Pawn))
	{
		return;
	}

	const bool bWasEmpty = OverlappingInteractors.IsEmpty();
	OverlappingInteractors.AddUnique(Pawn);

	if (bWasEmpty && OverlappingInteractors.Num() > 0)
	{
		NotifyTeleportOccupied(Pawn);
	}

	if (UDRTeleportComponent* TeleportComponent = Pawn->FindComponentByClass<UDRTeleportComponent>())
	{
		TeleportComponent->SetCurrentInteractableTeleport(this);
	}
}

void ADRTeleportPoint::HandleInteractionVolumeEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsValid(Pawn))
	{
		return;
	}

	OverlappingInteractors.Remove(Pawn);

	if (OverlappingInteractors.IsEmpty())
	{
		NotifyTeleportEmptied();
	}

	if (UDRTeleportComponent* TeleportComponent = Pawn->FindComponentByClass<UDRTeleportComponent>())
	{
		TeleportComponent->ClearCurrentInteractableTeleport(this);
	}
}

bool ADRTeleportPoint::IsInteractorInRange(APawn* Interactor) const
{
	return IsValid(Interactor) && OverlappingInteractors.Contains(Interactor);
}

void ADRTeleportPoint::NotifyTeleportOccupied(APawn* Interactor)
{
	// 첫 플레이어가 범위에 들어왔을 때만 호출해 occupied MI로 바꾸기 쉽게 한다.
	OnTeleportOccupied.Broadcast(Interactor);
	BP_OnTeleportOccupied(Interactor);
}

void ADRTeleportPoint::NotifyTeleportEmptied()
{
	// 마지막 플레이어가 범위에서 나갔을 때만 호출해 idle MI로 되돌리기 쉽게 한다.
	OnTeleportEmptied.Broadcast();
	BP_OnTeleportEmptied();
}

#pragma region Interact
bool ADRTeleportPoint::CanInteract_Implementation(APawn* Interactor) const
{
	if (!IsValid(Interactor) || !IsInteractorInRange(Interactor))
	{
		return false;
	}

	const ADRPlayerState* DRPlayerState = Interactor->GetPlayerState<ADRPlayerState>();
	const int32 TeamId = IsValid(DRPlayerState) ? DRPlayerState->GetTeamId() : INDEX_NONE;
	if (TeamId == INDEX_NONE)
	{
		return false;
	}

	if (const UDRTeleportComponent* TeleportComponent = Interactor->FindComponentByClass<UDRTeleportComponent>())
	{
		if (TeleportComponent->IsTeleportPointRegistered(this))
		{
			return true;
		}
	}

	return CanRegisterForTeam(TeamId, Interactor);
}

bool ADRTeleportPoint::Interact_Implementation(APawn* Interactor)
{
	if (!CanInteract(Interactor))
	{
		return false;
	}

	UDRTeleportComponent* TeleportComponent = Interactor->FindComponentByClass<UDRTeleportComponent>();
	if (!IsValid(TeleportComponent))
	{
		return false;
	}

	if (TeleportComponent->IsTeleportPointRegistered(this))
	{
		return TeleportComponent->RequestUseTeleportPoint(this);
	}

	const ADRPlayerState* DRPlayerState = Interactor->GetPlayerState<ADRPlayerState>();
	const int32 TeamId = IsValid(DRPlayerState) ? DRPlayerState->GetTeamId() : INDEX_NONE;
	if (!CanRegisterForTeam(TeamId, Interactor))
	{
		return false;
	}

	UWorld* World = GetWorld();
	UDRTeleportSubsystem* TeleportSubsystem = IsValid(World) ? World->GetSubsystem<UDRTeleportSubsystem>() : nullptr;
	if (!IsValid(TeleportSubsystem) || !TeleportSubsystem->TryRegisterTeleportPoint(this, Interactor, TeamId))
	{
		return false;
	}

	TeleportComponent->NotifyTeleportRegistered(this);
	return true;
}
#pragma endregion 

#include "DRIceWallSegment.h"

#include "DeepRaiders/Player/DRPlayerState.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

void ADRIceWallSegment::InitializeSegment(const int32 InOwnerTeamId, const float InMaxHealth, const FVector& InDimensions)
{
	check(HasAuthority());
	OwnerTeamId = InOwnerTeamId;
	MaxHealth = FMath::Max(1.f, InMaxHealth);
	SetSegmentDimensions(InDimensions);
}

void ADRIceWallSegment::SetSegmentDimensions(const FVector& InDimensions)
{
	const FVector SafeDimensions(
		FMath::Max(1.f, InDimensions.X),
		FMath::Max(1.f, InDimensions.Y),
		FMath::Max(1.f, InDimensions.Z));
	SegmentDimensions = SafeDimensions;
	if (HasActorBegunPlay())
	{
		ApplySegmentDimensions();
	}
}

void ADRIceWallSegment::BeginPlay()
{
	Super::BeginPlay();
	ApplySegmentDimensions();
}

void ADRIceWallSegment::OnRep_SegmentDimensions()
{
	ApplySegmentDimensions();
}

void ADRIceWallSegment::ApplySegmentDimensions()
{
	if (!IsValid(BreakableMeshComponent))
	{
		return;
	}

	const UStaticMesh* StaticMesh = BreakableMeshComponent->GetStaticMesh();
	if (!IsValid(StaticMesh))
	{
		return;
	}

	const FVector CurrentDimensions = StaticMesh->GetBounds().BoxExtent * 2.f
		* BreakableMeshComponent->GetRelativeScale3D();
	if (CurrentDimensions.X <= KINDA_SMALL_NUMBER || CurrentDimensions.Y <= KINDA_SMALL_NUMBER
		|| CurrentDimensions.Z <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	SetActorScale3D(GetActorScale3D() * (SegmentDimensions / CurrentDimensions));
}

float ADRIceWallSegment::TakeDamage(const float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!CanReceiveDamageFrom(EventInstigator))
	{
		return 0.f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool ADRIceWallSegment::CanReceiveDamageFrom(const AController* EventInstigator) const
{
	const ADRPlayerState* AttackerPlayerState = IsValid(EventInstigator)
		? EventInstigator->GetPlayerState<ADRPlayerState>()
		: nullptr;
	if (!IsValid(AttackerPlayerState) || OwnerTeamId == INDEX_NONE)
	{
		return false;
	}

	return AttackerPlayerState->GetTeamId() != OwnerTeamId;
}

void ADRIceWallSegment::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, OwnerTeamId);
	DOREPLIFETIME(ThisClass, SegmentDimensions);
}

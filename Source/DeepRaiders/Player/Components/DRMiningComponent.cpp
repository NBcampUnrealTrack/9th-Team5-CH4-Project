#include "DRMiningComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "DeepRaiders/Player/DRCNPlayerCharacter.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelWorld.h"

UDRMiningComponent::UDRMiningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UDRMiningComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ADRCNPlayerCharacter>(GetOwner());
}

void UDRMiningComponent::TryMine()
{
	if (!CanMine())
	{
		return;
	}

	FHitResult HitResult;
	if (!PerformMiningTrace(HitResult))
	{
		return;
	}

	if (!MineLocal(HitResult))
	{
		return;
	}

	LastMineTime = GetWorld()->GetTimeSeconds();
}

void UDRMiningComponent::Server_RequestMine_Implementation(FVector_NetQuantize TraceStart,	FVector_NetQuantize TraceEnd)
{
	if (!IsValid(OwnerCharacter.Get()))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ServerMineTrace), false);
	QueryParams.AddIgnoredActor(OwnerCharacter.Get());

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams);

	if (!bHit)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Mining] Server hit: Actor=%s Location=%s"),
		*GetNameSafe(HitResult.GetActor()),
		*HitResult.ImpactPoint.ToString());
}

bool UDRMiningComponent::CanMine() const
{
	if (!IsValid(OwnerCharacter.Get()))
	{
		return false;
	}

	if (!OwnerCharacter->IsLocallyControlled())
	{
		return false;
	}

	return !IsMineOnCooldown();
}

bool UDRMiningComponent::IsMineOnCooldown() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return true;
	}

	const float CurrentTime = World->GetTimeSeconds();
	return CurrentTime - LastMineTime < MineCooldown;
}

bool UDRMiningComponent::PerformMiningTrace(FHitResult& OutHitResult) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(OwnerCharacter.Get()))
	{
		return false;
	}

	FVector TraceStart;
	FRotator TraceRotation;
	GetTraceViewPoint(TraceStart, TraceRotation);

	const FVector TraceEnd =
		TraceStart + (TraceRotation.Vector() * MineTraceDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MineTrace), false);
	QueryParams.AddIgnoredActor(OwnerCharacter.Get());

	const bool bHit = World->LineTraceSingleByChannel(
		OutHitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams);

	if (bDrawDebugTrace)
	{
		const FColor TraceColor = bHit ? FColor::Green : FColor::Red;

		DrawDebugLine(
			World,
			TraceStart,
			TraceEnd,
			TraceColor,
			false,
			1.f,
			0,
			1.f);
	}

	return bHit;
}

bool UDRMiningComponent::MineLocal(const FHitResult& HitResult) const
{
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor());
	if (!IsValid(VoxelWorld))
	{
		return false;
	}

	const FVector MinePosition =
		HitResult.ImpactPoint - (HitResult.ImpactNormal * MineRadius * 0.5f);

	UVoxelSphereTools::RemoveSphere(
		VoxelWorld,
		MinePosition,
		MineRadius,
		nullptr,
		nullptr,
		true,
		true,
		true);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Mining] Local mine: VoxelWorld=%s Location=%s Radius=%.2f"),
		*GetNameSafe(VoxelWorld),
		*MinePosition.ToString(),
		MineRadius);

	return true;
}

void UDRMiningComponent::GetTraceViewPoint(
	FVector& OutLocation,
	FRotator& OutRotation) const
{
	if (!IsValid(OwnerCharacter.Get()))
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}

	if (AController* Controller = OwnerCharacter->GetController())
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}

	OwnerCharacter->GetActorEyesViewPoint(OutLocation, OutRotation);
}

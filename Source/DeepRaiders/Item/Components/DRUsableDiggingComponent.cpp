// Fill out your copyright notice in the Description page of Project Settings.


#include "DRUsableDiggingComponent.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Item/DRUsableDiggingDefinition.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UDRUsableDiggingComponent::UDRUsableDiggingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRUsableDiggingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStartOnBeginPlay)
	{
		StartDigging();
	}
}

void UDRUsableDiggingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDigging();

	Super::EndPlay(EndPlayReason);
}

void UDRUsableDiggingComponent::StartDigging()
{
	if (!CanRunDigging() || bDiggingStarted)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (DiggingDefinition->StartDelay <= 0.f)
	{
		StartDiggingInternal();
		return;
	}

	DrawDigRadiusDebug(
		GetOwner()->GetActorLocation(),
		FColor::Yellow,
		DebugPreviewDrawTime);

	World->GetTimerManager().SetTimer(
		StartTimerHandle,
		this,
		&ThisClass::StartDiggingInternal,
		DiggingDefinition->StartDelay,
		false);
}

void UDRUsableDiggingComponent::StopDigging()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(StartTimerHandle);
}

bool UDRUsableDiggingComponent::CanRunDigging() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner)
		&& Owner->HasAuthority()
		&& IsValid(DiggingDefinition)
		&& DiggingDefinition->DigRadius > 0.f;
}

void UDRUsableDiggingComponent::StartDiggingInternal()
{
	if (!CanRunDigging() || bDiggingStarted)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	bDiggingStarted = true;
	ExecuteDig();
}

void UDRUsableDiggingComponent::ExecuteDig()
{
	if (!CanRunDigging())
	{
		FinishDigging();
		return;
	}

	const FVector DigLocation = GetOwner()->GetActorLocation();
	DrawDigRadiusDebug(
		DigLocation,
		FColor::Red,
		DebugExplosionDrawTime);

	RequestDigAtLocation(DigLocation);
	FinishDigging();
}

bool UDRUsableDiggingComponent::RequestDigAtLocation(const FVector& DigLocation) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(DiggingDefinition))
	{
		return false;
	}

	UDRVoxelTerrainSubsystem* TerrainSubsystem =
		World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(TerrainSubsystem))
	{
		return false;
	}

	FDRTerrainDigOperation Operation;
	if (!TerrainSubsystem->RequestDigAtLocation(
		DigLocation,
		DiggingDefinition->DigRadius,
		&Operation))
	{
		return false;
	}

	ADRMiningGameStateBase* MiningGameState =
		World->GetGameState<ADRMiningGameStateBase>();
	if (IsValid(MiningGameState))
	{
		MiningGameState->RegisterTerrainDig(Operation);
	}

	return true;
}

void UDRUsableDiggingComponent::DrawDigRadiusDebug(
	const FVector& Center,
	const FColor& Color,
	float DrawTime) const
{
	if (!bDrawDebugDigRadius || !IsValid(DiggingDefinition))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	DrawDebugSphere(
		World,
		Center,
		DiggingDefinition->DigRadius,
		32,
		Color,
		false,
		DrawTime,
		0,
		2.f);
}

void UDRUsableDiggingComponent::FinishDigging()
{
	StopDigging();

	if (!IsValid(DiggingDefinition) || !DiggingDefinition->bDestroyOwnerOnFinished)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasAuthority())
	{
		Owner->Destroy();
	}
}


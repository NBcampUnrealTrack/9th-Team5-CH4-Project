#include "DRVoxelContainmentComponent.h"

#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelWorld.h"

UDRVoxelContainmentComponent::UDRVoxelContainmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDRVoxelContainmentComponent::EvaluateVoxelContainment(AVoxelWorld* VoxelWorld)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return;
	}

	const FVoxelCapsuleOccupancy Occupancy =
		GetVoxelCapsuleOccupancy(*VoxelWorld);
	if (Occupancy.FullySurroundedLayerCount >=
		FMath::Clamp(RequiredSurroundedLayers, 1, 3))
	{
		EnterVoxelContainedMode(*VoxelWorld);
	}
}

void UDRVoxelContainmentComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateVoxelContainedMode();
}

UDRCharacterMovementComponent* UDRVoxelContainmentComponent::GetMovementComponent() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return IsValid(Character)
		? Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
}

UDRVoxelContainmentComponent::FVoxelCapsuleOccupancy
UDRVoxelContainmentComponent::GetVoxelCapsuleOccupancy(
	AVoxelWorld& VoxelWorld) const
{
	FVoxelCapsuleOccupancy Result;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return Result;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		return Result;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float CylinderHalfHeight =
		FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
	const FVector Center = Capsule->GetComponentLocation();
	const FVector Forward = Character->GetActorForwardVector();
	const FVector Right = Character->GetActorRightVector();
	const FVector HorizontalOffsets[] =
	{
		FVector::ZeroVector,
		Forward * Radius * 0.55f,
		-Forward * Radius * 0.55f,
		Right * Radius * 0.55f,
		-Right * Radius * 0.55f
	};
	const float VerticalOffsets[] =
	{
		-CylinderHalfHeight * 0.75f,
		0.f,
		CylinderHalfHeight * 0.75f
	};

	const FVoxelIntBox WorldBounds = VoxelWorld.GetWorldBounds();
	FIntVector SamplePositions[UE_ARRAY_COUNT(VerticalOffsets)][UE_ARRAY_COUNT(HorizontalOffsets)];
	FVoxelIntBoxWithValidity LockBounds;
	for (int32 VerticalIndex = 0; VerticalIndex < UE_ARRAY_COUNT(VerticalOffsets); ++VerticalIndex)
	{
		for (int32 HorizontalIndex = 0; HorizontalIndex < UE_ARRAY_COUNT(HorizontalOffsets); ++HorizontalIndex)
		{
			const FVector SampleLocation =
				Center + HorizontalOffsets[HorizontalIndex] +
				FVector::UpVector * VerticalOffsets[VerticalIndex];
			const FIntVector VoxelPosition = VoxelWorld.GlobalToLocal(SampleLocation);
			SamplePositions[VerticalIndex][HorizontalIndex] = VoxelPosition;
			if (WorldBounds.Contains(VoxelPosition))
			{
				LockBounds += VoxelPosition;
			}
		}
	}

	if (!LockBounds.IsValid())
	{
		return Result;
	}

	FVoxelData& Data = VoxelWorld.GetData();
	FVoxelReadScopeLock Lock(Data, LockBounds.GetBox(), FUNCTION_FNAME);

	constexpr uint8 FullySurroundedMask =
		(1 << UE_ARRAY_COUNT(HorizontalOffsets)) - 1;
	for (int32 VerticalIndex = 0; VerticalIndex < UE_ARRAY_COUNT(VerticalOffsets); ++VerticalIndex)
	{
		uint8 SolidLayerMask = 0;
		for (int32 HorizontalIndex = 0; HorizontalIndex < UE_ARRAY_COUNT(HorizontalOffsets); ++HorizontalIndex)
		{
			const FIntVector& VoxelPosition = SamplePositions[VerticalIndex][HorizontalIndex];
			if (WorldBounds.Contains(VoxelPosition) &&
				!Data.GetValue(VoxelPosition, 0).IsEmpty())
			{
				SolidLayerMask |= 1 << HorizontalIndex;
			}
		}

		// 높이별 결과를 합치지 않는다. 경사진 한쪽 벽이 서로 다른 높이에서
		// 반대편 표본까지 채운 것처럼 보이는 오탐을 막는다.
		if ((SolidLayerMask & FullySurroundedMask) == FullySurroundedMask)
		{
			++Result.FullySurroundedLayerCount;
		}
	}

	return Result;
}

void UDRVoxelContainmentComponent::EnterVoxelContainedMode(AVoxelWorld& VoxelWorld)
{
	UDRCharacterMovementComponent* Movement = GetMovementComponent();
	if (!IsValid(Movement))
	{
		return;
	}

	VoxelContainmentWorld = &VoxelWorld;
	ReleaseStartTime = -1.f;
	NextCheckTime = 0.f;
	Movement->EnterVoxelContainedMode();
	SetComponentTickEnabled(true);
}

void UDRVoxelContainmentComponent::UpdateVoxelContainedMode()
{
	UDRCharacterMovementComponent* Movement = GetMovementComponent();
	if (!IsValid(Movement) ||
		!Movement->IsCustomMovementModeActive(EDRCustomMovementMode::VoxelContained))
	{
		ClearContainmentState();
		return;
	}

	UWorld* World = GetWorld();
	AVoxelWorld* VoxelWorld = VoxelContainmentWorld.Get();
	if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return;
	}

	const float Time = World->GetTimeSeconds();
	if (Time < NextCheckTime)
	{
		return;
	}
	NextCheckTime = Time + FMath::Max(0.01f, CheckInterval);

	const FVoxelCapsuleOccupancy Occupancy =
		GetVoxelCapsuleOccupancy(*VoxelWorld);
	if (Occupancy.FullySurroundedLayerCount >=
		FMath::Clamp(RequiredSurroundedLayers, 1, 3))
	{
		ReleaseStartTime = -1.f;
		return;
	}

	if (ReleaseStartTime < 0.f)
	{
		ReleaseStartTime = Time;
		return;
	}
	if (Time - ReleaseStartTime < FMath::Max(0.f, ReleaseDelay))
	{
		return;
	}

	Movement->ExitVoxelContainedMode();
	ClearContainmentState();
}

void UDRVoxelContainmentComponent::ClearContainmentState()
{
	VoxelContainmentWorld.Reset();
	ReleaseStartTime = -1.f;
	NextCheckTime = 0.f;
	SetComponentTickEnabled(false);
}

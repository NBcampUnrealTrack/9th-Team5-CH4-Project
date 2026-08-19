// DRTerrainAreaScannerActor.cpp

#include "DRTerrainAreaScannerActor.h"

#include "DrawDebugHelpers.h"
#include "DRVoxelTerrainQueryLibrary.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

ADRTerrainAreaScannerActor::ADRTerrainAreaScannerActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADRTerrainAreaScannerActor::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(
		ScanTimerHandle,
		this,
		&ThisClass::ScanVoxelArea,
		ScanInterval,
		true,
		0.f);
}

void ADRTerrainAreaScannerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrawDebugBox)
	{
		DrawScanDebugBox();
	}
}

void ADRTerrainAreaScannerActor::ScanVoxelArea()
{
	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		return;
	}

	if (bEnableSnowAccumulation)
	{
		int32 ModifiedVoxelCount = 0;

		const bool bSnowApplied = UDRVoxelTerrainQueryLibrary::AddSnowInBox(
			VoxelWorld,
			GetActorLocation(),
			BoxExtent,
			SampleStep,
			SnowAmountPerTick,
			SnowMaterialIndex,
			ModifiedVoxelCount);

		if (bSnowApplied)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Snow applied. ModifiedVoxelCount=%d"),
				ModifiedVoxelCount);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to apply snow."));
		}
	}

	TMap<uint8, int32> MaterialCounts;
	int32 TotalCount = 0;

	const bool bSuccess = UDRVoxelTerrainQueryLibrary::GetMaterialCountsInBox(
		VoxelWorld,
		GetActorLocation(),
		BoxExtent,
		SampleStep,
		TeamMaterialIndices,
		MaterialCounts,
		TotalCount);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to scan voxel area."));
		return;
	}

	if (TotalCount <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("No matching solid voxels found in area."));
		return;
	}

	for (const TPair<uint8, int32>& Pair : MaterialCounts)
	{
		const uint8 MaterialIndex = Pair.Key;
		const int32 Count = Pair.Value;
		const float Percent =
			static_cast<float>(Count) /
			static_cast<float>(TotalCount) *
			100.f;

		UE_LOG(
			LogTemp,
			Log,
			TEXT("Material %d: Count=%d Percent=%.2f%%"),
			MaterialIndex,
			Count,
			Percent);
	}
}

void ADRTerrainAreaScannerActor::DrawScanDebugBox() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	DrawDebugBox(
		World,
		GetActorLocation(),
		BoxExtent,
		FColor::Cyan,
		false,
		0.f,
		0,
		3.f);
}
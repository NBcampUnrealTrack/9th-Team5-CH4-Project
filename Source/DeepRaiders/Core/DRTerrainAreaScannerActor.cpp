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

	GetWorldTimerManager().SetTimer(
		SnowTimerHandle,
		this,
		&ThisClass::AddSnowArea,
		SnowInterval,
		true,
		0.f);
}

void ADRTerrainAreaScannerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// if (bDrawDebugBox)
	// {
	// 	DrawScanDebugBox();
	// }
	//
	// if (bEnableSnowAccumulation && DepositRequests.Num() > 0)
	// {
	// 	int32 ModifiedVoxelCount = 0;
	// 	int32 RemainingRequestCount = 0;
	//
	// 	UDRVoxelTerrainQueryLibrary::ProcessDepositInBoxRequestsTick(
	// 		DepositRequests,
	// 		MaxSnowColumnsPerTick,
	// 		ModifiedVoxelCount,
	// 		RemainingRequestCount);
	//
	// 	if (ModifiedVoxelCount > 0)
	// 	{
	// 		UE_LOG(
	// 			LogTemp,
	// 			Verbose,
	// 			TEXT("Deposit processed. ModifiedVoxelCount=%d RemainingRequestCount=%d"),
	// 			ModifiedVoxelCount,
	// 			RemainingRequestCount);
	// 	}
	// }
}

void ADRTerrainAreaScannerActor::ScanVoxelArea()
{
	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		return;
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

void ADRTerrainAreaScannerActor::AddSnowArea()
{
	// if (!bEnableSnowAccumulation)
	// {
	// 	DepositRequests.Reset();
	// 	return;
	// }
	//
	// if (!IsValid(VoxelWorld))
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
	// 	DepositRequests.Reset();
	// 	return;
	// }
	//
	// DepositRequests.Reset();
	//
	// const int32 RandomSeed = FMath::Rand();
	//
	// FDRVoxelDepositInBoxRequest Request;
	// const bool bRequestCreated = UDRVoxelTerrainQueryLibrary::MakeDepositInBoxRequest(
	// 	VoxelWorld,
	// 	GetActorLocation(),
	// 	BoxExtent,
	// 	SampleStep,
	// 	SnowAmountPerTick,
	// 	SnowMaterialIndex,
	// 	MaxHeightStep,
	// 	RandomSeed,
	// 	Request);
	//
	// if (!bRequestCreated)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Failed to create snow deposit request."));
	// 	return;
	// }
	//
	// DepositRequests.Add(MoveTemp(Request));
	//
	// UE_LOG(LogTemp, Verbose, TEXT("Snow deposit request created."));
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

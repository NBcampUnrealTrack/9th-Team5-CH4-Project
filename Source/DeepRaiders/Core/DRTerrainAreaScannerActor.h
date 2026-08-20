// DRTerrainAreaScannerActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTerrainAreaScannerActor.generated.h"

struct FDRVoxelDepositInBoxRequest;
class AVoxelWorld;

UCLASS()
class DEEPRAIDERS_API ADRTerrainAreaScannerActor : public AActor
{
	GENERATED_BODY()

public:
	ADRTerrainAreaScannerActor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain")
	FVector BoxExtent = FVector(500.f, 500.f, 500.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain")
	float SampleStep = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain")
	TArray<uint8> TeamMaterialIndices;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain")
	float ScanInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain")
	bool bDrawDebugBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain|Snow")
	float SnowInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain|Snow")
	bool bEnableSnowAccumulation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain|Snow")
	float SnowAmountPerTick = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain|Snow")
	uint8 SnowMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain|Snow")
	int32 SmoothRadius = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain|Snow")
	int32 MaxHeightStep = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel Terrain|Snow")
	int32 MaxSnowColumnsPerTick = 128;

private:
	UPROPERTY()
	TArray<FDRVoxelDepositInBoxRequest> DepositRequests;

	void ScanVoxelArea();
	void AddSnowArea();
	void DrawScanDebugBox() const;

	FTimerHandle ScanTimerHandle;
	FTimerHandle SnowTimerHandle;
};

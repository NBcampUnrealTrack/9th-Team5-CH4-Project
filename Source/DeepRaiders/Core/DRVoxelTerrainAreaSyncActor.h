#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelTerrainQueryLibrary.h"
#include "DRVoxelTerrainAreaSyncActor.generated.h"

class AVoxelWorld;

UCLASS()
class DEEPRAIDERS_API ADRVoxelTerrainAreaSyncActor : public AActor
{
	GENERATED_BODY()

public:
	ADRVoxelTerrainAreaSyncActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	FVector BoxExtent = FVector(500.f, 500.f, 500.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	float SampleStep = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	TArray<uint8> TeamMaterialIndices;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	float ScanInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	bool bDrawDebugBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug")
	bool bDrawDepositGridPoints = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug", meta=(ClampMin="1.0"))
	float DepositGridPointSize = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug", meta=(ClampMin="1"))
	int32 MaxDebugDepositGridPoints = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug")
	FColor DepositGridPointColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	float DepositInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	bool bEnableDepositAccumulation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	float DepositAmountPerTick = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	uint8 DepositMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	bool bOnlyTopSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	bool bUseJitteredDepositSamples = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DepositJitterRatio = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0"))
	int32 DepositPatchRadius = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinSurfaceDepositChance = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaxSurfaceDepositChance = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.01"))
	float LowerSurfaceSelectionBias = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	int32 MaxDepositScanColumnsPerTick = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	int32 MaxDepositVoxelsPerTick = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	int32 MaxReplicatedDeltaRecords = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	int32 TerrainEditRevision = 0;

	UPROPERTY(ReplicatedUsing=OnRep_DepositDeltaRecords, BlueprintReadOnly, Category="Voxel Terrain|Sync")
	TArray<FDRVoxelDepositDeltaRecord> DepositDeltaRecords;

	UFUNCTION()
	void OnRep_DepositDeltaRecords();

private:
	UPROPERTY()
	TArray<FDRVoxelDepositInBoxRequest> DepositRequests;

	int32 LastAppliedTerrainRevision = 0;

	FTimerHandle ScanTimerHandle;
	FTimerHandle DepositTimerHandle;

	void ScanVoxelArea();
	void RequestDepositArea();
	void ProcessServerDepositRequests();
	void ApplyPendingDeltaRecords();
	void TrimReplicatedDeltaRecords();
	void DrawScanDebugBox() const;
	void DrawDepositGridPoints() const;
};

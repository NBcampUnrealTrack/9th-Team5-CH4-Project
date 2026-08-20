#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowOwnershipStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVolumeStore.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSurfaceEditor.h"
#include "DRSnowSubsystem.generated.h"

class AVoxelWorld;

// Snow 도메인의 유일한 외부 진입점이다.
// 내부 구현의 Volume/Surface/Ownership/Snapshot 모듈 분리는 이 클래스 뒤에 숨긴다.
UCLASS()
class DEEPRAIDERS_API UDRSnowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	FDRSnowAddResult AddSnow(const FDRSnowSurfaceAddRequest& Request);
	FDRSnowRemoveResult RemoveSnow(const FDRSnowSurfaceRemoveRequest& Request);
	bool ApplyReplicatedSnowRemoval(const FDRSnowSurfaceRemoveRequest& Request, float AppliedAmount);
	bool RepaintSnowMaterialsAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	bool GetSnowCellAtLocation(FVector WorldLocation, FDRSnowCell& OutCell, int32& OutTeamIdA, int32& OutTeamIdB) const;
	int32 GetDominantTeamAtLocation(FVector WorldLocation) const;
	FDRSnowControlRatio QuerySnowInBounds(const FBox& WorldBounds, int32 TeamIdA = INDEX_NONE, int32 TeamIdB = INDEX_NONE) const;

	FDRJoinSnapshotSizeReport MeasureCompressedSnapshotSize(AVoxelWorld* TargetVoxelWorld = nullptr, bool bLogResult = true);
	bool CreateCheckpoint(int32 OperationSequence, AVoxelWorld* TargetVoxelWorld = nullptr);
	bool GetLatestCheckpoint(FDRSnowJoinCheckpoint& OutCheckpoint);
	bool GetCheckpoint(int32 SnapshotId, FDRSnowJoinCheckpoint& OutCheckpoint);
	bool ApplyCheckpoint(FName VoxelWorldName, const TArray<uint8>& VoxelSaveData, const TArray<uint8>& SnowVolumeData, const TArray<uint8>& OwnershipData);

private:
	void ConfigureSurfaceEditor();
	void ConfigureSnapshotSerializer();

	FDRSnowOwnershipStore OwnershipStore;
	FDRSnowVolumeStore VolumeStore;
	FDRSnowSurfaceEditor SurfaceEditor;
	FDRSnowSnapshotSerializer SnapshotSerializer;
};

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VoxelTools/Gen/VoxelToolsBase.h"
#include "DRVoxelTeamOwnershipSubsystem.generated.h"

class AVoxelWorld;

struct FDRVoxelTeamOwnershipWorldData
{
	TMap<FIntVector, int32> TeamByVoxel;
};

// DirectionalSurfaceTool이 실제로 수정한 voxel 좌표의 팀 소유권을 보관한다.
// SnowVolume의 넓은 cell 누적과 달리, 재질 복원은 이 좌표 데이터를 우선 사용한다.
UCLASS()
class DEEPRAIDERS_API UDRVoxelTeamOwnershipSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// 이번 편집으로 empty에서 filled가 된 voxel만 팀 소유권으로 기록한다.
	void RecordAddedVoxels(
		AVoxelWorld* VoxelWorld,
		const TArray<FModifiedVoxelValue>& ModifiedValues,
		int32 TeamId);

	// 이번 편집으로 filled에서 empty가 된 voxel의 팀 소유권만 지운다.
	void RemoveClearedVoxels(
		AVoxelWorld* VoxelWorld,
		const TArray<FModifiedVoxelValue>& ModifiedValues);

	bool GetTeamAtVoxel(
		AVoxelWorld* VoxelWorld,
		const FIntVector& VoxelPosition,
		int32& OutTeamId) const;

	bool GetNearestTeamAtVoxel(
		AVoxelWorld* VoxelWorld,
		const FIntVector& VoxelPosition,
		int32 SearchRadius,
		int32& OutTeamId) const;

	// 중도난입 checkpoint용 원본 소유권 데이터 입출력이다.
	void CopySnapshotData(
		AVoxelWorld* VoxelWorld,
		TMap<FIntVector, int32>& OutTeamByVoxel) const;
	void ReplaceSnapshotData(
		AVoxelWorld* VoxelWorld,
		TMap<FIntVector, int32>&& InTeamByVoxel);

private:
	FDRVoxelTeamOwnershipWorldData& FindOrCreateWorldData(AVoxelWorld* VoxelWorld);
	const FDRVoxelTeamOwnershipWorldData* FindWorldData(AVoxelWorld* VoxelWorld) const;

	TMap<TObjectKey<AVoxelWorld>, FDRVoxelTeamOwnershipWorldData> WorldDataByVoxelWorld;
};

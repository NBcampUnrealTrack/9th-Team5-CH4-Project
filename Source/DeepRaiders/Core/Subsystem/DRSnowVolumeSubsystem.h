#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DRSnowVolumeSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDRSnowAddedToVolumeDelegate,
	const FDRSnowSurfaceAddRequest&,
	const FDRSnowAddResult&);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDRSnowRemovedFromVolumeDelegate,
	const FDRSnowSurfaceRemoveRequest&,
	const FDRSnowRemoveResult&);

class UDRJoinSnapshotSubsystem;

UCLASS()
class DEEPRAIDERS_API UDRSnowVolumeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	friend class UDRJoinSnapshotSubsystem;

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// 팀별 눈 Amount의 원본 데이터 갱신 진입점이다.
	// VoxelWorld는 이 결과를 보여주는 렌더/충돌 표현으로만 사용한다.
	FDRSnowAddResult AddSnow(const FDRSnowSurfaceAddRequest& Request);

	// 팀별 눈 Amount를 줄이는 원본 데이터 갱신 진입점이다.
	// 제거 대상 팀을 고르지 않고, 해당 cell에 남은 중립/A/B 양을 현재 비율대로 감소시킨다.
	FDRSnowRemoveResult RemoveSnow(const FDRSnowSurfaceRemoveRequest& Request);

	// 월드 위치 하나가 어느 snow cell에 해당하는지 조회한다.
	// 표면 재질 복원, 흡수 판정, 디버그 표시처럼 현재 cell의 원본 density가 필요할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Volume")
	bool GetSnowCellAtLocation(
		FVector WorldLocation,
		FDRSnowCell& OutCell,
		int32& OutTeamIdA,
		int32& OutTeamIdB) const;

	// 지정 위치에서 가장 많이 남아 있는 팀을 돌려준다.
	// 중립이 우세하거나 눈이 없으면 INDEX_NONE이다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Volume")
	int32 GetDominantTeamAtLocation(FVector WorldLocation) const;

	// 지정한 월드 Bounds 안의 팀별 Amount를 합산한다.
	// ControlZone은 자신의 BoxComponent Bounds를 이 함수에 넘겨 점령률을 계산한다.
	FDRSnowControlRatio QuerySnowInBounds(
		const FBox& WorldBounds,
		int32 TeamIdA = INDEX_NONE,
		int32 TeamIdB = INDEX_NONE) const;

	// 회전/스케일된 정육각형 기둥 안의 팀별 Amount를 합산한다.
	// HexExtent.X/Y 중 작은 값을 외접 반지름으로 사용하고, HexExtent.Z를 높이 절반으로 사용한다.
	FDRSnowControlRatio QuerySnowInHexPrism(
		const FBox& WorldBounds,
		const FTransform& HexTransform,
		const FVector& HexExtent,
		int32 TeamIdA = INDEX_NONE,
		int32 TeamIdB = INDEX_NONE) const;

	FDRSnowAddedToVolumeDelegate OnSnowAddedToVolume;
	FDRSnowRemovedFromVolumeDelegate OnSnowRemovedFromVolume;

	// Join snapshot 적용 전용. Voxel 표현과 같은 checkpoint에서 복원되어야 한다.
	void ReplaceSnapshotData(
		float InCellSize,
		int32 InChunkSize,
		TMap<FIntVector, FDRSnowVolumeChunk>&& InChunks);

protected:
	FIntVector WorldToCell(const FVector& WorldLocation) const;
	FIntVector CellToChunkOrigin(const FIntVector& Cell) const;
	const FDRSnowVolumeChunk* FindChunk(const FIntVector& ChunkOrigin) const;
	FDRSnowVolumeChunk& FindOrCreateChunk(const FIntVector& ChunkOrigin);

	// 실제 cell density를 증가시키는 가장 작은 단위의 기록 함수다.
	// TeamId가 INDEX_NONE이면 중립 슬롯, 아니면 chunk의 A/B 팀 슬롯에 누적한다.
	bool AddSnowToCell(
		FDRSnowVolumeChunk& Chunk,
		const FIntVector& GlobalCell,
		int32 TeamId,
		float Amount);

	// cell에 남은 중립/A/B 양을 비율대로 감소시킨다.
	// 이렇게 해야 일부만 흡수했을 때 팀 경계 비율이 갑자기 한쪽으로 붕괴하지 않는다.
	float RemoveSnowFromCell(
		FDRSnowVolumeChunk& Chunk,
		const FIntVector& GlobalCell,
		float Amount);

	// QuerySnowInBounds에서 발견한 팀 양을 결과 구조체의 A/B 조회 슬롯에 더한다.
	// chunk 내부 A/B 슬롯과 control query의 A/B 슬롯은 서로 독립이라 여기서 다시 매핑한다.
	void AddQueriedAmount(
		FDRSnowControlRatio& InOutRatio,
		int32 TeamId,
		float Amount) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Snow|Volume", meta = (ClampMin = "1.0", Units = "cm"))
	float CellSize = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Snow|Volume", meta = (ClampMin = "1"))
	int32 ChunkSize = 32;

	TMap<FIntVector, FDRSnowVolumeChunk> Chunks;
};

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

UCLASS()
class DEEPRAIDERS_API UDRSnowVolumeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// 팀별 눈 Amount의 원본 데이터 갱신 진입점이다.
	// VoxelWorld는 이 결과를 보여주는 렌더/충돌 표현으로만 사용한다.
	FDRSnowAddResult AddSnow(const FDRSnowSurfaceAddRequest& Request);

	// 지정한 월드 Bounds 안의 팀별 Amount를 합산한다.
	// ControlZone은 자신의 BoxComponent Bounds를 이 함수에 넘겨 점령률을 계산한다.
	FDRSnowControlRatio QuerySnowInBounds(
		const FBox& WorldBounds,
		int32 TeamIdA = INDEX_NONE,
		int32 TeamIdB = INDEX_NONE) const;

	FDRSnowAddedToVolumeDelegate OnSnowAddedToVolume;

protected:
	FIntVector WorldToCell(const FVector& WorldLocation) const;
	FIntVector CellToChunkOrigin(const FIntVector& Cell) const;
	const FDRSnowVolumeChunk* FindChunk(const FIntVector& ChunkOrigin) const;
	FDRSnowVolumeChunk& FindOrCreateChunk(const FIntVector& ChunkOrigin);
	bool AddSnowToCell(
		FDRSnowVolumeChunk& Chunk,
		const FIntVector& GlobalCell,
		int32 TeamId,
		float Amount);

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

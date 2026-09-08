#pragma once

#include "CoreMinimal.h"
#include "DRSnowVolumeTypes.generated.h"

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowCell
{
	GENERATED_BODY()

	// 특정 팀 소유가 아닌 기본 지형 눈 밀도다. 팀별 양은 Chunk의 palette 저장소가 관리한다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float NeutralAmount = 0.f;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowTeamAmount
{
	GENERATED_BODY()

	// Control query가 팀별로 반환하는 누적량이다. Ratio의 분모에는 NeutralAmount도 포함된다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Team")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Team")
	float Amount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Team")
	float Ratio = 0.f;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowVolumeChunk
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	FIntVector Origin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 Size = 32;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float CellSize = 20.f;

	// Chunk에 눈을 남긴 팀의 palette다. TeamAmounts는 [TeamSlot * CellCount + LocalCellIndex] 형식이다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	TArray<int32> TeamIds;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	TArray<FDRSnowCell> Cells;

	// TeamIds palette의 각 팀이 모든 dense cell에 가진 양이다. 새 팀이 추가될 때만 한 구간이 확장된다.
	TArray<float> TeamAmounts;

	// 값이 있는 cell만 따로 추적해 Control query와 snapshot 순회 비용을 줄인다.
	TSet<int32> ActiveCellIndices;

	void Initialize(const FIntVector& InOrigin, int32 InSize, float InCellSize);
	bool GetLocalIndex(const FIntVector& LocalCell, int32& OutIndex) const;
	int32 FindOrAddTeamSlot(int32 TeamId);
	float GetTeamAmount(int32 LocalIndex, int32 TeamSlot) const;
	void SetTeamAmount(int32 LocalIndex, int32 TeamSlot, float Amount);
	void AddTeamAmount(int32 LocalIndex, int32 TeamSlot, float Amount);
	float GetCellTotalAmount(int32 LocalIndex) const;
	int32 GetDominantTeamId(int32 LocalIndex) const;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowAddResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float AddedAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TouchedCellCount = 0;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowRemoveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float RemovedAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TouchedCellCount = 0;

	// 로컬 Remove 표현에만 사용
	FBox EditedWorldBounds = FBox(ForceInit);
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowControlRatio
{
	GENERATED_BODY()

	// Bounds 안에서 발견한 모든 팀을 반환한다. 팀 수를 호출자가 미리 지정하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	TArray<FDRSnowTeamAmount> Teams;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float NeutralAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float TotalAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	int32 SampledCellCount = 0;
};

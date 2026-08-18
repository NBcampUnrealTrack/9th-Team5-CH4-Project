#pragma once

#include "CoreMinimal.h"
#include "DRSnowVolumeTypes.generated.h"

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowCell
{
	GENERATED_BODY()

	// 팀이 지정되지 않은 기본/중립 눈 밀도다.
	// 기본 지형에 박혀 있는 눈이나 월드가 먼저 가진 눈처럼 특정 팀 소유가 아닌 양을 보관한다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float NeutralAmount = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float AmountA = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float AmountB = 0.f;
	float GetTotalAmount() const;

	// 외부 TeamId를 이 cell 내부 슬롯(Neutral/A/B)에 맞춰 조회한다.
	// INDEX_NONE은 중립 눈을 의미한다.
	float GetAmountForTeam(int32 TeamId, int32 TeamIdA, int32 TeamIdB) const;

	// 이 cell에서 가장 많은 양을 가진 팀을 반환한다.
	// 중립이 가장 많거나 아무 양도 없으면 INDEX_NONE을 돌려준다.
	int32 GetDominantTeamId(int32 TeamIdA, int32 TeamIdB) const;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowVolumeChunk
{
	GENERATED_BODY()

	// 전체 snow grid 기준 cell origin이다. 월드 좌표가 아니라 cell 좌표다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	FIntVector Origin = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 Size = 32;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float CellSize = 20.f;

	// 첫 번째로 기록된 팀을 A, 두 번째 팀을 B로 매핑한다.
	// 실제 팀 식별자는 PlayerState의 int32 TeamId를 그대로 보관한다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TeamIdA = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TeamIdB = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	TArray<FDRSnowCell> Cells;

	void Initialize(const FIntVector& InOrigin, int32 InSize, float InCellSize);
	bool GetLocalIndex(const FIntVector& LocalCell, int32& OutIndex) const;
	bool ResolveTeamSlot(int32 TeamId, bool& bOutTeamA);
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

	// 흡수를 시도한 팀이다. 실제 감소 대상은 중립/A/B 전체에서 비율로 빠진다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	int32 TouchedCellCount = 0;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowControlRatio
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	int32 TeamIdA = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	int32 TeamIdB = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float AmountA = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float AmountB = 0.f;

	// 점령률 분모에는 포함하지만, 어느 팀의 점령량으로도 더하지 않는 중립 눈 양이다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float NeutralAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float TotalAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float RatioA = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float RatioB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	int32 SampledCellCount = 0;
};

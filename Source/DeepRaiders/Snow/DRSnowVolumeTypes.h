#pragma once

#include "CoreMinimal.h"
#include "DRSnowVolumeTypes.generated.h"

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowCell
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float AmountA = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Volume")
	float AmountB = 0.f;
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

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float TotalAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float RatioA = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	float RatioB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Control")
	int32 SampledCellCount = 0;
};

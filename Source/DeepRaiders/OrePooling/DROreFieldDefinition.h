#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DROreFieldDefinition.generated.h"

class ADROrePoolActor;
class UDRItemDefinition;

UENUM(BlueprintType)
enum class EDROrePlacementMode : uint8
{
    // Box 내부 전체에 배치
    Volume,

    // Box의 네 측면에 배치
    SideWalls
};

USTRUCT(BlueprintType)
struct FDROreWeight
{
    GENERATED_BODY()

    // 광물 Mesh와 아이템 정보를 가진 DA
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore")
    TObjectPtr<UDRItemDefinition> ItemDefinition;

    // 실제로 풀링할 광물 Actor 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore")
    TSubclassOf<ADROrePoolActor> OreActorClass;

    // 같은 섹터에서 해당 광물이 선택될 상대 가중치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore", meta = (ClampMin = "0.0"))
    float Weight = 1.f;
};

USTRUCT(BlueprintType)
struct FDROreDepthSector : public FTableRowBase
{
    GENERATED_BODY()

    // OreField 상단부터 섹터가 시작되는 깊이
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sector", meta = (ClampMin = "0.0"))
    float StartDepth = 0.f;

    // OreField 상단부터 섹터가 끝나는 깊이
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sector", meta = (ClampMin = "0.0"))
    float EndDepth = 1000.f;

    // 해당 섹터에 배치할 광물 개수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sector", meta = (ClampMin = "0"))
    int32 SpawnCount = 10;

    // 해당 섹터에서 가중치로 선택할 광물 목록
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sector")
    TArray<FDROreWeight> Ores;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDROreFieldDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 광물 종류와 위치를 동일하게 재현할 Seed
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field")
    int32 RandomSeed = 1337;

    // Box 내부 광물 배치 방식
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field")
    EDROrePlacementMode PlacementMode = EDROrePlacementMode::SideWalls;

    // 첫 로딩 프레임에 즉시 생성할 광물 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field", meta = (ClampMin = "1"))
    int32 InitialPrewarmCount = 200;

    // 이후 프레임마다 생성할 광물 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field", meta = (ClampMin = "1"))
    int32 PrewarmBatchSize = 100;

    // 플레이어 최심도보다 아래쪽을 미리 활성화할 거리
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field", meta = (ClampMin = "0.0"))
    float LoadAheadDistance = 1500.f;

    // Box 경계에서 안쪽으로 띄울 여백
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field", meta = (ClampMin = "0.0"))
    float BoundsPadding = 100.f;

    // FDROreDepthSector 기반 깊이별 섹터 테이블
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field")
    TObjectPtr<UDataTable> SectorTable;
};

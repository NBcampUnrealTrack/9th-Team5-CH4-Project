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
    SideWalls,

    // 절차적으로 생성된 동굴 표면에 배치
    CaveSurface
};

UENUM(BlueprintType)
enum class EDROreCaveType : uint8
{
    LongCave,
    BigCave,
    FlatCave,
    PillarCave
};

USTRUCT(BlueprintType)
struct FDROreCaveConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave")
    EDROreCaveType CaveType = EDROreCaveType::LongCave;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave", meta = (ClampMin = "1"))
    int32 Count = 1;

    // Seed 기반 랜덤 배치에 더해지는 기준 위치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave")
    FVector CenterOffset = FVector::ZeroVector;

    // OreField Bounds 대비 각 축의 랜덤 배치 범위 비율
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    FVector CenterRandomRangeRatio = FVector(0.49f, 0.49f, 0.35f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ConnectionChance = 0.42f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave", meta = (ClampMin = "10.0"))
    float ConnectionRadius = 245.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave", meta = (ClampMin = "10.0"))
    float ConnectionSpacing = 105.f;

    // OreField 최하단에서 연결 구에 적용할 반경 비율
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave",
        meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float ConnectionBottomRadiusScale = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Long Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::LongCave", EditConditionHides,
        ClampMin = "10.0"))
    float LongLength = 2100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Long Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::LongCave", EditConditionHides,
        ClampMin = "10.0"))
    float LongRadius = 560.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Long Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::LongCave", EditConditionHides,
        ClampMin = "10.0"))
    float LongSpacing = 140.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Long Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::LongCave", EditConditionHides,
        ClampMin = "0.0"))
    float LongPathVariation = 245.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Long Cave",
        meta = (EditCondition = "CaveType == EDROreCaveType::LongCave", EditConditionHides,
            ClampMin = "0.0", ClampMax = "0.8"))
    float LongRadiusVariation = 0.175f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Big Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::BigCave", EditConditionHides, ClampMin = "1"))
    int32 BigSphereCount = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Big Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::BigCave", EditConditionHides,
        ClampMin = "10.0"))
    float BigMinSphereRadius = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Big Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::BigCave", EditConditionHides,
        ClampMin = "10.0"))
    float BigMaxSphereRadius = 700.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Big Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::BigCave", EditConditionHides))
    FVector BigCaveSize = FVector(1260.f, 1260.f, 700.f);

    // X=길이, Y=너비, Z=높이
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flat Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::FlatCave", EditConditionHides))
    FVector FlatSize = FVector(2800.f, 2100.f, 420.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flat Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::FlatCave", EditConditionHides,
        ClampMin = "10.0"))
    float FlatSpacing = 168.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flat Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::FlatCave", EditConditionHides,
        ClampMin = "0.0"))
    float FlatNoiseStrength = 140.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flat Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::FlatCave", EditConditionHides,
        ClampMin = "0.1"))
    float FlatNoiseFrequency = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pillar Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::PillarCave", EditConditionHides))
    FVector2D PillarRadiusRange = FVector2D(120.f, 1000.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pillar Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::PillarCave", EditConditionHides))
    FVector2D PillarHeightRange = FVector2D(300.f, 1500.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pillar Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::PillarCave", EditConditionHides))
    FVector2D PillarCaveRadiusRange = FVector2D(50.f, 150.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pillar Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::PillarCave", EditConditionHides))
    FVector2D PillarSpacingRange = FVector2D(50.f, 100.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pillar Cave", meta = (
        EditCondition = "CaveType == EDROreCaveType::PillarCave", EditConditionHides,
        ClampMin = "0.0"))
    float PillarPositionNoise = 80.f;
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field|Cluster")
    bool bUseOreClusters = true;

    // 각 깊이 섹터에 만들 광맥 중심 개수 범위
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field|Cluster",
        meta = (EditCondition = "bUseOreClusters", ClampMin = "0"))
    FIntPoint OreClusterCountRange = FIntPoint(1, 3);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field|Cluster",
        meta = (EditCondition = "bUseOreClusters", ClampMin = "0.0", ClampMax = "1.0"))
    float OreClusterChance = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field|Cluster",
        meta = (EditCondition = "bUseOreClusters", ClampMin = "0.0"))
    float OreClusterRadius = 300.f;

    // 게임 시작 시 OreField Bounds 안에 가로형 동굴을 생성한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field|Cave")
    bool bGenerateCave = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field|Cave",
        meta = (EditCondition = "bGenerateCave"))
    TArray<FDROreCaveConfig> Caves;

    // FDROreDepthSector 기반 깊이별 섹터 테이블
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field")
    TObjectPtr<UDataTable> SectorTable;
};

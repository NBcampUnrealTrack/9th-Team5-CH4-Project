#pragma once

#include "CoreMinimal.h"
#include "DROreFieldDefinition.h"
#include "GameFramework/Actor.h"
#include "DROreFieldActor.generated.h"

class ADROrePoolActor;
class UBoxComponent;

USTRUCT()
struct FDROreSpawnPoint
{
    GENERATED_BODY()

    // 아이템 스폰 오프셋 적용 전 위치
    FTransform Transform;

    int32 SpawnPointId = INDEX_NONE;

    UPROPERTY()
    TObjectPtr<UDRItemDefinition> ItemDefinition;

    TSubclassOf<ADROrePoolActor> OreActorClass;

    UPROPERTY()
    TObjectPtr<ADROrePoolActor> ActiveActor;

    bool bDepleted = false;
};

USTRUCT()
struct FDROreRuntimeSector
{
    GENERATED_BODY()

    float StartDepth = 0.f;
    bool bActive = false;
    TArray<FDROreSpawnPoint> SpawnPoints;
};

struct FDROreCaveSample
{
    FVector LocalCenter = FVector::ZeroVector;
    float Radius = 0.f;
};

struct FDROrePillarProtection
{
    FVector LocalCenter = FVector::ZeroVector;
    float Radius = 0.f;
    float HalfHeight = 0.f;
};

// 위치를 선계산하고 최심도에 따라 섹터를 켠다.
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADROreFieldActor : public AActor
{
    GENERATED_BODY()

public:
    ADROreFieldActor();

    // 실제로 파인 구의 가장 낮은 지점까지 섹터 갱신
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ore Field")
    void ReportTerrainDig(const FVector& Location, float Radius);

    // 반환된 스폰 위치를 채굴 완료 상태로 변경
    void HandleOreReleased(int32 SpawnPointId, ADROrePoolActor* ReleasedActor);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 광물 생성 영역 크기
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field")
    FVector BoundSize = FVector(2000.0f, 2000.0f, 2000.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ore Field")
    TObjectPtr<UBoxComponent> Bounds;

    // 배치 규칙과 섹터 데이터
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field")
    TObjectPtr<UDROreFieldDefinition> Definition;

private:
    // 실제로 파인 깊이에 따라 활성화되는 런타임 섹터
    TArray<FDROreRuntimeSector> RuntimeSectors;
    float DeepestDugDepth = 0.f;
    int32 NextSpawnPointId = 0;
    TArray<FDROreCaveSample> CaveSamples;
    TArray<FDROrePillarProtection> PillarProtections;
    int32 NextCaveSampleIndex = 0;
    int32 CaveGenerationRetryCount = 0;
    FTimerHandle CaveGenerationRetryTimer;

    void UpdateActiveSectors();
    void SetSectorActive(FDROreRuntimeSector& Sector, bool bNewActive);
    void CheckNearbyOreGround(const FVector& Location, float Radius);

    // 시드 기반 광물 위치 생성
    void PrewarmPool();
    void BuildSpawnPoints();
    FTransform MakeSpawnTransform(const FDROreDepthSector& Sector, FRandomStream& Random) const;
    const FDROreWeight* ChooseOre(const FDROreDepthSector& Sector, FRandomStream& Random) const;
    
    // 시드 기반 동굴 생성
    void BuildCaveSamples();
    void GenerateCave();
    void GenerateLongCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random);
    void GenerateBigCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random);
    void GenerateFlatCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random);
    void GeneratePillarCave(const FDROreCaveConfig& Config, const FVector& Origin, FRandomStream& Random);
    void AddConnection(const FVector& Start, const FVector& End, float Radius, float Spacing,
        float BottomRadiusScale, FRandomStream& Random);
    void AddCaveSample(const FVector& Center, float Radius);
    bool IntersectsProtectedPillar(const FVector& Center, float Radius) const;
};

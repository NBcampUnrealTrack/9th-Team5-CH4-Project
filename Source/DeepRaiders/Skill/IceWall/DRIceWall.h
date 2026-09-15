#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRIceWall.generated.h"

class ADRIceWallSegment;
class USceneComponent;

/** 육각 얼음 조각을 생성하고, 아래에서 위로 솟는 연출 및 전체 수명을 관리한다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRIceWall : public AActor
{
	GENERATED_BODY()

public:
	ADRIceWall();

	/** Deferred Spawn 중 GA가 벽의 모든 게임플레이 설정을 전달한다. */
	void ConfigureWall(int32 InOwnerTeamId, const FVector& InWallDimensions, int32 InSegmentCount,
		float InRiseDuration, float InWallLifeSpan, float InSegmentMaxHealth,
		TSubclassOf<ADRIceWallSegment> InSegmentClass);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

private:
	void SpawnSegments();
	void UpdateRise(float DeltaSeconds);
	FVector MakeSegmentLocalLocation(int32 SegmentIndex) const;
	FVector GetSegmentDimensions() const;
	float GetRiseDistance() const;

	UPROPERTY(VisibleAnywhere, Category = "Ice Wall")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADRIceWallSegment>> Segments;

	TArray<FVector> SegmentFinalLocations;
	TSubclassOf<ADRIceWallSegment> SegmentClass;
	int32 OwnerTeamId = INDEX_NONE;
	FVector WallDimensions = FVector(600.f, 60.f, 300.f);
	int32 SegmentCount = 5;
	float RiseDuration = 0.5f;
	float WallLifeSpan = 8.f;
	float SegmentMaxHealth = 100.f;
	float RiseElapsed = 0.f;
	bool bIsRising = false;
};

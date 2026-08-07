#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMiningComponent.generated.h"

class ADRPlayerCharacter;
class AVoxelWorld;

UENUM(BlueprintType)
enum class EDRMiningTraceMode : uint8
{
	// 조준선이 직접 닿은 지점만 채굴 후보로 사용한다.
	LineTrace UMETA(DisplayName = "Line Trace"),

	// MineRadius 크기의 구를 조준 방향으로 밀어서 닿는 지점을 채굴 후보로 사용한다.
	SphereSweep UMETA(DisplayName = "Sphere Sweep")
};

// 플레이어의 채굴 판정, 미리보기, 임시 로컬 Voxel 편집을 담당한다.
UCLASS(
	ClassGroup = (Mining),
	BlueprintType,
	Blueprintable,
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRMiningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRMiningComponent();

	virtual void BeginPlay() override;

	// 현재 조준 위치에 채굴을 시도한다.
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void TryMine();

	// 지형은 변경하지 않고 현재 채굴 가능 범위만 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void PreviewMineTarget();

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestMine(
		FVector_NetQuantize TraceStart,
		FVector_NetQuantize TraceEnd);

	bool CanMine() const;
	bool IsMineOnCooldown() const;
	bool PerformMiningTrace(FHitResult& OutHitResult) const;
	bool MineLocal(const FHitResult& HitResult) const;

	// Hit Actor 또는 Hit Component의 Owner에서 채굴 대상 VoxelWorld를 찾는다.
	AVoxelWorld* GetVoxelWorldFromHit(const FHitResult& HitResult) const;

	// Hit 정보를 기준으로 실제 Voxel을 제거할 채굴 구의 중심 위치를 계산한다.
	bool GetMinePositionFromHit(
		const FHitResult& HitResult,
		FVector& OutMinePosition) const;

	// 플레이어 컨트롤러 또는 캐릭터 시점에서 채굴 Trace 시작 위치와 방향을 구한다.
	void GetTraceViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	void CacheOwnerCharacter();

protected:
	// 땅파기 사거리
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Trace",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MineTraceDistance = 500.f;
	
	// 땅파기 타입
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Trace")
	EDRMiningTraceMode TraceMode = EDRMiningTraceMode::SphereSweep;

	// 땅파기 크기
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Voxel",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MineRadius = 150.f;

	// 채굴 구의 중심을 표면 안쪽으로 얼마나 밀어 넣을지 정한다. 0은 표면, 1은 반지름만큼 안쪽이다.
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Voxel",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MineSurfaceDepthRatio = 0.65f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Timing",
		meta = (ClampMin = "0.0", Units = "s"))
	float MineCooldown = 0.25f;

#pragma region Debug
	void DrawMineArea(
		const FVector& MinePosition,
		const FColor& Color,
		float DrawTime) const;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug")
	bool bDrawDebugTrace = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug")
	uint8 bDrawMineAreaOnMine : 1;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug",
		meta = (ClampMin = "0.0", Units = "s"))
	float PreviewDebugDrawTime = 0.f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug",
		meta = (ClampMin = "0.0", Units = "s"))
	float MineDebugDrawTime = 0.15f;
#pragma endregion

private:
	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerCharacter> OwnerCharacter;

	float LastMineTime = -BIG_NUMBER;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMiningComponent.generated.h"

class ADRPlayerCharacter;
class AVoxelWorld;
class UDRVoxelInvokerControlComponent;

UENUM(BlueprintType)
enum class EDRMiningTraceMode : uint8
{
	// 조준선이 직접 닿은 지점만 채굴 후보로 사용한다.
	// 정확한 표면 타격이 필요할 때 사용한다.
	LineTrace UMETA(DisplayName = "Line Trace"),

	// MineRadius 크기의 구를 조준 방향으로 쓸어가며 닿는 지점을 채굴 후보로 사용한다.
	// 커서가 빈 공간을 가리켜도 주변 지형을 잡아야 할 때 사용한다.
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
	// 쿨타임, 로컬 소유권, Trace 성공 여부를 확인한 뒤 실제 Voxel을 제거한다.
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void TryMine();

	// 지형은 변경하지 않고 현재 채굴 가능 범위만 표시한다.
	// Tick에서 호출될 수 있으므로 잔상이 남지 않게 DrawTime 기본값을 0으로 둔다.
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void PreviewMineTarget();

	float GetMineTraceDistance() const { return MineTraceDistance; }
	float GetMineRadius() const { return MineRadius; }
	float GetMineSurfaceDepthRatio() const { return MineSurfaceDepthRatio; }
	EDRMiningTraceMode GetTraceMode() const { return TraceMode; }

protected:
	// 클라이언트 입력 요청을 서버로 전달하기 위한 자리다.
	// 현재 단계에서는 서버 구현을 비워 두고 로컬 채굴 흐름을 먼저 검증한다.
	UFUNCTION(Server, Reliable)
	void Server_RequestMine(
		FVector_NetQuantize TraceStart,
		FVector_NetQuantize TraceEnd,
		FVector_NetQuantize RequestedMinePosition);

	bool HandleMineRequestOnServer(
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& TraceEnd,
		const FVector_NetQuantize& RequestedMinePosition);

	// 채굴 입력을 받아도 되는 상태인지 확인한다.
	// 로컬 플레이어 소유 여부와 쿨타임을 함께 검사한다.
	bool CanMine() const;

	// 마지막 채굴 시각을 기준으로 아직 쿨타임 중인지 확인한다.
	bool IsMineOnCooldown() const;

	// 현재 TraceMode에 맞춰 조준 방향의 채굴 후보 지점을 찾는다.
	bool PerformMiningTrace(FHitResult& OutHitResult) const;

	// HitResult가 가리키는 VoxelWorld에 실제 RemoveSphere를 적용한다.
	// 멀티플레이 연동 전까지는 이 함수가 로컬 테스트용 채굴 진입점이다.
	bool MineLocal(const FHitResult& HitResult) const;

	// Hit Actor 또는 Hit Component의 Owner에서 채굴 대상 VoxelWorld를 찾는다.
	// Voxel 충돌 컴포넌트를 직접 맞는 경우를 위해 Component Owner도 함께 확인한다.
	AVoxelWorld* GetVoxelWorldFromHit(const FHitResult& HitResult) const;

	// Hit 정보를 기준으로 실제 Voxel을 제거할 채굴 구의 중심 위치를 계산한다.
	// 표면 바로 위가 아니라 살짝 안쪽으로 밀어 넣어 지형을 파고들게 만든다.
	bool GetMinePositionFromHit(
		const FHitResult& HitResult,
		FVector& OutMinePosition) const;

	// 플레이어 컨트롤러 또는 캐릭터 시점에서 채굴 Trace 시작 위치와 방향을 구한다.
	// 마우스/카메라 조준 기준으로 채굴하기 위해 캐릭터 위치보다 ViewPoint를 우선 사용한다.
	void GetTraceViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	// Owner를 플레이어 캐릭터로 캐싱한다.
	// BeginPlay 이전이나 BP 구성 직후처럼 Owner가 늦게 잡히는 상황을 대비해 필요 시 다시 호출한다.
	void CacheOwnerCharacter();
	void CacheVoxelInvokerControl();

protected:
	// 땅파기 최대 거리
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Trace",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MineTraceDistance = 500.f;

	// 땅파기 판정 방식
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

	// 채굴 구의 중심을 표면 안쪽으로 얼마나 밀어 넣을지 정한다.
	// 0이면 표면 중심, 1이면 반지름만큼 안쪽으로 들어간다.
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Voxel",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MineSurfaceDepthRatio = 0.65f;

	// 채굴 입력 간 최소 대기 시간
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Timing",
		meta = (ClampMin = "0.0", Units = "s"))
	float MineCooldown = 0.25f;

#pragma region Debug
	// 채굴 범위 디버그 구와 중심점을 화면에 그린다.
	void DrawMineArea(
		const FVector& MinePosition,
		const FColor& Color,
		float DrawTime) const;

	// 채굴 미리보기/범위 디버그 표시 전체를 켜고 끈다.
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug")
	bool bDrawDebugTrace = true;

	// 실제 채굴이 발생했을 때 파란색 채굴 범위를 표시할지 정한다.
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug")
	uint8 bDrawMineAreaOnMine : 1;

	// 미리보기 디버그 표시 유지 시간이다.
	// Tick에서 계속 갱신할 때 잔상을 남기지 않기 위해 기본값은 0초다.
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug",
		meta = (ClampMin = "0.0", Units = "s"))
	float PreviewDebugDrawTime = 0.f;

	// 클릭으로 실제 채굴했을 때 범위 표시가 유지되는 시간이다.
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug",
		meta = (ClampMin = "0.0", Units = "s"))
	float MineDebugDrawTime = 0.15f;
#pragma endregion

private:
	// 채굴 컴포넌트를 소유한 플레이어 캐릭터 캐시
	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerCharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UDRVoxelInvokerControlComponent> VoxelInvokerControl;

	// 마지막 채굴 성공 시각. 쿨타임 계산에 사용한다.
	float LastMineTime = -BIG_NUMBER;
};

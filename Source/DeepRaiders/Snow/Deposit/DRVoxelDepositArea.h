#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelDepositOperations.h"
#include "DRVoxelDepositArea.generated.h"

class AVoxelWorld;
class ADRMiningGameStateBase;

UCLASS()
class DEEPRAIDERS_API ADRVoxelDepositArea : public AActor
{
	GENERATED_BODY()

public:
	ADRVoxelDepositArea();

#if WITH_EDITOR
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	/** 기본값은 기존 박스 영역을 유지합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	EDRVoxelDepositAreaShape AreaShape = EDRVoxelDepositAreaShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain",
		meta=(EditCondition="AreaShape == EDRVoxelDepositAreaShape::Box", EditConditionHides))
	FVector BoxExtent = FVector(500.f, 500.f, 500.f);

	/** 스피어 반지름 또는 실린더 밑면 반지름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain",
		meta=(ClampMin="1.0", EditCondition="AreaShape != EDRVoxelDepositAreaShape::Box",
			EditConditionHides))
	float AreaRadius = 500.f;

	/** 실린더의 전체 높이이며 액터 위치는 높이의 중앙입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain",
		meta=(ClampMin="1.0", EditCondition="AreaShape == EDRVoxelDepositAreaShape::Cylinder",
			EditConditionHides))
	float AreaHeight = 1000.f;

	/** 영역 외곽선과 검사 박스를 표시합니다. 플레이 표시는 시작 전에 설정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.01"))
	float DepositInterval = 1.f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel Terrain|Deposit",
		meta = (ClampMin = "0"))
	int32 StartPhaseIndex = 0;

	/** 팀 ID 기반으로 머터리얼 인덱스를 자동 지정할지 여부입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Terrain|Deposit")
	bool bUseTeamId = true;

	/** 퇴적할 눈의 소유 팀입니다. INDEX_NONE(-1)이면 중립 눈(0번 머터리얼)입니다. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel Terrain|Deposit",
		meta = (EditCondition = "bUseTeamId", EditConditionHides))
	int32 TeamId = INDEX_NONE;

	/** 수동으로 지정할 복셀 머터리얼 인덱스입니다. (bUseTeamId가 false일 때 사용) */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Voxel Terrain|Deposit",
		meta = (EditCondition = "!bUseTeamId", EditConditionHides))
	uint8 ManualMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="1", ClampMax="1024"))
	int32 DropsPerInterval = 16;

	/** 페이즈만 무시합니다. 중도 난입과 진행 중인 눈 편집 대기는 유지합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	bool bIgnoreGamePhase = false;

	/** 복셀 단위 상승량. 한 번에 최대 0.25셀입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.0", ClampMax="0.25"))
	float DepositAmountPerPass = 0.05f;

	/** 월드 단위 반경. 작업량 상한 안에서 실제 분출 수가 제한됩니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.0"))
	float DepositSpreadRadius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.0", ClampMax="2.0"))
	float LevelingStrength = 1.f;

	/** StaticMesh 표면 퇴적 사용 여부입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bDepositOnStaticMeshes = false;

	/** 퇴적을 허용할 StaticMesh 표면의 최대 경사각입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh", meta=(ClampMin="0.0", ClampMax="90.0"))
	float MaxStaticMeshSlopeAngle = 50.f;

	/** 지정 시 같은 태그를 가진 StaticMesh만 표면으로 허용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh")
	FName RequiredStaticMeshSurfaceTag = NAME_None;

	/** StaticMesh 검사에 복잡 충돌을 사용할지 결정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bTraceComplexStaticMeshSurfaces = false;

private:
	friend class FDRVoxelDepositScatterTest;
	FVector GetAreaExtent() const;
	AVoxelWorld* EnsureVoxelWorld();
	uint8 GetDepositMaterialIndex() const;

	FTimerHandle DepositTimerHandle;
	TWeakObjectPtr<ADRMiningGameStateBase> MiningGameState;
	bool bDepositStarted = false;

	/**
	 * 현재 액터 설정으로 새로운 퇴적 명령을 만듭니다.
	 * @param[out] OutCommand 생성된 퇴적 명령입니다.
	 * @return 유효한 명령을 만들면 true입니다.
	 */
	bool MakeDepositCommand(FDRVoxelDepositCommand& OutCommand) const;

	UFUNCTION()
	void HandleGamePhaseChanged(
		int32 PhaseIndex,
		int32 PhaseRemainingSeconds,
		const TArray<FText>& PlayerMessages);

	void StartDepositing();
	void StopDepositing();
	void RequestDepositArea();
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelDepositOperations.h"
#include "DRVoxelDepositArea.generated.h"

class AVoxelWorld;
enum class EDRSnowJoinSnapshotResult : uint8;

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

	UPROPERTY(EditAnywhere, Category="Voxel Terrain|Deposit")
	FDRVoxelDepositInBoxSettings DepositSettings;

	/** 관리 영역 안에서 무작위로 고를 XY 검사 창의 월드 크기입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="1.0"))
	float RandomScanWorldSize = 2000.f;

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
	FVector GetAreaExtent() const;

	/**
	 * 퇴적 명령을 서버와 모든 현재 클라이언트에 전달합니다.
	 * @param Command 모든 인스턴스에서 준비할 퇴적 명령입니다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPrepareDeposit(const FDRVoxelDepositCommand& Command);

	FTimerHandle DepositTimerHandle;
	FTimerHandle DepositPipelineTimerHandle;
	FDRVoxelDepositPlan PreparedDepositPlan;
	TArray<FDRVoxelDepositCommand> QueuedDepositCommands;
	int32 ActiveJoinSnapshotCount = 0;

	/**
	 * 현재 액터 설정으로 새로운 퇴적 명령을 만듭니다.
	 * @param[out] OutCommand 생성된 퇴적 명령입니다.
	 * @return 유효한 명령을 만들면 true입니다.
	 */
	bool MakeDepositCommand(FDRVoxelDepositCommand& OutCommand) const;
	void HandleJoinSnapshotStarted();
	void HandleJoinSnapshotFinished(EDRSnowJoinSnapshotResult Result);
	void RequestDepositArea();
	void PrepareNextQueuedDeposit();
	void ApplyPreparedDeposit();
	void CancelDepositPipeline();
};

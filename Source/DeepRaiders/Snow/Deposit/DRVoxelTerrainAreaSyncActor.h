#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelDepositOperations.h"
#include "DRVoxelTerrainAreaSyncActor.generated.h"

class AVoxelWorld;

/** 지정 영역의 결정적 퇴적 명령을 모든 인스턴스에 전달하는 네트워크 액터입니다. */
UCLASS()
class DEEPRAIDERS_API ADRVoxelTerrainAreaSyncActor : public AActor
{
	GENERATED_BODY()

public:
	/** 기본 네트워크 및 Tick 설정을 초기화합니다. */
	ADRVoxelTerrainAreaSyncActor();

#if WITH_EDITOR
	/**
	 * 에디터 뷰포트에 관리 영역을 표시합니다.
	 * @param DeltaSeconds 이전 프레임 이후 경과 시간입니다.
	 */
	virtual void Tick(float DeltaSeconds) override;
	/**
	 * 게임 실행 전 에디터 뷰포트 Tick을 허용합니다.
	 * @return 항상 true입니다.
	 */
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

protected:
	/** 관리 영역을 표시하고 서버 퇴적 타이머를 시작합니다. */
	virtual void BeginPlay() override;
	/**
	 * 종료 시 진행 중인 퇴적 파이프라인을 정리합니다.
	 * @param EndPlayReason 액터가 종료되는 이유입니다.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 각 인스턴스에서 퇴적을 적용할 로컬 VoxelWorld입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	/** 액터 위치를 중심으로 하는 관리 영역의 월드 반크기입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	FVector BoxExtent = FVector(500.f, 500.f, 500.f);

	/** 서버가 퇴적 명령 생성을 시도하는 반복 간격입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.01"))
	float DepositInterval = 1.f;

	/** 자동 퇴적 활성화 여부입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	bool bEnableDepositAccumulation = false;

	/** 새 명령에 복사할 퇴적 설정입니다. */
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
	/**
	 * 퇴적 명령을 서버와 모든 현재 클라이언트에 전달합니다.
	 * @param[in] Command 모든 인스턴스에서 준비할 퇴적 명령입니다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPrepareDeposit(const FDRVoxelDepositCommand& Command);

	/** 서버의 반복 퇴적 요청 타이머입니다. */
	FTimerHandle DepositTimerHandle;
	/** 준비와 적용을 분리하는 다음 틱 타이머입니다. */
	FTimerHandle DepositPipelineTimerHandle;
	/** 다음 프레임에 적용할 일회용 퇴적 계획입니다. */
	FDRVoxelDepositPlan PreparedDepositPlan;
	/** 수신 순서를 유지하는 로컬 퇴적 명령 대기열입니다. */
	TArray<FDRVoxelDepositCommand> QueuedDepositCommands;

	/**
	 * 현재 액터 설정으로 새로운 퇴적 명령을 만듭니다.
	 * @param[out] OutCommand 생성된 퇴적 명령입니다.
	 * @return 유효한 명령을 만들면 true입니다.
	 */
	bool MakeDepositCommand(FDRVoxelDepositCommand& OutCommand) const;
	/** 파이프라인이 비어 있을 때 새 퇴적 명령을 요청합니다. */
	void RequestDepositArea();
	/** 다음 대기 명령을 준비하고 적용을 예약합니다. */
	void PrepareNextQueuedDeposit();
	/** 준비된 계획을 적용하고 다음 명령을 예약합니다. */
	void ApplyPreparedDeposit();
	/** 준비된 계획과 대기열 및 파이프라인 타이머를 초기화합니다. */
	void CancelDepositPipeline();
};

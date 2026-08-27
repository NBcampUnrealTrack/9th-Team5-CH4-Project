#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelTerrainOperationLibrary.h"
#include "DRVoxelTerrainAreaSyncActor.generated.h"

class AVoxelWorld;
class UDRVoxelTerrainSubsystem;

// 지정 영역의 무작위 퇴적을 서버에서 계산하고 같은 작은 고수준 명령을 RPC로 클라이언트에 전달한다.
// 장기 변경 이력이나 복제 프로퍼티를 소유하지 않으며 중도 접속 상태 복원은 VoxelWorld 체크포인트가 담당한다.
UCLASS()
class DEEPRAIDERS_API ADRVoxelTerrainAreaSyncActor : public AActor
{
	GENERATED_BODY()

public:
	ADRVoxelTerrainAreaSyncActor();
	virtual ~ADRVoxelTerrainAreaSyncActor() override;

#if WITH_EDITOR
	// 에디터 뷰포트에서 선택된 액터의 관리 영역만 한 프레임짜리 디버그 박스로 그린다.
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

protected:
	// 서버 전용 타이머와 굴착 델리게이트를 구성한다. 클라이언트는 이 작업을 실행하지 않는다.
	virtual void BeginPlay() override;
	// 액터 종료 시 서브시스템 델리게이트 바인딩을 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 레벨에 배치된 서버/클라이언트 액터가 각각 같은 VoxelWorld를 참조한다.
	// 네트워크 변수 복제를 사용하지 않으므로 런타임에 서버에서만 이 값을 교체하면 클라이언트에는 반영되지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	// 액터 위치를 중심으로 하는 월드 공간 반크기다. 무작위 표면 검사, 퇴적, 굴착 필터가 이 영역을 공유한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain")
	FVector BoxExtent = FVector(500.f, 500.f, 500.f);

	// 새 퇴적 명령을 만들고 동기 실행하는 서버 타이머 간격이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="0.01"))
	float DepositInterval = 1.f;

	// false면 타이머는 유지하되 퇴적 명령을 실행하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	bool bEnableDepositAccumulation = false;

	// 새 요청마다 복사되는 퇴적 형태 설정이다. RandomSeed는 요청 생성 시 별도 무작위 값으로 교체한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit")
	FDRVoxelDepositInBoxSettings DepositSettings;

	// 각 요청이 관리 영역 안에서 무작위로 고르는 정사각형 XY 검사 창의 한 변 길이다.
	// Z 범위는 이 값과 무관하게 BoxExtent 전체를 사용하므로 천장부터 지면까지 항상 하향 검사한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit", meta=(ClampMin="1.0"))
	float RandomScanWorldSize = 2000.f;

	// true면 복셀 지형뿐 아니라 관리 박스 안의 고정 StaticMesh 표면도 퇴적 지지면으로 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bDepositOnStaticMeshes = false;

	// true면 위쪽 WorldStatic 충돌을 천장으로 취급해 지붕 아래 퇴적을 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bBlockDepositBelowStaticMeshes = true;

	// 이 각도보다 가파른 고정 메시 표면은 퇴적 지지면에서 제외한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh", meta=(ClampMin="0.0", ClampMax="90.0"))
	float MaxStaticMeshSlopeAngle = 50.f;

	// None이면 모든 고정 StaticMesh를 허용한다. 값을 지정하면 같은 태그가 있는 컴포넌트/액터만 허용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh")
	FName RequiredStaticMeshSurfaceTag = NAME_None;

	// true면 렌더 삼각형 기준 정밀 트레이스를 사용한다. 단순 충돌보다 정확하지만 비용이 더 든다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Voxel Terrain|Deposit|Static Mesh")
	bool bTraceComplexStaticMeshSurfaces = false;

private:
	// 서버가 실행한 작은 적설 명령을 모든 현재 클라이언트에 전달한다. 복셀 배열은 보내지 않는다.
	// 일반 Client RPC는 소유 연결 하나에만 전송되므로 소유자가 없는 영역 액터에서는 NetMulticast가 올바른 경로다.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastExecuteDeposit(const FDRVoxelDepositCommand& Command);

	// 서버의 굴착 완료 이벤트를 현재 연결된 모든 클라이언트에 동일한 구 편집으로 전달한다.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyDig(const FVector& Location, float Radius);

	// 서버 타이머 핸들과 굴착 델리게이트 생명주기를 보관한다.
	FTimerHandle DepositTimerHandle;
	TWeakObjectPtr<UDRVoxelTerrainSubsystem> TerrainSubsystem;
	FDelegateHandle TerrainDugDelegateHandle;

	// 지형 라이브러리에 필요한 현재 설정과 월드 참조만 모아 명시적인 호출 컨텍스트를 만든다.
	FDRVoxelTerrainOperationContext MakeTerrainOperationContext();
	// 제한된 수의 무작위 표면을 검사하는 명령을 만들고 한 호출 안에서 완료한다.
	void RequestDepositArea();
	// 서버 TerrainSubsystem의 굴착 완료 이벤트를 구독/해제한다.
	void BindTerrainDugDelegate();
	void UnbindTerrainDugDelegate();
	// 관리 박스와 겹친 서버 굴착만 RPC로 전달한다.
	void HandleTerrainDug(const FVector& Location, float Radius);
};

#include "DRVoxelTerrainAreaSyncActor.h"

#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif
#include "Engine/World.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

ADRVoxelTerrainAreaSyncActor::ADRVoxelTerrainAreaSyncActor()
{
	// 적설 작업에는 Tick을 사용하지 않는다. 에디터 빌드에서만 선택 영역 표시용 Tick을 허용하고,
	// BeginPlay에서 다시 꺼서 PIE와 실제 게임의 프레임 비용에는 포함되지 않게 한다.
#if WITH_EDITOR
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
#else
	PrimaryActorTick.bCanEverTick = false;
#endif

	// 프로퍼티를 복제하지 않더라도 액터 RPC를 보내려면 네트워크 액터여야 한다.
	// 영역 액터에는 특정 소유 클라이언트가 없으므로 모든 연결에서 채널을 유지한다.
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
}

ADRVoxelTerrainAreaSyncActor::~ADRVoxelTerrainAreaSyncActor() = default;

#if WITH_EDITOR
void ADRVoxelTerrainAreaSyncActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld() || !IsSelectedInEditor())
	{
		return;
	}

	// 실제 계산 영역은 회전되지 않는 월드 축 정렬 박스이므로 액터 회전을 적용하지 않는다.
	// 비영구 선을 매 프레임 다시 그려 액터 이동이나 BoxExtent 변경 뒤에 이전 박스가 남지 않게 한다.
	DrawDebugBox(
		World,
		GetActorLocation(),
		FVector(FMath::Abs(BoxExtent.X), FMath::Abs(BoxExtent.Y), FMath::Abs(BoxExtent.Z)),
		FColor(64, 200, 255),
		false,
		0.f,
		0,
		2.f);
}

bool ADRVoxelTerrainAreaSyncActor::ShouldTickIfViewportsOnly() const
{
	return true;
}
#endif

FDRVoxelTerrainOperationContext ADRVoxelTerrainAreaSyncActor::MakeTerrainOperationContext()
{
	FDRVoxelTerrainOperationContext Context;
	Context.World = GetWorld();
	Context.VoxelWorld = VoxelWorld;
	Context.TraceOwner = this;
	Context.DepositSettings = &DepositSettings;
	Context.AreaCenter = GetActorLocation();
	Context.AreaExtent = BoxExtent;
	Context.RandomScanWorldSize = RandomScanWorldSize;
	Context.RequiredStaticMeshSurfaceTag = RequiredStaticMeshSurfaceTag;
	Context.MaxStaticMeshSlopeAngle = MaxStaticMeshSlopeAngle;
	Context.bDepositOnStaticMeshes = bDepositOnStaticMeshes;
	Context.bBlockDepositBelowStaticMeshes = bBlockDepositBelowStaticMeshes;
	Context.bTraceComplexStaticMeshSurfaces = bTraceComplexStaticMeshSurfaces;
	return Context;
}

void ADRVoxelTerrainAreaSyncActor::BeginPlay()
{
	Super::BeginPlay();
	// 에디터 디버그 박스용 Tick이 PIE/게임에 이어지지 않게 명시적으로 끈다.
	SetActorTickEnabled(false);

	// 클라이언트는 타이머나 표면 검사를 실행하지 않고 서버 RPC 결과만 적용한다.
	if (!HasAuthority())
	{
		return;
	}

	BindTerrainDugDelegate();

	// 기능이 현재 꺼져 있어도 타이머는 유지한다. 런타임에 켜면 다음 주기부터 요청을 만들 수 있다.
	if (DepositInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			DepositTimerHandle,
			this,
			&ThisClass::RequestDepositArea,
			DepositInterval,
			true,
			0.f);
	}
}

void ADRVoxelTerrainAreaSyncActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindTerrainDugDelegate();
	Super::EndPlay(EndPlayReason);
}

void ADRVoxelTerrainAreaSyncActor::RequestDepositArea()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bEnableDepositAccumulation)
	{
		return;
	}

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		return;
	}

	FDRVoxelDepositCommand Command;
	if (!UDRVoxelTerrainOperationLibrary::MakeDepositCommand(
		MakeTerrainOperationContext(),
		Command))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Failed to create random surface deposit command."));
		return;
	}

	// 서버가 먼저 한 호출 안에서 전체 명령을 적용한다. 성공한 명령만 클라이언트에 보내므로
	// 무효 월드나 잘못된 설정이 Reliable RPC 대기열을 차지하지 않는다.
	FDRVoxelTerrainOperationResult Result;
	if (!UDRVoxelTerrainOperationLibrary::ExecuteDepositCommand(
		GetWorld(),
		VoxelWorld,
		this,
		Command,
		Result))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to execute deposit command on server."));
		return;
	}

	// 변경이 없는 빈 요청은 클라이언트에서도 결과가 없으므로 Reliable RPC를 보내지 않는다.
	// 눈이 더 쌓일 수 없는 상황에서 장시간 실행해도 불필요한 전송이 누적되지 않게 한다.
	if (Result.ModifiedVoxelCount > 0)
	{
		MulticastExecuteDeposit(Command);
	}
	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("Deposit completed synchronously. ModifiedVoxelCount=%d ScannedColumnCount=%d"),
		Result.ModifiedVoxelCount,
		Result.ScannedColumnCount);
}

void ADRVoxelTerrainAreaSyncActor::MulticastExecuteDeposit_Implementation(
	const FDRVoxelDepositCommand& Command)
{
	// NetMulticast는 서버에서도 실행된다. 서버는 호출 직전에 이미 적용했으므로 클라이언트만 재생한다.
	if (HasAuthority())
	{
		return;
	}

	FDRVoxelTerrainOperationResult Result;
	if (!UDRVoxelTerrainOperationLibrary::ExecuteDepositCommand(
		GetWorld(),
		VoxelWorld,
		this,
		Command,
		Result))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to execute deposit RPC command."));
	}
}

void ADRVoxelTerrainAreaSyncActor::MulticastApplyDig_Implementation(
	const FVector& Location,
	float Radius)
{
	// 서버에서는 TerrainSubsystem이 이미 굴착을 완료했다. 현재 클라이언트만 동일 편집을 재생한다.
	if (!HasAuthority() &&
		!UDRVoxelTerrainOperationLibrary::ApplyDig(VoxelWorld, Location, Radius))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to apply dig RPC."));
	}
}

void ADRVoxelTerrainAreaSyncActor::BindTerrainDugDelegate()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !IsValid(World) || TerrainDugDelegateHandle.IsValid())
	{
		return;
	}

	UDRVoxelTerrainSubsystem* Subsystem = World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(Subsystem))
	{
		return;
	}

	TerrainSubsystem = Subsystem;
	TerrainDugDelegateHandle = Subsystem->OnTerrainDug.AddUObject(
		this,
		&ThisClass::HandleTerrainDug);
}

void ADRVoxelTerrainAreaSyncActor::UnbindTerrainDugDelegate()
{
	if (UDRVoxelTerrainSubsystem* Subsystem = TerrainSubsystem.Get())
	{
		Subsystem->OnTerrainDug.Remove(TerrainDugDelegateHandle);
	}

	TerrainDugDelegateHandle.Reset();
	TerrainSubsystem.Reset();
}

void ADRVoxelTerrainAreaSyncActor::HandleTerrainDug(const FVector& Location, float Radius)
{
	if (!HasAuthority() || Radius <= 0.f)
	{
		return;
	}

	// 맵 전체 굴착 이벤트 중 이 액터의 관리 박스와 구가 겹치는 경우만 전달한다.
	if (!UDRVoxelTerrainOperationLibrary::IsVoxelUpdateInBox(
		GetActorLocation(),
		BoxExtent,
		Location,
		Radius))
	{
		return;
	}

	// 퇴적 작업은 타이머 호출 안에서 끝나므로 취소할 진행 상태가 없다.
	MulticastApplyDig(Location, Radius);
}

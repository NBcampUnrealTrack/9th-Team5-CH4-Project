#include "DRVoxelDepositArea.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif
#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

#if ENABLE_DRAW_DEBUG
namespace
{
	void DrawDepositAreaBox(
		UWorld* World,
		const FVector& Center,
		const FVector& Extent,
		const FColor& Color,
		bool bPersistentLines,
		float LifeTime,
		float Thickness)
	{
		if (!IsValid(World))
		{
			return;
		}

		DrawDebugBox(
			World,
			Center,
			FVector(FMath::Abs(Extent.X), FMath::Abs(Extent.Y), FMath::Abs(Extent.Z)),
			Color,
			bPersistentLines,
			LifeTime,
			0,
			Thickness);
	}

	// 판정과 같은 크기로 월드 축 기준 외곽선을 표시합니다.
	void DrawDepositAreaShape(UWorld* World, const FVector& Center, const FVector& Extent,
		EDRVoxelDepositAreaShape Shape, bool bPersistentLines, float LifeTime)
	{
		if (!IsValid(World) || Extent.ContainsNaN() || Extent.GetMin() <= 0.0)
		{
			return;
		}
		const FColor Color(64, 200, 255);
		if (Shape == EDRVoxelDepositAreaShape::Box)
		{
			DrawDepositAreaBox(World, Center, Extent, Color, bPersistentLines, LifeTime, 2.f);
			return;
		}
		if (Shape == EDRVoxelDepositAreaShape::Sphere)
		{
			DrawDebugSphere(World, Center, Extent.X, 32, Color, bPersistentLines, LifeTime, 0, 2.f);
			return;
		}
		const FVector Bottom = Center - FVector(0.0, 0.0, Extent.Z);
		const FVector Top = Center + FVector(0.0, 0.0, Extent.Z);
		DrawDebugCylinder(World, Bottom, Top, Extent.X, 32,
			Color, bPersistentLines, LifeTime, 0, 2.f);
	}
}
#endif

ADRVoxelDepositArea::ADRVoxelDepositArea()
{
#if WITH_EDITOR
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
#else
	PrimaryActorTick.bCanEverTick = false;
#endif

	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;

	UpdateDepositMaterialIndex();
}

#if WITH_EDITOR
void ADRVoxelDepositArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!bDrawDebug || !IsValid(World) || World->IsGameWorld())
	{
		return;
	}

	DrawDepositAreaShape(World, GetActorLocation(), GetAreaExtent(), AreaShape, false, 0.f);
}

bool ADRVoxelDepositArea::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ADRVoxelDepositArea::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ADRVoxelDepositArea, bUseTeamId) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ADRVoxelDepositArea, TeamId) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ADRVoxelDepositArea, ManualMaterialIndex))
	{
		UpdateDepositMaterialIndex();
	}
}
#endif

uint8 ADRVoxelDepositArea::GetDepositMaterialIndex() const
{
	return bUseTeamId
		? DRSnowMaterialMapping::TeamToMaterialIndex(TeamId)
		: ManualMaterialIndex;
}

void ADRVoxelDepositArea::UpdateDepositMaterialIndex()
{
	DepositSettings.DepositMaterialIndex = GetDepositMaterialIndex();
}

FVector ADRVoxelDepositArea::GetAreaExtent() const
{
	switch (AreaShape)
	{
	case EDRVoxelDepositAreaShape::Sphere:
		return FVector(AreaRadius);
	case EDRVoxelDepositAreaShape::Cylinder:
		return FVector(AreaRadius, AreaRadius, AreaHeight * 0.5f);
	default:
		return BoxExtent.GetAbs();
	}
}

AVoxelWorld* ADRVoxelDepositArea::EnsureVoxelWorld()
{
	if (!IsValid(VoxelWorld) && IsValid(GetWorld()))
	{
		for (TActorIterator<AVoxelWorld> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It))
			{
				VoxelWorld = *It;
				break;
			}
		}
	}
	return VoxelWorld;
}

bool ADRVoxelDepositArea::MakeDepositCommand(
	FDRVoxelDepositCommand& OutCommand) const
{
	// 실패 시 이전 결과가 남지 않도록 출력을 초기화합니다.
	OutCommand = FDRVoxelDepositCommand();
	const FVector AreaExtent = GetAreaExtent();
	if (!IsValid(GetWorld()) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		GetActorLocation().ContainsNaN() || AreaExtent.ContainsNaN() ||
		!FMath::IsFinite(RandomScanWorldSize) || RandomScanWorldSize <= 0.f)
	{
		return false;
	}

	if (AreaExtent.X <= KINDA_SMALL_NUMBER ||
		AreaExtent.Y <= KINDA_SMALL_NUMBER ||
		AreaExtent.Z <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutCommand.Settings = DepositSettings;
	OutCommand.Settings.DepositMaterialIndex = GetDepositMaterialIndex();
	OutCommand.Settings.RandomSeed = FMath::Rand();
	// 검사 영역은 X/Y만 줄이고 Z는 관리 영역 전체를 사용합니다.
	const float RequestedHalfSize = RandomScanWorldSize * 0.5f;
	OutCommand.ScanExtent = FVector(
		FMath::Min(AreaExtent.X, RequestedHalfSize),
		FMath::Min(AreaExtent.Y, RequestedHalfSize),
		AreaExtent.Z);
	FRandomStream ScanAreaRandomStream(
		OutCommand.Settings.RandomSeed ^ 0x27D4EB2D);
	// 검사 영역이 관리 영역 안에 있도록 중심 이동 범위를 제한합니다.
	OutCommand.ScanCenter = GetActorLocation() + FVector(
		ScanAreaRandomStream.FRandRange(
			-(AreaExtent.X - OutCommand.ScanExtent.X),
			AreaExtent.X - OutCommand.ScanExtent.X),
		ScanAreaRandomStream.FRandRange(
			-(AreaExtent.Y - OutCommand.ScanExtent.Y),
			AreaExtent.Y - OutCommand.ScanExtent.Y),
		0.f);
	OutCommand.AreaCenter = GetActorLocation();
	OutCommand.AreaExtent = AreaExtent;
	OutCommand.AreaShape = AreaShape;
	OutCommand.RequiredStaticMeshSurfaceTag = RequiredStaticMeshSurfaceTag;
	OutCommand.MaxStaticMeshSlopeAngle = MaxStaticMeshSlopeAngle;
	OutCommand.bDepositOnStaticMeshes = bDepositOnStaticMeshes;
	OutCommand.bTraceComplexStaticMeshSurfaces = bTraceComplexStaticMeshSurfaces;
	return true;
}

void ADRVoxelDepositArea::BeginPlay()
{
	Super::BeginPlay();

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDepositAreaShape(GetWorld(), GetActorLocation(), GetAreaExtent(), AreaShape, true, -1.f);
	}
#endif

	SetActorTickEnabled(false);
	EnsureVoxelWorld();
	UpdateDepositMaterialIndex();

	if (!HasAuthority())
	{
		return;
	}

	MiningGameState = GetWorld()->GetGameState<ADRMiningGameStateBase>();
	if (MiningGameState.IsValid())
	{
		MiningGameState->OnGamePhaseChanged.AddDynamic(
			this,
			&ThisClass::HandleGamePhaseChanged);
		HandleGamePhaseChanged(
			MiningGameState->GetCurrentPhaseIndex(),
			MiningGameState->GetPhaseRemainingSeconds(),
			MiningGameState->GetCurrentPhaseMessages());
	}
}

void ADRVoxelDepositArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MiningGameState.IsValid())
	{
		MiningGameState->OnGamePhaseChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGamePhaseChanged);
	}

	StopDepositing();
	MiningGameState.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADRVoxelDepositArea::HandleGamePhaseChanged(
	int32 PhaseIndex,
	int32,
	const TArray<FText>&)
{
	if (PhaseIndex == INDEX_NONE)
	{
		StopDepositing();
		return;
	}
	if (!bDepositStarted && PhaseIndex >= StartPhaseIndex)
	{
		StartDepositing();
	}
}

void ADRVoxelDepositArea::StartDepositing()
{
	if (bDepositStarted || DepositInterval <= 0.f)
	{
		return;
	}

	bDepositStarted = true;
	GetWorldTimerManager().SetTimer(
		DepositTimerHandle,
		this,
		&ThisClass::RequestDepositArea,
		DepositInterval,
		true,
		0.f);
}

void ADRVoxelDepositArea::StopDepositing()
{
	bDepositStarted = false;
	GetWorldTimerManager().ClearTimer(DepositTimerHandle);
}

void ADRVoxelDepositArea::RequestDepositArea()
{
	if (!HasAuthority() || !bDepositStarted || !MiningGameState.IsValid())
	{
		return;
	}

	EnsureVoxelWorld();

	const ADRMiningGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>();
	const UDRSnowSubsystem* SnowSubsystem = GetWorld()->GetSubsystem<UDRSnowSubsystem>();
	// 기존 난입자 집합을 직접 확인합니다. 별도 카운터나 재개 대기열은 필요 없습니다.
	if (!IsValid(GameMode) || GameMode->IsSnowJoinInProgress() ||
		!IsValid(SnowSubsystem) || SnowSubsystem->IsSnowEditInProgress())
	{
		return;
	}

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		UE_LOG(LogTemp, Warning, TEXT("VoxelWorld is not valid."));
		return;
	}

	FDRVoxelDepositCommand Command;
	if (!MakeDepositCommand(Command))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Failed to create random surface deposit command."));
		return;
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDepositAreaBox(GetWorld(), Command.ScanCenter, Command.ScanExtent,
			FColor(255, 165, 0), false, 1.f, 3.f);
	}
#endif

	// 서버의 한 게임 스레드 콜백 안에서 끝냅니다. 스냅샷 사이에 남는 next-tick 쓰기가 없습니다.
	FDRVoxelDepositPlan Plan;
	FDRVoxelDepositResult Result;
	if (!FDRVoxelDepositOperations::PrepareDepositCommand(
		GetWorld(),
		VoxelWorld,
		this,
		Command,
		Plan) || !FDRVoxelDepositOperations::ApplyDepositPlan(VoxelWorld, Plan, Result))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to apply surface deposit."));
		return;
	}

	MiningGameState->RegisterSnowDeposit(Result);
}

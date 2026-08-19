#include "DRMiningGameStateBase.h"

#include "DeepRaiders/Core/Subsystem/DRSnowSurfaceSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRSnowVolumeSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRJoinSnapshotSubsystem.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Teleport/DRTeleportPoint.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "VoxelWorld.h"

void ADRMiningGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRMiningGameStateBase, TeamRegisteredTeleports);
}

#pragma region Terrain Dig
void ADRMiningGameStateBase::RegisterTerrainDig(const FDRTerrainDigOperation& Operation)
{
	if (!HasAuthority())
	{
		return;
	}

	// 이미 접속 중인 클라이언트에게 서버 확정 지형 변경 이벤트만 전파한다.
	Multicast_ApplyTerrainDig(Operation);
}

void ADRMiningGameStateBase::Multicast_ApplyTerrainDig_Implementation(const FDRTerrainDigOperation& Operation)
{
	if (HasAuthority())
	{
		return;
	}

	ApplyTerrainDigOnce(Operation);
}

bool ADRMiningGameStateBase::ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	UDRVoxelTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(TerrainSubsystem))
	{
		return false;
	}

	// VoxelWorld 준비 여부와 pending 처리는 TerrainSubsystem 하나에서 관리한다.
	return TerrainSubsystem->ApplyOrQueueDig(Operation);
}
#pragma endregion

#pragma region Snow
void ADRMiningGameStateBase::RegisterSnowAdd(const FDRSnowAddOperation& Operation)
{
	if (!HasAuthority())
	{
		return;
	}

	FDRSnowOperationRecord Record;
	Record.Sequence = ++NextSnowOperationSequence;
	Record.bIsAddOperation = true;
	Record.AddOperation = Operation;
	SnowOperationHistory.Add(Record);
	Multicast_ApplySnowOperation(Record);
	TryCreateSnowCheckpoint();
}

void ADRMiningGameStateBase::RegisterSnowRemove(const FDRSnowRemoveOperation& Operation)
{
	if (!HasAuthority())
	{
		return;
	}

	FDRSnowOperationRecord Record;
	Record.Sequence = ++NextSnowOperationSequence;
	Record.bIsAddOperation = false;
	Record.RemoveOperation = Operation;
	SnowOperationHistory.Add(Record);
	Multicast_ApplySnowOperation(Record);
	TryCreateSnowCheckpoint();
}

void ADRMiningGameStateBase::GetSnowOperationsAfter(int32 Sequence, TArray<FDRSnowOperationRecord>& OutOperations) const
{
	OutOperations.Reset();
	for (const FDRSnowOperationRecord& Record : SnowOperationHistory)
	{
		if (Record.Sequence > Sequence)
		{
			OutOperations.Add(Record);
		}
	}
}

void ADRMiningGameStateBase::DiscardSnowOperationsThrough(int32 Sequence)
{
	if (!HasAuthority())
	{
		return;
	}

	SnowOperationHistory.RemoveAll([Sequence](const FDRSnowOperationRecord& Record)
	{
		return Record.Sequence <= Sequence;
	});
}

void ADRMiningGameStateBase::TryCreateSnowCheckpoint()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	UDRJoinSnapshotSubsystem* SnapshotSubsystem = IsValid(World) ? World->GetSubsystem<UDRJoinSnapshotSubsystem>() : nullptr;
	if (!IsValid(SnapshotSubsystem))
	{
		return;
	}

	FDRSnowJoinCheckpoint Checkpoint;
	constexpr int32 CheckpointInterval = 250;
	const bool bNeedsCheckpoint =
		!SnapshotSubsystem->GetLatestCheckpoint(Checkpoint) ||
		NextSnowOperationSequence - Checkpoint.OperationSequence >= CheckpointInterval;
	if (bNeedsCheckpoint && SnapshotSubsystem->CreateCheckpoint(NextSnowOperationSequence))
	{
		DiscardSnowOperationsThrough(NextSnowOperationSequence);
	}
}

void ADRMiningGameStateBase::Multicast_ApplySnowOperation_Implementation(const FDRSnowOperationRecord& Record)
{
	if (HasAuthority())
	{
		return;
	}

	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		if (PlayerController->QueueSnowJoinOperation(Record))
		{
			return;
		}
	}

	ApplySnowOperationRecord(Record);
}

bool ADRMiningGameStateBase::ApplySnowOperationRecord(const FDRSnowOperationRecord& Record)
{
	return Record.bIsAddOperation
		? ApplySnowAddOnce(Record.AddOperation)
		: ApplySnowRemoveOnce(Record.RemoveOperation);
}

bool ADRMiningGameStateBase::ApplySnowAddOnce(const FDRSnowAddOperation& Operation)
{
	if (Operation.Radius <= 0.f || Operation.Amount <= 0.f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorldByName(Operation.VoxelWorldName);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	FDRSnowSurfaceAddRequest Request;
	Request.WorldLocation = Operation.WorldLocation;
	Request.SurfaceNormal = FVector(Operation.SurfaceNormal).IsNearlyZero()
		? FVector::UpVector
		: FVector(Operation.SurfaceNormal).GetSafeNormal();
	Request.ImpactDirection = FVector(Operation.ImpactDirection).IsNearlyZero()
		? -Request.SurfaceNormal
		: FVector(Operation.ImpactDirection).GetSafeNormal();
	Request.TargetVoxelWorld = VoxelWorld;
	Request.Radius = Operation.Radius;
	Request.Amount = Operation.Amount;
	Request.EditTool = Operation.EditTool;
	Request.Context.TeamId = Operation.TeamId;

	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
		{
			return SnowSurfaceSubsystem->AddSnowAtArea(Request) > 0.f;
		}

		return false;
	}

	bool bHandled = false;
	if (UDRSnowVolumeSubsystem* SnowVolumeSubsystem = World->GetSubsystem<UDRSnowVolumeSubsystem>())
	{
		const FDRSnowAddResult AddResult = SnowVolumeSubsystem->AddSnow(Request);
		bHandled = AddResult.AddedAmount > 0.f;
	}

	if (bHandled)
	{
		if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
		{
			SnowSurfaceSubsystem->AddSnowAtArea(Request);
		}
	}

	return bHandled;
}

bool ADRMiningGameStateBase::ApplySnowRemoveOnce(const FDRSnowRemoveOperation& Operation)
{
	if (Operation.Radius <= 0.f || Operation.RequestedAmount <= 0.f || Operation.AppliedAmount <= 0.f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorldByName(Operation.VoxelWorldName);
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	FDRSnowSurfaceRemoveRequest Request;
	Request.WorldLocation = Operation.WorldLocation;
	Request.SurfaceNormal = FVector(Operation.SurfaceNormal).IsNearlyZero()
		? FVector::UpVector
		: FVector(Operation.SurfaceNormal).GetSafeNormal();
	Request.TargetVoxelWorld = VoxelWorld;
	Request.Radius = Operation.Radius;
	Request.RequestedAmount = Operation.RequestedAmount;
	Request.bInvertSurfaceStrength = Operation.bInvertSurfaceStrength;
	Request.EditTool = Operation.EditTool;
	Request.Context.TeamId = Operation.TeamId;

	float RemovedAmount = 0.f;
	if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
	{
		RemovedAmount = SnowSurfaceSubsystem->RemoveSnowAtArea(Request);
	}

	if (RemovedAmount <= 0.f)
	{
		return false;
	}

	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		if (UDRSnowVolumeSubsystem* SnowVolumeSubsystem = World->GetSubsystem<UDRSnowVolumeSubsystem>())
		{
			FDRSnowSurfaceRemoveRequest VolumeRequest = Request;
			// 표면 처리의 재현 결과가 한 voxel 정도 달라도, 원본 점령 데이터는
			// 서버가 확정한 실제 제거량으로 동일하게 유지한다.
			VolumeRequest.RequestedAmount = Operation.AppliedAmount;
			SnowVolumeSubsystem->RemoveSnow(VolumeRequest);
		}
	}

	if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
	{
		SnowSurfaceSubsystem->RepaintSnowMaterialsAtArea(Request);
	}

	return true;
}

AVoxelWorld* ADRMiningGameStateBase::ResolveVoxelWorldByName(FName VoxelWorldName) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		AVoxelWorld* VoxelWorld = *It;
		if (!IsValid(VoxelWorld))
		{
			continue;
		}

		if (VoxelWorldName.IsNone() || VoxelWorld->GetFName() == VoxelWorldName)
		{
			return VoxelWorld;
		}
	}

	return nullptr;
}
#pragma endregion

#pragma region Teleport
void ADRMiningGameStateBase::AddTeamRegisteredTeleportPoint(int32 TeamId, ADRTeleportPoint* TeleportPoint)
{
	if (!HasAuthority() || TeamId == INDEX_NONE || !IsValid(TeleportPoint) || !TeleportPoint->IsRegisteredForTeam(TeamId))
	{
		return;
	}

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && RegisteredTeleport.TeleportPoint == TeleportPoint)
		{
			return;
		}
	}

	FDRTeamRegisteredTeleportPoint RegisteredTeleport;
	RegisteredTeleport.TeamId = TeamId;
	RegisteredTeleport.TeleportPoint = TeleportPoint;
	TeamRegisteredTeleports.Add(RegisteredTeleport);
	ForceNetUpdate();
}

void ADRMiningGameStateBase::RemoveRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint)
{
	if (!HasAuthority() || !IsValid(TeleportPoint))
	{
		return;
	}

	TeamRegisteredTeleports.RemoveAll([TeleportPoint](const FDRTeamRegisteredTeleportPoint& RegisteredTeleport)
	{
		return RegisteredTeleport.TeleportPoint == TeleportPoint;
	});

	ForceNetUpdate();
}

void ADRMiningGameStateBase::GetTeamRegisteredTeleportPoints(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && IsValid(RegisteredTeleport.TeleportPoint))
		{
			OutTeleportPoints.AddUnique(RegisteredTeleport.TeleportPoint);
		}
	}
}

void ADRMiningGameStateBase::GetRegisteredTeleportPointsForTeam(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && IsValid(RegisteredTeleport.TeleportPoint))
		{
			OutTeleportPoints.AddUnique(RegisteredTeleport.TeleportPoint);
		}
	}
}

void ADRMiningGameStateBase::GetRegisteredTeleportDestinationsForTeam(int32 TeamId, ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	GetRegisteredTeleportPointsForTeam(TeamId, OutTeleportPoints);
	OutTeleportPoints.Remove(CurrentTeleportPoint);
}

bool ADRMiningGameStateBase::CanTeamUseRegisteredTeleportPoint(int32 TeamId, const ADRTeleportPoint* TeleportPoint) const
{
	if (!IsValid(TeleportPoint) || !TeleportPoint->IsRegisteredForTeam(TeamId))
	{
		return false;
	}

	for (const FDRTeamRegisteredTeleportPoint& RegisteredTeleport : TeamRegisteredTeleports)
	{
		if (RegisteredTeleport.TeamId == TeamId && RegisteredTeleport.TeleportPoint == TeleportPoint)
		{
			return true;
		}
	}

	return false;
}

#pragma endregion

#include "DRSnowSubsystem.h"

#include "VoxelWorld.h"

UDRSnowSubsystem::UDRSnowSubsystem()
{
	SnapshotSerializer = MakeUnique<FDRSnowSnapshotSerializer>(
		VolumeStore,
		OwnershipStore);
}

bool UDRSnowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

FDRSnowAddResult UDRSnowSubsystem::AddSnow(const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddResult Result;
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}
	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.AddSnowAtArea(Request);
		Result.AddedAmount = EditResult.AppliedAmount;
		Result.TeamId = Request.Context.TeamId;
		// Directional 도구는 표면 편집 결과의 실제 변경 voxel만 원본 데이터에 반영한다.
		ApplyAddedSurfaceEdit(Request, EditResult);
		return Result;
	}
	Result = VolumeStore.AddSnow(Request);
	if (Result.AddedAmount > 0.f)
	{
		SurfaceEditor.AddSnowAtArea(Request);
	}
	return Result;
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnow(const FDRSnowSurfaceRemoveRequest& Request)
{
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}
	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.RemoveSnowAtArea(Request);
	Result.RemovedAmount = EditResult.AppliedAmount;
	if (Result.RemovedAmount <= 0.f)
	{
		return Result;
	}
	ApplyRemovedSurfaceEdit(Request, EditResult, Result.RemovedAmount);
	RepaintSnowMaterialsAtArea(Request);
	return Result;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	float AppliedAmount)
{
	UWorld* World = GetWorld();
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World) || AppliedAmount <= 0.f)
	{
		return false;
	}

	const FDRSnowSurfaceEditResult EditResult = SurfaceEditor.RemoveSnowAtArea(Request);
	if (EditResult.AppliedAmount <= 0.f)
	{
		return false;
	}

	ApplyRemovedSurfaceEdit(Request, EditResult, EditResult.AppliedAmount);

	return RepaintSnowMaterialsAtArea(Request);
}

bool UDRSnowSubsystem::RepaintSnowMaterialsAtArea(const FDRSnowSurfaceRemoveRequest& Request)
{
	SurfaceEditor.SetWorld(GetWorld());
	return SurfaceEditor.RepaintSnowMaterialsAtArea(
		Request,
		OwnershipStore,
		VolumeStore);
}

bool UDRSnowSubsystem::GetSnowCellAtLocation(FVector Location, FDRSnowCell& Cell, int32& A, int32& B) const
{
	return VolumeStore.GetSnowCellAtLocation(Location, Cell, A, B);
}

int32 UDRSnowSubsystem::GetDominantTeamAtLocation(FVector Location) const
{
	return VolumeStore.GetDominantTeamAtLocation(Location);
}

FDRSnowControlRatio UDRSnowSubsystem::QuerySnowInBounds(const FBox& Bounds, int32 A, int32 B) const
{
	return VolumeStore.QuerySnowInBounds(Bounds, A, B);
}

FDRJoinSnapshotSizeReport UDRSnowSubsystem::MeasureCompressedSnapshotSize(AVoxelWorld* Target, bool bLog)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->MeasureCompressedSnapshotSize(Target, bLog);
}

bool UDRSnowSubsystem::CreateCheckpoint(int32 Sequence, AVoxelWorld* Target)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->CreateCheckpoint(Sequence, Target);
}

bool UDRSnowSubsystem::GetLatestCheckpoint(FDRSnowJoinCheckpoint& Out)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetLatestCheckpoint(Out);
}

bool UDRSnowSubsystem::GetCheckpoint(int32 Id, FDRSnowJoinCheckpoint& Out)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetCheckpoint(Id, Out);
}

bool UDRSnowSubsystem::ApplyCheckpoint(FName Name, const TArray<uint8>& Voxel, const TArray<uint8>& Volume, const TArray<uint8>& Ownership)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->ApplyCheckpoint(Name, Voxel, Volume, Ownership);
}

void UDRSnowSubsystem::ApplyAddedSurfaceEdit(
	const FDRSnowSurfaceAddRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult)
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!EditResult.bUseModifiedValuesForVolume || !IsValid(VoxelWorld))
	{
		return;
	}

	OwnershipStore.RecordAddedVoxels(
		VoxelWorld,
		EditResult.ModifiedValues,
		Request.Context.TeamId);
	AddVolumeFromModifiedValues(
		*VoxelWorld,
		Request,
		EditResult.ModifiedValues,
		EditResult.AppliedAmount);
}

void UDRSnowSubsystem::ApplyRemovedSurfaceEdit(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	float VolumeAmount)
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!EditResult.bUseModifiedValuesForVolume || !IsValid(VoxelWorld))
	{
		return;
	}

	OwnershipStore.RemoveClearedVoxels(VoxelWorld, EditResult.ModifiedValues);
	RemoveVolumeFromModifiedValues(
		*VoxelWorld,
		Request,
		EditResult.ModifiedValues,
		VolumeAmount);
}

void UDRSnowSubsystem::AddVolumeFromModifiedValues(
	AVoxelWorld& VoxelWorld,
	const FDRSnowSurfaceAddRequest& Request,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	float MaxAddedAmount)
{
	float AddedAmount = 0.f;
	const float VoxelRadius = FMath::Max(1.f, VoxelWorld.VoxelSize * 0.75f);
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		const float RemainingAmount = MaxAddedAmount - AddedAmount;
		if (RemainingAmount <= 0.f)
		{
			break;
		}

		if (ModifiedValue.NewValue >= ModifiedValue.OldValue)
		{
			continue;
		}

		FDRSnowSurfaceAddRequest CellRequest = Request;
		CellRequest.WorldLocation = VoxelWorld.LocalToGlobal(ModifiedValue.Position);
		CellRequest.Radius = VoxelRadius;
		CellRequest.Amount = FMath::Min(
			RemainingAmount,
			FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue));
		AddedAmount += VolumeStore.AddSnow(CellRequest).AddedAmount;
	}
}

void UDRSnowSubsystem::RemoveVolumeFromModifiedValues(
	AVoxelWorld& VoxelWorld,
	const FDRSnowSurfaceRemoveRequest& Request,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	float MaxRemovedAmount)
{
	float RemovedAmount = 0.f;
	const float VoxelRadius = FMath::Max(1.f, VoxelWorld.VoxelSize * 0.75f);
	for (const FModifiedVoxelValue& ModifiedValue : ModifiedValues)
	{
		const float RemainingAmount = MaxRemovedAmount - RemovedAmount;
		if (RemainingAmount <= 0.f)
		{
			break;
		}

		if (ModifiedValue.NewValue <= ModifiedValue.OldValue)
		{
			continue;
		}

		FDRSnowSurfaceRemoveRequest CellRequest = Request;
		CellRequest.WorldLocation = VoxelWorld.LocalToGlobal(ModifiedValue.Position);
		CellRequest.Radius = VoxelRadius;
		CellRequest.RequestedAmount = FMath::Min(
			RemainingAmount,
			FMath::Abs(ModifiedValue.NewValue - ModifiedValue.OldValue));
		RemovedAmount += VolumeStore.RemoveSnow(CellRequest).RemovedAmount;
	}
}

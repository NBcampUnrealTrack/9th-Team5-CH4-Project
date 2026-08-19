#include "DRSnowSubsystem.h"

bool UDRSnowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

FDRSnowAddResult UDRSnowSubsystem::AddSnow(const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddResult Result;
	UWorld* World = GetWorld();
	ConfigureSurfaceEditor();
	if (!IsValid(World))
	{
		return Result;
	}
	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		Result.AddedAmount = SurfaceEditor.AddSnowAtArea(Request);
		Result.TeamId = Request.Context.TeamId;
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
	ConfigureSurfaceEditor();
	if (!IsValid(World))
	{
		return Result;
	}
	Result.RemovedAmount = SurfaceEditor.RemoveSnowAtArea(Request);
	if (Result.RemovedAmount <= 0.f)
	{
		return Result;
	}
	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		FDRSnowSurfaceRemoveRequest VolumeRequest = Request;
		VolumeRequest.RequestedAmount = Result.RemovedAmount;
		VolumeStore.RemoveSnow(VolumeRequest);
	}
	RepaintSnowMaterialsAtArea(Request);
	return Result;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	float AppliedAmount)
{
	UWorld* World = GetWorld();
	ConfigureSurfaceEditor();
	if (!IsValid(World) || AppliedAmount <= 0.f)
	{
		return false;
	}

	const float RemovedAmount = SurfaceEditor.RemoveSnowAtArea(Request);
	if (RemovedAmount <= 0.f)
	{
		return false;
	}

	if (Request.EditTool != EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		FDRSnowSurfaceRemoveRequest VolumeRequest = Request;
		VolumeRequest.RequestedAmount = AppliedAmount;
		VolumeStore.RemoveSnow(VolumeRequest);
	}

	return RepaintSnowMaterialsAtArea(Request);
}

bool UDRSnowSubsystem::RepaintSnowMaterialsAtArea(const FDRSnowSurfaceRemoveRequest& Request)
{
	ConfigureSurfaceEditor();
	return SurfaceEditor.RepaintSnowMaterialsAtArea(Request);
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

FDRSnowControlRatio UDRSnowSubsystem::QuerySnowInHexPrism(const FBox& Bounds, const FTransform& Transform, const FVector& Extent, int32 A, int32 B) const
{
	return VolumeStore.QuerySnowInHexPrism(Bounds, Transform, Extent, A, B);
}

FDRJoinSnapshotSizeReport UDRSnowSubsystem::MeasureCompressedSnapshotSize(AVoxelWorld* Target, bool bLog)
{
	ConfigureSnapshotSerializer();
	return SnapshotSerializer.MeasureCompressedSnapshotSize(Target, bLog);
}

bool UDRSnowSubsystem::CreateCheckpoint(int32 Sequence, AVoxelWorld* Target)
{
	ConfigureSnapshotSerializer();
	return SnapshotSerializer.CreateCheckpoint(Sequence, Target);
}

bool UDRSnowSubsystem::GetLatestCheckpoint(FDRSnowJoinCheckpoint& Out)
{
	ConfigureSnapshotSerializer();
	return SnapshotSerializer.GetLatestCheckpoint(Out);
}

bool UDRSnowSubsystem::GetCheckpoint(int32 Id, FDRSnowJoinCheckpoint& Out)
{
	ConfigureSnapshotSerializer();
	return SnapshotSerializer.GetCheckpoint(Id, Out);
}

bool UDRSnowSubsystem::ApplyCheckpoint(FName Name, const TArray<uint8>& Voxel, const TArray<uint8>& Volume, const TArray<uint8>& Ownership)
{
	ConfigureSnapshotSerializer();
	return SnapshotSerializer.ApplyCheckpoint(Name, Voxel, Volume, Ownership);
}

void UDRSnowSubsystem::ConfigureSurfaceEditor()
{
	SurfaceEditor.Configure(GetWorld(), VolumeStore, OwnershipStore);
}

void UDRSnowSubsystem::ConfigureSnapshotSerializer()
{
	SnapshotSerializer.Configure(GetWorld(), VolumeStore, OwnershipStore);
}

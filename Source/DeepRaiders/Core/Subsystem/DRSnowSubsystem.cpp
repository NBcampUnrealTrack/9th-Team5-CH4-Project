#include "DRSnowSubsystem.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowEditPipelines.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelWorld.h"

UDRSnowSubsystem::UDRSnowSubsystem()
{
	SnapshotSerializer = MakeUnique<FDRSnowSnapshotSerializer>(VolumeStore);
	RemovalPipeline = MakeShared<FDRSnowRemovalPipeline>(
		SurfaceEditor,
		VolumeStore);
	AddPipeline = MakeShared<FDRSnowAddPipeline>(
		SurfaceEditor,
		VolumeStore);
}

UDRSnowSubsystem::~UDRSnowSubsystem() = default;

void UDRSnowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	AddPipeline->Reset(SnowStateGeneration);
}

void UDRSnowSubsystem::Deinitialize()
{
	++SnowStateGeneration;
	if (AddPipeline)
	{
		AddPipeline->Reset(SnowStateGeneration);
	}
	Super::Deinitialize();
}

bool UDRSnowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

FDRSnowAddResult UDRSnowSubsystem::AddSnow(
	const FDRSnowSurfaceAddRequest& Request,
	TFunction<void(float)> DirectionalCompletion)
{
	return AddPipeline->Execute(GetWorld(), Request, MoveTemp(DirectionalCompletion));
}

FDRSnowAddResult UDRSnowSubsystem::ApplyReplicatedSnowAdd(
	const FDRSnowSurfaceAddRequest& Request,
	const float AppliedAmount,
	TFunction<void(float)> DirectionalCompletion)
{
	return AddPipeline->Replay(
		GetWorld(),
		Request,
		AppliedAmount,
		MoveTemp(DirectionalCompletion));
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnow(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	return RemovalPipeline->Execute(GetWorld(), Request, EDRSnowRemovalPath::Standard);
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnowWithAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	return RemovalPipeline->Execute(GetWorld(), Request, EDRSnowRemovalPath::Absorb);
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	FBox* OutEditedWorldBounds)
{
	return RemovalPipeline->Replay(
		GetWorld(),
		Request,
		AppliedAmount,
		Request.RemovalMode == EDRSnowRemovalMode::AbsorbTool
			? EDRSnowRemovalPath::Absorb : EDRSnowRemovalPath::Standard,
			OutEditedWorldBounds);
}


int32 UDRSnowSubsystem::GetDominantTeamAtLocation(FVector Location) const
{
	return VolumeStore.GetDominantTeamAtLocation(Location);
}

FDRSnowControlRatio UDRSnowSubsystem::QuerySnowInBounds(const FBox& Bounds) const
{
	return VolumeStore.QuerySnowInBounds(Bounds);
}

FDRJoinSnapshotSizeReport UDRSnowSubsystem::MeasureCompressedSnapshotSize(
	AVoxelWorld* Target,
	const bool bLog)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->MeasureCompressedSnapshotSize(Target, bLog);
}

bool UDRSnowSubsystem::CreateCheckpoint(const int32 Sequence, AVoxelWorld* Target)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->CreateCheckpoint(Sequence, Target);
}

bool UDRSnowSubsystem::GetLatestCheckpointOperationSequence(int32& OutOperationSequence)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetLatestCheckpointOperationSequence(OutOperationSequence);
}

void UDRSnowSubsystem::ResetCheckpoints()
{
	if (SnapshotSerializer)
	{
		SnapshotSerializer->ResetCheckpoints();
	}
}

void UDRSnowSubsystem::ResetSnowState()
{
	++SnowStateGeneration;
	AddPipeline->Reset(SnowStateGeneration);
	ResetCheckpoints();
	VolumeStore.Reset();
}

bool UDRSnowSubsystem::GetLatestCheckpoint(FDRSnowJoinCheckpoint& Out)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetLatestCheckpoint(Out);
}

bool UDRSnowSubsystem::GetCheckpoint(const int32 Id, FDRSnowJoinCheckpoint& Out)
{
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->GetCheckpoint(Id, Out);
}

bool UDRSnowSubsystem::ApplyCheckpoint(
	FName VoxelWorldName,
	const TArray<uint8>& VoxelSaveData,
	int32 OriginalVoxelSaveSize,
	const TArray<uint8>& SnowVolumeData,
	int32 OriginalSnowVolumeSize,
	bool* bOutWaitingForWorld)
{
	SnapshotSerializer->SetWorld(GetWorld());
	const bool bApplied = SnapshotSerializer->ApplyCheckpoint(VoxelWorldName, VoxelSaveData, OriginalVoxelSaveSize, SnowVolumeData, OriginalSnowVolumeSize, bOutWaitingForWorld);
	return bApplied;
}

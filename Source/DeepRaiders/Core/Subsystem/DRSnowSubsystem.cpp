#include "DRSnowSubsystem.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowAddPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowMaterialPatchApplyQueue.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowRemovalPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVoxelContainmentEvaluator.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelWorld.h"

UDRSnowSubsystem::UDRSnowSubsystem()
{
	ContainmentEvaluator = MakeShared<FDRSnowVoxelContainmentEvaluator>();
	SnapshotSerializer = MakeUnique<FDRSnowSnapshotSerializer>(VolumeStore);
	MaterialPatchApplyQueue = MakeShared<FDRSnowMaterialPatchApplyQueue>(SurfaceEditor);
	RemovalPipeline = MakeShared<FDRSnowRemovalPipeline>(
		SurfaceEditor,
		OwnershipStore,
		VolumeStore,
		MaterialPatchApplyQueue.ToSharedRef());
	AddPipeline = MakeShared<FDRSnowAddPipeline>(
		SurfaceEditor,
		OwnershipStore,
		VolumeStore,
		*ContainmentEvaluator);
}

UDRSnowSubsystem::~UDRSnowSubsystem() = default;

void UDRSnowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	AddPipeline->Reset(SnowStateGeneration);
}

void UDRSnowSubsystem::Deinitialize()
{
	RemovalPipeline->ResetPredictions();
	++SnowStateGeneration;
	if (AddPipeline)
	{
		AddPipeline->Reset(SnowStateGeneration);
	}
	if (MaterialPatchApplyQueue)
	{
		MaterialPatchApplyQueue->Reset();
		MaterialPatchApplyQueue.Reset();
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
	const float AppliedAmount)
{
	return RemovalPipeline->ReconcileServerAdd(
		GetWorld(),
		*AddPipeline,
		Request,
		AppliedAmount);
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnow(
	const FDRSnowSurfaceRemoveRequest& Request,
	FDRSnowMaterialPatch* OutMaterialPatch)
{
	FDRSnowRemovalExecutionResult Execution = RemovalPipeline->Execute(
		GetWorld(),
		Request,
		EDRSnowRemovalPath::Standard,
		OutMaterialPatch != nullptr);
	if (OutMaterialPatch)
	{
		*OutMaterialPatch = MoveTemp(Execution.MaterialPatch);
	}
	return Execution.RemoveResult;
}

FDRSnowRemoveResult UDRSnowSubsystem::PredictSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	return RemovalPipeline->Predict(GetWorld(), Request, EDRSnowRemovalPath::Standard);
}

FDRSnowRemoveResult UDRSnowSubsystem::RemoveSnowWithAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request,
	FDRSnowMaterialPatch* OutMaterialPatch)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_Pipeline_Total);
	FDRSnowRemovalExecutionResult Execution = RemovalPipeline->Execute(
		GetWorld(),
		Request,
		EDRSnowRemovalPath::Absorb,
		OutMaterialPatch != nullptr);
	if (OutMaterialPatch)
	{
		*OutMaterialPatch = MoveTemp(Execution.MaterialPatch);
	}
	return Execution.RemoveResult;
}

FDRSnowRemoveResult UDRSnowSubsystem::PredictSnowAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request)
{
	return RemovalPipeline->Predict(GetWorld(), Request, EDRSnowRemovalPath::Absorb);
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
{
	return RemovalPipeline->ApplyServerUpdate(
		GetWorld(),
		Request,
		FDRSnowServerResponse(
			AppliedAmount,
			AuthoritativeMaterialPatch,
			EDRSnowRemovalPath::Standard));
}

bool UDRSnowSubsystem::ApplyReplicatedSnowAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
{
	return RemovalPipeline->ApplyServerUpdate(
		GetWorld(),
		Request,
		FDRSnowServerResponse(
			AppliedAmount,
			AuthoritativeMaterialPatch,
			EDRSnowRemovalPath::Absorb));
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
	RemovalPipeline->ResetPredictions();
	++SnowStateGeneration;
	AddPipeline->Reset(SnowStateGeneration);
	ResetCheckpoints();
	VolumeStore.Reset();
	OwnershipStore.Reset();
	MaterialPatchApplyQueue->Reset();
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
	FName Name,
	const TArray<uint8>& Voxel,
	const TArray<uint8>& Volume)
{
	// 중도 난입 클라이언트에는 Ownership 원본을 복원하지 않는다.
	RemovalPipeline->ResetPredictions();
	OwnershipStore.Reset();
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->ApplyCheckpoint(Name, Voxel, Volume);
}

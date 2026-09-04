#include "DRSnowSubsystem.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowAddPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowMaterialPatchApplyQueue.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowRemovalPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVoxelContainmentEvaluator.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	constexpr int32 MaxPendingRemovalPredictions = 32;
}

UDRSnowSubsystem::UDRSnowSubsystem()
{
	ContainmentEvaluator = MakeUnique<FDRSnowVoxelContainmentEvaluator>();
	SnapshotSerializer = MakeUnique<FDRSnowSnapshotSerializer>(VolumeStore);
	RemovalPipeline = MakeUnique<FDRSnowRemovalPipeline>(SurfaceEditor, OwnershipStore, VolumeStore);
	AddPipeline = MakeUnique<FDRSnowAddPipeline>(
		SurfaceEditor,
		OwnershipStore,
		VolumeStore,
		*ContainmentEvaluator);
	MaterialPatchApplyQueue = MakeShared<FDRSnowMaterialPatchApplyQueue>(SurfaceEditor);
}

UDRSnowSubsystem::~UDRSnowSubsystem() = default;

void UDRSnowSubsystem::Deinitialize()
{
	ResetRemovalPredictions();
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

FDRSnowAddResult UDRSnowSubsystem::AddSnow(const FDRSnowSurfaceAddRequest& Request)
{
	return AddPipeline->Execute(GetWorld(), Request);
}

FDRSnowAddResult UDRSnowSubsystem::ApplyReplicatedSnowAdd(
	const FDRSnowSurfaceAddRequest& Request,
	const float AppliedAmount)
{
	return AddPipeline->Replay(GetWorld(), Request, AppliedAmount);
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
	return PredictSnowRemovalInternal(Request, EDRSnowRemovalPath::Standard);
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
	return PredictSnowRemovalInternal(Request, EDRSnowRemovalPath::Absorb);
}

FDRSnowRemoveResult UDRSnowSubsystem::PredictSnowRemovalInternal(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath)
{
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() != NM_Client ||
		!Request.PredictionKey.IsValid())
	{
		return Result;
	}

	FDRSnowSurfaceEditResult SurfaceEdit = RemovalPipeline->PredictSurface(
		World,
		Request,
		RemovalPath);
	Result.RemovedAmount = SurfaceEdit.AppliedAmount;
	if (Result.RemovedAmount <= 0.f)
	{
		return Result;
	}

	if (PendingRemovalPredictions.Num() >= MaxPendingRemovalPredictions)
	{
		PendingRemovalPredictions.RemoveAt(
			0,
			PendingRemovalPredictions.Num() - MaxPendingRemovalPredictions + 1);
	}

	FPendingRemovalPrediction& Prediction = PendingRemovalPredictions.AddDefaulted_GetRef();
	Prediction.PredictionKey = Request.PredictionKey;
	Prediction.SurfaceEdit = MoveTemp(SurfaceEdit);
	Prediction.RemovalPath = RemovalPath;
	return Result;
}

bool UDRSnowSubsystem::ConsumeMatchingRemovalPrediction(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath,
	FPendingRemovalPrediction& OutPrediction)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() != NM_Client)
	{
		return false;
	}

	for (int32 Index = 0; Index < PendingRemovalPredictions.Num(); ++Index)
	{
		const FPendingRemovalPrediction& Candidate = PendingRemovalPredictions[Index];
		if (Candidate.RemovalPath != RemovalPath ||
			Candidate.PredictionKey != Request.PredictionKey)
		{
			continue;
		}

		OutPrediction = MoveTemp(PendingRemovalPredictions[Index]);
		PendingRemovalPredictions.RemoveAt(Index);
		return true;
	}
	return false;
}

void UDRSnowSubsystem::ConfirmPredictedRemoval(
	const FPendingRemovalPrediction& Prediction,
	const FDRSnowSurfaceRemoveRequest& AuthoritativeRequest,
	const float AuthoritativeAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
{
	const FDRSnowRemovalReplayResult ReplayResult = RemovalPipeline->ConfirmPrediction(
		GetWorld(),
		AuthoritativeRequest,
		Prediction.SurfaceEdit,
		AuthoritativeAmount,
		AuthoritativeMaterialPatch,
		Prediction.RemovalPath);
	if (ReplayResult.bApplied && AuthoritativeMaterialPatch)
	{
		MaterialPatchApplyQueue->Enqueue(
			ReplayResult.VoxelWorld.Get(),
			*AuthoritativeMaterialPatch);
	}
}

void UDRSnowSubsystem::ResetRemovalPredictions()
{
	PendingRemovalPredictions.Reset();
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
{
	FPendingRemovalPrediction Prediction;
	if (ConsumeMatchingRemovalPrediction(
		Request,
		EDRSnowRemovalPath::Standard,
		Prediction))
	{
		ConfirmPredictedRemoval(
			Prediction,
			Request,
			AppliedAmount,
			AuthoritativeMaterialPatch);
		return true;
	}

	const FDRSnowRemovalReplayResult ReplayResult = RemovalPipeline->Replay(
		GetWorld(),
		Request,
		AppliedAmount,
		AuthoritativeMaterialPatch,
		EDRSnowRemovalPath::Standard);
	if (ReplayResult.bApplied && AuthoritativeMaterialPatch)
	{
		MaterialPatchApplyQueue->Enqueue(
			ReplayResult.VoxelWorld.Get(),
			*AuthoritativeMaterialPatch);
	}
	return ReplayResult.bApplied;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
{
	FPendingRemovalPrediction Prediction;
	if (ConsumeMatchingRemovalPrediction(
		Request,
		EDRSnowRemovalPath::Absorb,
		Prediction))
	{
		ConfirmPredictedRemoval(
			Prediction,
			Request,
			AppliedAmount,
			AuthoritativeMaterialPatch);
		return true;
	}

	const FDRSnowRemovalReplayResult ReplayResult = RemovalPipeline->Replay(
		GetWorld(),
		Request,
		AppliedAmount,
		AuthoritativeMaterialPatch,
		EDRSnowRemovalPath::Absorb);
	if (ReplayResult.bApplied && AuthoritativeMaterialPatch)
	{
		MaterialPatchApplyQueue->Enqueue(
			ReplayResult.VoxelWorld.Get(),
			*AuthoritativeMaterialPatch);
	}
	return ReplayResult.bApplied;
}

int32 UDRSnowSubsystem::GetDominantTeamAtLocation(FVector Location) const
{
	return VolumeStore.GetDominantTeamAtLocation(Location);
}

FDRSnowControlRatio UDRSnowSubsystem::QuerySnowInBounds(const FBox& Bounds) const
{
	return VolumeStore.QuerySnowInBounds(Bounds);
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
	ResetRemovalPredictions();
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

bool UDRSnowSubsystem::GetCheckpoint(int32 Id, FDRSnowJoinCheckpoint& Out)
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
	ResetRemovalPredictions();
	OwnershipStore.Reset();
	SnapshotSerializer->SetWorld(GetWorld());
	return SnapshotSerializer->ApplyCheckpoint(Name, Voxel, Volume);
}

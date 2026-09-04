#include "DRSnowSubsystem.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowAddPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowMaterialPatchApplyQueue.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowRemovalPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVoxelContainmentEvaluator.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogDRSnowPrediction, Log, All);

namespace
{
	constexpr int32 MaxPendingRemovalPredictions = 32;
}

UDRSnowSubsystem::UDRSnowSubsystem()
{
	ContainmentEvaluator = MakeShared<FDRSnowVoxelContainmentEvaluator>();
	SnapshotSerializer = MakeUnique<FDRSnowSnapshotSerializer>(VolumeStore);
	RemovalPipeline = MakeShared<FDRSnowRemovalPipeline>(SurfaceEditor, OwnershipStore, VolumeStore);
	AddPipeline = MakeShared<FDRSnowAddPipeline>(
		SurfaceEditor,
		OwnershipStore,
		VolumeStore,
		*ContainmentEvaluator);
	MaterialPatchApplyQueue = MakeShared<FDRSnowMaterialPatchApplyQueue>(SurfaceEditor);
}

UDRSnowSubsystem::~UDRSnowSubsystem() = default;

void UDRSnowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	AddPipeline->Reset(SnowStateGeneration);
}

void UDRSnowSubsystem::Deinitialize()
{
	ResetRemovalPredictions();
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
	TArray<FPendingRemovalPrediction> SuspendedPredictions = SuspendRemovalPredictions();
	const FDRSnowAddResult Result = AddPipeline->Replay(GetWorld(), Request, AppliedAmount);
	ResumeRemovalPredictions(MoveTemp(SuspendedPredictions));
	return Result;
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

	if (PendingRemovalPredictions.Num() >= MaxPendingRemovalPredictions)
	{
		if (!bPredictionCapacityWarningLogged)
		{
			UE_LOG(
				LogDRSnowPrediction,
				Warning,
				TEXT("Snow removal prediction capacity reached (%d). New requests will wait for authoritative replay."),
				MaxPendingRemovalPredictions);
			bPredictionCapacityWarningLogged = true;
		}
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

	FPendingRemovalPrediction& Prediction = PendingRemovalPredictions.AddDefaulted_GetRef();
	Prediction.PredictionKey = Request.PredictionKey;
	Prediction.Request = Request;
	Prediction.SurfaceEdit = MoveTemp(SurfaceEdit);
	Prediction.RemovalPath = RemovalPath;
	return Result;
}

int32 UDRSnowSubsystem::FindMatchingRemovalPrediction(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath) const
{
	for (int32 Index = 0; Index < PendingRemovalPredictions.Num(); ++Index)
	{
		const FPendingRemovalPrediction& Candidate = PendingRemovalPredictions[Index];
		if (Candidate.RemovalPath != RemovalPath ||
			Candidate.PredictionKey != Request.PredictionKey)
		{
			continue;
		}

		return Index;
	}
	return INDEX_NONE;
}

TArray<UDRSnowSubsystem::FPendingRemovalPrediction> UDRSnowSubsystem::SuspendRemovalPredictions()
{
	TArray<FPendingRemovalPrediction> SuspendedPredictions = MoveTemp(PendingRemovalPredictions);
	PendingRemovalPredictions.Reset();

	for (int32 Index = SuspendedPredictions.Num() - 1; Index >= 0; --Index)
	{
		const FDRSnowSurfaceEditResult& SurfaceEdit = SuspendedPredictions[Index].SurfaceEdit;
		const int32 RestoredVoxelCount = SurfaceEditor.RestoreSurfaceEdit(SurfaceEdit);
		if (!SurfaceEdit.ModifiedValues.IsEmpty() &&
			RestoredVoxelCount != SurfaceEdit.ModifiedValues.Num())
		{
			UE_LOG(
				LogDRSnowPrediction,
				Warning,
				TEXT("Snow prediction rollback restored %d/%d voxels for key %d:%d; newer authoritative edits were preserved."),
				RestoredVoxelCount,
				SurfaceEdit.ModifiedValues.Num(),
				SuspendedPredictions[Index].PredictionKey.OwnerPlayerId,
				SuspendedPredictions[Index].PredictionKey.LocalSequence);
		}
	}
	return SuspendedPredictions;
}

void UDRSnowSubsystem::ResumeRemovalPredictions(
	TArray<FPendingRemovalPrediction>&& Predictions)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() != NM_Client)
	{
		return;
	}

	PendingRemovalPredictions.Reserve(Predictions.Num());
	for (FPendingRemovalPrediction& Prediction : Predictions)
	{
		Prediction.SurfaceEdit = RemovalPipeline->PredictSurface(
			World,
			Prediction.Request,
			Prediction.RemovalPath);
		PendingRemovalPredictions.Add(MoveTemp(Prediction));
	}
	bPredictionCapacityWarningLogged = false;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemovalInternal(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AuthoritativeAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch,
	const EDRSnowRemovalPath RemovalPath)
{
	const int32 MatchingPredictionIndex = FindMatchingRemovalPrediction(Request, RemovalPath);
	TArray<FPendingRemovalPrediction> SuspendedPredictions = SuspendRemovalPredictions();
	if (MatchingPredictionIndex != INDEX_NONE)
	{
		SuspendedPredictions.RemoveAt(MatchingPredictionIndex);
	}

	FDRSnowRemovalReplayResult ReplayResult;
	if (AuthoritativeAmount > 0.f)
	{
		ReplayResult = RemovalPipeline->Replay(
			GetWorld(),
			Request,
			AuthoritativeAmount,
			AuthoritativeMaterialPatch,
			RemovalPath);
	}
	if (ReplayResult.bApplied && AuthoritativeMaterialPatch)
	{
		MaterialPatchApplyQueue->Enqueue(
			ReplayResult.VoxelWorld.Get(),
			*AuthoritativeMaterialPatch);
	}

	ResumeRemovalPredictions(MoveTemp(SuspendedPredictions));
	return AuthoritativeAmount <= 0.f
		? Request.PredictionKey.IsValid()
		: ReplayResult.bApplied;
}

void UDRSnowSubsystem::ResetRemovalPredictions()
{
	PendingRemovalPredictions.Reset();
	bPredictionCapacityWarningLogged = false;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemoval(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
{
	return ApplyReplicatedSnowRemovalInternal(
		Request,
		AppliedAmount,
		AuthoritativeMaterialPatch,
		EDRSnowRemovalPath::Standard);
}

bool UDRSnowSubsystem::ApplyReplicatedSnowAbsorbTool(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AppliedAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch)
{
	return ApplyReplicatedSnowRemovalInternal(
		Request,
		AppliedAmount,
		AuthoritativeMaterialPatch,
		EDRSnowRemovalPath::Absorb);
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

#include "DRSnowSubsystem.h"

#include "DeepRaiders/Core/Subsystem/Snow/DRSnowAddPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowMaterialPatchApplyQueue.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowRemovalPipeline.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowSnapshotSerializer.h"
#include "DeepRaiders/Core/Subsystem/Snow/DRSnowVoxelContainmentEvaluator.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelWorld.h"

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
	Prediction.LocalOrder = ++NextRemovalPredictionOrder;
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
	TArray<int32> AllPredictionIndices;
	AllPredictionIndices.Reserve(PendingRemovalPredictions.Num());
	for (int32 Index = 0; Index < PendingRemovalPredictions.Num(); ++Index)
	{
		AllPredictionIndices.Add(Index);
	}
	return SuspendRemovalPredictions(AllPredictionIndices);
}

TArray<UDRSnowSubsystem::FPendingRemovalPrediction> UDRSnowSubsystem::SuspendRemovalPredictions(
	const TArray<int32>& PredictionIndices)
{
	TArray<bool> bShouldSuspend;
	bShouldSuspend.Init(false, PendingRemovalPredictions.Num());
	for (const int32 Index : PredictionIndices)
	{
		if (bShouldSuspend.IsValidIndex(Index))
		{
			bShouldSuspend[Index] = true;
		}
	}

	TArray<FPendingRemovalPrediction> SuspendedPredictions;
	TArray<FPendingRemovalPrediction> RemainingPredictions;
	SuspendedPredictions.Reserve(PredictionIndices.Num());
	RemainingPredictions.Reserve(PendingRemovalPredictions.Num() - PredictionIndices.Num());
	for (int32 Index = 0; Index < PendingRemovalPredictions.Num(); ++Index)
	{
		if (bShouldSuspend[Index])
		{
			SuspendedPredictions.Add(MoveTemp(PendingRemovalPredictions[Index]));
		}
		else
		{
			RemainingPredictions.Add(MoveTemp(PendingRemovalPredictions[Index]));
		}
	}
	PendingRemovalPredictions = MoveTemp(RemainingPredictions);

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

FVoxelIntBox UDRSnowSubsystem::GetRemovalRequestBounds(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath) const
{
	AVoxelWorld* VoxelWorld = Request.TargetVoxelWorld.Get();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Request.Radius <= 0.f)
	{
		return {};
	}

	if (RemovalPath == EDRSnowRemovalPath::Absorb)
	{
		const FVector Padding(Request.Radius);
		const FBox GlobalBounds(
			FVector(
				FMath::Min(Request.BrushOrigin.X, Request.WorldLocation.X) - Padding.X,
				FMath::Min(Request.BrushOrigin.Y, Request.WorldLocation.Y) - Padding.Y,
				FMath::Min(Request.BrushOrigin.Z, Request.WorldLocation.Z) - Padding.Z),
			FVector(
				FMath::Max(Request.BrushOrigin.X, Request.WorldLocation.X) + Padding.X,
				FMath::Max(Request.BrushOrigin.Y, Request.WorldLocation.Y) + Padding.Y,
				FMath::Max(Request.BrushOrigin.Z, Request.WorldLocation.Z) + Padding.Z));
		FBox LocalBounds(ForceInit);
		for (int32 X = 0; X < 2; ++X)
		{
			for (int32 Y = 0; Y < 2; ++Y)
			{
				for (int32 Z = 0; Z < 2; ++Z)
				{
					const FVector Corner(
						X == 0 ? GlobalBounds.Min.X : GlobalBounds.Max.X,
						Y == 0 ? GlobalBounds.Min.Y : GlobalBounds.Max.Y,
						Z == 0 ? GlobalBounds.Min.Z : GlobalBounds.Max.Z);
					LocalBounds += VoxelWorld->GlobalToLocalFloat(Corner).ToFloat();
				}
			}
		}
		return FVoxelIntBox(LocalBounds).Extend(1);
	}

	FVector BrushCenter = Request.WorldLocation;
	if (Request.RemovalMode == EDRSnowRemovalMode::ContactBrush)
	{
		const FVector TowardTarget =
			(Request.WorldLocation - Request.BrushOrigin).GetSafeNormal();
		const FVector InwardDirection = TowardTarget.IsNearlyZero()
			? -Request.SurfaceNormal.GetSafeNormal()
			: TowardTarget;
		if (InwardDirection.IsNearlyZero())
		{
			return {};
		}

		const float PenetrationDepth = FMath::Min(
			Request.Radius,
			FMath::Clamp(
				VoxelWorld->VoxelSize * Request.RequestedAmount,
				VoxelWorld->VoxelSize * 0.5f,
				VoxelWorld->VoxelSize * 2.f));
		const float ShapeSupportDistance =
			Request.RemovalBrushShape == EDRSnowRemovalBrushShape::Box
				? Request.Radius * (
					FMath::Abs(InwardDirection.X) +
					FMath::Abs(InwardDirection.Y) +
					FMath::Abs(InwardDirection.Z))
				: Request.Radius;
		BrushCenter = Request.WorldLocation -
			InwardDirection * (ShapeSupportDistance - PenetrationDepth);
	}

	return UVoxelBlueprintLibrary::MakeIntBoxFromGlobalPositionAndRadius(
		VoxelWorld,
		BrushCenter,
		Request.Radius).Extend(3);
}

TArray<int32> UDRSnowSubsystem::FindAffectedRemovalPredictionIndices(
	AVoxelWorld* VoxelWorld,
	const FVoxelIntBox& SeedBounds,
	const int32 RequiredPredictionIndex) const
{
	TArray<bool> bAffected;
	bAffected.Init(false, PendingRemovalPredictions.Num());
	if (bAffected.IsValidIndex(RequiredPredictionIndex))
	{
		bAffected[RequiredPredictionIndex] = true;
	}

	if (IsValid(VoxelWorld) && SeedBounds.IsValid())
	{
		for (int32 Index = 0; Index < PendingRemovalPredictions.Num(); ++Index)
		{
			const FDRSnowSurfaceEditResult& Edit =
				PendingRemovalPredictions[Index].SurfaceEdit;
			if (Edit.VoxelWorld.Get() == VoxelWorld && Edit.EditedBounds.IsValid() &&
				Edit.EditedBounds.Intersect(SeedBounds))
			{
				bAffected[Index] = true;
			}
		}

		bool bAddedDependency = true;
		while (bAddedDependency)
		{
			bAddedDependency = false;
			for (int32 CandidateIndex = 0;
				 CandidateIndex < PendingRemovalPredictions.Num();
				 ++CandidateIndex)
			{
				if (bAffected[CandidateIndex])
				{
					continue;
				}

				const FDRSnowSurfaceEditResult& CandidateEdit =
					PendingRemovalPredictions[CandidateIndex].SurfaceEdit;
				if (CandidateEdit.VoxelWorld.Get() != VoxelWorld ||
					!CandidateEdit.EditedBounds.IsValid())
				{
					continue;
				}

				for (int32 AffectedIndex = 0;
					 AffectedIndex < PendingRemovalPredictions.Num();
					 ++AffectedIndex)
				{
					if (!bAffected[AffectedIndex])
					{
						continue;
					}

					const FDRSnowSurfaceEditResult& AffectedEdit =
						PendingRemovalPredictions[AffectedIndex].SurfaceEdit;
					if (AffectedEdit.VoxelWorld.Get() == VoxelWorld &&
						AffectedEdit.EditedBounds.IsValid() &&
						CandidateEdit.EditedBounds.Intersect(AffectedEdit.EditedBounds))
					{
						bAffected[CandidateIndex] = true;
						bAddedDependency = true;
						break;
					}
				}
			}
		}
	}

	TArray<int32> Result;
	for (int32 Index = 0; Index < bAffected.Num(); ++Index)
	{
		if (bAffected[Index])
		{
			Result.Add(Index);
		}
	}
	return Result;
}

void UDRSnowSubsystem::ResumeRemovalPredictions(
	TArray<FPendingRemovalPrediction>&& Predictions)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() != NM_Client)
	{
		return;
	}

	PendingRemovalPredictions.Reserve(
		PendingRemovalPredictions.Num() + Predictions.Num());
	for (FPendingRemovalPrediction& Prediction : Predictions)
	{
		Prediction.SurfaceEdit = RemovalPipeline->PredictSurface(
			World,
			Prediction.Request,
			Prediction.RemovalPath);
		PendingRemovalPredictions.Add(MoveTemp(Prediction));
	}
	PendingRemovalPredictions.Sort(
		[](const FPendingRemovalPrediction& Left, const FPendingRemovalPrediction& Right)
		{
			return Left.LocalOrder < Right.LocalOrder;
		});
	bPredictionCapacityWarningLogged = false;
}

bool UDRSnowSubsystem::ApplyReplicatedSnowRemovalInternal(
	const FDRSnowSurfaceRemoveRequest& Request,
	const float AuthoritativeAmount,
	const FDRSnowMaterialPatch* AuthoritativeMaterialPatch,
	const EDRSnowRemovalPath RemovalPath)
{
	const int32 MatchingPredictionIndex = FindMatchingRemovalPrediction(Request, RemovalPath);

	// 정상적인 예측 승인은 geometry가 이미 화면에 적용되어 있다. 서버 확정량까지
	// 예측량과 같다면 surface rollback/replay 없이 volume과 material만 확정한다.
	if (MatchingPredictionIndex != INDEX_NONE && AuthoritativeAmount > 0.f)
	{
		const FPendingRemovalPrediction& MatchingPrediction =
			PendingRemovalPredictions[MatchingPredictionIndex];
		const float AmountTolerance = FMath::Max(
			KINDA_SMALL_NUMBER,
			FMath::Abs(AuthoritativeAmount) * 0.0001f);
		if (FMath::IsNearlyEqual(
			MatchingPrediction.SurfaceEdit.AppliedAmount,
			AuthoritativeAmount,
			AmountTolerance))
		{
			FPendingRemovalPrediction ConfirmedPrediction =
				MoveTemp(PendingRemovalPredictions[MatchingPredictionIndex]);
			PendingRemovalPredictions.RemoveAt(MatchingPredictionIndex);
			bPredictionCapacityWarningLogged = false;

			const FDRSnowRemovalReplayResult ConfirmResult =
				RemovalPipeline->ConfirmPrediction(
					GetWorld(),
					Request,
					ConfirmedPrediction.SurfaceEdit,
					AuthoritativeAmount,
					AuthoritativeMaterialPatch,
					RemovalPath);
			if (ConfirmResult.bApplied && AuthoritativeMaterialPatch)
			{
				MaterialPatchApplyQueue->Enqueue(
					ConfirmResult.VoxelWorld.Get(),
					*AuthoritativeMaterialPatch);
			}
			return ConfirmResult.bApplied;
		}
	}

	// 다른 클라이언트의 거절 응답이나, 용량 제한 때문에 로컬 surface 예측을
	// 생략했던 요청은 되돌릴 geometry가 없다.
	if (AuthoritativeAmount <= 0.f && MatchingPredictionIndex == INDEX_NONE)
	{
		return Request.PredictionKey.IsValid();
	}

	AVoxelWorld* AffectedVoxelWorld = Request.TargetVoxelWorld.Get();
	FVoxelIntBox AffectedBounds = GetRemovalRequestBounds(Request, RemovalPath);
	if (MatchingPredictionIndex != INDEX_NONE)
	{
		const FDRSnowSurfaceEditResult& MatchingEdit =
			PendingRemovalPredictions[MatchingPredictionIndex].SurfaceEdit;
		if (IsValid(MatchingEdit.VoxelWorld.Get()))
		{
			AffectedVoxelWorld = MatchingEdit.VoxelWorld.Get();
		}
		if (MatchingEdit.EditedBounds.IsValid())
		{
			AffectedBounds = AffectedBounds.IsValid()
				? AffectedBounds + MatchingEdit.EditedBounds
				: MatchingEdit.EditedBounds;
		}
	}

	const TArray<int32> AffectedPredictionIndices =
		FindAffectedRemovalPredictionIndices(
			AffectedVoxelWorld,
			AffectedBounds,
			MatchingPredictionIndex);
	TArray<FPendingRemovalPrediction> SuspendedPredictions =
		SuspendRemovalPredictions(AffectedPredictionIndices);
	if (MatchingPredictionIndex != INDEX_NONE)
	{
		const int32 SuspendedMatchingIndex = SuspendedPredictions.IndexOfByPredicate(
			[&Request, RemovalPath](const FPendingRemovalPrediction& Prediction)
			{
				return Prediction.RemovalPath == RemovalPath &&
					Prediction.PredictionKey == Request.PredictionKey;
			});
		if (SuspendedMatchingIndex != INDEX_NONE)
		{
			SuspendedPredictions.RemoveAt(SuspendedMatchingIndex);
		}
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
	NextRemovalPredictionOrder = 0;
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

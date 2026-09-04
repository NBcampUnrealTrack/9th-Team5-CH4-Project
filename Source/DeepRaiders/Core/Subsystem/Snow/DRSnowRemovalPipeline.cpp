#include "DRSnowRemovalPipeline.h"

#include "DRSnowAddPipeline.h"
#include "DRSnowMaterialPatchApplyQueue.h"
#include "DRSnowOwnershipStore.h"
#include "DRSnowSurfaceEditor.h"
#include "DRSnowVolumeStore.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelWorld.h"

DEFINE_LOG_CATEGORY_STATIC(LogDRSnowPrediction, Log, All);

namespace
{
	constexpr int32 MaxPendingRemovalPredictions = 32;
}

FDRSnowRemovalPipeline::FDRSnowRemovalPipeline(
	FDRSnowSurfaceEditor& InSurfaceEditor,
	FDRSnowOwnershipStore& InOwnershipStore,
	FDRSnowVolumeStore& InVolumeStore,
	const TSharedRef<FDRSnowMaterialPatchApplyQueue>& InMaterialPatchApplyQueue)
	: SurfaceEditor(InSurfaceEditor)
	, OwnershipStore(InOwnershipStore)
	, VolumeStore(InVolumeStore)
	, MaterialPatchApplyQueue(InMaterialPatchApplyQueue)
{
}

FDRSnowRemoveResult FDRSnowRemovalPipeline::Predict(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath)
{
	FDRSnowRemoveResult Result;
	Result.TeamId = Request.Context.TeamId;
	if (!IsValid(World) || World->GetNetMode() != NM_Client ||
		!Request.PredictionKey.IsValid())
	{
		return Result;
	}

	if (PendingPredictions.Num() >= MaxPendingRemovalPredictions)
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

	FDRSnowSurfaceEditResult SurfaceEdit = PredictSurface(World, Request, RemovalPath);
	Result.RemovedAmount = SurfaceEdit.AppliedAmount;
	if (Result.RemovedAmount <= 0.f)
	{
		return Result;
	}

	FPendingRemovalPrediction& Prediction = PendingPredictions.AddDefaulted_GetRef();
	Prediction.PredictionKey = Request.PredictionKey;
	Prediction.Request = Request;
	Prediction.SurfaceEdit = MoveTemp(SurfaceEdit);
	Prediction.RemovalPath = RemovalPath;
	Prediction.LocalOrder = ++NextPredictionOrder;
	return Result;
}

bool FDRSnowRemovalPipeline::ApplyServerUpdate(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowServerResponse& Response)
{
	const int32 MatchingPredictionIndex = FindMatchingPrediction(
		Request,
		Response.RemovalPath);

	// 승인량이 예측량과 일치하면 geometry는 이미 정답이므로 rollback/replay를 생략한다.
	if (MatchingPredictionIndex != INDEX_NONE && Response.AuthoritativeAmount > 0.f)
	{
		const FPendingRemovalPrediction& MatchingPrediction =
			PendingPredictions[MatchingPredictionIndex];
		const float AmountTolerance = FMath::Max(
			KINDA_SMALL_NUMBER,
			FMath::Abs(Response.AuthoritativeAmount) * 0.0001f);
		if (FMath::IsNearlyEqual(
			MatchingPrediction.SurfaceEdit.AppliedAmount,
			Response.AuthoritativeAmount,
			AmountTolerance))
		{
			FPendingRemovalPrediction ConfirmedPrediction =
				MoveTemp(PendingPredictions[MatchingPredictionIndex]);
			PendingPredictions.RemoveAt(MatchingPredictionIndex);
			bPredictionCapacityWarningLogged = false;

			const FDRSnowRemovalReplayResult ConfirmResult = ConfirmPrediction(
				World,
				Request,
				ConfirmedPrediction.SurfaceEdit,
				Response);
			if (ConfirmResult.bApplied && Response.MaterialPatch)
			{
				MaterialPatchApplyQueue->Enqueue(
					ConfirmResult.VoxelWorld.Get(),
					*Response.MaterialPatch);
			}
			return ConfirmResult.bApplied;
		}
	}

	// 거절 응답이지만 로컬 예측이 없으면 되돌릴 geometry도 없다.
	if (Response.AuthoritativeAmount <= 0.f && MatchingPredictionIndex == INDEX_NONE)
	{
		return Request.PredictionKey.IsValid();
	}

	AVoxelWorld* AffectedVoxelWorld = Request.TargetVoxelWorld.Get();
	FVoxelIntBox AffectedBounds = GetRequestBounds(Request, Response.RemovalPath);
	if (MatchingPredictionIndex != INDEX_NONE)
	{
		const FDRSnowSurfaceEditResult& MatchingEdit =
			PendingPredictions[MatchingPredictionIndex].SurfaceEdit;
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

	const TArray<int32> AffectedPredictionIndices = FindAffectedPredictionIndices(
		AffectedVoxelWorld,
		AffectedBounds,
		MatchingPredictionIndex);
	TArray<FPendingRemovalPrediction> SuspendedPredictions =
		SuspendPredictions(AffectedPredictionIndices);
	if (MatchingPredictionIndex != INDEX_NONE)
	{
		const int32 SuspendedMatchingIndex = SuspendedPredictions.IndexOfByPredicate(
			[&Request, &Response](const FPendingRemovalPrediction& Prediction)
			{
				return Prediction.RemovalPath == Response.RemovalPath &&
					Prediction.PredictionKey == Request.PredictionKey;
			});
		if (SuspendedMatchingIndex != INDEX_NONE)
		{
			SuspendedPredictions.RemoveAt(SuspendedMatchingIndex);
		}
	}

	FDRSnowRemovalReplayResult ReplayResult;
	if (Response.AuthoritativeAmount > 0.f)
	{
		ReplayResult = Replay(World, Request, Response);
	}
	if (ReplayResult.bApplied && Response.MaterialPatch)
	{
		MaterialPatchApplyQueue->Enqueue(
			ReplayResult.VoxelWorld.Get(),
			*Response.MaterialPatch);
	}

	ResumePredictions(World, MoveTemp(SuspendedPredictions));
	return Response.AuthoritativeAmount <= 0.f
		? Request.PredictionKey.IsValid()
		: ReplayResult.bApplied;
}

FDRSnowAddResult FDRSnowRemovalPipeline::ReconcileServerAdd(
	UWorld* World,
	FDRSnowAddPipeline& AddPipeline,
	const FDRSnowSurfaceAddRequest& Request,
	const float AppliedAmount)
{
	TArray<FPendingRemovalPrediction> SuspendedPredictions = SuspendPredictions();
	const FDRSnowAddResult Result = AddPipeline.Replay(World, Request, AppliedAmount);
	ResumePredictions(World, MoveTemp(SuspendedPredictions));
	return Result;
}

void FDRSnowRemovalPipeline::ResetPredictions()
{
	PendingPredictions.Reset();
	bPredictionCapacityWarningLogged = false;
	NextPredictionOrder = 0;
}

int32 FDRSnowRemovalPipeline::FindMatchingPrediction(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath) const
{
	for (int32 Index = 0; Index < PendingPredictions.Num(); ++Index)
	{
		const FPendingRemovalPrediction& Candidate = PendingPredictions[Index];
		if (Candidate.RemovalPath == RemovalPath &&
			Candidate.PredictionKey == Request.PredictionKey)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

TArray<FDRSnowRemovalPipeline::FPendingRemovalPrediction>
FDRSnowRemovalPipeline::SuspendPredictions()
{
	TArray<int32> AllPredictionIndices;
	AllPredictionIndices.Reserve(PendingPredictions.Num());
	for (int32 Index = 0; Index < PendingPredictions.Num(); ++Index)
	{
		AllPredictionIndices.Add(Index);
	}
	return SuspendPredictions(AllPredictionIndices);
}

TArray<FDRSnowRemovalPipeline::FPendingRemovalPrediction>
FDRSnowRemovalPipeline::SuspendPredictions(const TArray<int32>& PredictionIndices)
{
	TArray<bool> bShouldSuspend;
	bShouldSuspend.Init(false, PendingPredictions.Num());
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
	RemainingPredictions.Reserve(PendingPredictions.Num() - PredictionIndices.Num());
	for (int32 Index = 0; Index < PendingPredictions.Num(); ++Index)
	{
		if (bShouldSuspend[Index])
		{
			SuspendedPredictions.Add(MoveTemp(PendingPredictions[Index]));
		}
		else
		{
			RemainingPredictions.Add(MoveTemp(PendingPredictions[Index]));
		}
	}
	PendingPredictions = MoveTemp(RemainingPredictions);

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

void FDRSnowRemovalPipeline::ResumePredictions(
	UWorld* World,
	TArray<FPendingRemovalPrediction>&& Predictions)
{
	if (!IsValid(World) || World->GetNetMode() != NM_Client)
	{
		return;
	}

	PendingPredictions.Reserve(PendingPredictions.Num() + Predictions.Num());
	for (FPendingRemovalPrediction& Prediction : Predictions)
	{
		Prediction.SurfaceEdit = PredictSurface(
			World,
			Prediction.Request,
			Prediction.RemovalPath);
		PendingPredictions.Add(MoveTemp(Prediction));
	}
	PendingPredictions.Sort(
		[](const FPendingRemovalPrediction& Left, const FPendingRemovalPrediction& Right)
		{
			return Left.LocalOrder < Right.LocalOrder;
		});
	bPredictionCapacityWarningLogged = false;
}

FVoxelIntBox FDRSnowRemovalPipeline::GetRequestBounds(
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

TArray<int32> FDRSnowRemovalPipeline::FindAffectedPredictionIndices(
	AVoxelWorld* VoxelWorld,
	const FVoxelIntBox& SeedBounds,
	const int32 RequiredPredictionIndex) const
{
	TArray<bool> bAffected;
	bAffected.Init(false, PendingPredictions.Num());
	if (bAffected.IsValidIndex(RequiredPredictionIndex))
	{
		bAffected[RequiredPredictionIndex] = true;
	}

	if (IsValid(VoxelWorld) && SeedBounds.IsValid())
	{
		for (int32 Index = 0; Index < PendingPredictions.Num(); ++Index)
		{
			const FDRSnowSurfaceEditResult& Edit = PendingPredictions[Index].SurfaceEdit;
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
				 CandidateIndex < PendingPredictions.Num();
				 ++CandidateIndex)
			{
				if (bAffected[CandidateIndex])
				{
					continue;
				}

				const FDRSnowSurfaceEditResult& CandidateEdit =
					PendingPredictions[CandidateIndex].SurfaceEdit;
				if (CandidateEdit.VoxelWorld.Get() != VoxelWorld ||
					!CandidateEdit.EditedBounds.IsValid())
				{
					continue;
				}

				for (int32 AffectedIndex = 0;
					 AffectedIndex < PendingPredictions.Num();
					 ++AffectedIndex)
				{
					if (!bAffected[AffectedIndex])
					{
						continue;
					}

					const FDRSnowSurfaceEditResult& AffectedEdit =
						PendingPredictions[AffectedIndex].SurfaceEdit;
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

FDRSnowRemovalExecutionResult FDRSnowRemovalPipeline::Execute(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath,
	const bool bBuildMaterialPatch)
{
	FDRSnowRemovalExecutionResult Result;
	Result.RemoveResult.TeamId = Request.Context.TeamId;
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World))
	{
		return Result;
	}

	const FDRSnowSurfaceEditResult EditResult = RemoveSurface(Request, RemovalPath);
	Result.RemoveResult.RemovedAmount = EditResult.AppliedAmount;
	if (Result.RemoveResult.RemovedAmount <= 0.f)
	{
		return Result;
	}

	ApplyRemovedSurfaceEdit(
		World,
		Request,
		EditResult,
		Result.RemoveResult.RemovedAmount);

	FDRSnowResolvedMaterialEdit ResolvedEdit;
	if (ResolveMaterials(Request, EditResult, RemovalPath, ResolvedEdit))
	{
		SurfaceEditor.ApplyResolvedSnowMaterials(
			ResolvedEdit,
			bBuildMaterialPatch ? &Result.MaterialPatch : nullptr);
	}
	return Result;
}

FDRSnowRemovalReplayResult FDRSnowRemovalPipeline::Replay(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowServerResponse& Response)
{
	FDRSnowRemovalReplayResult Result;
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World) || Response.AuthoritativeAmount <= 0.f)
	{
		return Result;
	}

	const FDRSnowSurfaceEditResult EditResult = RemoveSurface(Request, Response.RemovalPath);
	if (EditResult.AppliedAmount <= 0.f)
	{
		return Result;
	}

	ApplyRemovedSurfaceEdit(World, Request, EditResult, Response.AuthoritativeAmount);
	Result.VoxelWorld = EditResult.VoxelWorld;
	if (Response.MaterialPatch)
	{
		Result.bApplied = true;
		return Result;
	}

	// 패치 플래그가 없는 구형 record만 클라이언트의 로컬 Store로 다시 칠한다.
	Result.bApplied = RepaintWithoutPatch(Request, EditResult, Response.RemovalPath);
	return Result;
}

FDRSnowSurfaceEditResult FDRSnowRemovalPipeline::PredictSurface(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath)
{
	SurfaceEditor.SetWorld(World);
	return IsValid(World)
		? RemoveSurface(Request, RemovalPath)
		: FDRSnowSurfaceEditResult();
}

FDRSnowRemovalReplayResult FDRSnowRemovalPipeline::ConfirmPrediction(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& PredictedSurfaceEdit,
	const FDRSnowServerResponse& Response)
{
	FDRSnowRemovalReplayResult Result;
	SurfaceEditor.SetWorld(World);
	if (!IsValid(World) || Response.AuthoritativeAmount <= 0.f ||
		PredictedSurfaceEdit.AppliedAmount <= 0.f)
	{
		return Result;
	}

	ApplyRemovedSurfaceEdit(World, Request, PredictedSurfaceEdit, Response.AuthoritativeAmount);
	Result.VoxelWorld = PredictedSurfaceEdit.VoxelWorld;
	if (Response.MaterialPatch)
	{
		Result.bApplied = true;
		return Result;
	}

	Result.bApplied = RepaintWithoutPatch(Request, PredictedSurfaceEdit, Response.RemovalPath);
	return Result;
}

FDRSnowSurfaceEditResult FDRSnowRemovalPipeline::RemoveSurface(
	const FDRSnowSurfaceRemoveRequest& Request,
	const EDRSnowRemovalPath RemovalPath) const
{
	return RemovalPath == EDRSnowRemovalPath::Absorb
		? SurfaceEditor.RemoveSnowWithAbsorbTool(Request)
		: SurfaceEditor.RemoveSnowAtArea(Request);
}

bool FDRSnowRemovalPipeline::ResolveMaterials(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const EDRSnowRemovalPath RemovalPath,
	FDRSnowResolvedMaterialEdit& OutResolvedEdit) const
{
	return RemovalPath == EDRSnowRemovalPath::Absorb
		? SurfaceEditor.ResolveSnowMaterialsAtModifiedVoxels(
			Request,
			EditResult,
			OwnershipStore,
			VolumeStore,
			OutResolvedEdit)
		: SurfaceEditor.ResolveSnowMaterialsAtArea(
			Request,
			EditResult,
			OwnershipStore,
			VolumeStore,
			OutResolvedEdit);
}

bool FDRSnowRemovalPipeline::RepaintWithoutPatch(
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const EDRSnowRemovalPath RemovalPath) const
{
	FDRSnowResolvedMaterialEdit ResolvedEdit;
	const bool bRepainted =
		ResolveMaterials(Request, EditResult, RemovalPath, ResolvedEdit) &&
		SurfaceEditor.ApplyResolvedSnowMaterials(ResolvedEdit);

	// 기존 Absorb replay는 repaint 결과와 무관하게 geometry 적용 성공을 반환했다.
	return RemovalPath == EDRSnowRemovalPath::Absorb || bRepainted;
}

void FDRSnowRemovalPipeline::ApplyRemovedSurfaceEdit(
	UWorld* World,
	const FDRSnowSurfaceRemoveRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult,
	const float VolumeAmount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_ApplyRemovedSurfaceEdit);
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!EditResult.bUseModifiedValuesForVolume || !IsValid(VoxelWorld))
	{
		return;
	}

	if (IsValid(World) && World->GetNetMode() != NM_Client)
	{
		OwnershipStore.RemoveClearedVoxels(VoxelWorld, EditResult.ModifiedValues);
	}
	RemoveVolumeFromModifiedValues(
		*VoxelWorld,
		Request,
		EditResult.ModifiedValues,
		VolumeAmount);
}

void FDRSnowRemovalPipeline::RemoveVolumeFromModifiedValues(
	AVoxelWorld& VoxelWorld,
	const FDRSnowSurfaceRemoveRequest& Request,
	const TArray<FModifiedVoxelValue>& ModifiedValues,
	const float MaxRemovedAmount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRSnow_Absorb_RemoveVolumeFromModifiedValues);
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

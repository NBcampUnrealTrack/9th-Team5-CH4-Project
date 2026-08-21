#include "DRVoxelTerrainQueryLibrary.h"

#include "VoxelMaterial.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelWorld.h"

namespace
{
	constexpr float DRVoxelValueScale = 32767.f;

	int32 QuantizeVoxelValue(float Value)
	{
		return FMath::RoundToInt(FMath::Clamp(Value, -1.f, 1.f) * DRVoxelValueScale);
	}

	float DequantizeVoxelValue(int32 Value)
	{
		return static_cast<float>(Value) / DRVoxelValueScale;
	}

	bool IsDepositRequestFinished(const FDRVoxelDepositInBoxRequest& Request)
	{
		return Request.Phase == EDRVoxelDepositRequestPhase::Finished;
	}

	FDRVoxelDepositInBoxSettings SanitizeDepositSettings(
		const FDRVoxelDepositInBoxSettings& Settings)
	{
		FDRVoxelDepositInBoxSettings Result = Settings;
		Result.JitterRatio = FMath::Clamp(Result.JitterRatio, 0.f, 1.f);
		Result.DepositPatchRadius = FMath::Max(0, Result.DepositPatchRadius);
		Result.DepositFootprintRadius = FMath::Max(0, Result.DepositFootprintRadius);
		Result.FootprintEdgeStrength = FMath::Clamp(Result.FootprintEdgeStrength, 0.f, 1.f);
		Result.MinSurfaceDepositChance = FMath::Clamp(Result.MinSurfaceDepositChance, 0.f, 1.f);
		Result.MaxSurfaceDepositChance = FMath::Clamp(Result.MaxSurfaceDepositChance, 0.f, 1.f);
		Result.LowerSurfaceSelectionBias = FMath::Max(0.01f, Result.LowerSurfaceSelectionBias);
		return Result;
	}

	void ShuffleVoxels(TArray<FIntVector>& Voxels, FRandomStream& RandomStream)
	{
		for (int32 Index = Voxels.Num() - 1; Index > 0; --Index)
		{
			Voxels.Swap(Index, RandomStream.RandRange(0, Index));
		}

		Voxels.StableSort([](const FIntVector& A, const FIntVector& B)
		{
			return A.Z < B.Z;
		});
	}

	bool IsCandidateBuildFinished(const FDRVoxelDepositInBoxRequest& Request)
	{
		return Request.ScanCursor.X > Request.VoxelMax.X;
	}

	void AdvanceScanCursor(FDRVoxelDepositInBoxRequest& Request)
	{
		Request.ScanCursor.Y += Request.VoxelSampleStep;

		if (Request.ScanCursor.Y > Request.VoxelMax.Y)
		{
			Request.ScanCursor.Y = Request.VoxelMin.Y;
			Request.ScanCursor.X += Request.VoxelSampleStep;
		}
	}

	void ExpandModifiedBounds(
		const FIntVector& Position,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax)
	{
		if (!bHasModifiedBounds)
		{
			ModifiedMin = Position;
			ModifiedMax = Position;
			bHasModifiedBounds = true;
			return;
		}

		ModifiedMin.X = FMath::Min(ModifiedMin.X, Position.X);
		ModifiedMin.Y = FMath::Min(ModifiedMin.Y, Position.Y);
		ModifiedMin.Z = FMath::Min(ModifiedMin.Z, Position.Z);

		ModifiedMax.X = FMath::Max(ModifiedMax.X, Position.X);
		ModifiedMax.Y = FMath::Max(ModifiedMax.Y, Position.Y);
		ModifiedMax.Z = FMath::Max(ModifiedMax.Z, Position.Z);
	}

	bool IsInsideBounds(const FDRVoxelDepositInBoxRequest& Request, const FIntVector& Position)
	{
		return
			Position.X >= Request.VoxelMin.X && Position.X <= Request.VoxelMax.X &&
			Position.Y >= Request.VoxelMin.Y && Position.Y <= Request.VoxelMax.Y &&
			Position.Z >= Request.VoxelMin.Z && Position.Z <= Request.VoxelMax.Z;
	}

	int32 GetLocalIndex(const FIntVector& Position, const FIntVector& VoxelMin, const FIntVector& VoxelMax)
	{
		const int32 SizeX = VoxelMax.X - VoxelMin.X + 1;
		const int32 SizeY = VoxelMax.Y - VoxelMin.Y + 1;

		return
			(Position.X - VoxelMin.X) +
			(Position.Y - VoxelMin.Y) * SizeX +
			(Position.Z - VoxelMin.Z) * SizeX * SizeY;
	}

	FIntVector GetPositionFromLocalIndex(int32 LocalIndex, const FIntVector& VoxelMin, const FIntVector& VoxelMax)
	{
		const int32 SizeX = VoxelMax.X - VoxelMin.X + 1;
		const int32 SizeY = VoxelMax.Y - VoxelMin.Y + 1;
		const int32 SizeXY = SizeX * SizeY;

		const int32 LocalZ = LocalIndex / SizeXY;
		const int32 Remainder = LocalIndex % SizeXY;
		const int32 LocalY = Remainder / SizeX;
		const int32 LocalX = Remainder % SizeX;

		return FIntVector(
			VoxelMin.X + LocalX,
			VoxelMin.Y + LocalY,
			VoxelMin.Z + LocalZ);
	}

	bool TryAddDepositCandidate(
		AVoxelWorld* VoxelWorld,
		const FIntVector& DepositVoxelPosition,
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax,
		TSet<FIntVector>& PendingVoxelSet,
		TArray<FIntVector>& PendingVoxels)
	{
		if (DepositVoxelPosition.X < VoxelMin.X || DepositVoxelPosition.X > VoxelMax.X ||
			DepositVoxelPosition.Y < VoxelMin.Y || DepositVoxelPosition.Y > VoxelMax.Y ||
			DepositVoxelPosition.Z < VoxelMin.Z || DepositVoxelPosition.Z > VoxelMax.Z)
		{
			return false;
		}

		if (PendingVoxelSet.Contains(DepositVoxelPosition))
		{
			return false;
		}

		float DepositValue = 0.f;
		UVoxelDataTools::GetValue(DepositValue, VoxelWorld, DepositVoxelPosition);
		if (DepositValue <= 0.f)
		{
			return false;
		}

		PendingVoxelSet.Add(DepositVoxelPosition);
		PendingVoxels.Add(DepositVoxelPosition);
		return true;
	}

	int32 FindTopSurfaceZ(
		AVoxelWorld* VoxelWorld,
		int32 X,
		int32 Y,
		int32 MinZ,
		int32 MaxZ)
	{
		float AboveValue = 0.f;
		UVoxelDataTools::GetValue(AboveValue, VoxelWorld, FIntVector(X, Y, MaxZ));

		for (int32 Z = MaxZ - 1; Z >= MinZ; --Z)
		{
			float CurrentValue = 0.f;
			UVoxelDataTools::GetValue(CurrentValue, VoxelWorld, FIntVector(X, Y, Z));

			if (CurrentValue <= 0.f && AboveValue > 0.f)
			{
				return Z;
			}

			AboveValue = CurrentValue;
		}

		return MIN_int32;
	}

	struct FDRDepositPatchSurface
	{
		int32 X = 0;
		int32 Y = 0;
		int32 SurfaceZ = 0;
	};

	void AddPatchDepositCandidates(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		int32 CenterX,
		int32 CenterY,
		TSet<FIntVector>& PendingVoxelSet,
		TArray<FIntVector>& PendingVoxels)
	{
		const int32 Radius = FMath::Max(
			Request.DepositSettings.DepositPatchRadius,
			Request.VoxelSampleStep / 2);

		TArray<FDRDepositPatchSurface> PatchSurfaces;
		PatchSurfaces.Reserve(FMath::Square(Radius * 2 + 1));

		int32 MinSurfaceZ = MAX_int32;
		int32 MaxSurfaceZ = MIN_int32;

		for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				const int32 TargetX = CenterX + OffsetX;
				const int32 TargetY = CenterY + OffsetY;

				if (TargetX < Request.VoxelMin.X || TargetX > Request.VoxelMax.X ||
					TargetY < Request.VoxelMin.Y || TargetY > Request.VoxelMax.Y)
				{
					continue;
				}

				const int32 TargetSurfaceZ = FindTopSurfaceZ(
					VoxelWorld,
					TargetX,
					TargetY,
					Request.VoxelMin.Z,
					Request.VoxelMax.Z);

				if (TargetSurfaceZ == MIN_int32)
				{
					continue;
				}

				FDRDepositPatchSurface PatchSurface;
				PatchSurface.X = TargetX;
				PatchSurface.Y = TargetY;
				PatchSurface.SurfaceZ = TargetSurfaceZ;
				PatchSurfaces.Add(PatchSurface);

				MinSurfaceZ = FMath::Min(MinSurfaceZ, TargetSurfaceZ);
				MaxSurfaceZ = FMath::Max(MaxSurfaceZ, TargetSurfaceZ);
			}
		}

		if (PatchSurfaces.Num() == 0)
		{
			return;
		}

		const float MinChance = FMath::Min(
			Request.DepositSettings.MinSurfaceDepositChance,
			Request.DepositSettings.MaxSurfaceDepositChance);
		const float MaxChance = FMath::Max(
			Request.DepositSettings.MinSurfaceDepositChance,
			Request.DepositSettings.MaxSurfaceDepositChance);
		const float HeightRange = static_cast<float>(MaxSurfaceZ - MinSurfaceZ);

		for (const FDRDepositPatchSurface& PatchSurface : PatchSurfaces)
		{
			const float LowerSurfaceAlpha = HeightRange > 0.f
				? static_cast<float>(MaxSurfaceZ - PatchSurface.SurfaceZ) / HeightRange
				: 1.f;
			const float BiasedLowerSurfaceAlpha = FMath::Pow(
				FMath::Clamp(LowerSurfaceAlpha, 0.f, 1.f),
				Request.DepositSettings.LowerSurfaceSelectionBias);
			const float DepositChance = FMath::Lerp(MinChance, MaxChance, BiasedLowerSurfaceAlpha);

			if (Request.RandomStream.FRand() > DepositChance)
			{
				continue;
			}

			TryAddDepositCandidate(
				VoxelWorld,
				FIntVector(PatchSurface.X, PatchSurface.Y, PatchSurface.SurfaceZ + 1),
				Request.VoxelMin,
				Request.VoxelMax,
				PendingVoxelSet,
				PendingVoxels);
		}
	}

	bool TryWriteDepositVoxel(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		const FVoxelMaterial& DepositMaterial,
		const FIntVector& Position,
		float AmountScale,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax,
		FDRVoxelDepositDeltaRecord& DeltaRecord,
		int32& OutModifiedVoxelCount)
	{
		const float ScaledDepositAmount = Request.DepositSettings.DepositAmount * FMath::Max(0.f, AmountScale);
		if (ScaledDepositAmount <= SMALL_NUMBER)
		{
			return false;
		}

		if (!IsInsideBounds(Request, Position) || Request.WrittenVoxelPositions.Contains(Position))
		{
			return false;
		}

		Request.WrittenVoxelPositions.Add(Position);

		if (Position.Z <= Request.VoxelMin.Z)
		{
			return false;
		}

		float BelowValue = 0.f;
		UVoxelDataTools::GetValue(BelowValue, VoxelWorld, FIntVector(Position.X, Position.Y, Position.Z - 1));
		if (BelowValue > 0.f)
		{
			return false;
		}

		float CurrentValue = 0.f;
		UVoxelDataTools::GetValue(CurrentValue, VoxelWorld, Position);

		const float NewValue = FMath::Clamp(CurrentValue - ScaledDepositAmount, -1.f, 1.f);
		if (FMath::IsNearlyEqual(CurrentValue, NewValue))
		{
			return false;
		}

		UVoxelDataTools::SetValue(VoxelWorld, Position, NewValue);
		UVoxelDataTools::SetMaterial(VoxelWorld, Position, DepositMaterial);

		FDRVoxelCompressedValueDelta Delta;
		Delta.LocalIndex = GetLocalIndex(Position, Request.VoxelMin, Request.VoxelMax);
		Delta.QuantizedValue = QuantizeVoxelValue(NewValue);
		DeltaRecord.Deltas.Add(Delta);

		ExpandModifiedBounds(Position, bHasModifiedBounds, ModifiedMin, ModifiedMax);
		OutModifiedVoxelCount++;
		return true;
	}

	void WriteDepositFootprint(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		const FVoxelMaterial& DepositMaterial,
		const FIntVector& CenterPosition,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax,
		FDRVoxelDepositDeltaRecord& DeltaRecord,
		int32& OutModifiedVoxelCount)
	{
		const int32 Radius = Request.DepositSettings.DepositFootprintRadius;
		if (Radius == 0)
		{
			TryWriteDepositVoxel(
				Request,
				VoxelWorld,
				DepositMaterial,
				CenterPosition,
				1.f,
				bHasModifiedBounds,
				ModifiedMin,
				ModifiedMax,
				DeltaRecord,
				OutModifiedVoxelCount);
			return;
		}

		const float RadiusAsFloat = static_cast<float>(Radius);
		const float EdgeStrength = Request.DepositSettings.FootprintEdgeStrength;

		for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				const float Distance = FMath::Sqrt(
					static_cast<float>(OffsetX * OffsetX + OffsetY * OffsetY));
				if (Distance > RadiusAsFloat + 0.5f)
				{
					continue;
				}

				const float DistanceAlpha = FMath::Clamp(Distance / RadiusAsFloat, 0.f, 1.f);
				const float AmountScale = FMath::Lerp(1.f, EdgeStrength, DistanceAlpha);
				const FIntVector FootprintPosition(
					CenterPosition.X + OffsetX,
					CenterPosition.Y + OffsetY,
					CenterPosition.Z);

				TryWriteDepositVoxel(
					Request,
					VoxelWorld,
					DepositMaterial,
					FootprintPosition,
					AmountScale,
					bHasModifiedBounds,
					ModifiedMin,
					ModifiedMax,
					DeltaRecord,
					OutModifiedVoxelCount);
			}
		}
	}

	void BuildDepositCandidatesForCurrentColumn(FDRVoxelDepositInBoxRequest& Request, AVoxelWorld* VoxelWorld)
	{
		const int32 JitterRadius = Request.DepositSettings.bUseJitteredSamples
			? FMath::RoundToInt(static_cast<float>(Request.VoxelSampleStep) * Request.DepositSettings.JitterRatio)
			: 0;

		const int32 SampleX = FMath::Clamp(
			Request.ScanCursor.X + (JitterRadius > 0 ? Request.RandomStream.RandRange(-JitterRadius, JitterRadius) : 0),
			Request.VoxelMin.X,
			Request.VoxelMax.X);

		const int32 SampleY = FMath::Clamp(
			Request.ScanCursor.Y + (JitterRadius > 0 ? Request.RandomStream.RandRange(-JitterRadius, JitterRadius) : 0),
			Request.VoxelMin.Y,
			Request.VoxelMax.Y);

		if (Request.DepositSettings.bOnlyTopSurface)
		{
			const int32 SurfaceZ = FindTopSurfaceZ(
				VoxelWorld,
				SampleX,
				SampleY,
				Request.VoxelMin.Z,
				Request.VoxelMax.Z);

			if (SurfaceZ == MIN_int32)
			{
				return;
			}

			AddPatchDepositCandidates(
				Request,
				VoxelWorld,
				SampleX,
				SampleY,
				Request.PendingVoxelPositions,
				Request.PendingVoxels);

			return;
		}

		for (int32 Z = Request.VoxelMax.Z - 1; Z >= Request.VoxelMin.Z; --Z)
		{
			float CurrentValue = 0.f;
			float AboveValue = 0.f;
			UVoxelDataTools::GetValue(CurrentValue, VoxelWorld, FIntVector(SampleX, SampleY, Z));
			UVoxelDataTools::GetValue(AboveValue, VoxelWorld, FIntVector(SampleX, SampleY, Z + 1));

			if (CurrentValue <= 0.f && AboveValue > 0.f)
			{
				TryAddDepositCandidate(
					VoxelWorld,
					FIntVector(SampleX, SampleY, Z + 1),
					Request.VoxelMin,
					Request.VoxelMax,
					Request.PendingVoxelPositions,
					Request.PendingVoxels);
			}
		}
	}

	void FinishCandidateBuild(FDRVoxelDepositInBoxRequest& Request)
	{
		Request.PendingVoxelPositions.Reset();

		if (Request.PendingVoxels.Num() == 0)
		{
			Request.Phase = EDRVoxelDepositRequestPhase::Finished;
			return;
		}

		ShuffleVoxels(Request.PendingVoxels, Request.RandomStream);
		Request.NextVoxelIndex = 0;
		Request.Phase = EDRVoxelDepositRequestPhase::ApplyVoxels;
	}

	void ProcessCandidateBuildTick(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		int32 MaxScanColumnsToProcess,
		int32& OutScannedColumnCount)
	{
		const int32 ScanColumnsToProcess = FMath::Max(1, MaxScanColumnsToProcess);

		while (OutScannedColumnCount < ScanColumnsToProcess && !IsCandidateBuildFinished(Request))
		{
			BuildDepositCandidatesForCurrentColumn(Request, VoxelWorld);
			AdvanceScanCursor(Request);
			OutScannedColumnCount++;
		}

		if (IsCandidateBuildFinished(Request))
		{
			FinishCandidateBuild(Request);
		}
	}

	void ProcessApplyVoxelsTick(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		int32 MaxVoxelsToProcess,
		int32& OutModifiedVoxelCount,
		FDRVoxelDepositDeltaRecord& OutDeltaRecord)
	{
		OutDeltaRecord.VoxelMin = Request.VoxelMin;
		OutDeltaRecord.VoxelMax = Request.VoxelMax;
		OutDeltaRecord.MaterialIndex = Request.DepositSettings.DepositMaterialIndex;

		FVoxelMaterial DepositMaterial;
		DepositMaterial.SetSingleIndex(Request.DepositSettings.DepositMaterialIndex);

		bool bHasModifiedBounds = false;
		FIntVector ModifiedMin = FIntVector::ZeroValue;
		FIntVector ModifiedMax = FIntVector::ZeroValue;

		int32 RemainingVoxelsToProcess = FMath::Max(1, MaxVoxelsToProcess);
		while (RemainingVoxelsToProcess > 0 && Request.NextVoxelIndex < Request.PendingVoxels.Num())
		{
			const FIntVector DepositVoxelPosition = Request.PendingVoxels[Request.NextVoxelIndex++];

			WriteDepositFootprint(
				Request,
				VoxelWorld,
				DepositMaterial,
				DepositVoxelPosition,
				bHasModifiedBounds,
				ModifiedMin,
				ModifiedMax,
				OutDeltaRecord,
				OutModifiedVoxelCount);

			RemainingVoxelsToProcess--;
		}

		if (bHasModifiedBounds)
		{
			const FVoxelIntBox UpdateBounds(ModifiedMin, ModifiedMax);
			UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, UpdateBounds.Extend(1));
		}

		if (Request.NextVoxelIndex >= Request.PendingVoxels.Num())
		{
			Request.Phase = EDRVoxelDepositRequestPhase::Finished;
		}
	}
}

bool UDRVoxelTerrainQueryLibrary::GetMaterialCountsInBox(
	AVoxelWorld* VoxelWorld,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	float SampleStep,
	const TArray<uint8>& TargetMaterialIndices,
	TMap<uint8, int32>& OutMaterialCounts,
	int32& OutTotalCount)
{
	OutMaterialCounts.Reset();
	OutTotalCount = 0;

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || SampleStep <= 0.f)
	{
		return false;
	}

	const FVector AbsExtent(FMath::Abs(BoxExtent.X), FMath::Abs(BoxExtent.Y), FMath::Abs(BoxExtent.Z));
	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	TSet<uint8> TargetMaterialSet;
	for (const uint8 MaterialIndex : TargetMaterialIndices)
	{
		TargetMaterialSet.Add(MaterialIndex);
	}

	const bool bUseMaterialFilter = TargetMaterialSet.Num() > 0;
	const FVector Min = BoxCenter - AbsExtent;
	const FVector Max = BoxCenter + AbsExtent;

	for (float X = Min.X; X <= Max.X; X += SampleStep)
	{
		for (float Y = Min.Y; Y <= Max.Y; Y += SampleStep)
		{
			for (float Z = Min.Z; Z <= Max.Z; Z += SampleStep)
			{
				const FIntVector SampleVoxelPosition = VoxelWorld->GlobalToLocal(FVector(X, Y, Z));

				float Value = 0.f;
				UVoxelDataTools::GetValue(Value, VoxelWorld, SampleVoxelPosition);
				if (Value > 0.f)
				{
					continue;
				}

				FVoxelMaterial Material;
				UVoxelDataTools::GetMaterial(Material, VoxelWorld, SampleVoxelPosition);

				const uint8 MaterialIndex = Material.GetSingleIndex();
				if (bUseMaterialFilter && !TargetMaterialSet.Contains(MaterialIndex))
				{
					continue;
				}

				OutMaterialCounts.FindOrAdd(MaterialIndex)++;
				OutTotalCount++;
			}
		}
	}

	return true;
}

bool UDRVoxelTerrainQueryLibrary::IsVoxelUpdateInBox(
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	const FVector& Location,
	float Radius)
{
	if (Radius <= 0.f)
	{
		return false;
	}

	const FVector AbsExtent(FMath::Abs(BoxExtent.X), FMath::Abs(BoxExtent.Y), FMath::Abs(BoxExtent.Z));
	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	const FBox QueryBox(BoxCenter - AbsExtent, BoxCenter + AbsExtent);
	const FVector ClosestPoint = QueryBox.GetClosestPointTo(Location);

	return FVector::DistSquared(ClosestPoint, Location) <= FMath::Square(Radius);
}

bool UDRVoxelTerrainQueryLibrary::MakeDepositInBoxRequest(
	AVoxelWorld* VoxelWorld,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	const FDRVoxelDepositInBoxSettings& Settings,
	FDRVoxelDepositInBoxRequest& OutRequest)
{
	OutRequest = FDRVoxelDepositInBoxRequest();

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	if (Settings.SampleStep <= 0.f || Settings.DepositAmount <= 0.f || VoxelWorld->VoxelSize <= 0.f)
	{
		return false;
	}

	const FVector AbsExtent(FMath::Abs(BoxExtent.X), FMath::Abs(BoxExtent.Y), FMath::Abs(BoxExtent.Z));
	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	const FIntVector VoxelA = VoxelWorld->GlobalToLocal(BoxCenter - AbsExtent);
	const FIntVector VoxelB = VoxelWorld->GlobalToLocal(BoxCenter + AbsExtent);

	OutRequest.VoxelWorld = VoxelWorld;
	OutRequest.VoxelMin = FIntVector(
		FMath::Min(VoxelA.X, VoxelB.X),
		FMath::Min(VoxelA.Y, VoxelB.Y),
		FMath::Min(VoxelA.Z, VoxelB.Z));
	OutRequest.VoxelMax = FIntVector(
		FMath::Max(VoxelA.X, VoxelB.X),
		FMath::Max(VoxelA.Y, VoxelB.Y),
		FMath::Max(VoxelA.Z, VoxelB.Z));

	if (OutRequest.VoxelMax.Z - OutRequest.VoxelMin.Z < 1)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	OutRequest.DepositSettings = SanitizeDepositSettings(Settings);
	OutRequest.VoxelSampleStep = FMath::Max(
		1,
		FMath::RoundToInt(OutRequest.DepositSettings.SampleStep / VoxelWorld->VoxelSize));
	OutRequest.ScanCursor = FIntPoint(OutRequest.VoxelMin.X, OutRequest.VoxelMin.Y);
	OutRequest.RandomStream.Initialize(OutRequest.DepositSettings.RandomSeed);
	OutRequest.NextVoxelIndex = 0;
	OutRequest.Phase = EDRVoxelDepositRequestPhase::BuildCandidates;
	OutRequest.bIsValid = true;
	return true;
}

bool UDRVoxelTerrainQueryLibrary::ProcessDepositInBoxRequestsTick(
	TArray<FDRVoxelDepositInBoxRequest>& Requests,
	int32 MaxScanColumnsToProcess,
	int32 MaxVoxelsToProcess,
	int32& OutModifiedVoxelCount,
	int32& OutScannedColumnCount,
	FDRVoxelDepositDeltaRecord& OutDeltaRecord,
	int32& OutRemainingRequestCount)
{
	OutModifiedVoxelCount = 0;
	OutScannedColumnCount = 0;
	OutRemainingRequestCount = 0;
	OutDeltaRecord = FDRVoxelDepositDeltaRecord();

	if (Requests.Num() == 0)
	{
		return false;
	}

	FDRVoxelDepositInBoxRequest& Request = Requests[0];
	AVoxelWorld* VoxelWorld = Request.VoxelWorld.Get();

	if (!Request.bIsValid || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		Requests.RemoveAt(0, 1, EAllowShrinking::No);
		OutRemainingRequestCount = Requests.Num();
		return false;
	}

	if (Request.Phase == EDRVoxelDepositRequestPhase::BuildCandidates)
	{
		ProcessCandidateBuildTick(
			Request,
			VoxelWorld,
			MaxScanColumnsToProcess,
			OutScannedColumnCount);
	}
	else if (Request.Phase == EDRVoxelDepositRequestPhase::ApplyVoxels)
	{
		ProcessApplyVoxelsTick(
			Request,
			VoxelWorld,
			MaxVoxelsToProcess,
			OutModifiedVoxelCount,
			OutDeltaRecord);
	}

	if (IsDepositRequestFinished(Request))
	{
		Requests.RemoveAt(0, 1, EAllowShrinking::No);
	}

	OutRemainingRequestCount = Requests.Num();
	return OutRemainingRequestCount > 0;
}

bool UDRVoxelTerrainQueryLibrary::ApplyDepositDeltaRecord(
	AVoxelWorld* VoxelWorld,
	const FDRVoxelDepositDeltaRecord& DeltaRecord,
	int32& OutAppliedVoxelCount)
{
	OutAppliedVoxelCount = 0;

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || DeltaRecord.Deltas.Num() == 0)
	{
		return false;
	}

	FVoxelMaterial Material;
	Material.SetSingleIndex(DeltaRecord.MaterialIndex);

	bool bHasModifiedBounds = false;
	FIntVector ModifiedMin = FIntVector::ZeroValue;
	FIntVector ModifiedMax = FIntVector::ZeroValue;

	for (const FDRVoxelCompressedValueDelta& Delta : DeltaRecord.Deltas)
	{
		const FIntVector Position = GetPositionFromLocalIndex(
			Delta.LocalIndex,
			DeltaRecord.VoxelMin,
			DeltaRecord.VoxelMax);

		const float Value = DequantizeVoxelValue(Delta.QuantizedValue);

		UVoxelDataTools::SetValue(VoxelWorld, Position, Value);
		UVoxelDataTools::SetMaterial(VoxelWorld, Position, Material);

		ExpandModifiedBounds(Position, bHasModifiedBounds, ModifiedMin, ModifiedMax);
		OutAppliedVoxelCount++;
	}

	if (bHasModifiedBounds)
	{
		const FVoxelIntBox UpdateBounds(ModifiedMin, ModifiedMax);
		UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld, UpdateBounds.Extend(1));
	}

	return OutAppliedVoxelCount > 0;
}

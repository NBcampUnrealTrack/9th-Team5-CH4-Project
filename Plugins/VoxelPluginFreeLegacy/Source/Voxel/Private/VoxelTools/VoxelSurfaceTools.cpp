// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTools/VoxelSurfaceTools.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelTools/VoxelHardnessHandler.h"
#include "VoxelTools/VoxelSurfaceToolsImpl.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelUtilities/VoxelRichCurveUtilities.h"
#include "VoxelUtilities/VoxelDistanceFieldUtilities.h"
#include "VoxelUtilities/VoxelMiscUtilities.h"
#include "VoxelDirection.h"

#include "Curves/CurveFloat.h"
#include "Async/ParallelFor.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"

namespace
{
TAutoConsoleVariable<int32> CVarDRVoxelSnowPatchVersion(
	TEXT("voxel.DR.SnowPatchVersion"), 104,
	TEXT("Compiled Voxel-side SnowEdit patch version: 104 = 1.4."), ECVF_ReadOnly);
TAutoConsoleVariable<int32> CVarDRCpuJumpFloodParallel(
	TEXT("voxel.DR.CpuJumpFloodParallel"), 1,
	TEXT("1: forward the surface query multithreading option to CPU JumpFlood. 0: original single-threaded CPU JumpFlood. GPU selection is unchanged."), ECVF_Default);
TAutoConsoleVariable<int32> CVarDRDistanceFieldPerf(
	TEXT("voxel.DR.DistanceFieldPerf"), 0,
	TEXT("1: log JumpFlood path and call duration. This is not mesh/collision time."), ECVF_Default);
}

bool FVoxelSurfaceEditsStack::HasErrors(const FVoxelSurfaceEditsVoxels& Voxels, FString& OutErrors) const
{
	OutErrors.Reset();
	
	for (const auto& Element : Stack)
	{
		if ((Element.Flags & EVoxelSurfaceEditsStackElementFlags::NeedValues) && !Voxels.Info.bHasValues)
		{
			OutErrors += FString::Printf(TEXT("%s needs values to be computed!\n"), *Element.Name);
		}
		
		if ((Element.Flags & EVoxelSurfaceEditsStackElementFlags::NeedNormals) && !Voxels.Info.bHasNormals)
		{
			OutErrors += FString::Printf(TEXT("%s needs normals to be computed!\n"), *Element.Name);
		}
		
		if ((Element.Flags & EVoxelSurfaceEditsStackElementFlags::ShouldBeLast) && &Element != &Stack.Last())
		{
			OutErrors += FString::Printf(TEXT("%s needs to be the last element of the stack!\n"), *Element.Name);
		}
	}

	return !OutErrors.IsEmpty();
}

FVoxelSurfaceEditsProcessedVoxels FVoxelSurfaceEditsStack::Execute(const FVoxelSurfaceEditsVoxels& Voxels, bool bComputeBounds) const
{
	VOXEL_ASYNC_FUNCTION_COUNTER();
	
	auto ProcessedVoxels = TArray<FVoxelSurfaceEditsVoxel>(*Voxels.Voxels);
	for (const auto& Element : Stack)
	{
		Element.Apply(Voxels.Info, ProcessedVoxels);
	}
	
	FVoxelSurfaceEditsProcessedVoxels Result;

	if (ProcessedVoxels.Num() > 0 && bComputeBounds)
	{
		VOXEL_ASYNC_SCOPE_COUNTER("ComputeBounds");

		Result.Bounds = FVoxelIntBox(ProcessedVoxels[0].Position);
		for (auto& Voxel : ProcessedVoxels)
		{
			Result.Bounds = Result.Bounds + Voxel.Position;
		}
	}
	
	Result.Info = Voxels.Info;
	Result.Voxels = MakeVoxelSharedCopy(MoveTemp(ProcessedVoxels));
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<uint8 DirectionMask>
FVoxelSurfaceEditsVoxels UVoxelSurfaceTools::FindSurfaceVoxelsImpl(
		FVoxelData& Data,
		const FVoxelIntBox& Bounds,
		bool bComputeNormals_Dynamic, 
		bool bOnlyOutputNonEmptyVoxels_Dynamic)
{
	VOXEL_TOOL_FUNCTION_COUNTER(Bounds.Count());
	
	const FIntVector Size = Bounds.Size();

	const TArray<FVoxelValue> Values = Data.GetValues(Bounds);
	
	const auto GetValue = [&](int32 X, int32 Y, int32 Z)
	{
		checkVoxelSlow(0 <= X && X < Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);
		checkVoxelSlow(0 <= Z && Z < Size.Z);
		const int32 Index = X + Y * Size.X + Z * Size.X * Size.Y;
		return Values.GetData()[Index];
	};

	TArray<FVoxelSurfaceEditsVoxelBase> OutVoxels;
	OutVoxels.Reserve(Bounds.Count());

	FVoxelUtilities::StaticBranch(bComputeNormals_Dynamic, bOnlyOutputNonEmptyVoxels_Dynamic, [&](auto bComputeNormals_Static, auto bOnlyOutputNonEmptyVoxels_Static)
	{
		for (int32 X = 1; X < Size.X - 1; X++)
		{
			for (int32 Y = 1; Y < Size.Y - 1; Y++)
			{
				for (int32 Z = 1; Z < Size.Z - 1; Z++)
				{
					const FVoxelValue Value = GetValue(X, Y, Z);
					if (bOnlyOutputNonEmptyVoxels_Static && Value.IsEmpty())
					{
						continue;
					}
					
					bool bAdd = false;
					const auto GetOtherValue = [&](uint8 Direction, int32 DX, int32 DY, int32 DZ)
					{
						const FVoxelValue OtherValue = GetValue(X + DX, Y + DY, Z + DZ);
						if (!DirectionMask || (!Value.IsEmpty() && (Direction & DirectionMask)))
						{
							bAdd |= Value.IsEmpty() != OtherValue.IsEmpty();
						}
						return OtherValue.ToFloat();
					};
					
					// Note: if bComputeNormals = false, could be faster by not computing other values once bAdd = true
					const float GradientX = GetOtherValue(EVoxelDirectionFlag::XMax, 1, 0, 0) - GetOtherValue(EVoxelDirectionFlag::XMin, -1, 0, 0);
					const float GradientY = GetOtherValue(EVoxelDirectionFlag::YMax, 0, 1, 0) - GetOtherValue(EVoxelDirectionFlag::YMin, 0, -1, 0);
					const float GradientZ = GetOtherValue(EVoxelDirectionFlag::ZMax, 0, 0, 1) - GetOtherValue(EVoxelDirectionFlag::ZMin, 0, 0, -1);

					if (bAdd)
					{
						FVector Normal;
						if (bComputeNormals_Static)
						{
							Normal = FVector(GradientX, GradientY, GradientZ).GetSafeNormal();
						}
						else
						{
							Normal = FVector(ForceInit);
						}
						OutVoxels.Add({ Bounds.Min + FIntVector(X, Y, Z), Normal, Value.ToFloat() });
					}
				}
			}
		}
	});

	FVoxelSurfaceEditsVoxels EditsVoxels;
	EditsVoxels.Info.bHasValues = true;
	EditsVoxels.Info.bHasNormals = bComputeNormals_Dynamic;

	EditsVoxels.Voxels = MakeVoxelSharedCopy(MoveTemp(OutVoxels));
	
	return EditsVoxels;
}

template VOXEL_API FVoxelSurfaceEditsVoxels UVoxelSurfaceTools::FindSurfaceVoxelsImpl<0>(FVoxelData&, const FVoxelIntBox&, bool, bool);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceEditsVoxels UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceFieldImpl(
	FVoxelData& Data,
	const FVoxelIntBox& Bounds,
	const bool bMultiThreaded)
{
	VOXEL_TOOL_FUNCTION_COUNTER(Bounds.Count());
	
	// Else ParallelFor might crash
	if (!Bounds.IsValid())
	{
		return {};
	}
	
	const FIntVector Size = Bounds.Size();

	const TArray<FVoxelValue> Values = Data.ParallelGet<FVoxelValue>(Bounds.Extend(1), !bMultiThreaded);

	TArray<float> Distances;
	TArray<FVector3f> SurfacePositions;
	FVoxelDistanceFieldUtilities::GetSurfacePositionsFromDensities(Size, Values, Distances, SurfacePositions);
	const bool bCpuParallel = bMultiThreaded && CVarDRCpuJumpFloodParallel.GetValueOnAnyThread() != 0;
	const bool bMeasureJump = CVarDRDistanceFieldPerf.GetValueOnAnyThread() != 0;
	const bool bGpuPath = IsInGameThread() && FApp::CanEverRender();
	const double JumpStart = bMeasureJump ? FPlatformTime::Seconds() : 0.0;
	FVoxelDistanceFieldUtilities::JumpFlood(Size, SurfacePositions, bCpuParallel);
	if (bMeasureJump)
	{
		const double JumpMs = (FPlatformTime::Seconds() - JumpStart) * 1000.0;
		UE_LOG(LogTemp, Log, TEXT("[DRSnowDistanceField] Version=1.4 PID=%u Path=%s GameThread=%d CanRender=%d CPUParallel=%d SizeX=%d SizeY=%d SizeZ=%d JumpMs=%.3f"),
			FPlatformProcess::GetCurrentProcessId(), bGpuPath ? TEXT("GPUHelper") : TEXT("CPU"),
			IsInGameThread() ? 1 : 0, FApp::CanEverRender() ? 1 : 0,
			(!bGpuPath && bCpuParallel) ? 1 : 0, Size.X, Size.Y, Size.Z, JumpMs);
	}
	FVoxelDistanceFieldUtilities::GetDistancesFromSurfacePositions(Size, SurfacePositions, Distances);
	
	VOXEL_ASYNC_SCOPE_COUNTER("Create OutVoxels");
	TArray<FVoxelSurfaceEditsVoxelBase> OutVoxels;
	OutVoxels.Empty(Bounds.Count());
	OutVoxels.SetNumUninitialized(Bounds.Count());
	
	ParallelFor(Size.X, [&](int32 X)
	{
		for (int32 Y = 0; Y < Size.Y; Y++)
		{
			for (int32 Z = 0; Z < Size.Z; Z++)
			{
				const int32 Index = FVoxelUtilities::Get3DIndex(Size, X, Y, Z);

				FVoxelSurfaceEditsVoxelBase Voxel;
				Voxel.Position = Bounds.Min + FIntVector(X, Y, Z);
				Voxel.Normal = FVector(ForceInit);
				Voxel.Value = FVoxelUtilities::Get(Distances, Index);
				Voxel.SurfacePosition = FVector(Bounds.Min) + FVector(FVoxelUtilities::Get(SurfacePositions, Index));
				
				FVoxelUtilities::Get(OutVoxels, Index) = Voxel;
			}
		}
	}, !bMultiThreaded);
	
	FVoxelSurfaceEditsVoxels EditsVoxels;
	EditsVoxels.Info.bHasValues = true;
	EditsVoxels.Info.bHasExactDistanceField = true;
	EditsVoxels.Info.bHasSurfacePositions = true;

	EditsVoxels.Voxels = MakeVoxelSharedCopy(MoveTemp(OutVoxels));
	
	return EditsVoxels;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceEditsVoxels UVoxelSurfaceTools::FindSurfaceVoxels2DImpl(FVoxelData& Data, const FVoxelIntBox& Bounds, bool bComputeNormals)
{
	VOXEL_TOOL_FUNCTION_COUNTER(Bounds.Count());

	FVoxelSurfaceEditsVoxels EditsVoxels = FindSurfaceVoxelsImpl<EVoxelDirectionFlag::ZMax>(Data, Bounds, bComputeNormals);
	
	TMap<FIntPoint, FVoxelSurfaceEditsVoxelBase> Columns;
	Columns.Reserve(Bounds.Size().X * Bounds.Size().Y);
	
	for (const auto& Voxel : *EditsVoxels.Voxels)
	{
		const FIntPoint Point(Voxel.Position.X, Voxel.Position.Y);
		if (auto* Existing = Columns.Find(Point))
		{
			if (Existing->Position.Z < Voxel.Position.Z)
			{
				*Existing = Voxel;
			}
		}
		else
		{
			Columns.Add(Point, Voxel);
		}
	}

	TArray<FVoxelSurfaceEditsVoxelBase> OutVoxels;
	Columns.GenerateValueArray(OutVoxels);
	
	EditsVoxels.Voxels = MakeVoxelSharedCopy(MoveTemp(OutVoxels));
	EditsVoxels.Info.bIs2D = true;
	
	return EditsVoxels;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelSurfaceTools::FindSurfaceVoxels(
	FVoxelSurfaceEditsVoxels& Voxels,
	AVoxelWorld* World,
	FVoxelIntBox Bounds,
	bool bComputeNormals)
{
	VOXEL_TOOL_HELPER(Read, DoNotUpdateRender, NO_PREFIX, Voxels = FindSurfaceVoxelsImpl(Data, Bounds, bComputeNormals));
}

void UVoxelSurfaceTools::FindSurfaceVoxelsAsync(
	UObject* WorldContextObject, 
	FLatentActionInfo LatentInfo, 
	FVoxelSurfaceEditsVoxels& Voxels,
	AVoxelWorld* World, 
	FVoxelIntBox Bounds,
	bool bComputeNormals,
	bool bHideLatentWarnings)
{
	VOXEL_TOOL_LATENT_HELPER_WITH_VALUE(Voxels, Read, DoNotUpdateRender, NO_PREFIX, InVoxels = FindSurfaceVoxelsImpl(Data, Bounds, bComputeNormals));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelSurfaceTools::FindSurfaceVoxelsFromDistanceField(
	FVoxelSurfaceEditsVoxels& Voxels,
	AVoxelWorld* World,
	FVoxelIntBox Bounds,
	bool bMultiThreaded)
{
	// TODO compute normals
	VOXEL_TOOL_HELPER(Read, DoNotUpdateRender, NO_PREFIX, Voxels = FindSurfaceVoxelsFromDistanceFieldImpl(Data, Bounds, bMultiThreaded));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelSurfaceTools::FindSurfaceVoxels2D(
	FVoxelSurfaceEditsVoxels& Voxels,
	AVoxelWorld* World, 
	FVoxelIntBox Bounds,
	bool bComputeNormals)
{
	VOXEL_TOOL_HELPER(Read, DoNotUpdateRender, NO_PREFIX, Voxels = FindSurfaceVoxels2DImpl(Data, Bounds, bComputeNormals));
}

void UVoxelSurfaceTools::FindSurfaceVoxels2DAsync(
	UObject* WorldContextObject, 
	FLatentActionInfo LatentInfo, 
	FVoxelSurfaceEditsVoxels& Voxels, 
	AVoxelWorld* World, 
	FVoxelIntBox Bounds,
	bool bComputeNormals, 
	bool bHideLatentWarnings)
{
	VOXEL_TOOL_LATENT_HELPER_WITH_VALUE(Voxels, Read, DoNotUpdateRender, NO_PREFIX, InVoxels = FindSurfaceVoxels2DImpl(Data, Bounds, bComputeNormals));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceEditsStack UVoxelSurfaceTools::AddToStack(FVoxelSurfaceEditsStack Stack, FVoxelSurfaceEditsStackElement Element)
{
	Stack.Add(Element);
	return Stack;
}

FVoxelSurfaceEditsProcessedVoxels UVoxelSurfaceTools::ApplyStack(FVoxelSurfaceEditsVoxels Voxels, FVoxelSurfaceEditsStack Stack)
{
	FString Error;
	if (Stack.HasErrors(Voxels, Error))
	{
		FVoxelMessages::Warning(FString::Printf(TEXT("ApplyStack: %s"), *Error));
	}
	return Stack.Execute(Voxels);
}

void UVoxelSurfaceTools::ApplyStackAsync(
	UObject* WorldContextObject, 
	FLatentActionInfo LatentInfo, 
	FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	FVoxelSurfaceEditsVoxels Voxels, 
	FVoxelSurfaceEditsStack Stack, 
	bool bHideLatentWarnings)
{
	FString Error;
	if (Stack.HasErrors(Voxels, Error))
	{
		FVoxelMessages::Warning(FString::Printf(TEXT("ApplyStackAsync: %s"), *Error));
	}

	FVoxelToolHelpers::StartAsyncLatentAction_WithoutWorld_WithValue(
		WorldContextObject,
		LatentInfo,
		FUNCTION_FNAME,
		bHideLatentWarnings,
		ProcessedVoxels,
		[=](FVoxelSurfaceEditsProcessedVoxels& InProcessedVoxels)
		{
			InProcessedVoxels = Stack.Execute(Voxels);
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceEditsStackElement UVoxelSurfaceTools::ApplyConstantStrength(float Strength)
{
	return
	{
		"ApplyConstantStrength",
		EVoxelSurfaceEditsStackElementFlags::None,
		[=](auto& Info, auto& Voxels)
		{
			FVoxelSurfaceToolsImpl::ApplyConstantStrengthImpl(Voxels, Strength);
		}
	};
}

FVoxelSurfaceEditsStackElement UVoxelSurfaceTools::ApplyStrengthCurve(
	AVoxelWorld* World,
	FVector InCenter,
	float InRadius,
	UCurveFloat* StrengthCurve,
	bool bConvertToVoxelSpace)
{
	CHECK_VOXELWORLD_FOR_CONVERT_TO_VOXEL_SPACE();
	
	if (!StrengthCurve)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid Strength Curve!"));
		return {};
	}
	
	const FVoxelVector Center = GET_VOXEL_TOOL_REAL(InCenter);
	const float Radius = GET_VOXEL_TOOL_REAL(InRadius);
	
	return
	{
		"ApplyStrengthCurve",
		EVoxelSurfaceEditsStackElementFlags::None,
		[=](auto& Info, auto& Voxels)
		{
			FVoxelSurfaceToolsImpl::ApplyDistanceStrengthFunctionImpl(Voxels, Center, Info.bIs2D, [=](float Distance)
			{
				return FVoxelRichCurveUtilities::Eval(StrengthCurve->FloatCurve, Distance / Radius);
			});
		}
	};
}

FVoxelSurfaceEditsStackElement UVoxelSurfaceTools::ApplyFalloff(
	AVoxelWorld* World, 
	EVoxelFalloff FalloffType, 
	FVector InCenter, 
	float InRadius, 
	float Falloff, 
	bool bConvertToVoxelSpace)
{
	CHECK_VOXELWORLD_FOR_CONVERT_TO_VOXEL_SPACE();
	
	const FVoxelVector Center = GET_VOXEL_TOOL_REAL(InCenter);
	const float Radius = GET_VOXEL_TOOL_REAL(InRadius);

	return
	{
		"ApplyFalloff",
		EVoxelSurfaceEditsStackElementFlags::None,
		FVoxelUtilities::DispatchFalloff(FalloffType, Radius, Falloff, [&](auto GetFalloff) -> FVoxelSurfaceEditsStackElement::FApply
		{
			return [=](auto& Info, auto& Voxels)
			{
				FVoxelSurfaceToolsImpl::ApplyDistanceStrengthFunctionImpl(Voxels, Center, Info.bIs2D, GetFalloff);
			};
		})
	};
}

FVoxelSurfaceEditsStackElement UVoxelSurfaceTools::ApplyStrengthMask(
	AVoxelWorld* World,
	FVoxelFloatTexture Mask,
	FVector InEditPosition,
	float ScaleX,
	float ScaleY,
	FVector PlaneNormal,
	FVector PlaneTangent,
	EVoxelSamplerMode SamplerMode,
	bool bConvertToVoxelSpace)
{
	FVoxelMessages::Info(FUNCTION_ERROR("Masks require Voxel Plugin Pro"));
	
	return
	{
		"ApplyStrengthMask",
		EVoxelSurfaceEditsStackElementFlags::None,
		[=](auto& Info, auto& Voxels) {}
	};
}

void UVoxelSurfaceTools::GetStrengthMaskScale(
	float& ScaleX,
	float& ScaleY,
	AVoxelWorld* World,
	FVoxelFloatTexture Mask,
	float SizeX,
	float SizeY, 
	bool bConvertToVoxelSpace)
{
	FVoxelMessages::Info(FUNCTION_ERROR("Masks require Voxel Plugin Pro"));
}

FVoxelSurfaceEditsStackElement UVoxelSurfaceTools::ApplyTerrace(
	int32 TerraceHeightInVoxels,
	float Angle,
	int32 ImmutableVoxels)
{
	if (TerraceHeightInVoxels < 1)
	{
		FVoxelMessages::Error("TerraceHeightInVoxels must be >= 1");
		return {};
	}
	
	return
	{
		"ApplyTerrace",
		EVoxelSurfaceEditsStackElementFlags::NeedNormals,
		[=](auto& Info, auto& Voxels)
		{
			FVoxelSurfaceToolsImpl::ApplyTerraceImpl(Voxels, TerraceHeightInVoxels, Angle, ImmutableVoxels);
		}
	};
}

FVoxelSurfaceEditsStackElement UVoxelSurfaceTools::ApplyFlatten(
	AVoxelWorld* World,
	FVector PlanePoint,
	FVector PlaneNormal,
	EVoxelSDFMergeMode MergeMode,
	bool bConvertToVoxelSpace)
{
	CHECK_VOXELWORLD_FOR_CONVERT_TO_VOXEL_SPACE();
	
	PlanePoint = GET_VOXEL_TOOL_REAL(PlanePoint).ToFloat();

	if (bConvertToVoxelSpace)
	{
		PlaneNormal = World->GetActorTransform().InverseTransformVector(PlaneNormal).GetSafeNormal();
	}
	
	return
	{
		"ApplyFlatten",
		EVoxelSurfaceEditsStackElementFlags::NeedValues | EVoxelSurfaceEditsStackElementFlags::ShouldBeLast,
		[=](auto& Info, auto& Voxels)
		{
			FVoxelSurfaceToolsImpl::ApplyFlattenImpl(Info, Voxels, FPlane(PlanePoint, PlaneNormal.GetSafeNormal()), MergeMode);
		}
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelSurfaceTools::DebugSurfaceVoxels(
	AVoxelWorld* World, 
	const FVoxelSurfaceEditsProcessedVoxels& ProcessedVoxels,
	float Lifetime)
{
	VOXEL_FUNCTION_COUNTER();
	CHECK_VOXELWORLD_IS_CREATED_VOID();
	
	for (auto& Voxel : *ProcessedVoxels.Voxels)
	{
		const FVector Position = World->LocalToGlobal(Voxel.Position);
		const FLinearColor Color = FMath::Lerp(
			FLinearColor::Black,
			Voxel.Strength > 0 ? FLinearColor::Red : FLinearColor::Green,
			FMath::Clamp(FMath::Abs(Voxel.Strength), 0.f, 1.f));
		DrawDebugPoint(
			World->GetWorld(),
			Position,
			World->VoxelSize / 20,
			Color.ToFColor(false),
			false,
			Lifetime);
	}
}

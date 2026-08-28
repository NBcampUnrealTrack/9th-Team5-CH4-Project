// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "GlobalShader.h"
#include "RenderCommandFence.h"
#include "RenderGraphResources.h"
#include "ShaderParameterStruct.h"

#define VOXEL_DISTANCE_FIELD_NUM_THREADS_CS 8

class FVoxelJumpFloodCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoxelJumpFloodCS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelJumpFloodCS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SizeX)
		SHADER_PARAMETER(uint32, SizeY)
		SHADER_PARAMETER(uint32, SizeZ)
		SHADER_PARAMETER(uint32, Step)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Src)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Dst)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

class VOXEL_API FVoxelDistanceFieldShaderHelper : public TVoxelSharedFromThis<FVoxelDistanceFieldShaderHelper>
{
public:
	FVoxelDistanceFieldShaderHelper() = default;

	void WaitForCompletion() const;

	void StartCompute(
		const FIntVector& Size,
		const TVoxelSharedRef<TArray<FVector3f>>& InOutData,
		int32 MaxPasses_Debug = -1);

	void Compute_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		const FIntVector& Size,
		FVector3f* RESTRICT Data,
		int32 Num,
		int32 MaxPasses_Debug = -1);

private:
	FRenderCommandFence Fence;
};

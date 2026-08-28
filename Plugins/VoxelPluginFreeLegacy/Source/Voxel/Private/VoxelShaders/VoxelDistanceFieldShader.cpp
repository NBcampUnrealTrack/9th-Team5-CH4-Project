// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelShaders/VoxelDistanceFieldShader.h"
#include "VoxelUtilities/VoxelIntVectorUtilities.h"

#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FVoxelDistanceFieldParameters, "VoxelDistanceFieldParameters");

FVoxelDistanceFieldBaseCS::FVoxelDistanceFieldBaseCS(
	const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	Src.Bind(Initializer.ParameterMap, TEXT("Src"));
	Dst.Bind(Initializer.ParameterMap, TEXT("RWDst"));
}

void FVoxelDistanceFieldBaseCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("NUM_THREADS_CS"), VOXEL_DISTANCE_FIELD_NUM_THREADS_CS);
}

void FVoxelDistanceFieldBaseCS::SetBuffers(
	FRHIBatchedShaderParameters& BatchedParameters,
	const FRWBuffer& SrcBuffer,
	const FRWBuffer& DstBuffer) const
{
	// JumpFlood는 ping-pong 방식이다. Src는 이전 pass 결과를 읽기만 하므로
	// UAV로 묶지 않는다. SRV/UAV를 구분해야 AMD/NVIDIA가 동일한 resource
	// hazard 및 cache visibility 규칙으로 다음 pass를 실행한다.
	SetSRVParameter(BatchedParameters, Src, SrcBuffer.SRV);
	SetUAVParameter(BatchedParameters, Dst, DstBuffer.UAV);
}

void FVoxelDistanceFieldBaseCS::SetUniformBuffers(
	FRHIBatchedShaderParameters& BatchedParameters,
	const FVoxelDistanceFieldParameters& Parameters) const
{
	const FVoxelDistanceFieldParametersRef ParametersBuffer =
		FVoxelDistanceFieldParametersRef::CreateUniformBufferImmediate(
			Parameters,
			UniformBuffer_MultiFrame);

	SetUniformBufferParameter(
		BatchedParameters,
		GetUniformBufferParameter<FVoxelDistanceFieldParameters>(),
		ParametersBuffer);
}
void FVoxelDistanceFieldBaseCS::UnsetBuffers(FRHIBatchedShaderUnbinds& BatchedUnbinds) const
{
	UnsetSRVParameter(BatchedUnbinds, Src);
	UnsetUAVParameter(BatchedUnbinds, Dst);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_TYPE_LAYOUT(FVoxelDistanceFieldBaseCS)
IMPLEMENT_SHADER_TYPE(, FVoxelJumpFloodCS, TEXT("/Plugin/Voxel/Private/DistanceField.usf"), TEXT("ExpandDistanceField"), SF_Compute);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDistanceFieldShaderHelper::WaitForCompletion() const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());
	Fence.Wait();
}

void FVoxelDistanceFieldShaderHelper::StartCompute(const FIntVector& Size, const TVoxelSharedRef<TArray<FVector3f>>& InOutData, int32 MaxPasses_Debug)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());
	
	check(InOutData->Num() == Size.X * Size.Y * Size.Z);
	check(Size.X > 0 && Size.Y > 0 && Size.Z > 0);
	
	ensure(Fence.IsFenceComplete());
	
	ENQUEUE_RENDER_COMMAND(VoxelDistanceFieldCompute)(
		MakeWeakPtrLambda(this, [= UE_504_ONLY(, this)](FRHICommandListImmediate& RHICmdList)
		{
			Compute_RenderThread(RHICmdList, Size, GetData(*InOutData), GetNum(*InOutData), MaxPasses_Debug);
		}));
	
	Fence.BeginFence();
}

void FVoxelDistanceFieldShaderHelper::Compute_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	const FIntVector& Size,
	FVector3f* RESTRICT const Data,
	const int32 Num,
	int32 MaxPasses_Debug)
{
	VOXEL_RENDER_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	check(Size.X > 0 && Size.Y > 0 && Size.Z > 0);
	check(Num == Size.X * Size.Y * Size.Z);
	
	if (AllocatedSize != Size)
	{
		VOXEL_RENDER_SCOPE_COUNTER("Create Buffers");
		
		AllocatedSize = Size;
		
		SrcBuffer.Initialize(UE_503_ONLY(RHICmdList, )TEXT("DEBUG"), sizeof(float), 3 * Num, PF_R32_FLOAT);
		DstBuffer.Initialize(UE_503_ONLY(RHICmdList, )TEXT("DEBUG"), sizeof(float), 3 * Num, PF_R32_FLOAT);
	}
	
	{
		VOXEL_RENDER_SCOPE_COUNTER("Copy Data To Buffers");
		void* BufferData = RHICmdList.LockBuffer(SrcBuffer.Buffer, 0, SrcBuffer.NumBytes, EResourceLockMode::RLM_WriteOnly);
		FMemory::Memcpy(BufferData, Data, SrcBuffer.NumBytes);
		RHICmdList.UnlockBuffer(SrcBuffer.Buffer);
	}

	const int32 PowerOfTwo = FMath::CeilLogTwo(Size.GetMax());
	for (int32 Pass = 0; Pass < PowerOfTwo; Pass++)
	{
		if (MaxPasses_Debug == Pass)
		{
			break;
		}
	
		// -1: we want to start with half the size
		const int32 Step = 1 << (PowerOfTwo - 1 - Pass);
		ApplyComputeShader<FVoxelJumpFloodCS>(RHICmdList, Size, Step);
	}

	// The final output is stored in SrcBuffer after the last swap.
	RHICmdList.Transition(
		FRHITransitionInfo(
			SrcBuffer.UAV,
			ERHIAccess::UAVCompute,
			ERHIAccess::CopySrc));
	{
		VOXEL_RENDER_SCOPE_COUNTER("Copy Data From Buffers");
		void* BufferData = RHICmdList.LockBuffer(SrcBuffer.Buffer, 0, SrcBuffer.NumBytes, EResourceLockMode::RLM_ReadOnly);
		FMemory::Memcpy(Data, BufferData, SrcBuffer.NumBytes);
		RHICmdList.UnlockBuffer(SrcBuffer.Buffer);
	}

	// Make sure to release the buffers, else will crash on DX12!
	SrcBuffer.Release();
	DstBuffer.Release();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<typename T>
void FVoxelDistanceFieldShaderHelper::ApplyComputeShader(
	FRHICommandListImmediate& RHICmdList,
	const FIntVector& Size,
	int32 Step)
{
	check(IsInRenderingThread());

	const TShaderMapRef<T> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRHIComputeShader* const ShaderRHI = ComputeShader.GetComputeShader();

	SetComputePipelineState(RHICmdList, ShaderRHI);

	FVoxelDistanceFieldParameters Parameters;
	Parameters.SizeX = Size.X;
	Parameters.SizeY = Size.Y;
	Parameters.SizeZ = Size.Z;
	Parameters.Step = Step;

	const FIntVector NumThreads =
		FVoxelUtilities::DivideCeil(Size, VOXEL_DISTANCE_FIELD_NUM_THREADS_CS);

	check(NumThreads.X > 0 && NumThreads.Y > 0 && NumThreads.Z > 0);

	// SrcBuffer는 직전 pass에서 UAV로 기록되었을 수 있다. 다음 dispatch에서
	// SRV로 읽기 전에 명시적으로 전환해 UAV write가 모든 GPU에서 보이도록 한다.
	RHICmdList.Transition(
		FRHITransitionInfo(
			SrcBuffer.UAV,
			ERHIAccess::UAVCompute,
			ERHIAccess::SRVCompute));

	RHICmdList.Transition(
		FRHITransitionInfo(
			DstBuffer.UAV,
			ERHIAccess::UAVCompute,
			ERHIAccess::UAVCompute));

	FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

	ComputeShader->SetUniformBuffers(BatchedParameters, Parameters);
	ComputeShader->SetBuffers(BatchedParameters, SrcBuffer, DstBuffer);

	RHICmdList.SetBatchedShaderParameters(ShaderRHI, BatchedParameters);
	RHICmdList.DispatchComputeShader(NumThreads.X, NumThreads.Y, NumThreads.Z);

	if (RHICmdList.NeedsShaderUnbinds())
	{
		FRHIBatchedShaderUnbinds& BatchedUnbinds = RHICmdList.GetScratchShaderUnbinds();

		ComputeShader->UnsetBuffers(BatchedUnbinds);
		RHICmdList.SetBatchedShaderUnbinds(ShaderRHI, BatchedUnbinds);
	}

	Swap(SrcBuffer, DstBuffer);
}

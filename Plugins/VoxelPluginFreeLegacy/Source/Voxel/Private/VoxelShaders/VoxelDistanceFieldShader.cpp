// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelShaders/VoxelDistanceFieldShader.h"
#include "VoxelUtilities/VoxelIntVectorUtilities.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

void FVoxelJumpFloodCS::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("NUM_THREADS_CS"), VOXEL_DISTANCE_FIELD_NUM_THREADS_CS);
}

IMPLEMENT_GLOBAL_SHADER(
	FVoxelJumpFloodCS,
	"/Plugin/Voxel/Private/DistanceField.usf",
	"ExpandDistanceField",
	SF_Compute);

void FVoxelDistanceFieldShaderHelper::WaitForCompletion() const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());
	Fence.Wait();
}

void FVoxelDistanceFieldShaderHelper::StartCompute(
	const FIntVector& Size,
	const TVoxelSharedRef<TArray<FVector3f>>& InOutData,
	int32 MaxPasses_Debug)
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

	const uint32 NumFloatElements = 3u * static_cast<uint32>(Num);
	const uint32 NumBytes = NumFloatElements * sizeof(float);
	check(NumBytes == static_cast<uint32>(Num) * sizeof(FVector3f));

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("Voxel JumpFlood"));

	FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(float), NumFloatElements);
	BufferDesc.Usage |= EBufferUsageFlags::SourceCopy;

	FRDGBufferRef SrcBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("Voxel.JumpFlood.Src"));
	FRDGBufferRef DstBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("Voxel.JumpFlood.Dst"));
	GraphBuilder.QueueBufferUpload(SrcBuffer, Data, NumBytes);

	const int32 PowerOfTwo = FMath::CeilLogTwo(Size.GetMax());
	for (int32 Pass = 0; Pass < PowerOfTwo; Pass++)
	{
		if (MaxPasses_Debug == Pass)
		{
			break;
		}

		const int32 Step = 1 << (PowerOfTwo - 1 - Pass);

		FVoxelJumpFloodCS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FVoxelJumpFloodCS::FParameters>();
		PassParameters->SizeX = Size.X;
		PassParameters->SizeY = Size.Y;
		PassParameters->SizeZ = Size.Z;
		PassParameters->Step = Step;
		PassParameters->Src = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(SrcBuffer, PF_R32_FLOAT));
		PassParameters->Dst = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(DstBuffer, PF_R32_FLOAT));

		const TShaderMapRef<FVoxelJumpFloodCS> ComputeShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		const FIntVector GroupCount =
			FVoxelUtilities::DivideCeil(Size, VOXEL_DISTANCE_FIELD_NUM_THREADS_CS);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Voxel JumpFlood Step=%d", Step),
			ComputeShader,
			PassParameters,
			GroupCount);

		Swap(SrcBuffer, DstBuffer);
	}

	FRHIGPUBufferReadback Readback(TEXT("Voxel.JumpFlood.Readback"));
	AddEnqueueCopyPass(GraphBuilder, &Readback, SrcBuffer, NumBytes);
	GraphBuilder.Execute();

	// JumpFlood is synchronous today. Wait for this copy fence and read the
	// staging buffer instead of directly locking a GPU source buffer. Submit
	// the recorded copy first so waiting on its fence cannot deadlock.
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	Readback.Wait(RHICmdList, FRHIGPUMask::All());
	void* const BufferData = Readback.Lock(NumBytes);
	check(BufferData);
	FMemory::Memcpy(Data, BufferData, NumBytes);
	Readback.Unlock();
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelJumpFloodRDGTest,
	"Voxel.DistanceField.JumpFloodRDG",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelJumpFloodRDGTest::RunTest(const FString& Parameters)
{
	const FIntVector Size(4, 4, 4);
	const int32 Num = Size.X * Size.Y * Size.Z;
	const FVector3f InvalidPosition(1.e9f, 1.e9f, 1.e9f);

	const TVoxelSharedRef<TArray<FVector3f>> Data = MakeVoxelShared<TArray<FVector3f>>();
	Data->Init(InvalidPosition, Num);
	(*Data)[0] = FVector3f::ZeroVector;

	const TVoxelSharedRef<FVoxelDistanceFieldShaderHelper> Helper =
		MakeVoxelShared<FVoxelDistanceFieldShaderHelper>();
	Helper->StartCompute(Size, Data);
	Helper->WaitForCompletion();

	for (int32 Index = 0; Index < Data->Num(); Index++)
	{
		if (!(*Data)[Index].Equals(FVector3f::ZeroVector))
		{
			AddError(FString::Printf(
				TEXT("JumpFlood output mismatch at index %d: %s"),
				Index,
				*(*Data)[Index].ToString()));
			return false;
		}
	}

	return true;
}
#endif

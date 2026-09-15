#include "DRSnowAddComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "EngineUtils.h"
#include "VoxelWorld.h"

namespace
{
int64 BuildStaticMeshSupportMask(const FHitResult& HitResult, const float Radius, const float VoxelSize)
{
	UStaticMeshComponent* HitMesh = Cast<UStaticMeshComponent>(HitResult.GetComponent());
	const FVector Normal = HitResult.ImpactNormal.GetSafeNormal();
	if (!IsValid(HitMesh) || !HitResult.bBlockingHit || Radius <= 0.f || Normal.IsNearlyZero())
	{
		return 0;
	}

	FVector Tangent;
	FVector Bitangent;
	DRSnowVirtualSurfaceSupport::BuildBasis(Normal, Tangent, Bitangent);
	if (Tangent.IsNearlyZero() || Bitangent.IsNearlyZero())
	{
		return 0;
	}

	constexpr int32 Resolution = DRSnowVirtualSurfaceSupport::Resolution;
	const float SampleStep = (Radius * 2.f) / static_cast<float>(Resolution - 1);
	// Short component-local complex traces clip to the hit mesh silhouette without
	// accidentally accepting nearby actors or the fallback VoxelWorld.
	const float TraceHalfDepth = FMath::Max(10.f, VoxelSize * 2.f);
	constexpr float MinAlignedNormalDot = 0.75f;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	uint64 Mask = 0;
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		const float PlaneY = -Radius + SampleStep * Y;
		for (int32 X = 0; X < Resolution; ++X)
		{
			const float PlaneX = -Radius + SampleStep * X;

			const FVector SamplePoint =
				HitResult.ImpactPoint +
				Tangent * PlaneX +
				Bitangent * PlaneY;
			FHitResult SupportHit;
			if (!HitMesh->LineTraceComponent(
				SupportHit,
				SamplePoint + Normal * TraceHalfDepth,
				SamplePoint - Normal * TraceHalfDepth,
				QueryParams))
			{
				continue;
			}

			const FVector SupportNormal = SupportHit.ImpactNormal.GetSafeNormal();
			if (SupportNormal.IsNearlyZero() || FVector::DotProduct(SupportNormal, Normal) < MinAlignedNormalDot)
			{
				continue;
			}

			Mask |= uint64(1) << DRSnowVirtualSurfaceSupport::GetBitIndex(X, Y);
		}
	}

	// The original blocking hit is authoritative for the center sample.
	const int32 Center = Resolution / 2;
	Mask |= uint64(1) << DRSnowVirtualSurfaceSupport::GetBitIndex(Center, Center);
	return static_cast<int64>(Mask);
}

FDRSnowAddOperation MakeSnowAddOperation(const FDRSnowSurfaceAddRequest& Request)
{
	FDRSnowAddOperation Operation;
	Operation.WorldLocation = Request.WorldLocation;
	Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
	Operation.ImpactDirection = Request.ImpactDirection.GetSafeNormal();
	Operation.Radius = Request.Radius;
	Operation.Amount = Request.Amount;
	Operation.BoxExtent = Request.BoxExtent;
	Operation.BoxRotation = Request.BoxRotation;
	Operation.EditTool = Request.EditTool;
	Operation.bAllowVirtualSurfaceFallback = Request.bAllowVirtualSurfaceFallback;
	Operation.bUseVirtualSurface = Request.bUseVirtualSurface;
	Operation.VirtualSurfaceSupportMask = Request.VirtualSurfaceSupportMask;
	Operation.TeamId = Request.Context.TeamId;
	Operation.VoxelWorldName = IsValid(Request.TargetVoxelWorld.Get())
		? Request.TargetVoxelWorld->GetFName()
		: NAME_None;
	return Operation;
}
}

bool UDRSnowAddComponent::TryAddSnowFromHit(
	const FHitResult& HitResult)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryAddSnowFromHit(HitResult);
		return true;
	}

	if (!HitResult.bBlockingHit)
	{
		return false;
	}

	FDRSnowSurfaceAddRequest Request = MakeAddRequest(HitResult.ImpactPoint, HitResult.ImpactNormal);
	Request.TargetVoxelWorld = GetVoxelWorldFromHit(HitResult);
	if (!IsValid(Request.TargetVoxelWorld.Get()))
	{
		Request.TargetVoxelWorld = ResolveFallbackVoxelWorld();
		Request.bUseVirtualSurface = Request.bAllowVirtualSurfaceFallback;
		if (Request.bUseVirtualSurface &&
			Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool &&
			IsValid(Request.TargetVoxelWorld.Get()))
		{
			Request.VirtualSurfaceSupportMask = BuildStaticMeshSupportMask(
				HitResult, Request.Radius, Request.TargetVoxelWorld->VoxelSize);
		}
	}

	return ExecuteAddRequest(Request, GetInteractableActorFromHit(HitResult));
}

bool UDRSnowAddComponent::TryAddSnowAtLocation(FVector WorldLocation, FVector SurfaceNormal)
{
	return TryAddSnow(MakeAddRequest(WorldLocation, SurfaceNormal));
}

bool UDRSnowAddComponent::TryAddSnow(const FDRSnowSurfaceAddRequest& Request)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) && !Owner->HasAuthority())
	{
		ServerTryAddSnow(Request);
		return true;
	}

	return ExecuteAddRequest(Request);
}

void UDRSnowAddComponent::SetAddSettings(float InAddRadius, float InAddAmount)
{
	AddRadius = FMath::Max(0.f, InAddRadius); 
	AddAmount = FMath::Max(0.f, InAddAmount);
}
void UDRSnowAddComponent::SetAddEditTool(EDRSnowVoxelEditTool InEditTool)
{
	AddEditTool = InEditTool;
}

void UDRSnowAddComponent::SetAllowVirtualSurfaceFallback(bool bInAllowVirtualSurfaceFallback)
{
	bAllowVirtualSurfaceFallback = bInAllowVirtualSurfaceFallback;
}

bool UDRSnowAddComponent::ExecuteAddRequest(
	const FDRSnowSurfaceAddRequest& Request,
	AActor* FallbackTarget)
{
	bool bHandled = false;
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		if (UDRSnowSubsystem* SnowSubsystem = World->GetSubsystem<UDRSnowSubsystem>())
		{
			const FDRSnowAddOperation Operation = MakeSnowAddOperation(Request);
			const FDRSnowAddResult SnowResult = SnowSubsystem->AddSnow(Request);
			bHandled = SnowResult.AddedAmount > 0.f;
			if (bHandled)
			{
				if (ADRMiningGameStateBase* MiningGameState =
					World->GetGameState<ADRMiningGameStateBase>())
				{
					MiningGameState->RegisterSnowAdd(Operation, SnowResult.AddedAmount);
				}
			}
		}
	}

	if (!bHandled &&
		IsValid(FallbackTarget) &&
		FallbackTarget->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowAdd(FallbackTarget, Request))
	{
		IDRSnowInteractableInterface::Execute_ReceiveSnowAdded(FallbackTarget, Request);
		bHandled = true;
	}

	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

void UDRSnowAddComponent::ServerTryAddSnowFromHit_Implementation(const FHitResult& HitResult)
{
	TryAddSnowFromHit(HitResult);
}

void UDRSnowAddComponent::ServerTryAddSnow_Implementation(const FDRSnowSurfaceAddRequest& Request)
{
	ExecuteAddRequest(Request);
}

FDRSnowSurfaceAddRequest UDRSnowAddComponent::MakeAddRequest(FVector WorldLocation, FVector SurfaceNormal)
{
	FDRSnowSurfaceAddRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
	Request.ImpactDirection = -Request.SurfaceNormal;
	Request.Radius = AddRadius;
	Request.Amount = AddAmount;
	Request.EditTool = AddEditTool;
	Request.bAllowVirtualSurfaceFallback = bAllowVirtualSurfaceFallback;
	Request.Context = MakeInteractionContext();
	return Request;
}

AVoxelWorld* UDRSnowAddComponent::GetVoxelWorldFromHit(const FHitResult& HitResult) const
{
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return IsValid(HitComponent) ? Cast<AVoxelWorld>(HitComponent->GetOwner()) : nullptr;
}

AVoxelWorld* UDRSnowAddComponent::ResolveFallbackVoxelWorld() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}

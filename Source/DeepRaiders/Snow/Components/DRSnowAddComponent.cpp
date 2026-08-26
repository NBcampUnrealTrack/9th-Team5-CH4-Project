#include "DRSnowAddComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "EngineUtils.h"
#include "VoxelWorld.h"

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
	}
	if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
	{
		// 투사체 충돌 정보로 편집 영역을 만들며 표면 재탐색을 생략한다.
		Request.bUseVirtualSurface = true;
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
			bHandled = SnowSubsystem->AddSnow(Request).AddedAmount > 0.f;
		}

		if (bHandled)
		{
			if (ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>())
			{
				FDRSnowAddOperation Operation;
				Operation.WorldLocation = Request.WorldLocation;
				Operation.SurfaceNormal = Request.SurfaceNormal.GetSafeNormal();
				Operation.ImpactDirection = Request.ImpactDirection.GetSafeNormal();
				Operation.Radius = Request.Radius;
				Operation.Amount = Request.Amount;
				Operation.EditTool = Request.EditTool;
				Operation.bAllowVirtualSurfaceFallback = Request.bAllowVirtualSurfaceFallback;
				Operation.bUseVirtualSurface = Request.bUseVirtualSurface;
				Operation.TeamId = Request.Context.TeamId;
				Operation.VoxelWorldName = IsValid(Request.TargetVoxelWorld.Get())
					? Request.TargetVoxelWorld->GetFName()
					: NAME_None;
				MiningGameState->RegisterSnowAdd(Operation);
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

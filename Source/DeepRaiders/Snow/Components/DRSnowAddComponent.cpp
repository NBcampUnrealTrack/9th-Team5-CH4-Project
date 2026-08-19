#include "DRSnowAddComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DeepRaiders/Core/Interface/DRSnowInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSurfaceSubsystem.h"
#include "DeepRaiders/Core/Subsystem/DRSnowVolumeSubsystem.h"
#include "DeepRaiders/Voxel/DRVoxelTeamColorLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "VoxelWorld.h"

bool UDRSnowAddComponent::TryAddSnowFromHit(
	const FHitResult& HitResult)
{
	if (!HitResult.bBlockingHit)
	{
		return false;
	}

	FDRSnowSurfaceAddRequest Request = MakeAddRequest(HitResult.ImpactPoint, HitResult.ImpactNormal);
	Request.TargetVoxelWorld = GetVoxelWorldFromHit(HitResult);

	// VoxelWorld를 맞춘 경우에는 중앙 snow pipeline으로 먼저 보낸다.
	bool bHandled = ExecuteAddSnow(Request);

	AActor* TargetActor = GetInteractableActorFromHit(HitResult);
	if (!bHandled &&
		IsValid(TargetActor) &&
		TargetActor->GetClass()->ImplementsInterface(UDRSnowInteractableInterface::StaticClass()) &&
		IDRSnowInteractableInterface::Execute_CanReceiveSnowAdd(TargetActor, Request))
	{
		IDRSnowInteractableInterface::Execute_ReceiveSnowAdded(TargetActor, Request);
		bHandled = true;
	}

	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

bool UDRSnowAddComponent::TryAddSnowAtLocation(
	FVector WorldLocation,
	FVector SurfaceNormal)
{
	const FDRSnowSurfaceAddRequest Request = MakeAddRequest(WorldLocation, SurfaceNormal);

	const bool bHandled = ExecuteAddSnow(Request);
	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

bool UDRSnowAddComponent::TryAddSnowAtLocationForTeam(
	FVector WorldLocation,
	FVector SurfaceNormal,
	int32 TeamId,
	AVoxelWorld* TargetVoxelWorld)
{
	FDRSnowSurfaceAddRequest Request = MakeAddRequest(WorldLocation, SurfaceNormal);
	// 디버그/투사체처럼 Owner로 팀을 해석하기 어려운 호출자는 여기서 명시값을 덮어쓴다.
	Request.Context.TeamId = TeamId;
	Request.TargetVoxelWorld = TargetVoxelWorld;

	const bool bHandled = ExecuteAddSnow(Request);
	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

bool UDRSnowAddComponent::TryAddSnowImpactAtLocationForTeam(
	FVector WorldLocation,
	FVector SurfaceNormal,
	FVector ImpactDirection,
	int32 TeamId,
	AVoxelWorld* TargetVoxelWorld)
{
	FDRSnowSurfaceAddRequest Request = MakeAddRequest(WorldLocation, SurfaceNormal);
	Request.ImpactDirection = ImpactDirection.IsNearlyZero()
		? -Request.SurfaceNormal
		: ImpactDirection.GetSafeNormal();
	Request.Context.TeamId = TeamId;
	Request.TargetVoxelWorld = TargetVoxelWorld;

	const bool bHandled = ExecuteAddSnow(Request);
	OnSnowAdded.Broadcast(Request, bHandled);
	return bHandled;
}

void UDRSnowAddComponent::SetAddSettings(
	float InAddRadius,
	float InAddAmount)
{
	AddRadius = FMath::Max(0.f, InAddRadius);
	AddAmount = FMath::Max(0.f, InAddAmount);
}

void UDRSnowAddComponent::SetAddEditTool(EDRSnowVoxelEditTool InEditTool)
{
	AddEditTool = InEditTool;
}

bool UDRSnowAddComponent::DebugTryAddSnowFromView(
	float TraceDistance,
	int32 TeamId,
	AVoxelWorld* TargetVoxelWorld)
{
	FHitResult HitResult;
	return MakeDebugViewHit(TraceDistance, HitResult) &&
		DebugAddSnowFromHit(
			HitResult,
			TeamId,
			TargetVoxelWorld,
			AddEditTool);
}

bool UDRSnowAddComponent::DebugTryAddSnowFromViewWithTool(
	float TraceDistance,
	int32 TeamId,
	AVoxelWorld* TargetVoxelWorld,
	EDRSnowVoxelEditTool DebugEditTool)
{
	FHitResult HitResult;
	return MakeDebugViewHit(TraceDistance, HitResult) &&
		DebugAddSnowFromHit(
			HitResult,
			TeamId,
			TargetVoxelWorld,
			DebugEditTool);
}

bool UDRSnowAddComponent::MakeDebugViewHit(
	float TraceDistance,
	FHitResult& OutHitResult) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Owner) || !IsValid(World) || TraceDistance <= 0.f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Snow][DebugAdd] Invalid debug add request. Owner=%s World=%s TraceDistance=%.1f"),
			*GetNameSafe(Owner),
			*GetNameSafe(World),
			TraceDistance);
		return false;
	}

	FVector ViewLocation = Owner->GetActorLocation();
	FRotator ViewRotation = Owner->GetActorRotation();

	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		if (const AController* Controller = OwnerPawn->GetController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}

	const FVector TraceEnd =
		ViewLocation + ViewRotation.Vector() * TraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DebugAddSnowFromView), false);
	QueryParams.AddIgnoredActor(Owner);

	if (!World->LineTraceSingleByChannel(
		OutHitResult,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams))
	{
		const FString DebugMessage = FString::Printf(
			TEXT("[Snow][DebugAdd] TraceMiss Start=%s End=%s"),
			*ViewLocation.ToCompactString(),
			*TraceEnd.ToCompactString());

		UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMessage);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.f,
				FColor::Red,
				DebugMessage);
		}
		return false;
	}

	return true;
}

bool UDRSnowAddComponent::DebugAddSnowFromHit(
	const FHitResult& HitResult,
	int32 TeamId,
	AVoxelWorld* TargetVoxelWorld,
	EDRSnowVoxelEditTool DebugEditTool)
{
	FDRSnowSurfaceAddRequest Request = MakeAddRequest(HitResult.ImpactPoint, HitResult.ImpactNormal);
	if (!HitResult.TraceStart.Equals(HitResult.TraceEnd))
	{
		Request.ImpactDirection = (HitResult.TraceEnd - HitResult.TraceStart).GetSafeNormal();
	}
	Request.Context.TeamId = TeamId;
	Request.TargetVoxelWorld = IsValid(TargetVoxelWorld)
		? TargetVoxelWorld
		: GetVoxelWorldFromHit(HitResult);
	Request.EditTool = DebugEditTool;

	const bool bHandled = ExecuteAddSnow(Request);
	OnSnowAdded.Broadcast(Request, bHandled);

	// 이 색은 debug message용이다. 실제 지형 material index는 DRVoxelTeamColorLibrary가 결정한다.
	const FColor DebugColor = FColor::Green;

	const FString DebugMessage = FString::Printf(
		TEXT("[Snow][DebugAdd] Handled=%d TeamId=%d EditTool=%s Location=%s Radius=%.1f Amount=%.2f Color=%s HitActor=%s"),
		bHandled,
		TeamId,
		*StaticEnum<EDRSnowVoxelEditTool>()->GetNameStringByValue(static_cast<int64>(Request.EditTool)),
		*HitResult.ImpactPoint.ToCompactString(),
		AddRadius,
		AddAmount,
		*DebugColor.ToString(),
		*GetNameSafe(HitResult.GetActor()));

	UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMessage);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.f,
			bHandled ? DebugColor : FColor::Red,
			DebugMessage);
	}

	return bHandled;
}

bool UDRSnowAddComponent::ExecuteAddSnow(
	const FDRSnowSurfaceAddRequest& Request)
{
	bool bHandled = false;
	if (UWorld* World = GetWorld())
	{
		if (Request.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
		{
			if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
			{
				// Custom 계열 툴은 실제 생성 voxel 기준으로 SnowVolume을 기록하므로 SurfaceSubsystem이 먼저 처리한다.
				bHandled = SnowSurfaceSubsystem->AddSnowAtArea(Request) > 0.f;
			}

			return bHandled;
		}

		if (UDRSnowVolumeSubsystem* SnowVolumeSubsystem = World->GetSubsystem<UDRSnowVolumeSubsystem>())
		{
			// SnowVolume은 팀별 누적량의 원본 데이터다. Voxel은 이 결과를 보여주는 표현 계층이다.
			const FDRSnowAddResult AddResult = SnowVolumeSubsystem->AddSnow(Request);
			bHandled = AddResult.AddedAmount > 0.f;
		}

		if (bHandled)
		{
			if (UDRSnowSurfaceSubsystem* SnowSurfaceSubsystem = World->GetSubsystem<UDRSnowSurfaceSubsystem>())
			{
				// 표면 SDF와 팀 material index를 함께 갱신한다.
				SnowSurfaceSubsystem->AddSnowAtArea(Request);
			}
		}
	}

	return bHandled;
}

FDRSnowSurfaceAddRequest UDRSnowAddComponent::MakeAddRequest(
	FVector WorldLocation,
	FVector SurfaceNormal)
{
	FDRSnowSurfaceAddRequest Request;
	Request.WorldLocation = WorldLocation;
	Request.SurfaceNormal =
		SurfaceNormal.IsNearlyZero()
			? FVector::UpVector
			: SurfaceNormal.GetSafeNormal();
	Request.ImpactDirection = -Request.SurfaceNormal;
	Request.Radius = AddRadius;
	Request.Amount = AddAmount;
	Request.EditTool = AddEditTool;
	Request.Context = MakeInteractionContext();
	return Request;
}

AVoxelWorld* UDRSnowAddComponent::GetVoxelWorldFromHit(
	const FHitResult& HitResult) const
{
	if (AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(HitResult.GetActor()))
	{
		return VoxelWorld;
	}

	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return IsValid(HitComponent)
		? Cast<AVoxelWorld>(HitComponent->GetOwner())
		: nullptr;
}

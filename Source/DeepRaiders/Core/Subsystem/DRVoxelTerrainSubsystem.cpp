// Fill out your copyright notice in the Description page of Project Settings.


#include "DRVoxelTerrainSubsystem.h"

#include "EngineUtils.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelWorld.h"

bool UDRVoxelTerrainSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return false;
	}
	return true;
	// return World->GetMapName().Contains(TEXT("Test_Voxel_Map"));
}

void UDRVoxelTerrainSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	if (IsValid(VoxelWorld) && !VoxelWorld->IsCreated())
	{
		// Voxel data 생성 직후에 pending dig를 적용하면 LOD 전체 로딩을 기다릴 필요가 없다.
		BindVoxelWorldGenerated(VoxelWorld);
	}
	else if (!IsValid(VoxelWorld))
	{
		// 클라 입장 직후에는 레벨 actor 복제가 아직 끝나지 않았을 수 있다.
		BindVoxelWorldSpawned();
	}
}

void UDRVoxelTerrainSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (IsValid(World) && VoxelWorldSpawnedDelegateHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(VoxelWorldSpawnedDelegateHandle);
	}

	Super::Deinitialize();
}

bool UDRVoxelTerrainSubsystem::RequestDig(
	AVoxelWorld* TargetVoxelWorld,
	const FVector& Location,
	float Radius,
	FDRTerrainDigOperation* OutOperation)
{
	if (!IsValid(TargetVoxelWorld) || Radius <= 0.f)
	{
		return false;
	}

	FDRTerrainDigOperation Operation;
	Operation.OperationId = DigHistory.Num() + 1;
	Operation.Location = Location;
	Operation.Radius = Radius;
	Operation.VoxelWorld = TargetVoxelWorld;
	// Actor reference가 클라에서 아직 유효하지 않을 경우를 대비해 이름도 함께 보낸다.
	Operation.VoxelWorldName = TargetVoxelWorld->GetFName();

	if (!ApplyDig(Operation))
	{
		return false;
	}

	DigHistory.Add(Operation);
	if (OutOperation)
	{
		*OutOperation = Operation;
	}
	return true;
}

bool UDRVoxelTerrainSubsystem::RequestDigAtLocation(
	const FVector& Location,
	float Radius,
	FDRTerrainDigOperation* OutOperation)
{
	return RequestDig(ResolveVoxelWorld(), Location, Radius, OutOperation);
}

bool UDRVoxelTerrainSubsystem::ApplyDig(const FDRTerrainDigOperation& Operation)
{
	if (Operation.OperationId > 0 &&
		AppliedDigOperationIds.Contains(Operation.OperationId))
	{
		// 히스토리 RPC와 multicast가 겹쳐 들어와도 같은 dig는 한 번만 적용한다.
		return true;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorldForOperation(Operation);
	if (!IsValid(VoxelWorld))
	{
		return false;
	}

	if (!VoxelWorld->IsCreated())
	{
		// IsLoaded나 전체 LOD 완료가 아니라 voxel data 생성 여부만 적용 조건으로 본다.
		return false;
	}

	UVoxelSphereTools::RemoveSphere(
		VoxelWorld,
		Operation.Location,
		Operation.Radius,
		nullptr,
		nullptr,
		true,
		true,
		true);

	if (Operation.OperationId > 0)
	{
		AppliedDigOperationIds.Add(Operation.OperationId);
	}

	// 파인 땅 위치 브로드캐스트
	// OrePoolingSubsystem이 땅이 패인 위치를 통한 깊이별 풀링 작업을 진행함
	UWorld* World = GetWorld();
	if (IsValid(World) && World->GetNetMode() != NM_Client)
	{
		OnTerrainDug.Broadcast(Operation.Location, Operation.Radius);
	}

	return true;
}

bool UDRVoxelTerrainSubsystem::ApplyOrQueueDig(
	const FDRTerrainDigOperation& Operation)
{
	if (ApplyDig(Operation))
	{
		return true;
	}

	QueuePendingDig(Operation);
	return false;
}

void UDRVoxelTerrainSubsystem::HandleVoxelWorldGenerated()
{
	// OnGenerateWorld는 VoxelWorld 생성 직후 호출되므로 pending 지형 변경을 적용하기 충분하다.
	TryApplyPendingDigs();
}

void UDRVoxelTerrainSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(SpawnedActor);
	if (!IsValid(VoxelWorld))
	{
		return;
	}

	CachedVoxelWorld = VoxelWorld;
	if (VoxelWorld->IsCreated())
	{
		// 이미 생성된 actor가 늦게 잡힌 경우 바로 pending을 소진한다.
		TryApplyPendingDigs();
	}
	else
	{
		BindVoxelWorldGenerated(VoxelWorld);
	}
}

AVoxelWorld* UDRVoxelTerrainSubsystem::ResolveVoxelWorld()
{
	if (IsValid(CachedVoxelWorld.Get()))
	{
		return CachedVoxelWorld.Get();
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			CachedVoxelWorld = *It;
			return CachedVoxelWorld.Get();
		}
	}

	return nullptr;
}

AVoxelWorld* UDRVoxelTerrainSubsystem::ResolveVoxelWorldByName(FName VoxelWorldName)
{
	if (VoxelWorldName.IsNone())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		AVoxelWorld* VoxelWorld = *It;
		if (IsValid(VoxelWorld) && VoxelWorld->GetFName() == VoxelWorldName)
		{
			CachedVoxelWorld = VoxelWorld;
			return VoxelWorld;
		}
	}

	return nullptr;
}

AVoxelWorld* UDRVoxelTerrainSubsystem::ResolveVoxelWorldForOperation(
	const FDRTerrainDigOperation& Operation)
{
	AVoxelWorld* VoxelWorld = Operation.VoxelWorld;
	if (!IsValid(VoxelWorld))
	{
		VoxelWorld = ResolveVoxelWorldByName(Operation.VoxelWorldName);
	}

	if (!IsValid(VoxelWorld))
	{
		VoxelWorld = ResolveVoxelWorld();
	}

	return VoxelWorld;
}

void UDRVoxelTerrainSubsystem::QueuePendingDig(
	const FDRTerrainDigOperation& Operation)
{
	if (Operation.OperationId > 0 && HasPendingDig(Operation.OperationId))
	{
		return;
	}

	PendingDigs.Add(Operation);

	AVoxelWorld* VoxelWorld = ResolveVoxelWorldForOperation(Operation);
	if (IsValid(VoxelWorld) && !VoxelWorld->IsCreated())
	{
		// actor는 있지만 voxel data가 아직 없으면 생성 완료 delegate만 기다린다.
		BindVoxelWorldGenerated(VoxelWorld);
	}
	else if (!IsValid(VoxelWorld))
	{
		// actor 자체가 아직 없으면 spawn delegate를 통해 한 번 더 기회를 받는다.
		BindVoxelWorldSpawned();
	}
}

void UDRVoxelTerrainSubsystem::TryApplyPendingDigs()
{
	for (int32 Index = PendingDigs.Num() - 1; Index >= 0; --Index)
	{
		if (ApplyDig(PendingDigs[Index]))
		{
			PendingDigs.RemoveAtSwap(Index);
		}
	}
}

void UDRVoxelTerrainSubsystem::BindVoxelWorldGenerated(AVoxelWorld* VoxelWorld)
{
	if (!IsValid(VoxelWorld) || BoundVoxelWorlds.Contains(VoxelWorld))
	{
		return;
	}

	VoxelWorld->OnGenerateWorld.AddUniqueDynamic(
		this,
		&ThisClass::HandleVoxelWorldGenerated);
	BoundVoxelWorlds.Add(VoxelWorld);
}

void UDRVoxelTerrainSubsystem::BindVoxelWorldSpawned()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || VoxelWorldSpawnedDelegateHandle.IsValid())
	{
		return;
	}

	VoxelWorldSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(
			this,
			&ThisClass::HandleActorSpawned));
}

bool UDRVoxelTerrainSubsystem::HasPendingDig(int32 OperationId) const
{
	for (const FDRTerrainDigOperation& Operation : PendingDigs)
	{
		if (Operation.OperationId == OperationId)
		{
			return true;
		}
	}

	return false;
}

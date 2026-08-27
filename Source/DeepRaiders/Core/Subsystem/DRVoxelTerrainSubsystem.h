// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRVoxelTerrainSubsystem.generated.h"

class AVoxelWorld;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDRTerrainDugDelegate, const FVector&, float);

USTRUCT(BlueprintType)
struct FDRTerrainDigOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Voxel Terrain")
	int32 OperationId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Voxel Terrain")
	FVector_NetQuantize Location;

	UPROPERTY(BlueprintReadOnly, Category = "Voxel Terrain")
	float Radius = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Voxel Terrain")
	TObjectPtr<AVoxelWorld> VoxelWorld = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Voxel Terrain")
	FName VoxelWorldName = NAME_None;
};

UCLASS()
class DEEPRAIDERS_API UDRVoxelTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// 서버에서 실제 지형 제거가 완료된 위치와 반경
	FDRTerrainDugDelegate OnTerrainDug;

	UPROPERTY(Transient)
	TObjectPtr<AVoxelWorld> CachedVoxelWorld = nullptr;

	UPROPERTY(Transient)
	TArray<FDRTerrainDigOperation> DigHistory;

	UPROPERTY(Transient)
	TSet<int32> AppliedDigOperationIds;

	// 서버 권한으로 지형 변경을 확정하고 DigHistory에 원본 변경 이력을 저장한다.
	bool RequestDig(
		AVoxelWorld* TargetVoxelWorld,
		const FVector& Location,
		float Radius,
		FDRTerrainDigOperation* OutOperation = nullptr);

	bool RequestDigAtLocation(
		const FVector& Location,
		float Radius,
		FDRTerrainDigOperation* OutOperation = nullptr);

	// VoxelWorld가 이미 생성된 상태라면 즉시 지형 변경을 적용한다.
	bool ApplyDig(const FDRTerrainDigOperation& Operation);

	// VoxelWorld actor 또는 voxel data 생성이 아직 끝나지 않았으면 pending으로 보관한다.
	bool ApplyOrQueueDig(const FDRTerrainDigOperation& Operation);
	const TArray<FDRTerrainDigOperation>& GetDigHistory() const { return DigHistory; }
	void ResetTerrainState();

private:
	UFUNCTION()
	void HandleVoxelWorldGenerated();

	void HandleActorSpawned(AActor* SpawnedActor);

	AVoxelWorld* ResolveVoxelWorld();
	AVoxelWorld* ResolveVoxelWorldByName(FName VoxelWorldName);
	AVoxelWorld* ResolveVoxelWorldForOperation(const FDRTerrainDigOperation& Operation);
	void QueuePendingDig(const FDRTerrainDigOperation& Operation);
	void TryApplyPendingDigs();

	// 전체 LOD/렌더 로딩이 아니라 VoxelWorld 생성 완료 시점만 기다린다.
	void BindVoxelWorldGenerated(AVoxelWorld* VoxelWorld);

	// 중도난입 클라에서 VoxelWorld actor 자체가 아직 없을 때만 actor spawn 이벤트를 기다린다.
	void BindVoxelWorldSpawned();
	bool HasPendingDig(int32 OperationId) const;

	UPROPERTY(Transient)
	TArray<FDRTerrainDigOperation> PendingDigs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AVoxelWorld>> BoundVoxelWorlds;

	FDelegateHandle VoxelWorldSpawnedDelegateHandle;
};

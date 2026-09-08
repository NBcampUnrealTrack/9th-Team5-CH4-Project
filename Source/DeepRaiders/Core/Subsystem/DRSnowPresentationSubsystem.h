#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRSnowPresentationSubsystem.generated.h"

class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UVoxelProceduralMeshComponent;
class AVoxelWorld;
struct FDRSnowAddOperation;

USTRUCT()
struct FDRSnowPreviousSurfaceSnapshot
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UVoxelProceduralMeshComponent> Component;

	double EndTime = 0.0;
};

UCLASS()
class DEEPRAIDERS_API UDRSnowPresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// MPC slot contract:
	// CenterRadius=(WorldPosition.xyz, Radius), NormalStartTime=(SurfaceNormal.xyz, StartTime),
	// Timing=(Duration, CollapseHeight, EdgeWidth, Strength).
	static constexpr int32 MaxTransitionSlots = 8;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void PresentSnowAdd(const FDRSnowAddOperation& Operation);
	void ResetPresentation();

private:
	bool ValidateParameterContract() const;
	UMaterialParameterCollectionInstance* GetCollectionInstance() const;
	int32 AcquireSlot(double CurrentTime);
	AVoxelWorld* ResolveVoxelWorld(FName VoxelWorldName) const;
	void CapturePreviousSurface(
		AVoxelWorld& VoxelWorld,
		const FDRSnowAddOperation& Operation,
		float Radius,
		float EdgeWidth,
		double EndTime);
	UVoxelProceduralMeshComponent* AcquireSnapshotComponent(
		AVoxelWorld& VoxelWorld,
		const UVoxelProceduralMeshComponent& SourceComponent,
		int32 MaximumSnapshotComponents);
	void CleanupExpiredSnapshots();
	void ScheduleSnapshotCleanup();
	void ReleaseSnapshot(int32 SnapshotIndex);
	void ResetPreviousSurfaceSnapshots(bool bDestroyComponents);
	static FName GetCenterRadiusParameterName(int32 SlotIndex);
	static FName GetNormalStartTimeParameterName(int32 SlotIndex);
	static FName GetTimingParameterName(int32 SlotIndex);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> ParameterCollection;

	TArray<double> SlotEndTimes;
	int32 NextSlotIndex = 0;
	bool bParameterContractValid = false;

	UPROPERTY(Transient)
	TArray<FDRSnowPreviousSurfaceSnapshot> ActivePreviousSurfaceSnapshots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelProceduralMeshComponent>> AvailablePreviousSurfaceComponents;

	FTimerHandle PreviousSurfaceCleanupTimer;
	bool bLoggedMissingPreviousSurfaceParameter = false;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/LootBox/DRLootTypes.h"
#include "DRLootDropComponent.generated.h"

class DRItemDefinition;
class UDRLootDropProfile;

UENUM(BlueprintType)
enum class EDRLootSpawnMode : uint8
{
	AllAtOnce UMETA(DisplayName = "All At Once"),
	Sequential UMETA(DisplayName = "Sequential")
};

DECLARE_MULTICAST_DELEGATE(FDRLootSpawnSequenceCompleted);

UCLASS(ClassGroup = (Loot), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRLootDropComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UDRLootDropComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/* 서버에서 Loot를 추첨하고 WorldItem으로 생성한다.
	 * 일괄 모드는 생성에 성공한 Actor 수, 순차 모드는 대기열에 등록된 ItemInstance 수를 반환한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	int32 GenerateAndSpawnLoot(EDRLootTier LootTier, const FTransform& SourceTransform);
	
	// 고정 시드 테스트용 C++ 전용 오버로드
	int32 GenerateAndSpawnLoot(EDRLootTier LootTier, const FTransform& SourceTransform, FRandomStream& RandomStream);

	/* 기존 ItemInstance의 InstanceId, Quantity, RuntimeState를 유지하여 WorldItem으로 생성한다. */
	int32 SpawnItemInstances(const TArray<FDRItemInstance>& ItemInstances, const FTransform& SourceTransform);

	// 고정 시드 테스트용 C++ 전용 오버로드
	int32 SpawnItemInstances(const TArray<FDRItemInstance>& ItemInstances, const FTransform& SourceTransform,
		FRandomStream& RandomStream);

	void SetSpawnMode(EDRLootSpawnMode NewSpawnMode)
	{
		SpawnMode = NewSpawnMode;
	}

	bool IsSpawnSequenceActive() const
	{
		return bSpawnSequenceActive;
	}

	FDRLootSpawnSequenceCompleted OnSpawnSequenceCompleted;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDRLootDropProfile> LootProfile = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpawnRadius = 100.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxSpawnRadius = 180.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Spawn", meta = (ClampMin = "0.0", Units = "deg"))
	float MaxSpawnAngleJitterDegrees = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Spawn")
	EDRLootSpawnMode SpawnMode = EDRLootSpawnMode::AllAtOnce;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Spawn|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float SpawnInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Spawn|Timing",
		meta = (ClampMin = "0.0", Units = "s", ToolTip = "SpawnInterval에 더하거나 뺄 무작위 시간의 최대값"))
	float SpawnIntervalRandomDeviation = 0.05f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Spawn", meta = (ClampMin = "1"))
	int32 MaxIterationCount = 50;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Spawn")
	TObjectPtr<UDRItemDefinition> DefaultItemDefinition = nullptr;
	
private:
	bool BuildValidLootPools(TArray<const FDRLootTableRow*>& OutAllRows,
		TMap<EDRItemRarity, TArray<const FDRLootTableRow*>>& OutRowsByRarity) const;
	
	bool RollLootQuantities(EDRLootTier LootTier, const FDRLootTierConfig& TierConfig,
		const TArray<const FDRLootTableRow*>& AllRows,
		const TMap<EDRItemRarity, TArray<const FDRLootTableRow*>>& RowsByRarity,
		FRandomStream& RandomStream, TMap<UDRItemDefinition*, int32>& OutGeneratedQuantities)const;
	
	int32 SpawnLootQuantities(const TMap<UDRItemDefinition*, int32>& GeneratedQuantities,
		const FTransform& SourceTransform,
		FRandomStream& RandomStream);

	int32 SpawnAllItems(const TArray<FDRItemInstance>& ItemInstances, const FTransform& SourceTransform,
		FRandomStream& RandomStream);
	void BuildSpawnTargetTransforms(int32 ItemCount, const FTransform& SourceTransform, FRandomStream& RandomStream,
		TArray<FTransform>& OutTargetTransforms) const;
	bool SpawnPreparedItem(const FDRItemInstance& ItemInstance, const FTransform& SourceTransform,
		const FTransform& TargetTransform) const;
	void SpawnNextSequenceItem();
	void FinishSpawnSequence();
	void ResetSpawnSequence();
	float GetNextSpawnDelay(FRandomStream& RandomStream) const;
	
	bool SelectRarity(const FDRLootTierConfig& TierConfig, FRandomStream& RandomStream, EDRItemRarity& OutRarity) const;
	
	const FDRLootTableRow* SelectWeightedLootRow(const TArray<const FDRLootTableRow*>& CandidateRows,
		FRandomStream& RandomStream) const;

	UPROPERTY(Transient)
	TArray<FDRItemInstance> PendingItemInstances;

	TArray<FTransform> PendingTargetTransforms;
	TArray<float> PendingSpawnDelays;
	FTransform PendingSourceTransform = FTransform::Identity;
	FTimerHandle SpawnTimerHandle;
	int32 PendingItemIndex = 0;
	bool bSpawnSequenceActive = false;
};

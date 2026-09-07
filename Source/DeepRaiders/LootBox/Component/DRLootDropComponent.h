#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/LootBox/DRLootTypes.h"
#include "DRLootDropComponent.generated.h"

class DRItemDefinition;
class UDRLootDropProfile;

UCLASS(ClassGroup = (Loot), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRLootDropComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UDRLootDropComponent();
	
	/* 서버에서 Loot를 추첨하고 WorldItem으로 생성한다.
	 * 반환값은 생성에 성공한 WorldItemActor 개수
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	int32 GenerateAndSpawnLoot(EDRLootTier LootTier, const FTransform& SourceTransform);
	
	// 고정 시드 테스트용 C++ 전용 오버로드
	int32 GenerateAndSpawnLoot(EDRLootTier LootTier, const FTransform& SourceTransform, FRandomStream& RandomStream);

	/* 기존 ItemInstance의 InstanceId, Quantity, RuntimeState를 유지하여 WorldItem으로 생성한다. */
	int32 SpawnItemInstances(const TArray<FDRItemInstance>& ItemInstances, const FTransform& SourceTransform) const;

	// 고정 시드 테스트용 C++ 전용 오버로드
	int32 SpawnItemInstances(const TArray<FDRItemInstance>& ItemInstances, const FTransform& SourceTransform,
		FRandomStream& RandomStream) const;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDRLootDropProfile> LootProfile = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpawnRadius = 100.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxSpawnRadius = 180.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Spawn", meta = (ClampMin = "0.0", Units = "deg"))
	float MaxSpawnAngleJitterDegrees = 10.f;
	
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
		FRandomStream& RandomStream) const;
	
	bool SelectRarity(const FDRLootTierConfig& TierConfig, FRandomStream& RandomStream, EDRItemRarity& OutRarity) const;
	
	const FDRLootTableRow* SelectWeightedLootRow(const TArray<const FDRLootTableRow*>& CandidateRows,
		FRandomStream& RandomStream) const;
};

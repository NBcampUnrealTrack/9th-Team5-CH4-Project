#include "DRLootDropComponent.h"

#include "DeepRaiders/Core/Subsystem/DRWorldItemSubsystem.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Item/DRWorldItemTypes.h"
#include "DeepRaiders/LootBox/Data/DRLootDropProfile.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

UDRLootDropComponent::UDRLootDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

int32 UDRLootDropComponent::GenerateAndSpawnLoot(EDRLootTier LootTier, const FTransform& SourceTransform)
{
	FRandomStream RandomStream(FMath::Rand());

	return GenerateAndSpawnLoot(LootTier, SourceTransform, RandomStream);
}

int32 UDRLootDropComponent::GenerateAndSpawnLoot(EDRLootTier LootTier, const FTransform& SourceTransform,
                                                 FRandomStream& RandomStream)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!IsValid(Owner)
		|| !Owner->HasAuthority()
		|| !IsValid(World))
	{
		return 0;
	}

	if (!IsValid(LootProfile))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: LootProfile is not configured."), *GetName());
		return 0;
	}

	const FDRLootTierConfig* TierConfig = LootProfile->TierConfigs.Find(LootTier);
	if (TierConfig == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: LootTier '%s' has no configuration in profile '%s'."),
		       *GetName(), *UEnum::GetValueAsString(LootTier), *GetNameSafe(LootProfile));

		return 0;
	}

	TArray<const FDRLootTableRow*> AllRows;
	TMap<EDRItemRarity, TArray<const FDRLootTableRow*>> RowsByRarity;

	if (!BuildValidLootPools(AllRows, RowsByRarity))
	{
		return 0;
	}

	TMap<UDRItemDefinition*, int32> GeneratedQuantities;

	if (!RollLootQuantities(LootTier, *TierConfig, AllRows, RowsByRarity, RandomStream, GeneratedQuantities))
	{
		return 0;
	}

	return SpawnLootQuantities(GeneratedQuantities, SourceTransform, RandomStream);
}

bool UDRLootDropComponent::BuildValidLootPools(TArray<const FDRLootTableRow*>& OutAllRows,
                                               TMap<EDRItemRarity, TArray<const FDRLootTableRow*>>& OutRowsByRarity)
const
{
	OutAllRows.Reset();
	OutRowsByRarity.Reset();

	if (!IsValid(LootProfile)
		|| !IsValid(LootProfile->LootTable))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: LootProfile '%s' has no LootTable."),
		       *GetName(), *GetNameSafe(LootProfile));

		return false;
	}

	UDataTable* LootTable = LootProfile->LootTable;
	const UScriptStruct* RowStruct = LootTable->GetRowStruct();

	if (!IsValid(RowStruct)
		|| !RowStruct->IsChildOf(FDRLootTableRow::StaticStruct()))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s]: LootTable '%s' does not use FDRLootTableRow."),
		       *GetName(), *GetNameSafe(LootTable));

		return false;
	}

	TArray<FDRLootTableRow*> TableRows;
	LootTable->GetAllRows<FDRLootTableRow>(TEXT("UDRLootDropComponent::BuildValidLootPools"), TableRows);

	if (TableRows.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: LootTable '%s' has no rows."),
		       *GetName(), *GetNameSafe(LootTable));

		return false;
	}

	OutAllRows.Reserve(TableRows.Num());

	int32 MissingDefinitionCount = 0;

	for (const FDRLootTableRow* Row : TableRows)
	{
		if (Row == nullptr || !IsValid(Row->ItemDefinition))
		{
			++MissingDefinitionCount;
			continue;
		}

		/*
		 * Weight가 0인 행은 오류가 아니라 비활성화된 후보로 취급한다.
		 */
		if (Row->SelectionWeight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		OutAllRows.Add(Row);

		OutRowsByRarity.FindOrAdd(Row->ItemDefinition->Rarity).Add(Row);
	}

	if (MissingDefinitionCount > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: LootTable '%s' contains %d rows without a valid ItemDefinition."),
		       *GetName(), *GetNameSafe(LootTable), MissingDefinitionCount);
	}

	if (OutAllRows.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: LootTable '%s' has no selectable rows with a positive weight."),
		       *GetName(), *GetNameSafe(LootTable));

		return false;
	}

	return true;
}

bool UDRLootDropComponent::RollLootQuantities(EDRLootTier LootTier, const FDRLootTierConfig& TierConfig,
                                              const TArray<const FDRLootTableRow*>& AllRows,
                                              const TMap<EDRItemRarity, TArray<const FDRLootTableRow*>>& RowsByRarity,
                                              FRandomStream& RandomStream,
                                              TMap<UDRItemDefinition*, int32>& OutGeneratedQuantities) const
{
	OutGeneratedQuantities.Reset();

	const int32 MinDropCount = FMath::Max(0, TierConfig.MinDropCount);
	const int32 MaxDropCount = FMath::Max(MinDropCount, TierConfig.MaxDropCount);
	const int32 DropCount = RandomStream.RandRange(MinDropCount, MaxDropCount);

	if (DropCount <= 0)
	{
		return true;
	}

	TSet<EDRItemRarity> LoggedMissingRarities;

	int IterationCount = 0;
	int CurrentDropCount = 0;
	while (CurrentDropCount < DropCount && IterationCount < MaxIterationCount)
	{
		++IterationCount;
		EDRItemRarity SelectedRarity;

		if (!SelectRarity(TierConfig, RandomStream, SelectedRarity))
		{
			/*
			 * TierConfig는 반복 중 바뀌지 않으므로 이후 Roll도 전부 실패한다.
			 */
			UE_LOG(LogTemp, Warning, TEXT("[%s]: LootTier '%s' has no positive rarity weight."),
				   *GetName(), *UEnum::GetValueAsString(LootTier));

			return false;
		}

		const TArray<const FDRLootTableRow*>* RarityRows = RowsByRarity.Find(SelectedRarity);
		const FDRLootTableRow* SelectedRow = RarityRows != nullptr
												 ? SelectWeightedLootRow(*RarityRows, RandomStream) : nullptr;

		if (SelectedRow == nullptr
			|| !IsValid(SelectedRow->ItemDefinition))
		{
			if (!LoggedMissingRarities.Contains(SelectedRarity))
			{
				LoggedMissingRarities.Add(SelectedRarity);
				UE_LOG(LogTemp, Warning, TEXT("[%s]: No selectable '%s' item. Falling back to all loot rows."),
					   *GetName(), *UEnum::GetValueAsString(SelectedRarity));
			}

			// 유효한 등급의 아이템이 등장하기를 반복한다.
			continue;
		}
		
		const int32 MinQuantity = FMath::Max(1, SelectedRow->MinQuantity);
		const int32 MaxQuantity = FMath::Max(MinQuantity, SelectedRow->MaxQuantity);
		const int32 RolledQuantity = RandomStream.RandRange(MinQuantity, MaxQuantity);

		++CurrentDropCount;
		OutGeneratedQuantities.FindOrAdd(SelectedRow->ItemDefinition) += RolledQuantity;
	}
	
	// 반드시 무언가 등장할 수 있도록 기본 아이템 등록
	if (IsValid(DefaultItemDefinition))
	{
		for (;CurrentDropCount < DropCount; ++CurrentDropCount)
		{
			OutGeneratedQuantities.FindOrAdd(DefaultItemDefinition) += 1;
		}	
	}

	return true;
}

int32 UDRLootDropComponent::SpawnLootQuantities(const TMap<UDRItemDefinition*, int32>& GeneratedQuantities,
                                                const FTransform& SourceTransform, FRandomStream& RandomStream) const
{
	if (GeneratedQuantities.IsEmpty())
	{
		return 0;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!IsValid(Owner)
		|| !Owner->HasAuthority()
		|| !IsValid(World))
	{
		return 0;
	}

	UDRWorldItemSubsystem* WorldItemSubsystem = World->GetSubsystem<UDRWorldItemSubsystem>();
	if (!IsValid(WorldItemSubsystem))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s]: WorldItemSubsystem is unavailable."), *GetName());
		return 0;
	}

	struct FSpawnEntry
	{
		UDRItemDefinition* Definition = nullptr;
		int32 Quantity = 0;
	};

	TArray<FSpawnEntry> SpawnEntries;

	for (const TPair<UDRItemDefinition*, int32>& Pair : GeneratedQuantities)
	{
		UDRItemDefinition* Definition = Pair.Key;
		int32 RemainingQuantity = Pair.Value;

		if (!IsValid(Definition)
			|| RemainingQuantity <= 0)
		{
			continue;
		}

		const int32 MaxStackSize = FMath::Max(1, Definition->MaxStackSize);

		while (RemainingQuantity > 0)
		{
			FSpawnEntry& Entry = SpawnEntries.AddDefaulted_GetRef();
			Entry.Definition = Definition;
			Entry.Quantity = FMath::Min(RemainingQuantity, MaxStackSize);

			RemainingQuantity -= Entry.Quantity;
		}
	}

	if (SpawnEntries.IsEmpty())
	{
		return 0;
	}

	// TMap 순회 순서에 따라 Seed 기반 테스트의 스폰 순서가 바뀌지 않게 한다.
	SpawnEntries.Sort([](const FSpawnEntry& Left, const FSpawnEntry& Right)
	{
		return Left.Definition->GetPathName() < Right.Definition->GetPathName();
	});

	const float SafeMinRadius = FMath::Max(0.f, MinSpawnRadius);
	const float SafeMaxRadius = FMath::Max(SafeMinRadius, MaxSpawnRadius);
	const float MaxJitterRadians = FMath::DegreesToRadians(FMath::Max(0.f, MaxSpawnAngleJitterDegrees));
	const float BaseAngle = RandomStream.FRandRange(0.f, UE_TWO_PI);

	// 상자나 SpawnPoint의 Scale이 Item Actor에 전파되지 않게 한다.
	const FTransform CleanSourceTransform(SourceTransform.GetRotation(), SourceTransform.GetLocation(),
	                                      FVector::OneVector);

	int32 SpawnedActorCount = 0;

	for (int32 Index = 0; Index < SpawnEntries.Num(); ++Index)
	{
		const FSpawnEntry& Entry = SpawnEntries[Index];
		const float EvenAngle = UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(SpawnEntries.Num());
		const float SpawnAngle = BaseAngle * EvenAngle + RandomStream.FRandRange(-MaxJitterRadians, MaxJitterRadians);
		const float SpawnRadius = RandomStream.FRandRange(SafeMinRadius, SafeMaxRadius);

		const FVector SpawnDirection(FMath::Cos(SpawnAngle), FMath::Sin(SpawnAngle), 0.f);
		const FVector TargetLocation = CleanSourceTransform.GetLocation() + SpawnDirection * SpawnRadius;
		const FRotator TargetRotation(0.f, RandomStream.FRandRange(0.f, 360.f), 0.f);
		const FTransform TargetTransform(TargetRotation, TargetLocation, FVector::OneVector);
		
		FDRWorldItemSpawnParams SpawnParams;
		SpawnParams.SourceTransform = CleanSourceTransform;
		SpawnParams.TargetTransform = TargetTransform;
		SpawnParams.IgnoredActor = Owner;
		SpawnParams.bPlayEmergence = true;
		
		ADRWorldItemActor* SpawnedItem = WorldItemSubsystem->SpawnWorldItemFromDefinitionWithParams(
			Entry.Definition, SpawnParams, Entry.Quantity);
		
		if (IsValid(SpawnedItem))
		{
			++SpawnedActorCount;
			continue;
		}
		UE_LOG(LogTemp, Warning, TEXT("[%s]: Failed to spawn loot '%s'."),
			*GetName(),	*GetNameSafe(Entry.Definition));
	}

	return SpawnedActorCount;
}

bool UDRLootDropComponent::SelectRarity(const FDRLootTierConfig& TierConfig, FRandomStream& RandomStream,
                                        EDRItemRarity& OutRarity) const
{
	static constexpr EDRItemRarity OrderedRarities[] =
	{
		EDRItemRarity::Common,
		EDRItemRarity::Uncommon,
		EDRItemRarity::Rare,
		EDRItemRarity::Epic,
		EDRItemRarity::Legendary
	};

	float TotalWeight = 0.f;

	for (const EDRItemRarity Rarity : OrderedRarities)
	{
		const float* Weight = TierConfig.RarityWeights.Find(Rarity);

		if (Weight != nullptr)
		{
			TotalWeight += FMath::Max(0.f, *Weight);
		}
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Roll = RandomStream.FRandRange(0.f, TotalWeight);

	float AccumulatedWeight = 0.f;
	EDRItemRarity LastValidRarity = EDRItemRarity::Common;

	for (const EDRItemRarity Rarity : OrderedRarities)
	{
		const float* Weight = TierConfig.RarityWeights.Find(Rarity);
		const float SafeWeight = Weight != nullptr ? FMath::Max(0.f, *Weight) : 0.f;

		if (SafeWeight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		LastValidRarity = Rarity;
		AccumulatedWeight += SafeWeight;

		if (Roll <= AccumulatedWeight)
		{
			OutRarity = Rarity;
			return true;
		}
	}

	/*
	 * 부동소수점 누적 오차가 있어도 마지막 유효 항목을 반환한다.
	 */
	OutRarity = LastValidRarity;
	return true;
}

const FDRLootTableRow* UDRLootDropComponent::SelectWeightedLootRow(const TArray<const FDRLootTableRow*>& CandidateRows,
                                                                   FRandomStream& RandomStream) const
{
	float TotalWeight = 0.f;

	for (const FDRLootTableRow* Row : CandidateRows)
	{
		if (Row != nullptr && IsValid(Row->ItemDefinition))
		{
			TotalWeight += FMath::Max(0.f, Row->SelectionWeight);
		}
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	const float Roll = RandomStream.FRandRange(0.f, TotalWeight);

	float AccumulatedWeight = 0.f;
	const FDRLootTableRow* LastValidRow = nullptr;

	for (const FDRLootTableRow* Row : CandidateRows)
	{
		if (Row == nullptr || !IsValid(Row->ItemDefinition))
		{
			continue;
		}

		const float SafeWeight = FMath::Max(0.f, Row->SelectionWeight);

		if (SafeWeight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		LastValidRow = Row;
		AccumulatedWeight += SafeWeight;

		if (Roll <= AccumulatedWeight)
		{
			return Row;
		}
	}

	return LastValidRow;
}

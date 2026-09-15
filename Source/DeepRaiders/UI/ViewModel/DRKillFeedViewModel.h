#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DRKillFeedViewModel.generated.h"

class ADRPlayerController;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRKillFeedEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(
		const FString& InKillerName,
		int32 InKillerTeamId,
		const FString& InVictimName,
		int32 InVictimTeamId,
		EDRKillFeedCause InCause);

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	FText KillerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	int32 KillerTeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	FText VictimName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	int32 VictimTeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	EDRKillFeedCause Cause = EDRKillFeedCause::Player;

	/** TeamId 0=Red, 1=Blue. Snow source는 White. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	FLinearColor KillerColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	FLinearColor VictimColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	bool bIsSnowDeath = false;

private:
	static FLinearColor ResolveTeamColor(int32 TeamId);
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRKillFeedViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(ADRPlayerController* InPlayerController);
	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Kill Feed")
	TArray<TObjectPtr<UDRKillFeedEntryViewModel>> Entries;

private:
	UFUNCTION()
	void HandleKillFeedEntry(
		FString KillerName,
		int32 KillerTeamId,
		FString VictimName,
		int32 VictimTeamId,
		EDRKillFeedCause Cause);

	void RemoveEntry(UDRKillFeedEntryViewModel* Entry);

	TWeakObjectPtr<ADRPlayerController> PlayerController;

	static constexpr int32 MaxEntries = 5;
	static constexpr float EntryLifetimeSeconds = 8.0f;
};

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DeepRaiders/Player/Components/DRCombatStatsComponent.h"
#include "DRScoreboardViewModel.generated.h"

class ADRPlayerController;
class ADRPlayerState;
class UDRCombatStatsComponent;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRScoreboardPlayerEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(ADRPlayerState* InPlayerState, bool bInIsLocalPlayer);

	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	float DamageDealt = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	float DamageTaken = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	bool bIsLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	float LocalHighlightOpacity = 0.f;
	
private:
	UFUNCTION()
	void HandleCombatStatsChanged(FDRMatchCombatStats NewStats);

	void Refresh();

	void ApplyStats(const FDRMatchCombatStats& Stats);

	TWeakObjectPtr<ADRPlayerState> PlayerState;

	TWeakObjectPtr<UDRCombatStatsComponent> CombatStatsComponent;
	
	void HandlePlayerIdentityChanged();

	FDelegateHandle PlayerIdentityChangedHandle;
};


/**
 * 스코어보드 전체 ViewModel.
 *
 * GameState.PlayerArray를 읽고
 *
 * FriendlyEntries
 * EnemyEntries
 *
 * 두 배열로 나눈다.
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRScoreboardViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(ADRPlayerController* InPlayerController);

	void Deinitialize();

	/**
	 * 현재 GameState.PlayerArray를 다시 읽는다.
	 *
	 * 스코어보드를 열 때 호출하면
	 * 중간 Join/Leave도 반영된다.
	 */
	void RefreshPlayers();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> BlueTeamEntries;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scoreboard")
	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> RedTeamEntries;

private:
	void ClearEntries();

	TWeakObjectPtr<ADRPlayerController> PlayerController;
};

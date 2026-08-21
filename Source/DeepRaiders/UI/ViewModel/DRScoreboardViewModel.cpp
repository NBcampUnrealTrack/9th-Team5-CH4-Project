#include "DRScoreboardViewModel.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"


#pragma region PlayerEntry

void UDRScoreboardPlayerEntryViewModel::Initialize(ADRPlayerState* InPlayerState, bool bInIsLocalPlayer)
{
	Deinitialize();

	PlayerState = InPlayerState;

	UE_MVVM_SET_PROPERTY_VALUE(bIsLocalPlayer, bInIsLocalPlayer);

	if (!PlayerState.IsValid())
	{
		return;
	}

	CombatStatsComponent = PlayerState->GetCombatStatsComponent();

	if (CombatStatsComponent.IsValid())
	{
		CombatStatsComponent->OnCombatStatsChanged.AddDynamic(this, &ThisClass::HandleCombatStatsChanged);
	}

	/*
	 * Delegate를 연결하는 것만으로는
	 * 현재 값이 들어오지 않는다.
	 *
	 * 따라서 최초 값을 한 번 읽는다.
	 */
	Refresh();
}

void UDRScoreboardPlayerEntryViewModel::Deinitialize()
{
	if (CombatStatsComponent.IsValid())
	{
		CombatStatsComponent->OnCombatStatsChanged.RemoveDynamic(this, &ThisClass::HandleCombatStatsChanged);
	}

	PlayerState.Reset();
	CombatStatsComponent.Reset();
}

void UDRScoreboardPlayerEntryViewModel::HandleCombatStatsChanged(FDRMatchCombatStats NewStats)
{
	ApplyStats(NewStats);
}

void UDRScoreboardPlayerEntryViewModel::Refresh()
{
	if (!PlayerState.IsValid())
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(PlayerName, FText::FromString( PlayerState->GetPlayerName()));

	if (CombatStatsComponent.IsValid())
	{
		ApplyStats(CombatStatsComponent->GetMatchStats());
	}
	else
	{
		ApplyStats(FDRMatchCombatStats{});
	}
}

void UDRScoreboardPlayerEntryViewModel::ApplyStats(const FDRMatchCombatStats& Stats)
{
	UE_MVVM_SET_PROPERTY_VALUE(Kills, Stats.Kills);

	UE_MVVM_SET_PROPERTY_VALUE(Deaths, Stats.Deaths);

	UE_MVVM_SET_PROPERTY_VALUE(DamageDealt, Stats.DamageDealt);

	UE_MVVM_SET_PROPERTY_VALUE(DamageTaken, Stats.DamageTaken);
}

#pragma endregion


#pragma region Scoreboard

void UDRScoreboardViewModel::Initialize(ADRPlayerController* InPlayerController)
{
	Deinitialize();

	PlayerController = InPlayerController;

	RefreshPlayers();
}

void UDRScoreboardViewModel::Deinitialize()
{
	ClearEntries();

	PlayerController.Reset();
}

void UDRScoreboardViewModel::RefreshPlayers()
{
	ClearEntries();

	if (!PlayerController.IsValid())
	{
		return;
	}

	ADRPlayerState* LocalPlayerState = PlayerController->GetPlayerState<ADRPlayerState>();

	UWorld* World = PlayerController->GetWorld();

	AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;

	if (!IsValid(LocalPlayerState) || !IsValid(GameState))
	{
		return;
	}

	const int32 LocalTeamId = LocalPlayerState->GetTeamId();

	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> NewFriendlyEntries;

	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> NewEnemyEntries;

	NewFriendlyEntries.Reserve(3);
	NewEnemyEntries.Reserve(3);

	for (APlayerState* PlayerStateBase : GameState->PlayerArray)
	{
		ADRPlayerState* DRPlayerState = Cast<ADRPlayerState>(PlayerStateBase);

		if (!IsValid(DRPlayerState))
		{
			continue;
		}

		UDRScoreboardPlayerEntryViewModel* EntryViewModel = NewObject<UDRScoreboardPlayerEntryViewModel>(this);

		const bool bLocalPlayer = DRPlayerState == LocalPlayerState;

		EntryViewModel->Initialize(DRPlayerState, bLocalPlayer);

		if (DRPlayerState->GetTeamId() == LocalTeamId)
		{
			NewFriendlyEntries.Add(EntryViewModel);
		}
		else
		{
			NewEnemyEntries.Add(EntryViewModel);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(FriendlyEntries, MoveTemp(NewFriendlyEntries));

	UE_MVVM_SET_PROPERTY_VALUE(EnemyEntries, MoveTemp(NewEnemyEntries));
}

void UDRScoreboardViewModel::ClearEntries()
{
	for (UDRScoreboardPlayerEntryViewModel* Entry : FriendlyEntries)
	{
		if (IsValid(Entry))
		{
			Entry->Deinitialize();
		}
	}

	for (UDRScoreboardPlayerEntryViewModel* Entry : EnemyEntries)
	{
		if (IsValid(Entry))
		{
			Entry->Deinitialize();
		}
	}

	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> EmptyFriendlyEntries;

	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> EmptyEnemyEntries;

	UE_MVVM_SET_PROPERTY_VALUE(FriendlyEntries, MoveTemp(EmptyFriendlyEntries));

	UE_MVVM_SET_PROPERTY_VALUE(EnemyEntries, MoveTemp(EmptyEnemyEntries));
}

#pragma endregion

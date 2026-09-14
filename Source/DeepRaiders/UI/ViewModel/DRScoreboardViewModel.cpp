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
	UE_MVVM_SET_PROPERTY_VALUE(LocalHighlightOpacity, bInIsLocalPlayer ? 1.f : 0.f);
	
	if (PlayerState.IsValid())
	{
		PlayerIdentityChangedHandle =
			PlayerState->OnPlayerIdentityChanged.AddUObject(
				this,
				&ThisClass::HandlePlayerIdentityChanged);
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

	if (PlayerState.IsValid()
		&& PlayerIdentityChangedHandle.IsValid())
	{
		PlayerState->OnPlayerIdentityChanged.Remove(
			PlayerIdentityChangedHandle);
	}

	PlayerIdentityChangedHandle.Reset();
	
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

	UE_MVVM_SET_PROPERTY_VALUE(PlayerName, PlayerState->GetDisplayPlayerName());

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
	// 표시용 피해량은 누적값의 소수점 이하를 버린다.
	UE_MVVM_SET_PROPERTY_VALUE(DamageDealt, FMath::TruncToInt(Stats.DamageDealt));
	UE_MVVM_SET_PROPERTY_VALUE(DamageTaken, FMath::TruncToInt(Stats.DamageTaken));
}

void UDRScoreboardPlayerEntryViewModel::HandlePlayerIdentityChanged()
{
	Refresh();
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

	if (!IsValid(GameState))
	{
		return;
	}

	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> NewBlueTeamEntries;
	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> NewRedTeamEntries;

	NewBlueTeamEntries.Reserve(3);
	NewRedTeamEntries.Reserve(3);

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

		switch (DRPlayerState->GetTeamId())
		{

		case 0:
			NewRedTeamEntries.Add(EntryViewModel);
			break;
			
		case 1:
			NewBlueTeamEntries.Add(EntryViewModel);
			break;

		default: UE_LOG(LogTemp, Warning, TEXT( "[Scoreboard] Invalid TeamId. " "Player=%s TeamId=%d"), *GetNameSafe(DRPlayerState), DRPlayerState->GetTeamId());

			EntryViewModel->Deinitialize();
			break;
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(BlueTeamEntries, MoveTemp(NewBlueTeamEntries));
	UE_MVVM_SET_PROPERTY_VALUE(RedTeamEntries, MoveTemp(NewRedTeamEntries));
}

void UDRScoreboardViewModel::ClearEntries()
{
	for (UDRScoreboardPlayerEntryViewModel* Entry : BlueTeamEntries)
	{
		if (IsValid(Entry))
		{
			Entry->Deinitialize();
		}
	}

	for (UDRScoreboardPlayerEntryViewModel* Entry : RedTeamEntries)
	{
		if (IsValid(Entry))
		{
			Entry->Deinitialize();
		}
	}

	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> EmptyBlueEntries;
	TArray<TObjectPtr<UDRScoreboardPlayerEntryViewModel>> EmptyRedEntries;

	UE_MVVM_SET_PROPERTY_VALUE(BlueTeamEntries, MoveTemp(EmptyBlueEntries));
	UE_MVVM_SET_PROPERTY_VALUE(RedTeamEntries, MoveTemp(EmptyRedEntries));
}

#pragma endregion

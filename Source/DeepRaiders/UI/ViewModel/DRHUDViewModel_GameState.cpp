#include "DRHUDViewModel.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Gameplay/DRGameStartActor.h"

void UDRHUDViewModel::HandleReadyStateChanged(
	int32 InReadyPlayerCount,
	int32 InTotalPlayerCount,
	bool)
{
	ReadyPlayerCount = InReadyPlayerCount;
	TotalPlayerCount = InTotalPlayerCount;
	RefreshGameStartStatus();
}

void UDRHUDViewModel::HandleGameStartCountdownChanged(int32 SecondsRemaining)
{
	GameStartCountdown = SecondsRemaining;
	RefreshGameStartStatus();
}

void UDRHUDViewModel::HandleAllPlayersReady()
{
	RefreshGameStartStatus();
}

void UDRHUDViewModel::HandleGameTimerChanged(
	int32 RemainingSeconds,
	bool bInGameStarted,
	bool bInGameEnded)
{
	GameRemainingSeconds = RemainingSeconds;
	bGameStarted = bInGameStarted;
	bGameEnded = bInGameEnded;
	RefreshGameStartStatus();
}

void UDRHUDViewModel::HandleGameEndDebugTextChanged(const FString& DebugText)
{
	UE_MVVM_SET_PROPERTY_VALUE(GameEndDebugText, FText::FromString(DebugText));
}

void UDRHUDViewModel::HandleGameResultTextChanged(const FText& ResultText)
{
	UE_MVVM_SET_PROPERTY_VALUE(GameStateText, ResultText);
	UE_MVVM_SET_PROPERTY_VALUE(bIsGameStateTextVisible, !ResultText.IsEmpty());
}

void UDRHUDViewModel::RefreshGameStartStatus()
{
	FText NewStatusText;
	const bool bShowReadyState = GameStartActor.IsValid()
		&& !GameStartActor->IsGameStarted();
	if (bShowReadyState && GameStartCountdown > 0)
	{
		NewStatusText = FText::AsNumber(GameStartCountdown);
	}
	else if (bShowReadyState)
	{
		NewStatusText = FText::Format(
			NSLOCTEXT("DRGameStart", "ReadyCount", "{0} / {1}"),
			FText::AsNumber(ReadyPlayerCount),
			FText::AsNumber(TotalPlayerCount));
	}
	else if (bGameStarted)
	{
		const int32 Minutes = GameRemainingSeconds / 60;
		const int32 Seconds = GameRemainingSeconds % 60;
		NewStatusText = FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds));
	}
	else
	{
		NewStatusText = NSLOCTEXT("DRGameStart", "GameEnded", "게임 끝!");
	}

	UE_MVVM_SET_PROPERTY_VALUE(GameStartStatusText, NewStatusText);
	UE_MVVM_SET_PROPERTY_VALUE(
		bIsGameStartStatusVisible,
		GameStartActor.IsValid() || MiningGameState.IsValid());
}

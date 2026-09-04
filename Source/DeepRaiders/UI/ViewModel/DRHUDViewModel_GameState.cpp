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
	RefreshGameStateText();
}

void UDRHUDViewModel::HandleGamePhaseChanged(
	int32 PhaseIndex,
	int32,
	const TArray<FText>& PlayerMessages)
{
	// 현재 페이즈의 안내 문구를 기존 게임 시작 상태 영역에 함께 표시한다.
	CurrentPhaseMessageText = PhaseIndex != INDEX_NONE && !PlayerMessages.IsEmpty()
		? FText::Join(FText::FromString(TEXT("\n")), PlayerMessages)
		: FText::GetEmpty();
	RefreshGameStartStatus();
}

void UDRHUDViewModel::HandleGameEndDebugTextChanged(const FString& DebugText)
{
	UE_MVVM_SET_PROPERTY_VALUE(GameEndDebugText, FText::FromString(DebugText));
}

void UDRHUDViewModel::HandleGameResultTextChanged(const FText& ResultText)
{
	CurrentGameResultText = ResultText;
	RefreshGameStateText();
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
		NewStatusText = CurrentPhaseMessageText;
	}
	else
	{
		NewStatusText = NSLOCTEXT("DRGameStart", "GameEnded", "게임 끝!");
	}

	UE_MVVM_SET_PROPERTY_VALUE(GameStartStatusText, NewStatusText);
	UE_MVVM_SET_PROPERTY_VALUE(
		bIsGameStartStatusVisible,
		!NewStatusText.IsEmpty());
}

void UDRHUDViewModel::RefreshGameStateText()
{
	FText NewStateText = CurrentGameResultText;
	if (NewStateText.IsEmpty() && bGameStarted && !bGameEnded)
	{
		const int32 Minutes = GameRemainingSeconds / 60;
		const int32 Seconds = GameRemainingSeconds % 60;
		NewStateText = FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds));
	}

	UE_MVVM_SET_PROPERTY_VALUE(GameStateText, NewStateText);
	UE_MVVM_SET_PROPERTY_VALUE(bIsGameStateTextVisible, !NewStateText.IsEmpty());
}

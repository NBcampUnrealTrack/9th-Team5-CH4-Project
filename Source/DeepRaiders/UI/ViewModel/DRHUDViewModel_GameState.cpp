#include "DRHUDViewModel.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Gameplay/DRGameStartActor.h"
#include "DeepRaiders/Player/DRPlayerState.h"

namespace
{
	FText FormatHUDCountdown(const FText& Text, int32 RemainingSeconds)
	{
		if (RemainingSeconds <= 0)
		{
			return FText::GetEmpty();
		}
		const FText Seconds = FText::AsNumber(RemainingSeconds);
		if (Text.IsEmpty())
		{
			return Seconds;
		}
		if (Text.ToString().Contains(TEXT("{Seconds}")))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Seconds"), Seconds);
			return FText::Format(Text, Arguments);
		}
		return FText::Format(NSLOCTEXT("DRPhase", "Countdown", "{0} {1}"), Text, Seconds);
	}
}

void UDRHUDViewModel::HandleReadyStateChanged(
	int32 InReadyPlayerCount,
	int32 InTotalPlayerCount,
	bool)
{
	ReadyPlayerCount = InReadyPlayerCount;
	TotalPlayerCount = InTotalPlayerCount;
	HandleMatchHUDStateChanged();
}

void UDRHUDViewModel::HandleGameStartCountdownChanged(int32 SecondsRemaining)
{
	GameStartCountdown = SecondsRemaining;
	HandleMatchHUDStateChanged();
}

void UDRHUDViewModel::HandleAllPlayersReady()
{
	HandleMatchHUDStateChanged();
}

void UDRHUDViewModel::HandleGameTimerChanged(
	int32 RemainingSeconds,
	bool bInGameStarted,
	bool bInGameEnded)
{
	GameRemainingSeconds = RemainingSeconds;
	bGameStarted = bInGameStarted;
	bGameEnded = bInGameEnded;
	HandleMatchHUDStateChanged();
}

void UDRHUDViewModel::HandleGameFlowMessageChanged(const FText& GameFlowMessage)
{
	CurrentGameFlowMessage = GameFlowMessage;
	HandleMatchHUDStateChanged();
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
	HandleMatchHUDStateChanged();
}

void UDRHUDViewModel::HandleGameEndDebugTextChanged(const FString& DebugText)
{
	UE_MVVM_SET_PROPERTY_VALUE(GameEndDebugText, FText::FromString(DebugText));
}

void UDRHUDViewModel::HandleGameResultTextChanged(const FText& ResultText)
{
	CurrentGameResultText = ResultText;
	const FDRControlZoneGameResult Result = MiningGameState.IsValid()
		? MiningGameState->GetControlZoneResult() : FDRControlZoneGameResult();
	UE_MVVM_SET_PROPERTY_VALUE(FinalTeam0Ratio, Result.Team0Ratio);
	UE_MVVM_SET_PROPERTY_VALUE(FinalTeam1Ratio, Result.Team1Ratio);
	UE_MVVM_SET_PROPERTY_VALUE(FinalTeam0SnowTotal, FMath::TruncToInt(Result.Team0SnowTotal));
	UE_MVVM_SET_PROPERTY_VALUE(FinalTeam1SnowTotal, FMath::TruncToInt(Result.Team1SnowTotal));
	UE_MVVM_SET_PROPERTY_VALUE(WinningTeamId, Result.WinningTeamId);
	UE_MVVM_SET_PROPERTY_VALUE(bHasFinalResult, Result.bHasResult);
	HandleMatchHUDStateChanged();
}

void UDRHUDViewModel::RefreshGameStartStatus()
{
	const FDRPhaseCountdownState Countdown = bGameStarted && MiningGameState.IsValid()
		? MiningGameState->GetPhaseCountdown() : FDRPhaseCountdownState();
	const FText CountdownText = FormatHUDCountdown(Countdown.Text, Countdown.RemainingSeconds);
	UE_MVVM_SET_PROPERTY_VALUE(PhaseCountdownText, CountdownText);
	UE_MVVM_SET_PROPERTY_VALUE(PhaseCountdownSeconds, Countdown.RemainingSeconds);
	UE_MVVM_SET_PROPERTY_VALUE(bIsPhaseCountdownVisible, Countdown.RemainingSeconds > 0);
	FText NewStatusText;
	const EDRGameFlowState FlowState = MiningGameState.IsValid()
		? MiningGameState->GetGameFlowState() : EDRGameFlowState::WaitingForPlayers;
	switch (FlowState)
	{
	case EDRGameFlowState::Countdown:
		NewStatusText = FormatHUDCountdown(CurrentGameFlowMessage, GameStartCountdown);
		break;
	case EDRGameFlowState::Playing:
		NewStatusText = bIsPhaseCountdownVisible ? PhaseCountdownText : CurrentPhaseMessageText;
		break;
	case EDRGameFlowState::Results:
		NewStatusText = FormatHUDCountdown(MiningGameState->GetResultCountdownText(),
			MiningGameState->GetResultRemainingSeconds());
		break;
	default:
		break;
	}
	UE_MVVM_SET_PROPERTY_VALUE(GameStartStatusText, NewStatusText);
	UE_MVVM_SET_PROPERTY_VALUE(bIsGameStartStatusVisible, !NewStatusText.IsEmpty());
}

void UDRHUDViewModel::RefreshGameStateText()
{
	FText NewStateText;
	const EDRGameFlowState FlowState = MiningGameState.IsValid()
		? MiningGameState->GetGameFlowState() : EDRGameFlowState::WaitingForPlayers;
	switch (FlowState)
	{
	case EDRGameFlowState::WaitingForPlayers:
	case EDRGameFlowState::Countdown:
		NewStateText = FText::Format(
			NSLOCTEXT("DRGameStart", "ReadyParticipants", "준비 {0} / {1}"),
			FText::AsNumber(ReadyPlayerCount), FText::AsNumber(TotalPlayerCount));
		if (FlowState == EDRGameFlowState::WaitingForPlayers && !CurrentGameFlowMessage.IsEmpty())
		{
			NewStateText = CurrentGameFlowMessage;
		}
		break;
	case EDRGameFlowState::Loading:
		NewStateText = CurrentGameFlowMessage;
		break;
	case EDRGameFlowState::Playing:
		NewStateText = FText::FromString(FString::Printf(TEXT("%02d:%02d"),
			GameRemainingSeconds / 60, GameRemainingSeconds % 60));
		break;
	case EDRGameFlowState::Results:
		NewStateText = NSLOCTEXT("DRHUD", "GameEnded", "게임 종료");
		if (bHasFinalResult)
		{
			if (WinningTeamId == 0)
			{
				NewStateText = NSLOCTEXT("DRHUD", "RedWins", "게임 종료 · Red 팀 승리");
			}
			else if (WinningTeamId == 1)
			{
				NewStateText = NSLOCTEXT("DRHUD", "BlueWins", "게임 종료 · Blue 팀 승리");
			}
			else
			{
				NewStateText = NSLOCTEXT("DRHUD", "Draw", "게임 종료 · 무승부");
			}
		}
		break;
	}
	UE_MVVM_SET_PROPERTY_VALUE(GameStateText, NewStateText);
	UE_MVVM_SET_PROPERTY_VALUE(bIsGameStateTextVisible, !NewStateText.IsEmpty());
}

void UDRHUDViewModel::RefreshTeamTexts()
{
	FText RedText;
	FText BlueText;
	if (MiningGameState.IsValid())
	{
		// 팀 보유량도 스코어보드와 동일하게 소수점 없이 표시한다.
		const EDRGameFlowState FlowState = MiningGameState->GetGameFlowState();
		const bool bPreparing = FlowState == EDRGameFlowState::WaitingForPlayers
			|| FlowState == EDRGameFlowState::Loading || FlowState == EDRGameFlowState::Countdown;
		if (bPreparing)
		{
			// 준비 여부와 무관하게, 팀 배정이 끝난 현재 참가자만 센다.
			int32 TeamCounts[2] = {0, 0};
			for (const APlayerState* Player : MiningGameState->PlayerArray)
			{
				const ADRPlayerState* Participant = Cast<ADRPlayerState>(Player);
				if (!IsValid(Participant) || Participant->IsOnlyASpectator()
					|| !Participant->HasAssignedTeam())
				{
					continue;
				}
				const int32 TeamId = Participant->GetTeamId();
				if (TeamId == 0 || TeamId == 1)
				{
					++TeamCounts[TeamId];
				}
			}
			const FText CountFormat = NSLOCTEXT("DRHUD", "TeamPlayerCount", "{0}명");
			RedText = FText::Format(CountFormat, FText::AsNumber(TeamCounts[0]));
			BlueText = FText::Format(CountFormat, FText::AsNumber(TeamCounts[1]));
		}
		else if (MiningGameState->IsGameEnded() && bHasFinalResult)
		{
			RedText = FText::AsNumber(FinalTeam0SnowTotal);
			BlueText = FText::AsNumber(FinalTeam1SnowTotal);
		}
		else
		{
			RedText = FText::AsNumber(FMath::TruncToInt(MiningGameState->GetDisplayedTeamSnowTotal(0)));
			BlueText = FText::AsNumber(FMath::TruncToInt(MiningGameState->GetDisplayedTeamSnowTotal(1)));
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(TeamRedText, RedText);
	UE_MVVM_SET_PROPERTY_VALUE(TeamBlueText, BlueText);
}

void UDRHUDViewModel::HandleMatchHUDStateChanged()
{
	RefreshGameStartStatus();
	RefreshGameStateText();
	RefreshTeamTexts();
}

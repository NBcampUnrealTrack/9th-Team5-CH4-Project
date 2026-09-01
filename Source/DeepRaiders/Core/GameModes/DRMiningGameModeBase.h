#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "DRMiningGameModeBase.generated.h"

enum class EDRSnowJoinSnapshotResult : uint8
{
	Applied,
	InvalidCheckpoint,
};

DECLARE_MULTICAST_DELEGATE(FOnJoinSnapshotStarted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJoinSnapshotFinished, EDRSnowJoinSnapshotResult);

// 채굴 테스트/플레이용 GameState를 사용하는 GameMode이다.
UCLASS()
class DEEPRAIDERS_API ADRMiningGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRMiningGameModeBase();

	/** 모든 경기 데이터를 초기화한 뒤 경기를 시작한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	bool StartGame();

	UFUNCTION(BlueprintPure, Category = "Game")
	bool IsGameStarted() const { return bIsGameStart; }

	UFUNCTION(BlueprintPure, Category = "Game")
	bool IsGameEnded() const { return bIsGameEnd; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	void EndGame();

	/** 서버 경기 시간을 기준으로 패시브 코인 지급을 시작한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	void StartTimer();

	/** 진행 중인 패시브 코인 지급을 종료한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game")
	void EndTimer();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	FOnJoinSnapshotStarted OnJoinSnapshotStarted;
	FOnJoinSnapshotFinished OnJoinSnapshotFinished;

	// 중도 접속자의 눈 스냅샷 적용이 끝난 뒤 실제 플레이어를 생성한다.
	bool HandleSnowJoinSnapshotApplied(
		APlayerController* PlayerController,
		bool bNotifySnapshotFinished = true);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "0.01", Units = "s"))
	float PassiveCoinInterval = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "1"))
	int32 PassiveCoinAmount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "0.0", Units = "s"))
	float PassiveCoinIncreaseInterval = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Coin", meta = (ClampMin = "0"))
	int32 PassiveCoinIncreaseAmount = 0;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Team Movement",
		meta = (ClampMin = "0.01", Units = "s"))
	float TeamSwitchInterval = 10.f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Game",
		meta = (ClampMin = "1.0", Units = "s"))
	float GameDuration = 180.f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Game",
		meta = (ClampMin = "0.1", Units = "s"))
	float GameResultDisplayDuration = 5.f;

private:
	void ResetGameState();
	void TickGameTimer();
	void ClearGameResultText();
	void RefreshGameStartPlayerRoster();
	int32 AssignBalancedTeam(class ADRPlayerState* PlayerState) const;
	bool TryStartSnowJoinSnapshot(class ADRPlayerController* PlayerController);

	/** 마지막 처리 회차 이후의 지급액을 합산해 각 플레이어에게 지급한다. */
	void GrantPassiveCoins();

	/** 지정된 지급 회차에서 적용할 코인 지급량을 반환한다. */
	int64 GetPassiveCoinAmountAtGrantIndex(int64 GrantIndex) const;

	FTimerHandle PassiveCoinTimerHandle;
	FTimerHandle TeamSwitchTimerHandle;
	FTimerHandle GameTimerHandle;
	FTimerHandle GameResultTimerHandle;
	int32 GameRemainingSeconds = 0;
	int32 ActiveTeamId = INDEX_NONE;

	void StartTeamSwitchTimer();
	void RefreshActiveTeam();
	void ApplyActiveTeam(bool bImmediate);

	/** 패시브 코인 지급을 시작한 서버 경기 시간이다. */
	double PassiveCoinStartTime = 0.0;

	/** 중복 지급을 방지하기 위해 마지막으로 처리한 지급 회차를 저장한다. */
	int64 LastProcessedGrantIndex = 0;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Game",
		meta = (AllowPrivateAccess = "true"))
	bool bIsGameStart = false;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Game",
		meta = (AllowPrivateAccess = "true"))
	bool bIsGameEnd = false;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRMapMusicActor.generated.h"

class ADRMiningGameStateBase;
class UAudioComponent;
class USoundBase;

/** 레벨의 대기, 게임, 게임오버 음악 목록을 로컬 클라이언트에서 재생한다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRMapMusicActor : public AActor
{
	GENERATED_BODY()

public:
	ADRMapMusicActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Music")
	TObjectPtr<UAudioComponent> MusicComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<TObjectPtr<USoundBase>> MapMusicPlaylist;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<TObjectPtr<USoundBase>> GameMusicPlaylist;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<TObjectPtr<USoundBase>> GameOverMusicPlaylist;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music",
		meta = (ClampMin = "0.0", Units = "s"))
	float FadeInDuration = 0.5f;

private:
	enum class EMusicPhase : uint8
	{
		None,
		Map,
		Game,
		GameOver
	};

	UFUNCTION()
	void HandleGameTimerChanged(int32 RemainingSeconds, bool bGameStarted, bool bGameEnded);

	UFUNCTION()
	void HandleMusicFinished();

	void SetMusicPhase(EMusicPhase NewPhase);
	void PlayNextMusic();
	const TArray<TObjectPtr<USoundBase>>& GetCurrentPlaylist() const;

	UPROPERTY()
	TObjectPtr<ADRMiningGameStateBase> MiningGameState;

	EMusicPhase MusicPhase = EMusicPhase::None;
	int32 MusicIndex = INDEX_NONE;
	bool bChangingMusicPhase = false;
};

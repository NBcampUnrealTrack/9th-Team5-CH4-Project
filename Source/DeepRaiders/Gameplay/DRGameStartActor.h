#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "DRGameStartActor.generated.h"

class APlayerState;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FDRReadyStateChanged,
	int32,
	ReadyPlayerCount,
	int32,
	TotalPlayerCount,
	bool,
	bAllPlayersReady);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRAllPlayersReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRGameStartCountdownChanged,
	int32,
	SecondsRemaining);

/** 플레이어별 Ready를 토글하고 전원 Ready를 판정하는 게임 시작 액터다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRGameStartActor : public AActor, public IDRInteractableInterface
{
	GENERATED_BODY()

public:
	ADRGameStartActor();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual bool Interact_Implementation(APawn* Interactor) override;
	virtual bool GetInteractionPromptData_Implementation(
		APawn* Interactor,
		FDRInteractionPromptData& OutPromptData) const override;

	UFUNCTION(BlueprintPure, Category = "Game Start")
	bool IsPlayerReady(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Game Start")
	TArray<APlayerState*> GetReadyPlayers() const;

	UFUNCTION(BlueprintPure, Category = "Game Start")
	int32 GetReadyPlayerCount() const;

	UFUNCTION(BlueprintPure, Category = "Game Start")
	int32 GetTotalPlayerCount() const;

	/** 접속 및 퇴장 후 준비 인원과 전체 인원을 다시 계산한다. */
	void RefreshPlayerRoster();

	UFUNCTION(BlueprintPure, Category = "Game Start")
	int32 GetCountdownSecondsRemaining() const { return CountdownSecondsRemaining; }

	UFUNCTION(BlueprintPure, Category = "Game Start")
	bool IsGameStarted() const { return bGameStarted; }

	/** 경기 종료 후 다시 준비할 수 있도록 상태를 초기화한다. */
	void ResetForNextGame();

	// GameMode가 결정한 카운트다운과 시작 상태를 기존 UI 이벤트에 전달한다.
	void SetCountdownSecondsRemaining(int32 SecondsRemaining);
	void NotifyGameStarted();

	UPROPERTY(BlueprintAssignable, Category = "Game Start")
	FDRReadyStateChanged OnReadyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Start")
	FDRAllPlayersReady OnAllPlayersReady;

	UPROPERTY(BlueprintAssignable, Category = "Game Start")
	FDRGameStartCountdownChanged OnGameStartCountdownChanged;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> InteractionMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Start|Visual")
	FName ReadyColorParameterName = TEXT("BaseColor");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Start|Visual")
	FLinearColor ReadyColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Start|Visual")
	FLinearColor NotReadyColor = FLinearColor::Gray;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Game Start",
		meta = (ClampMin = "1", Units = "s"))
	int32 GameStartCountdownSeconds = 3;

private:
	UFUNCTION()
	void OnRep_ReadyPlayers();

	UFUNCTION()
	void OnRep_GameStarted();

	UFUNCTION()
	void OnRep_CountdownSecondsRemaining();

	UFUNCTION()
	void OnRep_TotalPlayerCount();

	void RefreshReadyState();
	void RefreshLocalReadyColor();
	void BroadcastReadyStatus();
	int32 GetEligiblePlayerCount() const;

	UPROPERTY(ReplicatedUsing = OnRep_ReadyPlayers)
	TArray<TObjectPtr<APlayerState>> ReadyPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_GameStarted)
	bool bGameStarted = false;

	UPROPERTY(ReplicatedUsing = OnRep_CountdownSecondsRemaining)
	int32 CountdownSecondsRemaining = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TotalPlayerCount)
	int32 TotalPlayerCount = 0;

};

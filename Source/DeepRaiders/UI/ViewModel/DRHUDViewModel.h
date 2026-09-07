#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "DRHUDViewModel.generated.h"

class ADRPlayerCharacter;
class UAbilitySystemComponent;
class UDRItemDefinition;
class UDRQuickSlotComponent;
class AActor;
class UDRInteractionComponent;
class ADRGameStartActor;
class ADRMiningGameStateBase;
struct FDRInteractionPromptData;
struct FOnAttributeChangeData;

/** 플레이어의 체력, 눈 및 빙결 게이지를 HUD 바인딩용 값으로 제공한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** HUD가 표시할 로컬 플레이어를 연결한다. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void Initialize(ADRPlayerCharacter* InPlayerCharacter);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void Deinitialize();

	/** HUD에 표시되는 게이지 비율을 목표값까지 부드럽게 갱신한다. */
	void TickGaugeInterpolation(float DeltaSeconds);

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Health")
	float CurrentHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Health")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Health")
	float HealthRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Shield")
	float CurrentShield = 0.f;

	/** 현재 최대 체력 대비 쉴드 비율이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Shield")
	float ShieldRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Snow")
	FText SnowGaugeText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Snow")
	float SnowGaugeOpacity = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Heat")
	float HeatGauge = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Heat")
	float MaxHeatGauge = 100.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Heat")
	float HeatGaugeRatio = 0.f;

	/** 과열 진행도에 따라 차가운 하늘색에서 경고색으로 변한다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Heat")
	FLinearColor HeatGaugeColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Heat")
	float HeatIconOpacity = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Heat")
	float HeatGaugeOpacity = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Heat")
	bool bIsOverheated = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Freeze")
	float FreezeGauge = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Freeze")
	float FreezeGaugeRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Ammo")
	bool bIsAmmoVisible = false;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleShieldChanged(const FOnAttributeChangeData& ChangeData);
	void HandleSnowGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleHeatGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHeatGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleOverheatedTagChanged(FGameplayTag Tag, int32 NewCount);
	void HandleFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleQuickSlotsChanged();

	UFUNCTION()
	void HandleSelectedQuickSlotItemChanged(UDRItemDefinition* ItemDefinition);

	void RefreshHealth();
	void RefreshShield();
	void RefreshSnowGaugeText();
	void RefreshHeatGauge();
	void RefreshOverheatedState();
	void RefreshFreezeGauge();
	void RefreshAmmoVisibility();

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle ShieldChangedHandle;
	FDelegateHandle SnowGaugeChangedHandle;
	FDelegateHandle HeatGaugeChangedHandle;
	FDelegateHandle MaxHeatGaugeChangedHandle;
	FDelegateHandle OverheatedTagChangedHandle;
	FDelegateHandle FreezeGaugeChangedHandle;
	float TargetCurrentHealth = 0.f;
	float TargetHeatGauge = 0.f;
	float TargetFreezeGauge = 0.f;
	float TargetHealthRatio = 0.f;
	float TargetHeatGaugeRatio = 0.f;
	float TargetFreezeGaugeRatio = 0.f;
	float SnowGaugeIdleDuration = 0.f;
	float HeatGaugeZeroDuration = 0.f;
	float HeatGaugeBlinkElapsed = 0.f;
	bool bHeatGaugeWasActive = false;
	bool bSnowGaugeFadeActive = false;
	bool bHoldHeatGaugeEndColor = false;
	bool bInterpolateGauges = false;

#pragma region Interaction
protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Interaction")
	bool bIsInteractionPromptVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Interaction")
	FText InteractionActionText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Interaction")
	FText InteractionTitleText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Interaction")
	FText InteractionDetailText;

private:
	void HandleFocusedInteractableChanged(AActor* Target, const FDRInteractionPromptData& PromptData);
	void RefreshInteractionPrompt();

	TWeakObjectPtr<UDRInteractionComponent> InteractionComponent;
	FDelegateHandle InteractionFocusChangedHandle;

#pragma endregion

#pragma region GameState
protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Game Start")
	FText GameStartStatusText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Game Start")
	bool bIsGameStartStatusVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Game Start")
	FText GameEndDebugText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Game State")
	FText GameStateText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Game State")
	bool bIsGameStateTextVisible = false;

private:
	UFUNCTION()
	void HandleReadyStateChanged(
		int32 ReadyPlayerCount,
		int32 TotalPlayerCount,
		bool bAllPlayersReady);

	UFUNCTION()
	void HandleGameStartCountdownChanged(int32 SecondsRemaining);

	UFUNCTION()
	void HandleAllPlayersReady();

	UFUNCTION()
	void HandleGameTimerChanged(int32 RemainingSeconds, bool bGameStarted, bool bGameEnded);

	UFUNCTION()
	void HandleGameFlowMessageChanged(const FText& GameFlowMessage);

	UFUNCTION()
	void HandleGamePhaseChanged(
		int32 PhaseIndex,
		int32 PhaseRemainingSeconds,
		const TArray<FText>& PlayerMessages);

	UFUNCTION()
	void HandleGameEndDebugTextChanged(const FString& DebugText);

	UFUNCTION()
	void HandleGameResultTextChanged(const FText& ResultText);

	void RefreshGameStartStatus();
	void RefreshGameStateText();

	TWeakObjectPtr<ADRGameStartActor> GameStartActor;
	TWeakObjectPtr<ADRMiningGameStateBase> MiningGameState;
	int32 ReadyPlayerCount = 0;
	int32 TotalPlayerCount = 0;
	int32 GameStartCountdown = 0;
	int32 GameRemainingSeconds = 0;
	FText CurrentPhaseMessageText;
	FText CurrentGameFlowMessage;
	FText CurrentGameResultText;
	bool bGameStarted = false;
	bool bGameEnded = false;
#pragma endregion
};

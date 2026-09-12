#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "Slate/WidgetTransform.h"
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
	/** 상태 게이지의 공통 기준 폭과 내부 Slot Padding을 설정한다. */
	void ConfigureStatusGauge(float InGaugeWidth, const FMargin& InSlotPadding);

	/** 서버가 소유 클라이언트에 보낸 확정 SnowGauge HUD 스냅샷이다. */
	void ReceiveSnowGaugePresentation(float SnowGauge, uint32 Sequence);

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

	/** HP와 Freeze SizeBox가 함께 바인딩할 상태 게이지 기준 폭이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Status")
	float StatusGaugeWidth = 0.f;

	/** Background, Shield, HP, Freeze 및 Deco가 함께 사용할 Slot Padding이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Status")
	FMargin StatusGaugeSlotPadding;

	/** ShieldRatio가 반영된 쉴드 구간의 실제 폭이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Shield")
	float ShieldGaugeWidth = 0.f;

	/** 쉴드 구간을 현재 체력 게이지의 오른쪽 끝으로 옮기는 Transform이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Shield")
	FWidgetTransform ShieldGaugeTransform;

	/** 폭으로 쉴드 비율을 표현하므로 쉴드가 존재할 때는 항상 1이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Shield")
	float ShieldGaugePercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Shield")
	ESlateVisibility ShieldGaugeVisibility = ESlateVisibility::Collapsed;

	/** 기본 게이지와 쉴드 구간 및 오른쪽 여백을 포함한 전체 폭이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Status")
	float StatusExtentWidth = 0.f;

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

	/** 현재 빙결 게이지 / 최대 빙결 게이지의 표시 비율이다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Freeze")
	float FreezeGaugeRatio = 0.f;

	/** 화면 효과 전용 비율로, 빙결 상태에서는 즉시 1을 유지한다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Freeze")
	float FreezeScreenEffectRatio = 0.f;

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
	void HandleFrozenTagChanged(FGameplayTag Tag, int32 NewCount);
	void HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleQuickSlotsChanged();

	UFUNCTION()
	void HandleSelectedQuickSlotItemChanged(UDRItemDefinition* ItemDefinition);

	void RefreshHealth();
	void RefreshShield();
	void RefreshStatusGaugePresentation();
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
	FDelegateHandle MaxFreezeGaugeChangedHandle;
	FDelegateHandle FrozenTagChangedHandle;
	float TargetCurrentHealth = 0.f;
	float TargetHeatGauge = 0.f;
	float TargetFreezeGauge = 0.f;
	float TargetScreenEffectRatio = 0.f;
	float TargetHealthRatio = 0.f;
	float TargetHeatGaugeRatio = 0.f;
	float TargetFreezeGaugeRatio = 0.f;
	float TargetFreezeScreenEffectRatio = 0.f;
	float TargetSnowGauge = 0.f;
	float DisplaySnowGauge = 0.f;
	float SnowGaugeIdleDuration = 0.f;
	uint32 LastSnowGaugePresentationSequence = 0;
	float HeatGaugeZeroDuration = 0.f;
	float HeatGaugeBlinkElapsed = 0.f;
	bool bHeatGaugeWasActive = false;
	bool bSnowGaugeFadeActive = false;
	bool bHasSnowGaugePresentation = false;
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

	// 준비 중에는 팀 인원, 경기 중과 결과에서는 팀 눈 보유량이다.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Team")
	FText TeamRedText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Team")
	FText TeamBlueText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Game State")
	bool bIsGameStateTextVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Phase")
	FText PhaseCountdownText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Phase")
	int32 PhaseCountdownSeconds = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Phase")
	bool bIsPhaseCountdownVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Result")
	float FinalTeam0Ratio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Result")
	float FinalTeam1Ratio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Result")
	float FinalTeam0SnowTotal = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Result")
	float FinalTeam1SnowTotal = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Result")
	int32 WinningTeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Result")
	bool bHasFinalResult = false;

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

	UFUNCTION()
	void HandleMatchHUDStateChanged();

	void RefreshGameStartStatus();
	void RefreshGameStateText();
	void RefreshTeamTexts();

	TWeakObjectPtr<ADRGameStartActor> GameStartActor;
	TWeakObjectPtr<ADRMiningGameStateBase> MiningGameState;
	float TeamRosterRefreshElapsed = 0.f;
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

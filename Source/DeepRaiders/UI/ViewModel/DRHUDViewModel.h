#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
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

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Health")
	float CurrentHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Health")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Health")
	float HealthRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Snow")
	int32 SnowGauge = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Snow")
	int32 MaxSnowGauge = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Snow")
	float SnowGaugeRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Freeze")
	float FreezeGauge = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Freeze")
	float FreezeGaugeRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Ammo")
	bool bIsAmmoVisible = false;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleSnowGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxSnowGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleQuickSlotsChanged();

	UFUNCTION()
	void HandleSelectedQuickSlotItemChanged(UDRItemDefinition* ItemDefinition);

	void RefreshHealth();
	void RefreshSnowGauge();
	void RefreshFreezeGauge();
	void RefreshAmmoVisibility();

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle SnowGaugeChangedHandle;
	FDelegateHandle MaxSnowGaugeChangedHandle;
	FDelegateHandle FreezeGaugeChangedHandle;
	
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

#pragma region GameStart
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
	void HandleGameEndDebugTextChanged(const FString& DebugText);

	UFUNCTION()
	void HandleGameResultTextChanged(const FText& ResultText);

	void RefreshGameStartStatus();

	TWeakObjectPtr<ADRGameStartActor> GameStartActor;
	TWeakObjectPtr<ADRMiningGameStateBase> MiningGameState;
	int32 ReadyPlayerCount = 0;
	int32 TotalPlayerCount = 0;
	int32 GameStartCountdown = 0;
	int32 GameRemainingSeconds = 0;
	bool bGameStarted = false;
	bool bGameEnded = false;
#pragma endregion
};

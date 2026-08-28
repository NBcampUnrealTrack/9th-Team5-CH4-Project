#include "DRHUDViewModel.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Gameplay/DRGameStartActor.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/Components/DRInteractionComponent.h"
#include "EngineUtils.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"

void UDRHUDViewModel::Initialize(ADRPlayerCharacter* InPlayerCharacter)
{
	Deinitialize();

	if (!IsValid(InPlayerCharacter))
	{
		return;
	}

	AbilitySystemComponent = InPlayerCharacter->GetAbilitySystemComponent();
	ADRPlayerController* PlayerController =
		Cast<ADRPlayerController>(InPlayerCharacter->GetController());
	QuickSlotComponent = IsValid(PlayerController)
		? PlayerController->GetQuickSlotComponent()
		: nullptr;
	InteractionComponent = IsValid(PlayerController)
		? PlayerController->GetInteractionComponent()
		: nullptr;
	
	// GAS 속성 변경 델리게이트 연결
	if (AbilitySystemComponent.IsValid())
	{
		HealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetHealthAttribute()).AddUObject(
				this, &ThisClass::HandleHealthChanged);
		MaxHealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxHealthAttribute()).AddUObject(
				this, &ThisClass::HandleMaxHealthChanged);
		SnowGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetSnowGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleSnowGaugeChanged);
		MaxSnowGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleMaxSnowGaugeChanged);
		FreezeGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleFreezeGaugeChanged);
	}

	if (QuickSlotComponent.IsValid())
	{
		QuickSlotComponent->OnQuickSlotsChangedDelegate.AddDynamic(
			this,
			&ThisClass::HandleQuickSlotsChanged);
		QuickSlotComponent->OnSelectedQuickSlotItemChangedDelegate.AddDynamic(
			this,
			&ThisClass::HandleSelectedQuickSlotItemChanged);
	}
	
	if (InteractionComponent.IsValid())
	{
		InteractionFocusChangedHandle = InteractionComponent->OnFocusedInteractableChanged
			.AddUObject(this, &ThisClass::HandleFocusedInteractableChanged);
	}

	for (
		TActorIterator<ADRGameStartActor> Iterator(InPlayerCharacter->GetWorld());
		Iterator;
		++Iterator)
	{
		GameStartActor = *Iterator;
		break;
	}

	if (GameStartActor.IsValid())
	{
		GameStartActor->OnReadyStateChanged.AddDynamic(
			this,
			&ThisClass::HandleReadyStateChanged);
		GameStartActor->OnGameStartCountdownChanged.AddDynamic(
			this,
			&ThisClass::HandleGameStartCountdownChanged);
		GameStartActor->OnAllPlayersReady.AddDynamic(
			this,
			&ThisClass::HandleAllPlayersReady);
		ReadyPlayerCount = GameStartActor->GetReadyPlayerCount();
		TotalPlayerCount = GameStartActor->GetTotalPlayerCount();
		GameStartCountdown = GameStartActor->GetCountdownSecondsRemaining();
	}

	MiningGameState = InPlayerCharacter->GetWorld()->GetGameState<ADRMiningGameStateBase>();
	if (MiningGameState.IsValid())
	{
		MiningGameState->OnGameTimerChanged.AddDynamic(
			this,
			&ThisClass::HandleGameTimerChanged);
		MiningGameState->OnGameEndDebugTextChanged.AddDynamic(
			this,
			&ThisClass::HandleGameEndDebugTextChanged);
		MiningGameState->OnGameResultTextChanged.AddDynamic(
			this,
			&ThisClass::HandleGameResultTextChanged);
		GameRemainingSeconds = MiningGameState->GetGameRemainingSeconds();
		bGameStarted = MiningGameState->IsGameStarted();
		bGameEnded = MiningGameState->IsGameEnded();
		UE_MVVM_SET_PROPERTY_VALUE(
			GameEndDebugText,
			FText::FromString(MiningGameState->GetGameEndDebugText()));
		HandleGameResultTextChanged(MiningGameState->GetGameResultText());
	}

	// 최초 리프레쉬
	RefreshHealth();
	RefreshSnowGauge();
	RefreshFreezeGauge();
	RefreshAmmoVisibility();
	RefreshInteractionPrompt();
	RefreshGameStartStatus();
	bInterpolateGauges = true;
}

void UDRHUDViewModel::Deinitialize()
{
	if (QuickSlotComponent.IsValid())
	{
		QuickSlotComponent->OnQuickSlotsChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleQuickSlotsChanged);
		QuickSlotComponent->OnSelectedQuickSlotItemChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleSelectedQuickSlotItemChanged);
	}
	
	if (InteractionComponent.IsValid()
		&& InteractionFocusChangedHandle.IsValid())
	{
		InteractionComponent->OnFocusedInteractableChanged.Remove(InteractionFocusChangedHandle);
	}

	if (GameStartActor.IsValid())
	{
		GameStartActor->OnReadyStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleReadyStateChanged);
		GameStartActor->OnGameStartCountdownChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameStartCountdownChanged);
		GameStartActor->OnAllPlayersReady.RemoveDynamic(
			this,
			&ThisClass::HandleAllPlayersReady);
	}

	if (MiningGameState.IsValid())
	{
		MiningGameState->OnGameTimerChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameTimerChanged);
		MiningGameState->OnGameEndDebugTextChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameEndDebugTextChanged);
		MiningGameState->OnGameResultTextChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameResultTextChanged);
	}

	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetSnowGaugeAttribute()).Remove(SnowGaugeChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute()).Remove(MaxSnowGaugeChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).Remove(FreezeGaugeChangedHandle);
	}

	AbilitySystemComponent.Reset();
	QuickSlotComponent.Reset();
	InteractionComponent.Reset();
	GameStartActor.Reset();
	MiningGameState.Reset();
	
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	SnowGaugeChangedHandle.Reset();
	MaxSnowGaugeChangedHandle.Reset();
	FreezeGaugeChangedHandle.Reset();
	InteractionFocusChangedHandle.Reset();
	ReadyPlayerCount = 0;
	TotalPlayerCount = 0;
	GameStartCountdown = 0;
	GameRemainingSeconds = 0;
	bGameStarted = false;
	bGameEnded = false;
	TargetHealthRatio = 0.f;
	TargetSnowGaugeRatio = 0.f;
	TargetFreezeGaugeRatio = 0.f;
	TargetCurrentHealth = 0.f;
	TargetSnowGauge = 0;
	InterpolatedSnowGauge = 0.f;
	TargetFreezeGauge = 0.f;
	bInterpolateGauges = false;
}

void UDRHUDViewModel::TickGaugeInterpolation(float DeltaSeconds)
{
	if (!bInterpolateGauges || DeltaSeconds <= 0.f)
	{
		return;
	}

	const auto InterpolateRatio = [DeltaSeconds](float DisplayRatio, float TargetRatio)
	{
		constexpr float InterpolationSpeed = 8.f;
		constexpr float CompletionTolerance = 0.001f;
		return FMath::IsNearlyEqual(DisplayRatio, TargetRatio, CompletionTolerance)
			? TargetRatio
			: FMath::FInterpTo(DisplayRatio, TargetRatio, DeltaSeconds, InterpolationSpeed);
	};
	const auto InterpolateValue = [DeltaSeconds](float DisplayValue, float TargetValue)
	{
		constexpr float InterpolationSpeed = 8.f;
		constexpr float CompletionTolerance = 0.01f;
		return FMath::IsNearlyEqual(DisplayValue, TargetValue, CompletionTolerance)
			? TargetValue
			: FMath::FInterpTo(DisplayValue, TargetValue, DeltaSeconds, InterpolationSpeed);
	};

	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, InterpolateValue(CurrentHealth, TargetCurrentHealth));
	InterpolatedSnowGauge = InterpolateValue(InterpolatedSnowGauge, TargetSnowGauge);
	UE_MVVM_SET_PROPERTY_VALUE(SnowGauge, FMath::RoundToInt(InterpolatedSnowGauge));
	UE_MVVM_SET_PROPERTY_VALUE(FreezeGauge, InterpolateValue(FreezeGauge, TargetFreezeGauge));
	UE_MVVM_SET_PROPERTY_VALUE(HealthRatio, InterpolateRatio(HealthRatio, TargetHealthRatio));
	UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeRatio, InterpolateRatio(SnowGaugeRatio, TargetSnowGaugeRatio));
	UE_MVVM_SET_PROPERTY_VALUE(
		FreezeGaugeRatio,
		InterpolateRatio(FreezeGaugeRatio, TargetFreezeGaugeRatio));
}

void UDRHUDViewModel::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
}

void UDRHUDViewModel::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
}

void UDRHUDViewModel::HandleSnowGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshSnowGauge();
}

void UDRHUDViewModel::HandleMaxSnowGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshSnowGauge();
}

void UDRHUDViewModel::HandleFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFreezeGauge();
}

void UDRHUDViewModel::HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFreezeGauge();
}

void UDRHUDViewModel::HandleQuickSlotsChanged()
{
	RefreshAmmoVisibility();
}

void UDRHUDViewModel::HandleSelectedQuickSlotItemChanged(UDRItemDefinition*)
{
	RefreshAmmoVisibility();
}

void UDRHUDViewModel::RefreshHealth()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	
	const float NewCurrentHealth = IsValid(AttributeSet) ? AttributeSet->GetHealth() : 0.f;
	const float NewMaxHealth = IsValid(AttributeSet) ? AttributeSet->GetMaxHealth() : 0.f;
	const float NewHealthRatio = NewMaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewCurrentHealth / NewMaxHealth, 0.f, 1.f)
		: 0.f;

	TargetCurrentHealth = NewCurrentHealth;
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaxHealth);
	TargetHealthRatio = NewHealthRatio;
	if (!bInterpolateGauges)
	{
		UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, TargetCurrentHealth);
		UE_MVVM_SET_PROPERTY_VALUE(HealthRatio, TargetHealthRatio);
	}
}

void UDRHUDViewModel::RefreshSnowGauge()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	const float NewSnowGauge = IsValid(AttributeSet) ? AttributeSet->GetSnowGauge() : 0.f;
	const float NewMaxSnowGauge = IsValid(AttributeSet) ? AttributeSet->GetMaxSnowGauge() : 0.f;
	const float NewSnowGaugeRatio = NewMaxSnowGauge > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewSnowGauge / NewMaxSnowGauge, 0.f, 1.f)
		: 0.f;

	TargetSnowGauge = FMath::RoundToInt(NewSnowGauge);
	UE_MVVM_SET_PROPERTY_VALUE(MaxSnowGauge, FMath::RoundToInt(NewMaxSnowGauge));
	TargetSnowGaugeRatio = NewSnowGaugeRatio;
	if (!bInterpolateGauges)
	{
		InterpolatedSnowGauge = TargetSnowGauge;
		UE_MVVM_SET_PROPERTY_VALUE(SnowGauge, TargetSnowGauge);
		UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeRatio, TargetSnowGaugeRatio);
	}
}

void UDRHUDViewModel::RefreshFreezeGauge()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	const float NewFreezeGauge = IsValid(AttributeSet) ? AttributeSet->GetFreezeGauge() : 0.f;
	const float NewFreezeGaugeRatio = MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewFreezeGauge / MaxHealth, 0.f, 1.f)
		: 0.f;

	TargetFreezeGauge = NewFreezeGauge;
	TargetFreezeGaugeRatio = NewFreezeGaugeRatio;
	if (!bInterpolateGauges)
	{
		UE_MVVM_SET_PROPERTY_VALUE(FreezeGauge, TargetFreezeGauge);
		UE_MVVM_SET_PROPERTY_VALUE(FreezeGaugeRatio, TargetFreezeGaugeRatio);
	}
}

void UDRHUDViewModel::RefreshAmmoVisibility()
{
	FDRItemInstance SelectedItem;

	const int32 SelectedSlotIndex =
		QuickSlotComponent.IsValid()
			? QuickSlotComponent->GetSelectedSlotIndex()
			: INDEX_NONE;

	const bool bHasSelectedItem =
		QuickSlotComponent.IsValid()
		&& QuickSlotComponent->GetQuickSlot(
			SelectedSlotIndex,
			SelectedItem);

	const UDRRangedWeaponDefinition* RangedWeapon =
		bHasSelectedItem
			? Cast<UDRRangedWeaponDefinition>(
				SelectedItem.Definition)
			: nullptr;

	UE_MVVM_SET_PROPERTY_VALUE(
		bIsAmmoVisible,
		IsValid(RangedWeapon));
}

void UDRHUDViewModel::HandleFocusedInteractableChanged(AActor* Target, const FDRInteractionPromptData& PromptData)
{
	RefreshInteractionPrompt();
}

void UDRHUDViewModel::RefreshInteractionPrompt()
{
	AActor* FocusedTarget = InteractionComponent.IsValid() ? InteractionComponent->GetFocusedTarget() : nullptr;

	const bool bNewVisible = IsValid(FocusedTarget);

	const FDRInteractionPromptData PromptData = bNewVisible ?
		InteractionComponent->GetFocusedPromptData() : FDRInteractionPromptData();

	UE_MVVM_SET_PROPERTY_VALUE(bIsInteractionPromptVisible, bNewVisible);
	UE_MVVM_SET_PROPERTY_VALUE(InteractionActionText, PromptData.ActionText);
	UE_MVVM_SET_PROPERTY_VALUE(InteractionTitleText, PromptData.TitleText);
	UE_MVVM_SET_PROPERTY_VALUE(InteractionDetailText, PromptData.DetailText);
}

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
	const bool bShowReadyState = GameStartActor.IsValid() && !GameStartActor->IsGameStarted();
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

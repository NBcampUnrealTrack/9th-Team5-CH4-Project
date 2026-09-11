#include "DRHUDViewModel.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Gameplay/DRGameStartActor.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/Components/DRInteractionComponent.h"
#include "EngineUtils.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

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
		ShieldChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetShieldAttribute()).AddUObject(
				this, &ThisClass::HandleShieldChanged);
		SnowGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetSnowGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleSnowGaugeChanged);
		HeatGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetHeatGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleHeatGaugeChanged);
		MaxHeatGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxHeatGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleMaxHeatGaugeChanged);
		OverheatedTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_Overheated, EGameplayTagEventType::NewOrRemoved).AddUObject(
				this, &ThisClass::HandleOverheatedTagChanged);
		FreezeGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleFreezeGaugeChanged);
		FrozenTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_Frozen, EGameplayTagEventType::NewOrRemoved).AddUObject(
				this, &ThisClass::HandleFrozenTagChanged);
		MaxFreezeGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleMaxFreezeGaugeChanged);
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
		MiningGameState->OnMatchHUDStateChanged.AddDynamic(
			this, &ThisClass::HandleMatchHUDStateChanged);
		MiningGameState->OnGameTimerChanged.AddDynamic(
			this,
			&ThisClass::HandleGameTimerChanged);
		MiningGameState->OnGameFlowMessageChanged.AddDynamic(
			this,
			&ThisClass::HandleGameFlowMessageChanged);
		MiningGameState->OnGamePhaseChanged.AddDynamic(
			this,
			&ThisClass::HandleGamePhaseChanged);
		MiningGameState->OnGameEndDebugTextChanged.AddDynamic(
			this,
			&ThisClass::HandleGameEndDebugTextChanged);
		MiningGameState->OnGameResultTextChanged.AddDynamic(
			this,
			&ThisClass::HandleGameResultTextChanged);
		GameRemainingSeconds = MiningGameState->GetGameRemainingSeconds();
		bGameStarted = MiningGameState->IsGameStarted();
		bGameEnded = MiningGameState->IsGameEnded();
		HandleGameFlowMessageChanged(MiningGameState->GetGameFlowMessage());
		HandleGamePhaseChanged(
			MiningGameState->GetCurrentPhaseIndex(),
			MiningGameState->GetPhaseRemainingSeconds(),
			MiningGameState->GetCurrentPhaseMessages());
		UE_MVVM_SET_PROPERTY_VALUE(
			GameEndDebugText,
			FText::FromString(MiningGameState->GetGameEndDebugText()));
		HandleGameResultTextChanged(MiningGameState->GetGameResultText());
	}

	// 최초 리프레쉬
	RefreshHealth();
	RefreshShield();
	RefreshSnowGaugeText();
	RefreshHeatGauge();
	RefreshOverheatedState();
	RefreshFreezeGauge();
	RefreshAmmoVisibility();
	RefreshInteractionPrompt();
	HandleMatchHUDStateChanged();
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
		MiningGameState->OnMatchHUDStateChanged.RemoveDynamic(
			this, &ThisClass::HandleMatchHUDStateChanged);
		MiningGameState->OnGameTimerChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameTimerChanged);
		MiningGameState->OnGameFlowMessageChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameFlowMessageChanged);
		MiningGameState->OnGamePhaseChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGamePhaseChanged);
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
			UDRPlayerAttributeSet::GetShieldAttribute()).Remove(ShieldChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetSnowGaugeAttribute()).Remove(SnowGaugeChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetHeatGaugeAttribute()).Remove(HeatGaugeChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxHeatGaugeAttribute()).Remove(MaxHeatGaugeChangedHandle);
		if (OverheatedTagChangedHandle.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(
				DRGameplayTags::State_Overheated, EGameplayTagEventType::NewOrRemoved)
				.Remove(OverheatedTagChangedHandle);
		}
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).Remove(FreezeGaugeChangedHandle);
		if (FrozenTagChangedHandle.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(
				DRGameplayTags::State_Frozen, EGameplayTagEventType::NewOrRemoved)
				.Remove(FrozenTagChangedHandle);
		}
	}

	AbilitySystemComponent.Reset();
	QuickSlotComponent.Reset();
	InteractionComponent.Reset();
	GameStartActor.Reset();
	MiningGameState.Reset();

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ShieldChangedHandle.Reset();
	SnowGaugeChangedHandle.Reset();
	HeatGaugeChangedHandle.Reset();
	MaxHeatGaugeChangedHandle.Reset();
	OverheatedTagChangedHandle.Reset();
	FreezeGaugeChangedHandle.Reset();
	FrozenTagChangedHandle.Reset();
	InteractionFocusChangedHandle.Reset();
	TeamRosterRefreshElapsed = 0.f;
	ReadyPlayerCount = 0;
	TotalPlayerCount = 0;
	GameStartCountdown = 0;
	GameRemainingSeconds = 0;
	CurrentPhaseMessageText = FText::GetEmpty();
	CurrentGameResultText = FText::GetEmpty();
	CurrentGameFlowMessage = FText::GetEmpty();
	UE_MVVM_SET_PROPERTY_VALUE(GameStateText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(GameStartStatusText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bIsGameStateTextVisible, false);
	UE_MVVM_SET_PROPERTY_VALUE(bIsGameStartStatusVisible, false);
	UE_MVVM_SET_PROPERTY_VALUE(TeamRedText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(TeamBlueText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(PhaseCountdownText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(PhaseCountdownSeconds, 0);
	UE_MVVM_SET_PROPERTY_VALUE(bIsPhaseCountdownVisible, false);
	UE_MVVM_SET_PROPERTY_VALUE(bIsExitCountdown, false);
	UE_MVVM_SET_PROPERTY_VALUE(FinalTeam0Ratio, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(FinalTeam1Ratio, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(WinningTeamId, INDEX_NONE);
	UE_MVVM_SET_PROPERTY_VALUE(bHasFinalResult, false);
	bGameStarted = false;
	bGameEnded = false;
	TargetHealthRatio = 0.f;
	TargetHeatGaugeRatio = 0.f;
	TargetFreezeGaugeRatio = 0.f;
	TargetCurrentHealth = 0.f;
	TargetHeatGauge = 0.f;
	TargetFreezeGauge = 0.f;
	TargetSnowGauge = 0.f;
	DisplaySnowGauge = 0.f;
	SnowGaugeIdleDuration = 0.f;
	LastSnowGaugePresentationSequence = 0;
	HeatGaugeZeroDuration = 0.f;
	HeatGaugeBlinkElapsed = 0.f;
	bHeatGaugeWasActive = false;
	bSnowGaugeFadeActive = false;
	bHasSnowGaugePresentation = false;
	bHoldHeatGaugeEndColor = false;
	bInterpolateGauges = false;

	UE_MVVM_SET_PROPERTY_VALUE(HeatGauge, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(FreezeGaugeRatio, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentShield, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(ShieldRatio, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeText, FText::AsNumber(0));
	UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeOpacity, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHeatGauge, 100.f);
	UE_MVVM_SET_PROPERTY_VALUE(HeatGaugeRatio, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HeatGaugeColor, FLinearColor::White);
	UE_MVVM_SET_PROPERTY_VALUE(HeatIconOpacity, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HeatGaugeOpacity, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(bIsOverheated, false);
	bInterpolateGauges = false;
}

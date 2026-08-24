#include "DRHUDViewModel.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/Components/DRInteractionComponent.h"

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

	// 최초 리프레쉬
	RefreshHealth();
	RefreshSnowGauge();
	RefreshFreezeGauge();
	RefreshAmmoVisibility();
	RefreshInteractionPrompt();
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
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute()).Remove(MaxFreezeGaugeChangedHandle);
	}

	AbilitySystemComponent.Reset();
	QuickSlotComponent.Reset();
	InteractionComponent.Reset();
	
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	SnowGaugeChangedHandle.Reset();
	MaxSnowGaugeChangedHandle.Reset();
	FreezeGaugeChangedHandle.Reset();
	MaxFreezeGaugeChangedHandle.Reset();
	InteractionFocusChangedHandle.Reset();
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

	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, NewCurrentHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthRatio, NewHealthRatio);
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

	UE_MVVM_SET_PROPERTY_VALUE(SnowGauge, NewSnowGauge);
	UE_MVVM_SET_PROPERTY_VALUE(MaxSnowGauge, NewMaxSnowGauge);
	UE_MVVM_SET_PROPERTY_VALUE(SnowGaugeRatio, NewSnowGaugeRatio);
}

void UDRHUDViewModel::RefreshFreezeGauge()
{
	const UDRPlayerAttributeSet* AttributeSet = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>()
		: nullptr;
	const float NewFreezeGauge = IsValid(AttributeSet) ? AttributeSet->GetFreezeGauge() : 0.f;
	const float NewMaxFreezeGauge = IsValid(AttributeSet) ? AttributeSet->GetMaxFreezeGauge() : 0.f;
	const float NewFreezeGaugeRatio = NewMaxFreezeGauge > KINDA_SMALL_NUMBER
		? FMath::Clamp(NewFreezeGauge / NewMaxFreezeGauge, 0.f, 1.f)
		: 0.f;

	UE_MVVM_SET_PROPERTY_VALUE(FreezeGauge, NewFreezeGauge);
	UE_MVVM_SET_PROPERTY_VALUE(MaxFreezeGauge, NewMaxFreezeGauge);
	UE_MVVM_SET_PROPERTY_VALUE(FreezeGaugeRatio, NewFreezeGaugeRatio);
}

void UDRHUDViewModel::RefreshAmmoVisibility()
{
	FDRItemInstance SelectedItem;
	const int32 SelectedSlotIndex = QuickSlotComponent.IsValid()
		? QuickSlotComponent->GetSelectedSlotIndex()
		: INDEX_NONE;
	const bool bHasSelectedItem = QuickSlotComponent.IsValid()
		&& QuickSlotComponent->GetQuickSlot(SelectedSlotIndex, SelectedItem);
	const UDRProjectileWeaponItemDefinition* WeaponDefinition = bHasSelectedItem
		? Cast<UDRProjectileWeaponItemDefinition>(SelectedItem.Definition)
		: nullptr;

	UE_MVVM_SET_PROPERTY_VALUE(
		bIsAmmoVisible,
		IsValid(WeaponDefinition));
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

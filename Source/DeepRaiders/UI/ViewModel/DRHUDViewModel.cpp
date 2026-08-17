#include "DRHUDViewModel.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/Components/DRHealthComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

void UDRHUDViewModel::Initialize(ADRPlayerCharacter* InPlayerCharacter)
{
	Deinitialize();

	if (!IsValid(InPlayerCharacter))
	{
		return;
	}

	HealthComponent = InPlayerCharacter->GetHealthComponent();
	AbilitySystemComponent = InPlayerCharacter->GetAbilitySystemComponent();

	// 헬스 컴포넌트 HP 변경 구독
	if (HealthComponent.IsValid())
	{
		HealthComponent->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	}

	// GAS 델리게이트 연결
	if (AbilitySystemComponent.IsValid())
	{
		SnowGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetSnowGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleSnowGaugeChanged);
		MaxSnowGaugeChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute()).AddUObject(
				this, &ThisClass::HandleMaxSnowGaugeChanged);
	}

	// 최초 리프레쉬
	RefreshHealth();
	RefreshSnowGauge();
}

void UDRHUDViewModel::Deinitialize()
{
	if (HealthComponent.IsValid())
	{
		HealthComponent->OnHealthChanged.RemoveAll(this);
	}

	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetSnowGaugeAttribute()).Remove(SnowGaugeChangedHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute()).Remove(MaxSnowGaugeChangedHandle);
	}

	HealthComponent.Reset();
	AbilitySystemComponent.Reset();
	SnowGaugeChangedHandle.Reset();
	MaxSnowGaugeChangedHandle.Reset();
}

void UDRHUDViewModel::HandleHealthChanged(float OldHealth, float NewHealth)
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

void UDRHUDViewModel::RefreshHealth()
{
	const float NewCurrentHealth = HealthComponent.IsValid()
		? HealthComponent->GetCurrentHealth()
		: 0.f;
	const float NewMaxHealth = HealthComponent.IsValid() ? HealthComponent->GetMaxHealth() : 0.f;
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

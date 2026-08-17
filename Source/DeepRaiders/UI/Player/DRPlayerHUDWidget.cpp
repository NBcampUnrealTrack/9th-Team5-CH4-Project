#include "DRPlayerHUDWidget.h"

#include "AbilitySystemComponent.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

void UDRPlayerHUDWidget::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (BoundAbilitySystemComponent.Get() == InAbilitySystemComponent)
	{
		RefreshAll();
		return;
	}

	UnbindAttributeDelegates();

	BoundAbilitySystemComponent = InAbilitySystemComponent;

	if (!BoundAbilitySystemComponent.IsValid())
	{
		return;
	}

	BindAttributeDelegates();

	// Delegate는 "변경"될 때 호출되므로
	// 현재 값을 처음 한 번 직접 반영한다.
	RefreshAll();
}

void UDRPlayerHUDWidget::NativeDestruct()
{
	UnbindAttributeDelegates();

	Super::NativeDestruct();
}

void UDRPlayerHUDWidget::BindAttributeDelegates()
{
	UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();

	if (!IsValid(ASC))
	{
		return;
	}
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleMaxHealthChanged);

	FreezeGaugeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).AddUObject(this, &ThisClass::HandleFreezeGaugeChanged);
	MaxFreezeGaugeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute()).AddUObject(this, &ThisClass::HandleMaxFreezeGaugeChanged);

	SnowGaugeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetSnowGaugeAttribute()).AddUObject(this, &ThisClass::HandleSnowGaugeChanged);
	MaxSnowGaugeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute()).AddUObject(this, &ThisClass::HandleMaxSnowGaugeChanged);
}

void UDRPlayerHUDWidget::UnbindAttributeDelegates()
{
	UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();

	if (IsValid(ASC))
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		}
		
		if (FreezeGaugeChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetFreezeGaugeAttribute()).Remove(FreezeGaugeChangedHandle);
		}
		if (MaxFreezeGaugeChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute()).Remove(MaxFreezeGaugeChangedHandle);
		}
		
		if (SnowGaugeChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetSnowGaugeAttribute()).Remove(SnowGaugeChangedHandle);
		}
		if (MaxSnowGaugeChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute()).Remove(MaxSnowGaugeChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();

	FreezeGaugeChangedHandle.Reset();
	MaxFreezeGaugeChangedHandle.Reset();

	SnowGaugeChangedHandle.Reset();
	MaxSnowGaugeChangedHandle.Reset();

	BoundAbilitySystemComponent.Reset();
}

void UDRPlayerHUDWidget::RefreshHealth()
{
	const UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();

	if (!IsValid(ASC))
	{
		return;
	}

	const float CurrentValue = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetHealthAttribute());

	const float MaxValue = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetMaxHealthAttribute());

	if (IsValid(HealthGaugeBar))
	{
		HealthGaugeBar->SetPercent(CalculatePercent(CurrentValue, MaxValue));
	}

	if (IsValid(HealthGaugeText))
	{
		HealthGaugeText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentValue, MaxValue)));
	}
}

void UDRPlayerHUDWidget::RefreshFreezeGauge()
{
	const UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();

	if (!IsValid(ASC))
	{
		return;
	}

	const float CurrentValue = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetFreezeGaugeAttribute());

	const float MaxValue = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetMaxFreezeGaugeAttribute());

	if (IsValid(FreezeGaugeBar))
	{
		FreezeGaugeBar->SetPercent(CalculatePercent(CurrentValue, MaxValue));
	}

	if (IsValid(FreezeGaugeText))
	{
		FreezeGaugeText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentValue, MaxValue)));
	}
}

void UDRPlayerHUDWidget::RefreshSnowGauge()
{
	const UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();

	if (!IsValid(ASC))
	{
		return;
	}

	const float CurrentValue = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetSnowGaugeAttribute());

	const float MaxValue = ASC->GetNumericAttribute(UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute());

	if (IsValid(SnowGaugeBar))
	{
		SnowGaugeBar->SetPercent(CalculatePercent(CurrentValue, MaxValue));
	}

	if (IsValid(SnowGaugeText))
	{
		SnowGaugeText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentValue, MaxValue)));
	}
}

void UDRPlayerHUDWidget::RefreshAll()
{
	RefreshHealth();
	RefreshFreezeGauge();
	RefreshSnowGauge();
}

void UDRPlayerHUDWidget::HandleHealthChanged(const FOnAttributeChangeData&)
{
	RefreshHealth();
}

void UDRPlayerHUDWidget::HandleMaxHealthChanged(const FOnAttributeChangeData&)
{
	RefreshHealth();
}

void UDRPlayerHUDWidget::HandleFreezeGaugeChanged(const FOnAttributeChangeData&)
{
	RefreshFreezeGauge();
}

void UDRPlayerHUDWidget::HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData&)
{
	RefreshFreezeGauge();
}

void UDRPlayerHUDWidget::HandleSnowGaugeChanged(const FOnAttributeChangeData&)
{
	RefreshSnowGauge();
}

void UDRPlayerHUDWidget::HandleMaxSnowGaugeChanged(const FOnAttributeChangeData&)
{
	RefreshSnowGauge();
}

float UDRPlayerHUDWidget::CalculatePercent(float CurrentValue, float MaxValue)
{
	if (MaxValue <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::Clamp(CurrentValue / MaxValue, 0.f, 1.f);
}

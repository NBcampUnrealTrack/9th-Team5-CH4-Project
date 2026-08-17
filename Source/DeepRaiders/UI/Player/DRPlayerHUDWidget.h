#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPlayerHUDWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;

struct FOnAttributeChangeData;

UCLASS()
class DEEPRAIDERS_API UDRPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

protected:
	virtual void NativeDestruct() override;

private:
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();

	void RefreshHealth();
	void RefreshFreezeGauge();
	void RefreshSnowGauge();
	void RefreshAll();

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleFreezeGaugeChanged(const FOnAttributeChangeData& Data);
	void HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& Data);
	void HandleSnowGaugeChanged(const FOnAttributeChangeData& Data);
	void HandleMaxSnowGaugeChanged(const FOnAttributeChangeData& Data);

	static float CalculatePercent(float CurrentValue, float MaxValue);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthGaugeBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthGaugeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> FreezeGaugeBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> FreezeGaugeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> SnowGaugeBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SnowGaugeText;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	
	FDelegateHandle FreezeGaugeChangedHandle;
	FDelegateHandle MaxFreezeGaugeChangedHandle;

	FDelegateHandle SnowGaugeChangedHandle;
	FDelegateHandle MaxSnowGaugeChangedHandle;
};

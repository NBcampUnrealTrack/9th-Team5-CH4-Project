#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRHUDViewModel.generated.h"

class ADRPlayerCharacter;
class UAbilitySystemComponent;
class UDRHealthComponent;
struct FOnAttributeChangeData;

/** 플레이어의 체력과 눈 게이지를 HUD 바인딩용 값으로 제공한다. */
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
	float SnowGauge = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Snow")
	float MaxSnowGauge = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Snow")
	float SnowGaugeRatio = 0.f;

private:
	void HandleHealthChanged(float OldHealth, float NewHealth);
	void HandleSnowGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxSnowGaugeChanged(const FOnAttributeChangeData& ChangeData);
	void RefreshHealth();
	void RefreshSnowGauge();

	TWeakObjectPtr<UDRHealthComponent> HealthComponent;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	FDelegateHandle SnowGaugeChangedHandle;
	FDelegateHandle MaxSnowGaugeChangedHandle;
};

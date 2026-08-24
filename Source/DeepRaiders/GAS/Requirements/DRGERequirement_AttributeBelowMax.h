#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayEffectCustomApplicationRequirement.h"
#include "DRGERequirement_AttributeBelowMax.generated.h"

/*
 * 지정한 현재 Attribute가 최대 Attribute보다 작을 때만 GE 적용을 허용한다.
 * 
 * 회복 GE에서 Health와 MaxHealth를 설정하면 최대 체력일 때
 * GE 적용과 소모품 소비를 막을 수 있다.
 */
UCLASS(Blueprintable)
class DEEPRAIDERS_API UDRGERequirement_AttributeBelowMax : public UGameplayEffectCustomApplicationRequirement
{
	GENERATED_BODY()
	
public:
	virtual bool CanApplyGameplayEffect_Implementation(const UGameplayEffect* GameplayEffect,
		const FGameplayEffectSpec& Spec, UAbilitySystemComponent* ASC) const override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirement")
	FGameplayAttribute CurrentAttribute;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirement")
	FGameplayAttribute MaxAttribute;
	
};

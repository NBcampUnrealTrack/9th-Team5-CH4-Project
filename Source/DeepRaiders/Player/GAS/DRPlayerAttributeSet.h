#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRPlayerAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

struct FGameplayEffectModCallbackData;

UCLASS()
class DEEPRAIDERS_API UDRPlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UDRPlayerAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, Health)
	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, MaxHealth)

	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, FreezeGauge)

	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, SnowGauge)
	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, MaxSnowGauge)
	
	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, IncomingDamage)
	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, MoveSpeedMultiplier)

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Player|Health")
	FGameplayAttributeData Health;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Player|Health")
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FreezeGauge, Category = "Player|Freeze")
	FGameplayAttributeData FreezeGauge;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SnowGauge, Category = "Player|Snow")
	FGameplayAttributeData SnowGauge;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSnowGauge, Category = "Player|Snow")
	FGameplayAttributeData MaxSnowGauge;
	UPROPERTY(BlueprintReadOnly, Category = "Player|Meta")
	FGameplayAttributeData IncomingDamage;

	UPROPERTY(
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_MoveSpeedMultiplier,
		Category = "Player|Movement")
	FGameplayAttributeData MoveSpeedMultiplier;
	
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_FreezeGauge(const FGameplayAttributeData& OldFreezeGauge);

	UFUNCTION()
	void OnRep_SnowGauge(const FGameplayAttributeData& OldSnowGauge);
	UFUNCTION()
	void OnRep_MaxSnowGauge(const FGameplayAttributeData& OldMaxSnowGauge);
	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(
		const FGameplayAttributeData& OldMoveSpeedMultiplier);
	
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	void ClampAttributeValue(const FGameplayAttribute& Attribute, float& NewValue) const;
	
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};

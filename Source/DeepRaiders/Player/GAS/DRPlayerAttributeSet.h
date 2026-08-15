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

UCLASS()
class DEEPRAIDERS_API UDRPlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UDRPlayerAttributeSet();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, SnowGauge)
	ATTRIBUTE_ACCESSORS(UDRPlayerAttributeSet, MaxSnowGauge)

protected:
	UPROPERTY(
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_SnowGauge,
		Category = "Player|Snow")
	FGameplayAttributeData SnowGauge;

	UPROPERTY(
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_MaxSnowGauge,
		Category = "Player|Snow")
	FGameplayAttributeData MaxSnowGauge;

	UFUNCTION()
	void OnRep_SnowGauge(
		const FGameplayAttributeData& OldSnowGauge);

	UFUNCTION()
	void OnRep_MaxSnowGauge(
		const FGameplayAttributeData& OldMaxSnowGauge);
};
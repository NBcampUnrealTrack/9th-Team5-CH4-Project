#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGA_UseConsumableEffects.generated.h"

class UDRConsumableItemDefinition;
class UDRInventoryComponent;

/*
 * 선택된 버프,회복형 소모품의 UseEffects를 사용자 본인에게 적용
 * 
 * 하나 이상의 GE가 실제 적용된 경우에만 서버에서
 * 선택된 ItemInstance 수량을 하나 제거한다.
 */
UCLASS()
class DEEPRAIDERS_API UDRGA_UseConsumableEffects : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UDRGA_UseConsumableEffects();
	
protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
private:
	bool HasValidUseEffect(const UDRConsumableItemDefinition* ConsumableDefinition) const;
	
	bool ResolveSelectedConsumable(const FGameplayAbilityActorInfo* ActorInfo,
		const UDRConsumableItemDefinition* ExpectedDefinition,
		UDRInventoryComponent*& OutInventoryComponent, FGuid& OutInstanceId) const;
	
	int32 ApplyUseEffects(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const UDRConsumableItemDefinition* ConsumableDefinition) const;
};

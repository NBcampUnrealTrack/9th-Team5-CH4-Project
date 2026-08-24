#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DRConsumableItemDefinition.generated.h"

/*
 * 사용자 본인에게 GameplayEffect를 적용하는 소모성 아이템 Definition
 * 
 * 사용 Ability는 기존 ItemAbilitySet을 통해 부여하고,
 * 실제 사용 시 적용할 Effect는 UseEffects에 별도로 설정한다. 
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRConsumableItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()
	
public:
	UDRConsumableItemDefinition();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effects")
	TArray<FDRGameplayEffectData> UseEffects;
	
};

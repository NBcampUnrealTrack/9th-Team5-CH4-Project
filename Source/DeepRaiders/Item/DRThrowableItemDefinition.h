#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DRThrowableItemTypes.h"
#include "DRThrowableItemDefinition.generated.h"

/* 
 * 투척하여 충돌 지점 주변 대상에게 효과를 적용하는 소모성 아이템 Definition
 */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRThrowableItemDefinition : public UDRItemDefinition
{
	GENERATED_BODY()
	
public:
	UDRThrowableItemDefinition();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable", meta = (ShowOnlyInnerProperties))
	FDRThrowableItemSettings ThrowSettings;	
};

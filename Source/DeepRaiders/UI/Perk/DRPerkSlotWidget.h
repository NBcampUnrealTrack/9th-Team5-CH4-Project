#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPerkSlotWidget.generated.h"

class UDRPerkDefinition;
class UImage;

/** 디자이너에서 구성한 퍽 슬롯의 아이콘만 갱신한다. */
UCLASS()
class DEEPRAIDERS_API UDRPerkSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 퍽 정의의 아이콘을 표시하며, 정의나 아이콘이 없으면 빈 슬롯으로 만든다. */
	void SetPerkDefinition(const UDRPerkDefinition* PerkDefinition);

private:
	/** 슬롯 배경과 별개로 구매한 퍽 아이콘만 표시한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PerkIcon;
};

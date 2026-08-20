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
	void SetPerkDefinition(const UDRPerkDefinition* PerkDefinition);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PerkIcon;
};

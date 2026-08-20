#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPerkWidget.generated.h"

class UDRPerkComponent;
class UDRPerkSlotWidget;

/** 미리 배치된 슬롯에 현재 보유 퍽 아이콘을 표시한다. */
UCLASS()
class DEEPRAIDERS_API UDRPerkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializePerks(UDRPerkComponent* NewPerkComponent);

protected:
	virtual void NativeDestruct() override;

private:
	void CachePerkSlots();

	UFUNCTION()
	void RefreshPerks();

	void UnbindPerkComponent();

	UPROPERTY(Transient)
	TObjectPtr<UDRPerkComponent> PerkComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRPerkSlotWidget>> PerkSlots;
};

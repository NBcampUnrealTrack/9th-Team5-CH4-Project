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
	/** 표시할 플레이어 퍽 컴포넌트를 연결하고 현재 슬롯 상태를 갱신한다. */
	void InitializePerks(UDRPerkComponent* NewPerkComponent);

protected:
	/** 위젯 제거 시 퍽 변경 이벤트와 캐시를 정리한다. */
	virtual void NativeDestruct() override;

private:
	/** 디자이너에서 미리 배치한 모든 퍽 슬롯을 최초 한 번만 수집한다. */
	void CachePerkSlots();

	/** 전체 슬롯 수와 보유 퍽 순서에 맞춰 슬롯 표시와 아이콘을 갱신한다. */
	UFUNCTION()
	void RefreshPerks();

	/** 기존 퍽 컴포넌트의 변경 이벤트 연결을 해제한다. */
	void UnbindPerkComponent();

	/** 현재 표시 중인 플레이어의 퍽 상태다. */
	UPROPERTY(Transient)
	TObjectPtr<UDRPerkComponent> PerkComponent;

	/** 위젯 블루프린트에 미리 배치된 퍽 슬롯 캐시다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRPerkSlotWidget>> PerkSlots;
};

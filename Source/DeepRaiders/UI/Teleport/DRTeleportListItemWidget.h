#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "DRTeleportListItemWidget.generated.h"

class ADRTeleportPoint;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRTeleportListItemSelectedSignature, ADRTeleportPoint*, TeleportPoint);

UCLASS()
class DEEPRAIDERS_API UDRTeleportListItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTeleportPoint(ADRTeleportPoint* NewTeleportPoint);

	UFUNCTION(BlueprintCallable, Category = "Teleport|UI")
	void SetSelected(bool bNewSelected);

	UFUNCTION(BlueprintPure, Category = "Teleport|UI")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "Teleport|UI")
	ADRTeleportPoint* GetTeleportPoint() const { return TeleportPoint; }

	UPROPERTY(BlueprintAssignable, Category = "Teleport|UI")
	FDRTeleportListItemSelectedSignature OnTeleportSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Teleport|UI")
	void BP_OnSelectedChanged(bool bNewSelected);

private:
	void ApplyTeleportPoint();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_Name;

	UPROPERTY(Transient)
	TObjectPtr<ADRTeleportPoint> TeleportPoint;

	bool bIsWidgetConstructed = false;
	bool bIsSelected = false;
};

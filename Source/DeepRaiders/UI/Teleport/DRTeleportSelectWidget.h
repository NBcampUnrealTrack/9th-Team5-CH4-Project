// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRTeleportSelectWidget.generated.h"

class ADRTeleportPoint;
class UButton;
class UDRTeleportListItemWidget;
class UScrollBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRTeleportDestinationSelectedSignature, ADRTeleportPoint*, DestinationTeleportPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRTeleportCloseRequestedSignature);

UCLASS()
class DEEPRAIDERS_API UDRTeleportSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Teleport|UI")
	void InitializeTeleportList(ADRTeleportPoint* NewCurrentTeleportPoint, const TArray<ADRTeleportPoint*>& NewDestinationTeleportPoints);

	UFUNCTION(BlueprintCallable, Category = "Teleport|UI")
	void InitializeRegisteredTeleportList(int32 TeamId, ADRTeleportPoint* NewCurrentTeleportPoint);

	UPROPERTY(BlueprintAssignable, Category = "Teleport|UI")
	FDRTeleportDestinationSelectedSignature OnDestinationSelected;

	UPROPERTY(BlueprintAssignable, Category = "Teleport|UI")
	FDRTeleportCloseRequestedSignature OnCloseRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void RefreshCurrentTeleport();
	void RefreshDestinationList();
	void ClearSelectedDestination();
	UDRTeleportListItemWidget* FindItemWidgetByTeleportPoint(ADRTeleportPoint* TeleportPoint) const;

	UFUNCTION()
	void HandleDestinationItemSelected(ADRTeleportPoint* DestinationTeleportPoint);

	UFUNCTION()
	void HandleCloseButtonClicked();
	
	UFUNCTION()
	void HandleConfirmButtonClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_CurrentTeleportName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_Destination;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Close;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm;

	UPROPERTY(EditDefaultsOnly, Category = "Teleport|UI")
	TSubclassOf<UDRTeleportListItemWidget> DestinationItemWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ADRTeleportPoint> CurrentTeleportPoint;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADRTeleportPoint>> DestinationTeleportPoints;

	UPROPERTY(Transient)
	TObjectPtr<UDRTeleportListItemWidget> SelectedDestinationItemWidget;

	UPROPERTY(Transient)
	TObjectPtr<ADRTeleportPoint> SelectedDestinationTeleportPoint;

};

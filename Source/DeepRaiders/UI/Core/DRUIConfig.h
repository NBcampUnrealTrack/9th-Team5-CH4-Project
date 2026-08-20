#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRUIConfig.generated.h"

class UDRInventoryWidget;
class UDRQuickSlotWidget;
class UDRShopWidget;
class UDRTeleportSelectWidget;
class UUserWidget;

/** 위젯이 표시될 UI 레이어다. */
UENUM(BlueprintType)
enum class EDRUILayer : uint8
{
	HUD,
	Menu,
	Modal
};

/** 로컬 플레이어 UI에서 사용하는 위젯 클래스와 공통 설정이다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRUIConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UDRInventoryWidget> PlayerInventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	EDRUILayer PlayerInventoryLayer = EDRUILayer::Menu;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UDRInventoryWidget> StorageInventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	EDRUILayer StorageInventoryLayer = EDRUILayer::Menu;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QuickSlot")
	TSubclassOf<UDRQuickSlotWidget> QuickSlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QuickSlot")
	EDRUILayer QuickSlotLayer = EDRUILayer::HUD;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	EDRUILayer HUDLayer = EDRUILayer::HUD;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|MVVM")
	FName HUDViewModelName = TEXT("DRHUDViewModel");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	EDRUILayer ShopLayer = EDRUILayer::Menu;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	TSubclassOf<UDRTeleportSelectWidget> TeleportSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	EDRUILayer TeleportLayer = EDRUILayer::Modal;
};

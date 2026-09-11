#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
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
	VFX,
	Menu,
	Modal
};

/** 화면 태그에 대응하는 위젯 클래스와 표시 레이어다. */
USTRUCT(BlueprintType)
struct FDRUIScreenDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EDRUILayer Layer = EDRUILayer::Menu;

	/** 레이어 기준 ZOrder에 더할 표시 순서 오프셋이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 Order = 0;
};

/** 로컬 플레이어 UI에서 사용하는 위젯 클래스와 공통 설정이다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRUIConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Screens")
	TMap<FGameplayTag, FDRUIScreenDefinition> Screens;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|MVVM")
	FName HUDViewModelName = TEXT("DRHUDViewModel");

	/** HP, Freeze 및 Shield 배치 계산이 공통으로 사용하는 상태 게이지 기준 폭이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Status", meta = (ClampMin = "0.0"))
	float StatusGaugeWidth = 465.f;

	/** 상태 게이지 내부 SizeBox들이 공통으로 사용하는 Overlay Slot Padding이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Status")
	FMargin StatusGaugeSlotPadding = FMargin(12.f, 5.f, 26.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<UDRShopWidget> ShopWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	EDRUILayer ShopLayer = EDRUILayer::Menu;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	TSubclassOf<UDRTeleportSelectWidget> TeleportSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	EDRUILayer TeleportLayer = EDRUILayer::Modal;
	const FDRUIScreenDefinition* FindScreen(FGameplayTag ScreenTag) const
	{
		return Screens.Find(ScreenTag);
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoreboard|MVVM")
	FName ScoreboardViewModelName = TEXT("DRScoreboardViewModel");
};

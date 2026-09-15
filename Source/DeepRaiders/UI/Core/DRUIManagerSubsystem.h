#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DRUIManagerSubsystem.generated.h"

class APlayerController;
class UDRUIConfig;
class UUserWidget;
class IInputProcessor;
struct FKeyEvent;
enum class EDRUILayer : uint8;

/** 로컬 플레이어의 UI 설정과 생성된 위젯 생명주기를 관리한다. */
UCLASS()
class DEEPRAIDERS_API UDRUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void Configure(APlayerController* InPlayerController, UDRUIConfig* InUIConfig);

	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* PushScreen(FGameplayTag ScreenTag);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void PopScreen(FGameplayTag ScreenTag);

	/** HUD/VFX는 유지하고 가장 위의 Menu/Modal만 닫는다. */
	UFUNCTION(BlueprintCallable, Category = "UI")
	bool PopTopScreen();

	bool HandleEscapeKey(const FKeyEvent& KeyEvent);

	/** 화면 또는 열린 자식 팝업의 기존 종료 함수를 등록한다. */
	void RegisterCloseHandler(UUserWidget* Widget, FSimpleDelegate Handler);
	void UnregisterCloseHandler(UUserWidget* Widget);

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsScreenOpen(FGameplayTag ScreenTag) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	UUserWidget* GetScreen(FGameplayTag ScreenTag) const;

	UUserWidget* CreateManagedWidget(
		TSubclassOf<UUserWidget> WidgetClass,
		EDRUILayer Layer,
		int32 Order = 0);
	void SetManagedWidgetVisible(UUserWidget* Widget, bool bVisible);

	/** 입력 모드, 포커스, 커서를 변경하지 않고 표시 여부만 바꾼다. */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetManagedWidgetVisibilityOnly(UUserWidget* Widget, bool bVisible);

	void ReleaseManagedWidget(UUserWidget* Widget);

	const UDRUIConfig* GetUIConfig() const { return UIConfig; }

private:
	struct FCloseTarget
	{
		TWeakObjectPtr<UUserWidget> Widget;
		FSimpleDelegate Handler;
	};

	TArray<FCloseTarget> CloseTargets;
	TSharedPtr<IInputProcessor> EscapeInputProcessor;
	void ClearManagedWidgets();
	void RefreshInputMode();
	UUserWidget* GetTopScreen() const;
	static int32 GetLayerZOrder(EDRUILayer Layer);

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRUIConfig> UIConfig;

	// 사용중인 위젯
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> ManagedWidgets;

	TMap<TWeakObjectPtr<UUserWidget>, EDRUILayer> WidgetLayers;
	TMap<TWeakObjectPtr<UUserWidget>, int32> WidgetZOrders;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UUserWidget>> ActiveScreens;
};

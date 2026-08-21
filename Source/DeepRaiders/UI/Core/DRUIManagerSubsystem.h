#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DRUIManagerSubsystem.generated.h"

class APlayerController;
class UDRUIConfig;
class UUserWidget;
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

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsScreenOpen(FGameplayTag ScreenTag) const;

	UFUNCTION(BlueprintPure, Category = "UI")
	UUserWidget* GetScreen(FGameplayTag ScreenTag) const;

	UUserWidget* CreateManagedWidget(
		TSubclassOf<UUserWidget> WidgetClass,
		EDRUILayer Layer);
	void SetManagedWidgetVisible(UUserWidget* Widget, bool bVisible);
	void ReleaseManagedWidget(UUserWidget* Widget);

	const UDRUIConfig* GetUIConfig() const { return UIConfig; }

private:
	void RefreshInputMode();
	static int32 GetLayerZOrder(EDRUILayer Layer);

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRUIConfig> UIConfig;

	// 사용중인 위젯
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> ManagedWidgets;

	TMap<TWeakObjectPtr<UUserWidget>, EDRUILayer> WidgetLayers;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UUserWidget>> ActiveScreens;
};

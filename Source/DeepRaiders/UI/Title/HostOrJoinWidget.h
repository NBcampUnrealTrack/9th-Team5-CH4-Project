#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HostOrJoinWidget.generated.h"

class UDRTitleJoinWidget;
class UDRTitleMapChoiceWidget;
class UDRTitleSettingsWidget;
class UDRRoomServiceWidget;
class UDRCreateRoomWidget;

UCLASS()
class DEEPRAIDERS_API UHostOrJoinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandlePublicMatchClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandlePrivateCreateClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandlePrivateMatchClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleSettingsClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleExitGameClicked();

	// 기존 WBP 이벤트가 남아 있어도 분리된 패널로 전달한다.
	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleCreateMapClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleCloseChoiceMapClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleJoinClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleCloseJoinClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleSettingsApplyClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleSettingsCancelClicked();

protected:
	virtual void NativeConstruct() override;
	// Title 루트는 유지하고 메뉴 영역만 숨긴다. 두 화면은 이 영역 밖의 형제다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Overlay_Title;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRRoomServiceWidget> WBP_RoomService;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRCreateRoomWidget> WBP_CreateRoom;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleJoinWidget> WBP_Join;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleSettingsWidget> WBP_Settings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleMapChoiceWidget> WBP_ChoiceMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session")
	FString DedicatedServerAddress = TEXT("shees95.myddns.me:17777");
	//FString DedicatedServerAddress = TEXT("katherine-fc.tun.ply.gg:55341");

};

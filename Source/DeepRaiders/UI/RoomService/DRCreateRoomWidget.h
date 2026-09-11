#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/UI/Title/DRTitleMapChoiceWidget.h"
#include "RoomServiceTypes.h"
#include "DRCreateRoomWidget.generated.h"

class UCheckBox;
class UEditableTextBox;
class UDRRoomServiceWidget;
class UTextBlock;
class UDRTitleTextSettingRowWidget;

// WBP_CreateRoom은 기존 맵 선택 UI에 제목/Private 입력만 추가한다.
UCLASS()
class DEEPRAIDERS_API UDRCreateRoomWidget : public UDRTitleMapChoiceWidget
{
	GENERATED_BODY()

public:
	void Open(UDRRoomServiceWidget* InOwner);
	void SetFeedback(const FText& Message);
	void SetSubmitting(bool bSubmitting);
	virtual void HandleCreateMapClicked() override;
	virtual void HandleCloseChoiceMapClicked() override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rooms")
	void OnFeedbackChanged(const FText& Message);
	// 일반 입력창 또는 기존 텍스트 입력 행 중 하나를 배치한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomTitleInput;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleTextSettingRowWidget> RoomTitleRow;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> PrivateCheckBox;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(EditDefaultsOnly, Category = "Rooms", meta = (ClampMin = "0"))
	int32 MaxRoomPlayers = 0;

private:
	TWeakObjectPtr<UDRRoomServiceWidget> RoomOwner;
};

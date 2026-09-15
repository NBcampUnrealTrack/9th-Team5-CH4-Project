#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRTitleTextSettingRowWidget.generated.h"

class UEditableTextBox;
class UTextBlock;

/** 타이틀 설정의 이름과 텍스트 입력 값을 한 행으로 관리한다. */
UCLASS()
class DEEPRAIDERS_API UDRTitleTextSettingRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativePreConstruct() override;

	void SetSettingText(const FText& NewText);
	FText GetSettingText() const;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ListName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	FText InName;

	/** 디자이너 미리보기와 초기 표시용 값이다. 런타임 설정값은 상위 SettingsWidget이 덮어쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	FText InText;
};

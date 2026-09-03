#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "DRTitleComboBoxSettingRowWidget.generated.h"

class UTextBlock;

UENUM(BlueprintType)
enum class EDRTitleComboBoxSettingType : uint8
{
	ScreenMode,
	Resolution
};

/** 타이틀 설정의 이름과 문자열 선택 목록을 한 행으로 관리한다. */
UCLASS()
class DEEPRAIDERS_API UDRTitleComboBoxSettingRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	void SetOptions(const TArray<FString>& NewOptions);
	void SetSelectedIndex(int32 NewIndex);
	int32 GetSelectedIndex() const;

	EDRTitleComboBoxSettingType GetSettingType() const { return SettingType; }

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ListName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> ComboBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	EDRTitleComboBoxSettingType SettingType = EDRTitleComboBoxSettingType::ScreenMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	FText InName;

private:
	UFUNCTION()
	void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	TArray<FString> Options;
	int32 SelectedIndex = INDEX_NONE;
};

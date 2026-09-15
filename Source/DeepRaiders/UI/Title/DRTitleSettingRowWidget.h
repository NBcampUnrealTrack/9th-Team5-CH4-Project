#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRTitleSettingRowWidget.generated.h"

class USlider;
class UTextBlock;

UENUM(BlueprintType)
enum class EDRTitleSettingType : uint8
{
	MasterVolume,
	MusicVolume,
	SFXVolume,
	MouseSensitivityX,
	MouseSensitivityY
};

/** 타이틀 설정의 이름, 슬라이더, 현재 값을 한 행으로 관리한다. */
UCLASS()
class DEEPRAIDERS_API UDRTitleSettingRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	void SetSettingValue(float NewValue);
	float GetSettingValue() const;

	EDRTitleSettingType GetSettingType() const { return SettingType; }

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ListName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> ListSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ListVolumn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	EDRTitleSettingType SettingType = EDRTitleSettingType::MasterVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	FText InName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	float InSliderValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setting")
	float MaxValue = 100.f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Setting",
		meta = (ClampMin = "0", ClampMax = "3"))
	int32 FractionalDigits = 0;

private:
	UFUNCTION()
	void HandleSliderValueChanged(float NormalizedValue);

	void RefreshValueText(float Value);
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPointLocationWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class DEEPRAIDERS_API UDRPointLocationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이미지에 설정된 투명도는 유지하고 팀 색상만 변경한다.
	UFUNCTION(BlueprintCallable, Category = "Point Location")
	void SetIndicatorColor(const FLinearColor& TeamColor);

	UFUNCTION(BlueprintCallable, Category = "Point Location")
	void SetDisplayName(const FText& DisplayName);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PointIndicator_Back;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PointIndicator_Front;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PointIndicator_Text;

private:
	static void ApplyColorPreservingAlpha(UImage* Image, const FLinearColor& TeamColor);
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPointLocationWidget.generated.h"

class UImage;

UCLASS()
class DEEPRAIDERS_API UDRPointLocationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이미지에 설정된 투명도는 유지하고 팀 색상만 변경한다.
	UFUNCTION(BlueprintCallable, Category = "Point Location")
	void SetIndicatorColor(const FLinearColor& TeamColor);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PointIndicator_Back;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PointIndicator_Front;

private:
	static void ApplyColorPreservingAlpha(UImage* Image, const FLinearColor& TeamColor);
};

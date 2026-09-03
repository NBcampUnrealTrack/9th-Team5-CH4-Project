#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Styling/SlateColor.h"
#include "DRPlayerNameplateViewModel.generated.h"

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRPlayerNameplateViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetDisplayData(const FText& InDisplayName, const FLinearColor& InNameColor);

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Nameplate")
	FText DisplayName;

	/*
	 * TextBlock.ColorAndOpacity의 타입과 일치시킨다.
	 * 게임 쪽 팀 색은 FLinearColor로 받고,
	 * UI 경계에서 FSlateColor로 변환한다.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Nameplate")
	FSlateColor NameColor = FSlateColor(FLinearColor::White);
};

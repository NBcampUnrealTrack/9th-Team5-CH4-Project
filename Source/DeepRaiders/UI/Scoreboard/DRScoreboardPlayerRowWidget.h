#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRScoreboardPlayerRowWidget.generated.h"


/**
 * 스코어보드의 플레이어 한 줄 Widget.
 *
 * 실제 Text 표시 등은 Widget Blueprint + MVVM이 담당한다.
 */
UCLASS()
class DEEPRAIDERS_API UDRScoreboardPlayerRowWidget
	: public UUserWidget
{
	GENERATED_BODY()
};
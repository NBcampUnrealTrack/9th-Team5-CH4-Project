#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPlayerNameplateWidget.generated.h"

/**
 * 플레이어 머리 위 이름표 Widget의 C++ 기반 클래스.
 *
 * 실제 TextBlock 구성과 MVVM Binding은
 * WBP_PlayerNameplate가 담당한다.
 */
UCLASS()
class DEEPRAIDERS_API UDRPlayerNameplateWidget
	: public UUserWidget
{
	GENERATED_BODY()
};
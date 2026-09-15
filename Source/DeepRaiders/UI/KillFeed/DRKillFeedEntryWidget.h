#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRKillFeedEntryWidget.generated.h"

/**
 * 킬피드 한 줄 Widget.
 * 실제 표시 내용은 WBP + MVVM binding이 담당한다.
 */
UCLASS()
class DEEPRAIDERS_API UDRKillFeedEntryWidget : public UUserWidget
{
	GENERATED_BODY()
};

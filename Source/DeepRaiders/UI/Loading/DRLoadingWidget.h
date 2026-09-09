#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRLoadingWidget.generated.h"

class UDataTable;
class UImage;

/** 현재 맵의 Title 정의 이미지를 표시하는 Loading 화면이다. */
UCLASS()
class DEEPRAIDERS_API UDRLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Background;

	// Title 맵 선택 화면과 같은 MapDefinition DataTable을 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading|Map")
	TObjectPtr<UDataTable> MapDefinitionTable;

private:
	void RefreshBackgroundImage();
};

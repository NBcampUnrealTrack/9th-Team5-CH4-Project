#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "DRTitleMapChoiceWidget.generated.h"

class UDataTable;
class UImage;
class UOverlay;
class UTexture2D;
class UWidget;
class UWorld;

UCLASS()
class DEEPRAIDERS_API UDRTitleMapChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	void Show();

	UFUNCTION(BlueprintCallable, Category = "Title|Map")
	virtual void HandleCreateMapClicked();

	UFUNCTION(BlueprintCallable, Category = "Title|Map")
	virtual void HandleCloseChoiceMapClicked();

protected:
	// 생성 화면은 기존 맵 선택/미리보기를 재사용하고 생성 방식만 교체한다.
	TSoftObjectPtr<UWorld> GetSelectedPlayMap() const
	{
		return SelectedPlayMap;
	}

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_ChoiceMap;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> ComboBoxString_ChoiceMap;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CreateMap;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ChoosedImageMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session")
	TObjectPtr<UDataTable> MapDefinitionTable;

private:
	UFUNCTION()
	void HandleMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void RefreshMapOptions();
	void SelectMapDefinition(int32 DefinitionIndex);
	void SelectPlayMap(TSoftObjectPtr<UWorld> InPlayMap, UTexture2D* InPreview);

	TArray<FName> MapDefinitionRowNames;

	UPROPERTY(Transient)
	TSoftObjectPtr<UWorld> SelectedPlayMap;
};

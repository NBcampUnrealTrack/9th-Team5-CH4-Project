#include "DRTitleMapChoiceWidget.h"

#include "DRTitleMapDefinition.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

bool UDRTitleMapChoiceWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (IsDesignTime())
	{
		return true;
	}

	Overlay_ChoiceMap->SetVisibility(ESlateVisibility::Collapsed);
	ComboBoxString_ChoiceMap->OnSelectionChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleMapSelectionChanged);
	RefreshMapOptions();
	return true;
}

void UDRTitleMapChoiceWidget::Show()
{
	Overlay_ChoiceMap->SetVisibility(ESlateVisibility::Visible);
	SelectMapDefinition(ComboBoxString_ChoiceMap->GetSelectedIndex());
}

void UDRTitleMapChoiceWidget::HandleMapSelectionChanged(
	FString SelectedItem,
	ESelectInfo::Type SelectionType)
{
	(void)SelectedItem;
	(void)SelectionType;
	SelectMapDefinition(ComboBoxString_ChoiceMap->GetSelectedIndex());
}

void UDRTitleMapChoiceWidget::HandleCreateMapClicked()
{
	if (SelectedPlayMap.IsNull())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(GameInstance))
	{
		if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
		{
			SessionSubsystem->CreateListenServerSession(SelectedPlayMap);
		}
	}
}

void UDRTitleMapChoiceWidget::HandleCloseChoiceMapClicked()
{
	Overlay_ChoiceMap->SetVisibility(ESlateVisibility::Collapsed);
}

void UDRTitleMapChoiceWidget::RefreshMapOptions()
{
	ComboBoxString_ChoiceMap->ClearOptions();
	MapDefinitionRowNames.Reset();

	if (!IsValid(MapDefinitionTable))
	{
		CreateMap->SetIsEnabled(false);
		return;
	}

	TArray<FName> DefinitionRowNames = MapDefinitionTable->GetRowNames();
	DefinitionRowNames.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName RowName : DefinitionRowNames)
	{
		const FDRTitleMapDefinition* Definition = MapDefinitionTable->FindRow<FDRTitleMapDefinition>(
			RowName,
			TEXT("Populate title map options"));
		if (Definition == nullptr)
		{
			continue;
		}

		const FString OptionName = Definition->DisplayName.IsEmpty()
			? RowName.ToString()
			: Definition->DisplayName.ToString();
		MapDefinitionRowNames.Add(RowName);
		ComboBoxString_ChoiceMap->AddOption(OptionName);
	}

	if (ComboBoxString_ChoiceMap->GetOptionCount() > 0)
	{
		ComboBoxString_ChoiceMap->SetSelectedIndex(0);
		SelectMapDefinition(0);
		return;
	}

	CreateMap->SetIsEnabled(false);
}

void UDRTitleMapChoiceWidget::SelectMapDefinition(int32 DefinitionIndex)
{
	if (!MapDefinitionRowNames.IsValidIndex(DefinitionIndex) || !IsValid(MapDefinitionTable))
	{
		SelectPlayMap(TSoftObjectPtr<UWorld>(), nullptr);
		return;
	}

	const FDRTitleMapDefinition* Definition = MapDefinitionTable->FindRow<FDRTitleMapDefinition>(
		MapDefinitionRowNames[DefinitionIndex],
		TEXT("Select title map"));
	if (Definition == nullptr)
	{
		SelectPlayMap(TSoftObjectPtr<UWorld>(), nullptr);
		return;
	}

	SelectPlayMap(Definition->Map, Definition->PreviewImage.LoadSynchronous());
}

void UDRTitleMapChoiceWidget::SelectPlayMap(
	TSoftObjectPtr<UWorld> InPlayMap,
	UTexture2D* InPreview)
{
	SelectedPlayMap = InPlayMap;
	CreateMap->SetIsEnabled(!SelectedPlayMap.IsNull());
	ChoosedImageMap->SetBrushFromTexture(InPreview);
}

#include "DRLoadingWidget.h"

#include "Components/Image.h"
#include "DeepRaiders/UI/Title/DRTitleMapDefinition.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

bool UDRLoadingWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!IsDesignTime())
	{
		RefreshBackgroundImage();
	}
	return true;
}

void UDRLoadingWidget::RefreshBackgroundImage()
{
	UWorld* World = GetWorld();
	if (!IsValid(Image_Background) || !IsValid(MapDefinitionTable) || !IsValid(World))
	{
		return;
	}

	FString CurrentMapName = World->GetMapName();
	CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);
	for (const FName RowName : MapDefinitionTable->GetRowNames())
	{
		const FDRTitleMapDefinition* Definition = MapDefinitionTable->FindRow<FDRTitleMapDefinition>(
			RowName, TEXT("Set loading map image"));
		if (Definition == nullptr
			|| Definition->Map.ToSoftObjectPath().GetAssetName() != CurrentMapName)
		{
			continue;
		}

		Image_Background->SetBrushFromTexture(Definition->PreviewImage.LoadSynchronous());
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("Loading image definition was not found for map %s."), *CurrentMapName);
}

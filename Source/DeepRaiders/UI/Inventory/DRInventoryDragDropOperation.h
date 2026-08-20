#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "DRInventoryDragDropOperation.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SourceSlotIndex = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGuid SourceInstanceId;	
};
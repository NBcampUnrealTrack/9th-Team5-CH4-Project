#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"
#include "DRItemDataParser.generated.h"

class UDataTable;

const FString DRITEM_ASSET_PATH = TEXT("/Game/DeepRaiders/Data/DataAssets/Item");
const FString DRITEM_NAME_FORMAT = TEXT("DA_DR{0}");

UCLASS(EditInlineNew)
class DEEPRAIDERSEDITOR_API UDRItemDataParser : public UGoogleSheetParserBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<UDataTable> TargetTable;

protected:
	virtual bool OnParseComplete(FString& OutError) override;
};
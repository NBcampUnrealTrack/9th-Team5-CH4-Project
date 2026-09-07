#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"
#include "DRItemDataParser.generated.h"

class UDataTable;

UCLASS(EditInlineNew)
class DEEPRAIDERSEDITOR_API UDRItemDataParser : public UGoogleSheetParserBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<UDataTable> TargetTable;

	UPROPERTY(EditAnywhere, Category = "Output", meta = (
		DisplayName = "Target Asset Root Folder",
		ToolTip = "DA_DR{RowName} Item Definition을 재귀 검색할 Content 루트 폴더입니다."))
	FString TargetAssetFolder;

protected:
	virtual bool OnParseComplete(FString& OutError) override;
};

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

protected:
	virtual bool OnParseComplete(FString& OutError) override;
};
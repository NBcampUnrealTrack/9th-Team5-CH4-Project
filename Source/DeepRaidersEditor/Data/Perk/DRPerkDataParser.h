#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"
#include "DRPerkDataParser.generated.h"

UCLASS(EditInlineNew)
class DEEPRAIDERSEDITOR_API UDRPerkDataParser : public UGoogleSheetParserBase
{
	GENERATED_BODY()

protected:
	virtual bool OnParseComplete(FString& OutError) override;
};

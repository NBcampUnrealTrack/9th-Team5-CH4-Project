#include "DRItemDataParser.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
//#include "DRItemTableRow.h"
#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetParserUtils.h"
#include "Parser/SheetValidation.h"

namespace ItemColumns
{
    const FString RowName = TEXT("RowName");
    const FString DisplayName = TEXT("DisplayName");
    const FString Description = TEXT("Description");

    const TArray<FString> RequiredHeaders =
    {
        RowName,
        DisplayName,
        Description
    };
}

bool UDRItemDataParser::OnParseComplete(FString& OutError)
{
    using namespace SheetDataTableUtils;
    using namespace SheetParserUtils;
    using namespace SheetValidation;

    static constexpr const TCHAR* ParserName = TEXT("ItemData");
    FParseReport Report;
    OutError.Reset();

    if (!ValidateTargetTable(
            ParserName,
            TargetTable,
            FDRItemDataTableRow::StaticStruct(),
            OutError))
    {
        return false;
    }

    ValidateRequiredHeaders(GetHeaders(), ItemColumns::RequiredHeaders, &Report);
    if (Report.HasErrors())
    {
        return FinalizeParseReport(ParserName, Report, OutError);
    }

    FScopedDataTableEditNotification TableEdit(TargetTable);

    for (int32 Index = 0; Index < GetRowCount(); ++Index)
    {
        TMap<FString, FString> RowData;
        if (!GetRowAt(Index, RowData))
        {
            continue;
        }

        FSheetRowReader Row(RowData, Index, Report);
        FDRItemDataTableRow NewRow;

        NewRow.RowName = Row.GetRequiredName(ItemColumns::RowName);
        NewRow.DisplayName = Row.Get(ItemColumns::DisplayName);
        NewRow.Description = Row.Get(ItemColumns::Description);

        if (!Row.IsValid())
        {
            continue;
        }

        TargetTable->AddRow(NewRow.RowName, NewRow);
        Report.AddSuccess();
    }

    return FinalizeParseReport(ParserName, Report, OutError);
}
#include "DRItemDataParser.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetParserUtils.h"
#include "Parser/SheetValidation.h"

namespace ItemColumns
{
    const FString RowName = TEXT("RowName");
    const FString DisplayName = TEXT("DisplayName");
    const FString Description = TEXT("Description");
    const FString Category = TEXT("Category");
    const FString MaxStackSize = TEXT("MaxStackSize");
    const FString bCanBeSold = TEXT("bCanBeSold");
    const FString Price = TEXT("Price");
    
    const TArray<FString> RequiredHeaders =
    {
        RowName,
        DisplayName,
        Description,
        Category,
        MaxStackSize,
        bCanBeSold,
        Price
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
            ensureMsgf(false, TEXT("DRItemDataParser : RowData %d is invalid."), Index);
            continue;
        }
        
        FSheetRowReader Row(RowData, Index, Report);
        if (!Row.IsValid())
        {
            ensureMsgf(false, TEXT("DRItemDataParser : SheetRowReader is invalid."));
            continue;
        }
        
        FDRItemDataTableRow NewRow;
        NewRow.RowName = Row.GetRequiredName(ItemColumns::RowName);
        NewRow.DisplayName = Row.Get(ItemColumns::DisplayName);
        NewRow.Description = Row.Get(ItemColumns::Description);
        NewRow.Category = ParseEnumValue<EDRItemCategory>(
            Row.GetRequiredString(ItemColumns::Category), EDRItemCategory::End);
        NewRow.MaxStackSize = Row.GetRequiredInt(ItemColumns::MaxStackSize);
        NewRow.bCanBeSold = ParseBoolValue(Row.GetRequiredString(ItemColumns::bCanBeSold), false);
        NewRow.Price = Row.GetRequiredInt(ItemColumns::Price);
        
        TargetTable->AddRow(NewRow.RowName, NewRow);
        Report.AddSuccess();
        
        // 테이블에 따른 신규 데이터 에셋 생성 혹은 탐색 기능 테스트 완료
        // 해당 기능은 데이터 테이블의 활용 사양이 결정되면 주석 해제 예정
        //UDRItemDefinition* NewDataAsset = GetOrCreateDataAsset<UDRItemDefinition>(
        //    DRITEM_ASSET_PATH, DRITEM_NAME_FORMAT, NewRow.RowName);
        // 
        // if (NewDataAsset)
        // {
        //     NewDataAsset->DisplayName = FText::FromString(NewRow.DisplayName);
        //     NewDataAsset->Description = FText::FromString(NewRow.Description);
        //     NewDataAsset->Category = NewRow.Category;
        // }
    }

    return FinalizeParseReport(ParserName, Report, OutError);
}
#include "DRPerkDataParser.h"

#include "DeepRaiders/Perk/DRPerkDefinition.h"
#include "Parser/SheetParserUtils.h"
#include "Parser/SheetValidation.h"

namespace PerkColumns
{
	const FString RowName = TEXT("RowName");
	const FString Price = TEXT("Price");
	const FString Values = TEXT("Values");

	const TArray<FString> RequiredHeaders =
	{
		RowName,
		Price,
		Values
	};
}

namespace PerkParser
{
	const FString AssetPath = TEXT("/Game/DeepRaiders/Data/DataAssets/Perk");
	const FString AssetNameFormat = TEXT("DA_DR{0}");
	const FName EffectValueTagName = TEXT("Data.Perk.Value");

	bool ParseEffectValues(
		const FString& Source,
		TMap<FGameplayTag, float>& OutValues)
	{
		using namespace SheetParserUtils;

		OutValues.Reset();
		float Value = 0.0f;
		const FGameplayTag EffectValueTag = FGameplayTag::RequestGameplayTag(
			EffectValueTagName,
			false);
		if (!EffectValueTag.IsValid()
			|| !Detail::TryParseFloat(Source, Value))
		{
			return false;
		}

		OutValues.Add(EffectValueTag, Value);
		return true;
	}
}

bool UDRPerkDataParser::OnParseComplete(FString& OutError)
{
	using namespace SheetParserUtils;
	using namespace SheetValidation;

	static constexpr const TCHAR* ParserName = TEXT("PerkData");
	FParseReport Report;
	OutError.Reset();

	ValidateRequiredHeaders(GetHeaders(), PerkColumns::RequiredHeaders, &Report);
	if (Report.HasErrors())
	{
		return FinalizeParseReport(ParserName, Report, OutError);
	}

	for (int32 Index = 0; Index < GetRowCount(); ++Index)
	{
		TMap<FString, FString> RowData;
		if (!GetRowAt(Index, RowData))
		{
			continue;
		}

		FSheetRowReader Row(RowData, Index, Report);
		const FName RowName = Row.GetRequiredName(PerkColumns::RowName);
		const int32 Price = Row.GetRequiredInt(PerkColumns::Price);
		const FString Values = Row.GetRequiredString(PerkColumns::Values);
		TMap<FGameplayTag, float> ParsedValues;
		if (!PerkParser::ParseEffectValues(Values, ParsedValues))
		{
			Report.AddIssue(
				EParseIssueSeverity::Error,
				TEXT("숫자로 변환할 수 없거나 Data.Perk.Value 태그가 등록되지 않았습니다."),
				Index,
				RowName,
				PerkColumns::Values,
				Values);
			continue;
		}

		if (!Row.IsValid())
		{
			continue;
		}

		UDRPerkDefinition* PerkDefinition = GetOrCreateDataAsset<UDRPerkDefinition>(
			PerkParser::AssetPath,
			PerkParser::AssetNameFormat,
			RowName);
		if (!IsValid(PerkDefinition))
		{
			Report.AddIssue(
				EParseIssueSeverity::Error,
				TEXT("퍽 데이터 에셋을 생성하거나 불러올 수 없습니다."),
				Index,
				RowName);
			continue;
		}

		PerkDefinition->Modify();
		PerkDefinition->Price = Price;
		PerkDefinition->EffectValues = MoveTemp(ParsedValues);
		PerkDefinition->MarkPackageDirty();
		Report.AddSuccess();
	}

	return FinalizeParseReport(ParserName, Report, OutError);
}

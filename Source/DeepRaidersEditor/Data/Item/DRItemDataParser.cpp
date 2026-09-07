#include "DRItemDataParser.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRMeleeWeaponDefinition.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Item/DRSprayerWeaponDefinition.h"
#include "DeepRaiders/Item/DRThrowableItemDefinition.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "Misc/PackageName.h"
#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetParserUtils.h"
#include "Parser/SheetValidation.h"
#include "UObject/UnrealType.h"

namespace ItemParser
{
	using namespace SheetParserUtils;
	using namespace SheetValidation;

	const FString AssetNameFormat = TEXT("DA_DR{0}");
	const FString RowNameColumn = TEXT("RowName");
	using FDefinitionAssetsByName = TMap<FName, TArray<FAssetData>>;

	FDefinitionAssetsByName FindDefinitionAssets(const FString& TargetAssetRootFolder)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(*TargetAssetRootFolder);
		Filter.ClassPaths.Add(UDRItemDefinition::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;

		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> DefinitionAssets;
		AssetRegistryModule.Get().GetAssets(Filter, DefinitionAssets);

		FDefinitionAssetsByName AssetsByName;
		for (const FAssetData& AssetData : DefinitionAssets)
		{
			AssetsByName.FindOrAdd(AssetData.AssetName).Add(AssetData);
		}
		return AssetsByName;
	}

	void AddValueError(FParseReport& Report, const int32 RowIndex, const FName RowName, const FString& ColumnName,
		const FString& SourceValue, const FString& Message)
	{
		Report.AddIssue(EParseIssueSeverity::Error, Message, RowIndex, RowName, ColumnName, SourceValue);
	}

	TArray<FString> GetRequiredHeaders(const UScriptStruct* RowStruct)
	{
		TArray<FString> Headers;
		for (TFieldIterator<FProperty> Property(RowStruct, EFieldIterationFlags::IncludeSuper); Property; ++Property)
		{
			Headers.Add(Property->GetName());
		}
		return Headers;
	}

	bool TryParseBool(const FString& SourceValue, bool& OutValue)
	{
		if (SourceValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| SourceValue.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
			|| SourceValue == TEXT("1"))
		{
			OutValue = true;
			return true;
		}

		if (SourceValue.Equals(TEXT("false"), ESearchCase::IgnoreCase)
			|| SourceValue.Equals(TEXT("no"), ESearchCase::IgnoreCase)
			|| SourceValue == TEXT("0"))
		{
			OutValue = false;
			return true;
		}

		return false;
	}

	bool TryParseVector(const FString& SourceValue, FVector& OutValue)
	{
		FVector ParsedValue;
		if (ParsedValue.InitFromString(SourceValue) && !ParsedValue.ContainsNaN())
		{
			OutValue = ParsedValue;
			return true;
		}

		const TArray<float> Components = ParseFloatArray(SourceValue);
		if (Components.Num() != 3)
		{
			return false;
		}

		ParsedValue = FVector(Components[0], Components[1], Components[2]);
		if (ParsedValue.ContainsNaN())
		{
			return false;
		}

		OutValue = ParsedValue;
		return true;
	}

	bool ImportCellValue(FProperty& Property, void* ValueAddress, const FString& SourceValue, FParseReport& Report,
		const int32 RowIndex, const FName RowName)
	{
		const FString ColumnName = Property.GetName();
		const FString TrimmedValue = TrimCell(SourceValue);

		if (FStrProperty* StringProperty = CastField<FStrProperty>(&Property))
		{
			StringProperty->SetPropertyValue(ValueAddress, TrimmedValue);
			return true;
		}

		if (TrimmedValue.IsEmpty())
		{
			AddValueError(Report, RowIndex, RowName, ColumnName, SourceValue, TEXT("필수 값이 비어 있습니다."));
			return false;
		}

		if (FNameProperty* NameProperty = CastField<FNameProperty>(&Property))
		{
			const FName ParsedValue(*TrimmedValue);
			if (!ParsedValue.IsNone())
			{
				NameProperty->SetPropertyValue(ValueAddress, ParsedValue);
				return true;
			}
		}
		else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
		{
			const int64 ParsedValue = EnumProperty->GetEnum()->GetValueByNameString(TrimmedValue);
			if (ParsedValue != INDEX_NONE)
			{
				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValueAddress, ParsedValue);
				return true;
			}
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(&Property))
		{
			if (ByteProperty->Enum)
			{
				const int64 ParsedValue = ByteProperty->Enum->GetValueByNameString(TrimmedValue);
				if (ParsedValue != INDEX_NONE)
				{
					ByteProperty->SetPropertyValue(ValueAddress, static_cast<uint8>(ParsedValue));
					return true;
				}
			}
		}
		else if (FIntProperty* IntProperty = CastField<FIntProperty>(&Property))
		{
			int32 ParsedValue = 0;
			if (LexTryParseString(ParsedValue, *TrimmedValue))
			{
				IntProperty->SetPropertyValue(ValueAddress, ParsedValue);
				return true;
			}
		}
		else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(&Property))
		{
			float ParsedValue = 0.f;
			if (LexTryParseString(ParsedValue, *TrimmedValue) && FMath::IsFinite(ParsedValue))
			{
				FloatProperty->SetPropertyValue(ValueAddress, ParsedValue);
				return true;
			}
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
		{
			bool ParsedValue = false;
			if (TryParseBool(TrimmedValue, ParsedValue))
			{
				BoolProperty->SetPropertyValue(ValueAddress, ParsedValue);
				return true;
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				FVector ParsedValue;
				if (TryParseVector(TrimmedValue, ParsedValue))
				{
					*static_cast<FVector*>(ValueAddress) = ParsedValue;
					return true;
				}
			}
			else if (StructProperty->Struct == FGameplayTag::StaticStruct())
			{
				const FGameplayTag ParsedValue = FGameplayTag::RequestGameplayTag(FName(*TrimmedValue), false);
				if (ParsedValue.IsValid())
				{
					*static_cast<FGameplayTag*>(ValueAddress) = ParsedValue;
					return true;
				}
			}
		}

		AddValueError(
			Report, RowIndex, RowName, ColumnName, SourceValue, TEXT("지원하는 형식의 값으로 변환할 수 없습니다."));
		return false;
	}

	template <typename TRow>
	bool ParseRow(const TMap<FString, FString>& RowData, const int32 RowIndex, FParseReport& Report, TRow& OutRow)
	{
		const FString* RowNameSource = RowData.Find(RowNameColumn);
		const FName ContextRowName = RowNameSource && !IsUnsetValue(*RowNameSource)
			? FName(*TrimCell(*RowNameSource))
			: NAME_None;
		bool bValid = true;

		for (TFieldIterator<FProperty> Property(TRow::StaticStruct(), EFieldIterationFlags::IncludeSuper);
			Property;
			++Property)
		{
			const FString ColumnName = Property->GetName();
			const FString* SourceValue = RowData.Find(ColumnName);
			if (!SourceValue)
			{
				AddValueError(
					Report, RowIndex, ContextRowName, ColumnName, FString(), TEXT("필수 헤더가 없습니다."));
				bValid = false;
				continue;
			}

			void* ValueAddress = Property->ContainerPtrToValuePtr<void>(&OutRow);
			bValid &= ImportCellValue(**Property, ValueAddress, *SourceValue, Report, RowIndex, ContextRowName);
		}

		return bValid;
	}

	bool ValidateMinimum(FParseReport& Report, const int32 RowIndex, const FName RowName, const FString& ColumnName,
		const float Value, const float Minimum, const bool bAllowEqual = true)
	{
		if ((bAllowEqual && Value >= Minimum) || (!bAllowEqual && Value > Minimum))
		{
			return true;
		}

		const FString Message = bAllowEqual
			? FString::Printf(TEXT("값은 %.3f 이상이어야 합니다."), Minimum)
			: FString::Printf(TEXT("값은 %.3f보다 커야 합니다."), Minimum);
		AddValueError(Report, RowIndex, RowName, ColumnName, LexToString(Value), Message);
		return false;
	}

	bool ValidateMinimum(FParseReport& Report, const int32 RowIndex, const FName RowName, const FString& ColumnName,
		const int32 Value, const int32 Minimum)
	{
		if (Value >= Minimum)
		{
			return true;
		}

		AddValueError(Report, RowIndex, RowName, ColumnName, LexToString(Value),
			FString::Printf(TEXT("값은 %d 이상이어야 합니다."), Minimum));
		return false;
	}

	bool ValidateRange(FParseReport& Report, const int32 RowIndex, const FName RowName, const FString& ColumnName,
		const float Value, const float Minimum, const float Maximum)
	{
		if (Value >= Minimum && Value <= Maximum)
		{
			return true;
		}

		AddValueError(Report, RowIndex, RowName, ColumnName, LexToString(Value),
			FString::Printf(TEXT("값은 %.3f 이상 %.3f 이하여야 합니다."), Minimum, Maximum));
		return false;
	}

	bool ValidateRowName(const FName RowName, FParseReport& Report, const int32 RowIndex)
	{
		if (!RowName.IsNone())
		{
			return true;
		}

		AddValueError(Report, RowIndex, RowName, RowNameColumn, FString(), TEXT("RowName은 비어 있을 수 없습니다."));
		return false;
	}

	bool ValidateItemRow(const FDRItemDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		bool bValid = ValidateRowName(Row.RowName, Report, RowIndex);
		if (Row.DisplayName.IsEmpty())
		{
			AddValueError(Report, RowIndex, Row.RowName,
				GET_MEMBER_NAME_STRING_CHECKED(FDRItemDataTableRow, DisplayName), Row.DisplayName,
				TEXT("DisplayName은 비어 있을 수 없습니다."));
			bValid = false;
		}
		if (Row.Category == EDRItemCategory::End)
		{
			AddValueError(Report, RowIndex, Row.RowName,
				GET_MEMBER_NAME_STRING_CHECKED(FDRItemDataTableRow, Category), TEXT("End"),
				TEXT("Category에 End를 사용할 수 없습니다."));
			bValid = false;
		}

		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName,
			GET_MEMBER_NAME_STRING_CHECKED(FDRItemDataTableRow, MaxStackSize), Row.MaxStackSize, 1);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName,
			GET_MEMBER_NAME_STRING_CHECKED(FDRItemDataTableRow, Price), Row.Price, 0);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName,
			GET_MEMBER_NAME_STRING_CHECKED(FDRItemDataTableRow, QuickSlotActivationInterval),
			Row.QuickSlotActivationInterval, 0.f);
		return bValid;
	}

	bool ValidateRangedWeaponRow(const FDRRangedWeaponDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		bool bValid = ValidateRowName(Row.RowName, Report, RowIndex);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("HeatPerShot"), Row.HeatPerShot, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("HeatPerSecond"), Row.HeatPerSecond, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("HeatDecayDelay"), Row.HeatDecayDelay, 0.f);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("HeatRecoveryDuration"), Row.HeatRecoveryDuration, 0.f, false);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowAbsorbRadius"), Row.SnowAbsorbRadius, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowAbsorbPower"), Row.SnowAbsorbPower, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowAbsorbSpeed"), Row.SnowAbsorbSpeed, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowAbsorbRange"), Row.SnowAbsorbRange, 0.f);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("SnowAbsorbSweepRadius"), Row.SnowAbsorbSweepRadius, 1.f);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("SnowAbsorbMaxSweepsPerTick"), Row.SnowAbsorbMaxSweepsPerTick, 1);
		bValid &= ValidateRange(Report, RowIndex, Row.RowName, TEXT("SnowAbsorbInnerRadiusRatio"),
			Row.SnowAbsorbInnerRadiusRatio, 0.f, 1.f);
		return bValid;
	}

	bool ValidateRowValues(const FDRItemDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		return ValidateItemRow(Row, Report, RowIndex);
	}

	bool ValidateRowValues(const FDRRangedWeaponDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		return ValidateRangedWeaponRow(Row, Report, RowIndex);
	}

	bool ValidateRowValues(const FDRMeleeWeaponDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		bool bValid = ValidateRowName(Row.RowName, Report, RowIndex);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("BaseDamage"), Row.BaseDamage, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SweepRadius"), Row.SweepRadius, 0.f);
		return bValid;
	}

	bool ValidateRowValues(const FDRProjectileWeaponDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		bool bValid = ValidateRowName(Row.RowName, Report, RowIndex);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowCostPerShot"), Row.SnowCostPerShot, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("InitialAmmo"), Row.InitialAmmo, 1);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("BaseFireInterval"), Row.BaseFireInterval, 0.f, false);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("MaxAttackDistance"), Row.MaxAttackDistance, 1.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("NoHitAimDistance"), Row.NoHitAimDistance, 1.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("BreakableDamage"), Row.BreakableDamage, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("ProjectileCount"), Row.ProjectileCount, 1);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("SpreadHalfAngleDegrees"), Row.SpreadHalfAngleDegrees, 0.f);
		bValid &= ValidateRange(
			Report, RowIndex, Row.RowName, TEXT("FullStrengthRangeRatio"), Row.FullStrengthRangeRatio, 0.f, 0.99f);
		bValid &= ValidateRange(
			Report, RowIndex, Row.RowName, TEXT("MinSizeMultiplier"), Row.MinSizeMultiplier, 0.f, 1.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowAddRadius"), Row.SnowAddRadius, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowAddAmount"), Row.SnowAddAmount, 0.f);
		return bValid;
	}

	bool ValidateRowValues(const FDRSprayerWeaponDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		bool bValid = ValidateRowName(Row.RowName, Report, RowIndex);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("SnowCostPerSecond"), Row.SnowCostPerSecond, 0.f);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("SprayTickInterval"), Row.SprayTickInterval, 0.f, false);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SprayRange"), Row.SprayRange, 1.f);
		bValid &= ValidateRange(
			Report, RowIndex, Row.RowName, TEXT("SprayHalfAngleDegrees"), Row.SprayHalfAngleDegrees, 0.f, 89.f);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("HitReactionInterval"), Row.HitReactionInterval, 0.f);
		return bValid;
	}

	bool ValidateRowValues(const FDRThrowableItemDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		bool bValid = ValidateRowName(Row.RowName, Report, RowIndex);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("InitialSpeed"), Row.InitialSpeed, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("GravityScale"), Row.GravityScale, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("ExplosionRadius"), Row.ExplosionRadius, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("MaxAimDistance"), Row.MaxAimDistance, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowRadius"), Row.SnowRadius, 0.f);
		bValid &= ValidateMinimum(Report, RowIndex, Row.RowName, TEXT("SnowAmount"), Row.SnowAmount, 0.f);
		return bValid;
	}

	bool ValidateRowValues(const FDRSkillDataTableRow& Row, FParseReport& Report, const int32 RowIndex)
	{
		bool bValid = ValidateRowName(Row.RowName, Report, RowIndex);
		bValid &= ValidateMinimum(
			Report, RowIndex, Row.RowName, TEXT("CooldownDuration"), Row.CooldownDuration, 0.f, false);
		if (Row.SkillSlot == EDRSkillSlot::Count)
		{
			AddValueError(Report, RowIndex, Row.RowName, TEXT("SkillSlot"), TEXT("Count"),
				TEXT("SkillSlot에 Count를 사용할 수 없습니다."));
			bValid = false;
		}
		return bValid;
	}

	void ApplyItemFields(UDRItemDefinition& Definition, const FDRItemDataTableRow& Row)
	{
		Definition.ItemId = Row.RowName;
		Definition.DisplayName = FText::FromString(Row.DisplayName);
		Definition.Description = FText::FromString(Row.Description);
		Definition.WorldInteractionText = FText::FromString(Row.WorldInteractionText);
		Definition.Category = Row.Category;
		Definition.Rarity = Row.Rarity;
		Definition.MaxStackSize = Row.MaxStackSize;
		Definition.bCanBeDropped = Row.bCanBeDropped;
		Definition.bCanBeSold = Row.bCanBeSold;
		Definition.Price = Row.Price;
		Definition.QuickSlotActivationInterval = Row.QuickSlotActivationInterval;
	}

	void ApplyRangedWeaponFields(UDRRangedWeaponDefinition& Definition, const FDRRangedWeaponDataTableRow& Row)
	{
		Definition.HeatSettings.bEnabled = Row.bHeatEnabled;
		Definition.HeatSettings.HeatPerShot = Row.HeatPerShot;
		Definition.HeatSettings.HeatPerSecond = Row.HeatPerSecond;
		Definition.HeatSettings.DecayDelay = Row.HeatDecayDelay;
		Definition.HeatSettings.RecoveryDuration = Row.HeatRecoveryDuration;
		Definition.SnowAbsorbSettings.bEnabled = Row.bSnowAbsorbEnabled;
		Definition.SnowAbsorbSettings.Radius = Row.SnowAbsorbRadius;
		Definition.SnowAbsorbSettings.Power = Row.SnowAbsorbPower;
		Definition.SnowAbsorbSettings.Speed = Row.SnowAbsorbSpeed;
		Definition.SnowAbsorbSettings.Range = Row.SnowAbsorbRange;
		Definition.SnowAbsorbSettings.SweepRadius = Row.SnowAbsorbSweepRadius;
		Definition.SnowAbsorbSettings.MaxSweepsPerTick = Row.SnowAbsorbMaxSweepsPerTick;
		Definition.SnowAbsorbSettings.bUseAdaptiveQuery = Row.bSnowAbsorbUseAdaptiveQuery;
		Definition.SnowAbsorbSettings.InnerRadiusRatio = Row.SnowAbsorbInnerRadiusRatio;
	}

	void ApplyProjectileWeaponFields(UDRProjectileWeaponItemDefinition& Definition,
		const FDRProjectileWeaponDataTableRow& Row)
	{
		Definition.ResourceType = Row.ResourceType;
		Definition.SnowCostPerShot = Row.SnowCostPerShot;
		Definition.InitialAmmo = Row.InitialAmmo;
		Definition.BaseFireInterval = Row.BaseFireInterval;
		Definition.bAutomaticFire = Row.bAutomaticFire;
		Definition.MaxAttackDistance = Row.MaxAttackDistance;
		Definition.NoHitAimDistance = Row.NoHitAimDistance;
		Definition.BreakableDamage = Row.BreakableDamage;
		Definition.ProjectileCount = Row.ProjectileCount;
		Definition.SpreadHalfAngleDegrees = Row.SpreadHalfAngleDegrees;
		Definition.FalloffSettings.bEnabled = Row.bFalloffEnabled;
		Definition.FalloffSettings.FullStrengthRangeRatio = Row.FullStrengthRangeRatio;
		Definition.FalloffSettings.MinSizeMultiplier = Row.MinSizeMultiplier;
		Definition.bCanPenetrateTargets = Row.bCanPenetrateTargets;
		Definition.SnowAddSettings.bEnabled = Row.bSnowAddEnabled;
		Definition.SnowAddSettings.Radius = Row.SnowAddRadius;
		Definition.SnowAddSettings.Amount = Row.SnowAddAmount;
	}

	void ApplySprayerWeaponFields(UDRSprayerWeaponDefinition& Definition, const FDRSprayerWeaponDataTableRow& Row)
	{
		Definition.SnowCostPerSecond = Row.SnowCostPerSecond;
		Definition.SprayTickInterval = Row.SprayTickInterval;
		Definition.SprayRange = Row.SprayRange;
		Definition.SprayHalfAngleDegrees = Row.SprayHalfAngleDegrees;
		Definition.SprayOriginForwardOffset = Row.SprayOriginForwardOffset;
		Definition.SprayOriginHeightOffset = Row.SprayOriginHeightOffset;
		Definition.HitReactionInterval = Row.HitReactionInterval;
	}

	void ApplyMeleeWeaponFields(UDRMeleeWeaponItemDefinition& Definition, const FDRMeleeWeaponDataTableRow& Row)
	{
		Definition.BaseDamage = Row.BaseDamage;
		Definition.SweepRadius = Row.SweepRadius;
	}

	void ApplyThrowableItemFields(UDRThrowableItemDefinition& Definition, const FDRThrowableItemDataTableRow& Row)
	{
		Definition.ThrowSettings.InitialSpeed = Row.InitialSpeed;
		Definition.ThrowSettings.GravityScale = Row.GravityScale;
		Definition.ThrowSettings.ExplosionRadius = Row.ExplosionRadius;
		Definition.ThrowSettings.MaxAimDistance = Row.MaxAimDistance;
		Definition.ThrowSettings.WorldImpactData.bAddSnow = Row.bAddSnow;
		Definition.ThrowSettings.WorldImpactData.SnowRadius = Row.SnowRadius;
		Definition.ThrowSettings.WorldImpactData.SnowAmount = Row.SnowAmount;
		Definition.ThrowSettings.WorldImpactData.bAllowVirtualSurfaceFallback = Row.bAllowVirtualSurfaceFallback;
	}

	void ApplySkillFields(UDRSkillDefinition& Definition, const FDRSkillDataTableRow& Row)
	{
		Definition.DisplayName = FText::FromString(Row.DisplayName);
		Definition.Description = FText::FromString(Row.Description);
		Definition.CooldownDuration = Row.CooldownDuration;
		Definition.SkillSlot = Row.SkillSlot;
	}

	template <typename TRow, typename TDefinition, typename TApplyRow>
	void ProcessDefinitionRows(const TArray<FString>& Headers, const TArray<TMap<FString, FString>>& SourceRows,
		UDataTable* TargetTable, const FString& TargetAssetRootFolder,
		const FDefinitionAssetsByName& DefinitionAssetsByName, const bool bAllowDerivedDefinitionClass,
		TApplyRow&& ApplyRow, FParseReport& Report)
	{
		ValidateRequiredHeaders(Headers, GetRequiredHeaders(TRow::StaticStruct()), &Report);
		if (Report.HasErrors())
		{
			return;
		}

		if (SourceRows.IsEmpty())
		{
			Report.AddIssue(EParseIssueSeverity::Error, TEXT("시트에 데이터 행이 없습니다."));
			return;
		}

		struct FValidatedRow
		{
			TRow Row;
			TDefinition* Definition = nullptr;
		};

		TArray<FValidatedRow> ValidatedRows;
		ValidatedRows.Reserve(SourceRows.Num());
		TSet<FName> RowNames;

		for (int32 Index = 0; Index < SourceRows.Num(); ++Index)
		{
			FValidatedRow ValidatedRow;
			if (!ParseRow(SourceRows[Index], Index, Report, ValidatedRow.Row)
				|| !ValidateRowValues(ValidatedRow.Row, Report, Index))
			{
				continue;
			}

			if (RowNames.Contains(ValidatedRow.Row.RowName))
			{
				Report.AddIssue(EParseIssueSeverity::Error, TEXT("중복 RowName입니다."), Index,
					ValidatedRow.Row.RowName, RowNameColumn, ValidatedRow.Row.RowName.ToString());
				continue;
			}
			RowNames.Add(ValidatedRow.Row.RowName);

			const FString AssetName = MakeGeneratedAssetName(AssetNameFormat, ValidatedRow.Row.RowName);
			const TArray<FAssetData>* MatchingAssets = DefinitionAssetsByName.Find(*AssetName);
			if (!MatchingAssets || MatchingAssets->IsEmpty())
			{
				Report.AddIssue(EParseIssueSeverity::Error, TEXT("대상 Item Definition 에셋을 찾을 수 없습니다."),
					Index, ValidatedRow.Row.RowName, RowNameColumn,
					FString::Printf(TEXT("%s/**/%s"), *TargetAssetRootFolder, *AssetName));
				continue;
			}

			if (MatchingAssets->Num() > 1)
			{
				TArray<FString> MatchingAssetPaths;
				MatchingAssetPaths.Reserve(MatchingAssets->Num());
				for (const FAssetData& MatchingAsset : *MatchingAssets)
				{
					MatchingAssetPaths.Add(MatchingAsset.GetSoftObjectPath().ToString());
				}
				Report.AddIssue(EParseIssueSeverity::Error,
					TEXT("검색 루트 아래에 같은 이름의 Item Definition 에셋이 여러 개 있습니다."), Index,
					ValidatedRow.Row.RowName, RowNameColumn, FString::Join(MatchingAssetPaths, TEXT(", ")));
				continue;
			}

			const FString AssetReferencePath = (*MatchingAssets)[0].GetSoftObjectPath().ToString();
			UObject* LoadedAsset = (*MatchingAssets)[0].GetAsset();
			if (!IsValid(LoadedAsset))
			{
				Report.AddIssue(EParseIssueSeverity::Error, TEXT("대상 Item Definition 에셋을 로드할 수 없습니다."),
					Index, ValidatedRow.Row.RowName, RowNameColumn, AssetReferencePath);
				continue;
			}

			ValidatedRow.Definition = Cast<TDefinition>(LoadedAsset);
			const bool bClassMatches = IsValid(ValidatedRow.Definition)
				&& (bAllowDerivedDefinitionClass || LoadedAsset->GetClass() == TDefinition::StaticClass());
			if (!bClassMatches)
			{
				Report.AddIssue(EParseIssueSeverity::Error,
					FString::Printf(TEXT("Definition 클래스가 일치하지 않습니다. Expected=%s Actual=%s"),
						*TDefinition::StaticClass()->GetName(), *LoadedAsset->GetClass()->GetName()),
					Index, ValidatedRow.Row.RowName, RowNameColumn, AssetReferencePath);
				continue;
			}

			ValidatedRows.Add(MoveTemp(ValidatedRow));
		}

		if (Report.HasErrors())
		{
			return;
		}

		SheetDataTableUtils::FScopedDataTableEditNotification TableEdit(TargetTable);
		for (FValidatedRow& ValidatedRow : ValidatedRows)
		{
			TargetTable->AddRow(ValidatedRow.Row.RowName, ValidatedRow.Row);
			ValidatedRow.Definition->Modify();
			ApplyRow(*ValidatedRow.Definition, ValidatedRow.Row);
			ValidatedRow.Definition->MarkPackageDirty();
			Report.AddSuccess();
		}
	}
}

bool UDRItemDataParser::OnParseComplete(FString& OutError)
{
	using namespace ItemParser;
	using namespace SheetValidation;

	static constexpr const TCHAR* ParserName = TEXT("ItemData");
	FParseReport Report;
	OutError.Reset();

	if (!IsValid(TargetTable))
	{
		OutError = TEXT("TargetTable이 비어 있습니다.");
		return false;
	}

	FString NormalizedAssetFolder = TargetAssetFolder.TrimStartAndEnd();
	while (NormalizedAssetFolder.EndsWith(TEXT("/")))
	{
		NormalizedAssetFolder.LeftChopInline(1);
	}
	if (!FPackageName::IsValidLongPackageName(NormalizedAssetFolder))
	{
		OutError = FString::Printf(TEXT("TargetAssetFolder가 올바른 Content 경로가 아닙니다: %s"),
			*TargetAssetFolder);
		return false;
	}

	TArray<TMap<FString, FString>> SourceRows;
	SourceRows.Reserve(GetRowCount());
	for (int32 Index = 0; Index < GetRowCount(); ++Index)
	{
		TMap<FString, FString> RowData;
		if (!GetRowAt(Index, RowData))
		{
			Report.AddIssue(EParseIssueSeverity::Error, TEXT("파싱된 Sheet Row를 읽을 수 없습니다."), Index);
			continue;
		}
		SourceRows.Add(MoveTemp(RowData));
	}

	if (Report.HasErrors())
	{
		return FinalizeParseReport(ParserName, Report, OutError);
	}

	const TArray<FString> SheetHeaders = GetHeaders();
	const FDefinitionAssetsByName DefinitionAssetsByName = FindDefinitionAssets(NormalizedAssetFolder);
	const UScriptStruct* RowStruct = TargetTable->GetRowStruct();
	if (RowStruct == FDRItemDataTableRow::StaticStruct())
	{
		ProcessDefinitionRows<FDRItemDataTableRow, UDRItemDefinition>(
			SheetHeaders, SourceRows, TargetTable, NormalizedAssetFolder, DefinitionAssetsByName, true,
			ApplyItemFields, Report);
	}
	else if (RowStruct == FDRMeleeWeaponDataTableRow::StaticStruct())
	{
		ProcessDefinitionRows<FDRMeleeWeaponDataTableRow, UDRMeleeWeaponItemDefinition>(
			SheetHeaders, SourceRows, TargetTable, NormalizedAssetFolder, DefinitionAssetsByName, false,
			ApplyMeleeWeaponFields, Report);
	}
	else if (RowStruct == FDRRangedWeaponDataTableRow::StaticStruct())
	{
		ProcessDefinitionRows<FDRRangedWeaponDataTableRow, UDRRangedWeaponDefinition>(
			SheetHeaders, SourceRows, TargetTable, NormalizedAssetFolder, DefinitionAssetsByName, true,
			ApplyRangedWeaponFields, Report);
	}
	else if (RowStruct == FDRProjectileWeaponDataTableRow::StaticStruct())
	{
		ProcessDefinitionRows<FDRProjectileWeaponDataTableRow, UDRProjectileWeaponItemDefinition>(
			SheetHeaders, SourceRows, TargetTable, NormalizedAssetFolder, DefinitionAssetsByName, false,
			ApplyProjectileWeaponFields, Report);
	}
	else if (RowStruct == FDRSprayerWeaponDataTableRow::StaticStruct())
	{
		ProcessDefinitionRows<FDRSprayerWeaponDataTableRow, UDRSprayerWeaponDefinition>(
			SheetHeaders, SourceRows, TargetTable, NormalizedAssetFolder, DefinitionAssetsByName, false,
			ApplySprayerWeaponFields, Report);
	}
	else if (RowStruct == FDRThrowableItemDataTableRow::StaticStruct())
	{
		ProcessDefinitionRows<FDRThrowableItemDataTableRow, UDRThrowableItemDefinition>(
			SheetHeaders, SourceRows, TargetTable, NormalizedAssetFolder, DefinitionAssetsByName, false,
			ApplyThrowableItemFields, Report);
	}
	else if (RowStruct == FDRSkillDataTableRow::StaticStruct())
	{
		ProcessDefinitionRows<FDRSkillDataTableRow, UDRSkillDefinition>(
			SheetHeaders, SourceRows, TargetTable, NormalizedAssetFolder, DefinitionAssetsByName, true,
			ApplySkillFields, Report);
	}
	else
	{
		Report.AddIssue(EParseIssueSeverity::Error,
			FString::Printf(TEXT("지원하지 않는 Item DataTable RowStruct입니다: %s"), *GetNameSafe(RowStruct)));
	}

	return FinalizeParseReport(ParserName, Report, OutError);
}

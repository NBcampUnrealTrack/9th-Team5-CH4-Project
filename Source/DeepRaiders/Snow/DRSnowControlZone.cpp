#include "DRSnowControlZone.h"

#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Core/Subsystem/DRSnowVolumeSubsystem.h"
#include "DeepRaiders/Voxel/DRVoxelTeamColorLibrary.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelData/VoxelDataLock.h"
#include "VoxelIntBox.h"
#include "VoxelMaterial.h"
#include "VoxelUtilities/VoxelMathUtilities.h"
#include "VoxelValue.h"
#include "VoxelWorld.h"

namespace
{
	int32 GetDominantMaterialIndex(
		const FVoxelMaterial& Material,
		EVoxelMaterialConfig MaterialConfig)
	{
		if (MaterialConfig == EVoxelMaterialConfig::SingleIndex)
		{
			return Material.GetSingleIndex();
		}

		if (MaterialConfig == EVoxelMaterialConfig::MultiIndex)
		{
			const float Blend0 = Material.GetMultiIndex_Blend0_AsFloat();
			const float Blend1 = Material.GetMultiIndex_Blend1_AsFloat();
			const float Blend2 = Material.GetMultiIndex_Blend2_AsFloat();
			const TVoxelStaticArray<float, 4> Strengths =
				FVoxelUtilities::XWayBlend_AlphasToStrengths_Static<4>(
					{ Blend0, Blend1, Blend2 });

			int32 BestChannel = 0;
			float BestStrength = Strengths[0];
			for (int32 Channel = 1; Channel < 4; ++Channel)
			{
				if (Strengths[Channel] > BestStrength)
				{
					BestChannel = Channel;
					BestStrength = Strengths[Channel];
				}
			}

			switch (BestChannel)
			{
			case 0:
				return Material.GetMultiIndex_Index0();
			case 1:
				return Material.GetMultiIndex_Index1();
			case 2:
				return Material.GetMultiIndex_Index2();
			case 3:
				return Material.GetMultiIndex_Index3();
			default:
				return 0;
			}
		}

		return INDEX_NONE;
	}

	int32 MaterialIndexToTeamId(int32 MaterialIndex)
	{
		return MaterialIndex <= 0
			? INDEX_NONE
			: MaterialIndex - 1;
	}

	void ExpandVoxelBoundsForWorldPoint(
		const AVoxelWorld* VoxelWorld,
		const FVector& WorldPoint,
		FIntVector& InOutMin,
		FIntVector& InOutMax)
	{
		const FIntVector VoxelPoint =
			VoxelWorld->GlobalToLocal(
				WorldPoint,
				EVoxelWorldCoordinatesRounding::RoundDown);
		InOutMin.X = FMath::Min(InOutMin.X, VoxelPoint.X);
		InOutMin.Y = FMath::Min(InOutMin.Y, VoxelPoint.Y);
		InOutMin.Z = FMath::Min(InOutMin.Z, VoxelPoint.Z);
		InOutMax.X = FMath::Max(InOutMax.X, VoxelPoint.X + 1);
		InOutMax.Y = FMath::Max(InOutMax.Y, VoxelPoint.Y + 1);
		InOutMax.Z = FMath::Max(InOutMax.Z, VoxelPoint.Z + 1);
	}

	FVoxelIntBox MakeVoxelBoundsFromWorldBounds(
		const AVoxelWorld* VoxelWorld,
		const FBox& WorldBounds)
	{
		if (!IsValid(VoxelWorld) || !WorldBounds.IsValid)
		{
			return FVoxelIntBox();
		}

		FIntVector Min(MAX_int32);
		FIntVector Max(MIN_int32);
		for (int32 X = 0; X < 2; ++X)
		{
			for (int32 Y = 0; Y < 2; ++Y)
			{
				for (int32 Z = 0; Z < 2; ++Z)
				{
					ExpandVoxelBoundsForWorldPoint(
						VoxelWorld,
						FVector(
							X == 0 ? WorldBounds.Min.X : WorldBounds.Max.X,
							Y == 0 ? WorldBounds.Min.Y : WorldBounds.Max.Y,
							Z == 0 ? WorldBounds.Min.Z : WorldBounds.Max.Z),
						Min,
						Max);
				}
			}
		}

		return FVoxelIntBox(Min, Max);
	}

	FString GetRatioWinnerText(const FDRSnowControlRatio& Ratio)
	{
		if (Ratio.TotalAmount <= 0.f)
		{
			return TEXT("None");
		}

		if (FMath::IsNearlyEqual(Ratio.AmountA, Ratio.AmountB))
		{
			return TEXT("Draw");
		}

		return Ratio.AmountA > Ratio.AmountB
			? FString::Printf(TEXT("Team %d"), Ratio.TeamIdA)
			: FString::Printf(TEXT("Team %d"), Ratio.TeamIdB);
	}

	FString BuildControlRatioDebugText(
		const AActor* ZoneActor,
		const FDRSnowControlRatio& Ratio)
	{
		return FString::Printf(
			TEXT("[Snow][ControlZone] %s Winner=%s TeamA=%d %.1f(%.1f%%) TeamB=%d %.1f(%.1f%%) Total=%.1f Cells=%d"),
			*GetNameSafe(ZoneActor),
			*GetRatioWinnerText(Ratio),
			Ratio.TeamIdA,
			Ratio.AmountA,
			Ratio.RatioA * 100.f,
			Ratio.TeamIdB,
			Ratio.AmountB,
			Ratio.RatioB * 100.f,
			Ratio.TotalAmount,
			Ratio.SampledCellCount);
	}
}

ADRSnowControlZone::ADRSnowControlZone()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
	ZoneBounds->SetupAttachment(Root);
	ZoneBounds->SetBoxExtent(FVector(500.f, 500.f, 200.f));
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneBounds->SetHiddenInGame(true);
}

void ADRSnowControlZone::BeginPlay()
{
	Super::BeginPlay();

	if (bCreateDebugWidget && DebugWidgetClass)
	{
		APlayerController* PlayerController =
			GetWorld()
				? GetWorld()->GetFirstPlayerController()
				: nullptr;
		if (IsValid(PlayerController))
		{
			DebugWidget = CreateWidget<UUserWidget>(
				PlayerController,
				DebugWidgetClass);
			if (IsValid(DebugWidget))
			{
				DebugWidget->AddToViewport();
				DebugTextBlock = Cast<UTextBlock>(
					DebugWidget->GetWidgetFromName(DebugTextBlockName));
			}
		}
	}

	UpdateDebugWidget();
	TimeUntilNextDebugUpdate = DebugUpdateInterval;
}

void ADRSnowControlZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(DebugWidget))
	{
		DebugWidget->RemoveFromParent();
		DebugWidget = nullptr;
		DebugTextBlock = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ADRSnowControlZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bCreateDebugWidget && !IsValid(DebugTextBlock))
	{
		return;
	}

	TimeUntilNextDebugUpdate -= DeltaSeconds;
	if (TimeUntilNextDebugUpdate > 0.f)
	{
		return;
	}

	UpdateDebugWidget();
	TimeUntilNextDebugUpdate = FMath::Max(0.01f, DebugUpdateInterval);
}

FBox ADRSnowControlZone::GetZoneWorldBounds() const
{
	return IsValid(ZoneBounds)
		? ZoneBounds->Bounds.GetBox()
		: FBox(ForceInit);
}

FDRSnowControlRatio ADRSnowControlZone::GetControlRatio() const
{
	FDRSnowControlRatio EmptyRatio;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return EmptyRatio;
	}

	const UDRSnowVolumeSubsystem* SnowVolumeSubsystem =
		World->GetSubsystem<UDRSnowVolumeSubsystem>();
	if (!IsValid(SnowVolumeSubsystem))
	{
		return EmptyRatio;
	}

	if (bUseHexPrismShape && IsValid(ZoneBounds))
	{
		return SnowVolumeSubsystem->QuerySnowInHexPrism(
			GetZoneWorldBounds(),
			ZoneBounds->GetComponentTransform(),
			ZoneBounds->GetUnscaledBoxExtent(),
			TeamIdA,
			TeamIdB);
	}

	return SnowVolumeSubsystem->QuerySnowInBounds(
		GetZoneWorldBounds(),
		TeamIdA,
		TeamIdB);
}

FDRSnowVoxelMaterialScanResult ADRSnowControlZone::ScanVoxelMaterials() const
{
	FDRSnowVoxelMaterialScanResult Result;
	Result.TeamIdA = TeamIdA;
	Result.TeamIdB = TeamIdB;

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	if (!IsValid(VoxelWorld) ||
		!VoxelWorld->IsCreated() ||
		!IsValid(ZoneBounds))
	{
		return Result;
	}

	const FVoxelIntBox VoxelBounds =
		MakeVoxelBoundsFromWorldBounds(
			VoxelWorld,
			GetZoneWorldBounds());
	if (!VoxelBounds.IsValid())
	{
		return Result;
	}

	const int32 TeamMaterialIndexA =
		TeamIdA == INDEX_NONE
			? INDEX_NONE
			: UDRVoxelTeamColorLibrary::GetTeamMaterialIndex(TeamIdA);
	const int32 TeamMaterialIndexB =
		TeamIdB == INDEX_NONE
			? INDEX_NONE
			: UDRVoxelTeamColorLibrary::GetTeamMaterialIndex(TeamIdB);

	FVoxelData& Data = VoxelWorld->GetData();
	{
		FVoxelReadScopeLock Lock(Data, VoxelBounds, FUNCTION_FNAME);
		for (int32 Z = VoxelBounds.Min.Z; Z < VoxelBounds.Max.Z; ++Z)
		{
			for (int32 Y = VoxelBounds.Min.Y; Y < VoxelBounds.Max.Y; ++Y)
			{
				for (int32 X = VoxelBounds.Min.X; X < VoxelBounds.Max.X; ++X)
				{
					if (MaxVoxelScanCount > 0 &&
						Result.ScannedVoxelCount >= MaxVoxelScanCount)
					{
						Result.bTruncated = true;
						break;
					}

					const FIntVector VoxelPosition(X, Y, Z);
					const FVector WorldLocation =
						VoxelWorld->LocalToGlobal(VoxelPosition);
					if (!IsWorldLocationInsideQueryShape(WorldLocation))
					{
						continue;
					}

					++Result.ScannedVoxelCount;

					const FVoxelValue Value = Data.GetValue(VoxelPosition, 0);
					if (Value.IsEmpty())
					{
						continue;
					}

					++Result.FilledVoxelCount;

					const FVoxelMaterial Material =
						Data.GetMaterial(VoxelPosition, 0);
					const int32 MaterialIndex =
						GetDominantMaterialIndex(
							Material,
							VoxelWorld->MaterialConfig);
					if (MaterialIndex == INDEX_NONE)
					{
						++Result.UnknownCount;
						continue;
					}

					if (MaterialIndex == 0)
					{
						++Result.NeutralCount;
						continue;
					}

					++Result.MaterialVoxelCount;
					if (TeamMaterialIndexA != INDEX_NONE &&
						MaterialIndex == TeamMaterialIndexA)
					{
						++Result.CountA;
					}
					else if (TeamMaterialIndexB != INDEX_NONE &&
						MaterialIndex == TeamMaterialIndexB)
					{
						++Result.CountB;
					}
					else
					{
						const int32 TeamId = MaterialIndexToTeamId(MaterialIndex);
						if (Result.TeamIdA == INDEX_NONE)
						{
							Result.TeamIdA = TeamId;
							++Result.CountA;
						}
						else if (Result.TeamIdA == TeamId)
						{
							++Result.CountA;
						}
						else if (Result.TeamIdB == INDEX_NONE)
						{
							Result.TeamIdB = TeamId;
							++Result.CountB;
						}
						else if (Result.TeamIdB == TeamId)
						{
							++Result.CountB;
						}
						else
						{
							++Result.UnknownCount;
						}
					}
				}

				if (Result.bTruncated)
				{
					break;
				}
			}

			if (Result.bTruncated)
			{
				break;
			}
		}
	}

	const int32 KnownTeamCount = Result.CountA + Result.CountB;
	if (KnownTeamCount > 0)
	{
		Result.RatioA =
			static_cast<float>(Result.CountA) /
			static_cast<float>(KnownTeamCount);
		Result.RatioB =
			static_cast<float>(Result.CountB) /
			static_cast<float>(KnownTeamCount);
	}

	if (Result.FilledVoxelCount > 0)
	{
		Result.Coverage =
			static_cast<float>(Result.MaterialVoxelCount) /
			static_cast<float>(Result.FilledVoxelCount);
	}

	return Result;
}

FDRSnowControlRatio ADRSnowControlZone::DebugPrintControlRatio(
	float DisplayTime) const
{
	const FDRSnowControlRatio Ratio = GetControlRatio();
	const FString DebugMessage = BuildControlRatioDebugText(this, Ratio);

	UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMessage);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			FMath::Max(0.1f, DisplayTime),
			FColor::Yellow,
			DebugMessage);
	}

	return Ratio;
}

FString ADRSnowControlZone::DebugGetControlRatioText() const
{
	return BuildControlRatioDebugText(this, GetControlRatio());
}

FString ADRSnowControlZone::BuildSnowCountDebugText() const
{
	const FDRSnowVoxelMaterialScanResult MaterialScan = ScanVoxelMaterials();
	const FDRSnowControlRatio VolumeRatio = GetControlRatio();

	const float DiffA =
		FMath::Abs(
			static_cast<float>(MaterialScan.CountA) -
			VolumeRatio.AmountA);
	const float DiffB =
		FMath::Abs(
			static_cast<float>(MaterialScan.CountB) -
			VolumeRatio.AmountB);

	return FString::Printf(
		TEXT("[Hex Zone Debug]%s\n\n")
		TEXT("Voxel Material Scan\n")
		TEXT("A: %d  B: %d  Neutral: %d  Unknown: %d\n")
		TEXT("Filled Voxels: %d\n")
		TEXT("Material Voxels: %d\n")
		TEXT("Coverage: %.1f%%\n")
		TEXT("Scanned: %d%s\n\n"),
		MaterialScan.bTruncated ? TEXT(" [Truncated]") : TEXT(""),
		MaterialScan.CountA,
		MaterialScan.CountB,
		MaterialScan.NeutralCount,
		MaterialScan.UnknownCount,
		MaterialScan.FilledVoxelCount,
		MaterialScan.MaterialVoxelCount,
		MaterialScan.Coverage * 100.f,
		MaterialScan.ScannedVoxelCount,
		bUseHexPrismShape ? TEXT(" Hex") : TEXT(" Box"));
}

AVoxelWorld* ADRSnowControlZone::ResolveVoxelWorld() const
{
	if (IsValid(TargetVoxelWorld.Get()))
	{
		return TargetVoxelWorld.Get();
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}

bool ADRSnowControlZone::IsWorldLocationInsideQueryShape(
	const FVector& WorldLocation) const
{
	if (!IsValid(ZoneBounds))
	{
		return false;
	}

	const FVector LocalLocation =
		ZoneBounds->GetComponentTransform().InverseTransformPosition(
			WorldLocation);
	const FVector Extent = ZoneBounds->GetUnscaledBoxExtent();

	if (FMath::Abs(LocalLocation.Z) > Extent.Z)
	{
		return false;
	}

	if (!bUseHexPrismShape)
	{
		return FMath::Abs(LocalLocation.X) <= Extent.X &&
			FMath::Abs(LocalLocation.Y) <= Extent.Y;
	}

	const float HexRadius = FMath::Max(1.f, FMath::Min(Extent.X, Extent.Y));
	const float AbsX = FMath::Abs(LocalLocation.X);
	const float AbsY = FMath::Abs(LocalLocation.Y);
	const float HalfSqrt3 = 0.86602540378f;

	return AbsX <= HexRadius &&
		AbsY <= HalfSqrt3 * HexRadius &&
		HalfSqrt3 * AbsX + 0.5f * AbsY <= HalfSqrt3 * HexRadius;
}

void ADRSnowControlZone::UpdateDebugWidget()
{
	if (!IsValid(DebugTextBlock) && IsValid(DebugWidget))
	{
		DebugTextBlock = Cast<UTextBlock>(
			DebugWidget->GetWidgetFromName(DebugTextBlockName));
	}

	if (!IsValid(DebugTextBlock))
	{
		return;
	}

	DebugTextBlock->SetText(FText::FromString(BuildSnowCountDebugText()));
}

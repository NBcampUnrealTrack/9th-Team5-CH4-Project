#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "VoxelIntBox.h"
#include "VoxelMaterial.h"
#include "DRSnowControlZone.generated.h"

class AVoxelWorld;
class UBoxComponent;
class UDRPointLocationWidget;
class USceneComponent;
class UTextBlock;
class UUserWidget;
class UWidgetComponent;

#pragma region Debug

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowVoxelMaterialTeamCount
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 VoxelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	float Ratio = 0.f;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowVoxelMaterialScanResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	TArray<FDRSnowVoxelMaterialTeamCount> Teams;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 NeutralCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 UnknownCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 FilledVoxelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 MaterialVoxelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 ScannedVoxelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	bool bTruncated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	float Coverage = 0.f;
};

#pragma endregion

UCLASS()
class DEEPRAIDERS_API ADRSnowControlZone : public AActor
{
	GENERATED_BODY()

public:
	ADRSnowControlZone();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Snow|Control")
	FBox GetZoneWorldBounds() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Control")
	FDRSnowControlRatio GetControlRatio() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Control")
	void RefreshControlRatio();

	UFUNCTION(BlueprintPure, Category = "Snow|Control")
	int32 GetLeadingTeamId() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<USceneComponent> Root;

	// 레벨에 배치한 뒤 BoxExtent로 점령/계산 구역을 지정한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<UBoxComponent> ZoneBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<UWidgetComponent> PointLocationWidgetComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Visual")
	FLinearColor Team0Color = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Visual")
	FLinearColor Team1Color = FLinearColor::Blue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Visual")
	FLinearColor NeutralColor = FLinearColor::White;

	// true면 매 틱, false면 ControlUpdateInterval마다 점령 비율을 갱신한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Control|Update")
	bool bUpdateControlRatioEveryTick = false;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Snow|Control|Update",
		meta = (ClampMin = "0.01", Units = "s", EditCondition = "!bUpdateControlRatioEveryTick"))
	float ControlUpdateInterval = 1.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Snow|Control|Update")
	FDRSnowControlRatio CachedControlRatio;

private:
	void RefreshPointLocationWidget();

#pragma region Debug

public:
	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	FDRSnowVoxelMaterialScanResult ScanVoxelMaterials() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	FString BuildSnowCountDebugText() const;

	FString BuildSnowCountDebugTextFromScan(const FDRSnowVoxelMaterialScanResult& MaterialScan) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug")
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (ClampMin = "0.01"))
	float DebugUpdateInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug")
	bool bCreateDebugWidget = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (EditCondition = "bCreateDebugWidget"))
	TSubclassOf<UUserWidget> DebugWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (EditCondition = "bCreateDebugWidget"))
	FName DebugTextBlockName = TEXT("TextBlock_SnowCount");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (ClampMin = "1"))
	int32 MaxVoxelScanCount = 250000;

private:
	static int32 GetDominantMaterialIndex(const FVoxelMaterial& Material, EVoxelMaterialConfig MaterialConfig);
	static int32 MaterialIndexToTeamId(int32 MaterialIndex);
	static void ExpandVoxelBoundsForWorldPoint(
		const AVoxelWorld* VoxelWorld,
		const FVector& WorldPoint,
		FIntVector& InOutMin,
		FIntVector& InOutMax);
	static FVoxelIntBox MakeVoxelBoundsFromWorldBounds(
		const AVoxelWorld* VoxelWorld,
		const FBox& WorldBounds);
	void InitializeDebug();
	void DeinitializeDebug();
	AVoxelWorld* ResolveVoxelWorld() const;
	bool IsWorldLocationInsideZoneBounds(const FVector& WorldLocation) const;
	void UpdateDebugWidget();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DebugWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DebugTextBlock = nullptr;

	FTimerHandle DebugUpdateTimerHandle;
	FTimerHandle ControlUpdateTimerHandle;

#pragma endregion
};

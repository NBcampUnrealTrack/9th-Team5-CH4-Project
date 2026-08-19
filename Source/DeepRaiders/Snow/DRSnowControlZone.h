#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "DRSnowControlZone.generated.h"

class AVoxelWorld;
class UBoxComponent;
class USceneComponent;
class UTextBlock;
class UUserWidget;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowVoxelMaterialScanResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 TeamIdA = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 TeamIdB = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 CountA = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	int32 CountB = 0;

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
	float RatioA = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	float RatioB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Debug")
	float Coverage = 0.f;
};

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

	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	FDRSnowVoxelMaterialScanResult ScanVoxelMaterials() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	FDRSnowControlRatio DebugPrintControlRatio(float DisplayTime = 2.f) const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	FString DebugGetControlRatioText() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	FString BuildSnowCountDebugText() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<USceneComponent> Root;

	// 레벨에 배치한 뒤 BoxExtent로 점령/계산 구역을 지정한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<UBoxComponent> ZoneBounds;

	// 지정하면 해당 두 팀만 점령률로 계산한다.
	// 비워두면 Bounds 안에서 처음 발견되는 두 팀을 A/B로 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	int32 TeamIdA = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	int32 TeamIdB = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug")
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug")
	bool bUseHexPrismShape = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (ClampMin = "0.01"))
	float DebugUpdateInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug")
	bool bCreateDebugWidget = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (EditCondition = "bCreateDebugWidget"))
	TSubclassOf<UUserWidget> DebugWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (EditCondition = "bCreateDebugWidget"))
	FName DebugTextBlockName = TEXT("TextBlock_SnowCount");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Debug", meta = (ClampMin = "1"))
	int32 MaxVoxelScanCount = 250000;

private:
	AVoxelWorld* ResolveVoxelWorld() const;
	bool IsWorldLocationInsideQueryShape(const FVector& WorldLocation) const;
	void UpdateDebugWidget();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DebugWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DebugTextBlock = nullptr;

	float TimeUntilNextDebugUpdate = 0.f;
};

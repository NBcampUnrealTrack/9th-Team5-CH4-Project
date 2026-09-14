#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Snow/DRSnowVolumeTypes.h"
#include "VoxelIntBox.h"
#include "VoxelMaterial.h"
#include "DeepRaiders/Gameplay/Voxel/DRMeshVoxelCarver.h"
#include "DRSnowControlZone.generated.h"

class AVoxelWorld;
class UBoxComponent;
class UDRPointLocationWidget;
class USceneComponent;
class UTextBlock;
class UUserWidget;
class UWidgetComponent;
class USoundBase;
class UGameplayEffect;
class UStaticMesh;

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
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ResetForGame();
	bool PrepareForGame() const;
	void ShowActivationReveal();
	void ActivateForPhase(int32 PhaseIndex);
	void FreezeForGameEnd();
	bool StartEndCleanup(TFunction<void(bool)>&& Completion);
	bool TryClaimCompletionReward();
	float GetRewardSnowGauge() const { return RewardSnowGauge; }
	const TArray<TSubclassOf<UGameplayEffect>>& GetRewardEffects() const { return RewardEffects; }

	UFUNCTION(BlueprintPure, Category = "Snow|Control")
	bool IsZoneActive() const { return bZoneActive; }

	bool ShouldActivateForPhase(int32 PhaseIndex) const
	{
		return !bZoneActive && !bControlFrozen && PhaseIndex >= ActivationPhaseIndex;
	}

	int32 GetActivationCountdownRemaining(int32 PhaseIndex, int32 PhaseElapsedSeconds) const;
	const FText& GetActivationCountdownText() const { return ActivationCountdownText; }
	USoundBase* GetActivationCountdownSound() const { return ActivationCountdownSound; }
	USoundBase* GetActivatedSound() const { return ActivatedSound; }

	UFUNCTION(BlueprintPure, Category = "Snow|Control")
	bool IsZoneCompleted() const { return bZoneCompleted; }

	UFUNCTION(BlueprintPure, Category = "Snow|Control")
	float GetCompletionRatio() const { return CompletionRatio; }

	UFUNCTION(BlueprintPure, Category = "Snow|Control")
	FBox GetZoneWorldBounds() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Control")
	FDRSnowControlRatio GetControlRatio() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Control")
	void RefreshControlRatio();

	UFUNCTION(BlueprintPure, Category = "Snow|Control")
	int32 GetLeadingTeamId() const;

protected:
	// 닫힌 메쉬를 지정한다. 패키징 시 메쉬의 Allow CPU Access가 필요하다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	// 이 Box 안에서 목표 메쉬 외부의 기존 복셀만 제거한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<UBoxComponent> CleanupBounds;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control", meta = (ClampMin = "0"))
	int32 ActivationPhaseIndex = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control|Activation")
	bool bShowActivationCountdown = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control|Activation",
		meta = (ClampMin = "1", EditCondition = "bShowActivationCountdown", Units = "s"))
	int32 ActivationCountdownSeconds = 3;

	// {Seconds} 자리에 남은 초를 표시한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control|Activation",
		meta = (EditCondition = "bShowActivationCountdown"))
	FText ActivationCountdownText = NSLOCTEXT(
		"DRControlZone", "ActivationCountdown", "거점 활성화까지 {Seconds}초");

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control|Activation")
	TObjectPtr<USoundBase> ActivationCountdownSound;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control|Activation")
	TObjectPtr<USoundBase> ActivatedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control",
		meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RequiredCompletionRatio = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Reward",
		meta = (ClampMin = "0"))
	float RewardSnowGauge = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Reward")
	TArray<TSubclassOf<UGameplayEffect>> RewardEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Visual",
		meta = (ClampMin = "1", ClampMax = "255"))
	int32 OutlineStencilValue = 1;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control|Visual",
		meta = (ClampMin = "0.0", Units = "s"))
	float ActivationRevealDuration = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Cleanup")
	bool bCleanupOnGameEnd = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Cleanup",
		meta = (ClampMin = "1"))
	int32 MaxCleanupVoxelCount = 2000000;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow|Control")
	TObjectPtr<UWidgetComponent> PointLocationWidgetComponent;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Snow|Control")
	FText DisplayName;

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

	UPROPERTY(ReplicatedUsing = OnRep_ControlState, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Snow|Control|Update")
	FDRSnowControlRatio CachedControlRatio;

private:
	void RefreshPointLocationWidget();
	bool EnsureTargetMask() const;
	void RefreshControlVisuals();
	void ClearActivationReveal();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowActivationReveal(float Duration);

	UFUNCTION()
	void OnRep_ControlState();

	UPROPERTY(ReplicatedUsing = OnRep_ControlState)
	bool bZoneActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_ControlState)
	bool bZoneCompleted = false;

	UPROPERTY(ReplicatedUsing = OnRep_ControlState)
	float CompletionRatio = 0.f;

	bool bRewardGranted = false;
	bool bControlFrozen = false;
	bool bActivationRevealShown = false;
	bool bActivationRevealActive = false;
	mutable FDRMeshVoxelMask TargetMask;
	mutable TWeakObjectPtr<UStaticMesh> CachedTargetMesh;
	mutable TWeakObjectPtr<AVoxelWorld> CachedVoxelWorld;
	mutable FTransform CachedMeshTransform;
	mutable FTransform CachedVoxelTransform;
	mutable FIntVector CachedWorldOffset = FIntVector::ZeroValue;
	mutable float CachedVoxelSize = 0.f;
	mutable bool bMaskAttempted = false;
	mutable int32 CachedMaskLimit = 0;
	TSharedPtr<FThreadSafeBool, ESPMode::ThreadSafe> CleanupCancellation;

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

	// 메쉬 Bounds의 마스크 샘플 한도다. 초과 시 부분 집계 대신 준비를 중단한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snow|Control|Sampling",
		meta = (ClampMin = "1"))
	int32 MaxVoxelScanCount = 250000;

private:
	static int32 GetDominantMaterialIndex(const FVoxelMaterial& Material, EVoxelMaterialConfig MaterialConfig);
	void InitializeDebug();
	void DeinitializeDebug();
	AVoxelWorld* ResolveVoxelWorld() const;
	void UpdateDebugWidget();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DebugWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DebugTextBlock = nullptr;

	FTimerHandle DebugUpdateTimerHandle;
	FTimerHandle ControlUpdateTimerHandle;
	FTimerHandle ActivationRevealTimerHandle;

#pragma endregion
};

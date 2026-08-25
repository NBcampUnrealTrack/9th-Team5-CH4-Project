#pragma once

#include "CoreMinimal.h"
#include "DRWorldItemActor.h"
#include "DRWorldItemTypes.h"
#include "DRHoveringWorldItemActor.generated.h"

class UDRWorldItemPresentationProfile;
class UNiagaraComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRHoveringWorldItemActor : public ADRWorldItemActor
{
	GENERATED_BODY()
	
public:
	ADRHoveringWorldItemActor();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	
	// Deferred Spawn이 완료되기 전에 서버에서 호출
	bool InitializeHoverPresentation(const FVector& SourceWorldLocation, bool bPlayEmergence);
	
protected:
	virtual void BeginPlay() override;
	
	// Mesh, VFX 등, 전반의 Presentation 재설정
	virtual void RefreshItemPresentation() override;
	virtual void HandleWorldItemStateChanged() override;
	
	UFUNCTION()
	void OnRep_EmergenceData();
	
	// 외관만 표시할 StaticMesh, 기존 Mesh는 Root로 설정되어 불편
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Item|Presentation")
	TObjectPtr<UStaticMeshComponent> PresentationMeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Item|Interaction")
	TObjectPtr<USphereComponent> InteractionSphereComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Item|VFX")
	TObjectPtr<UNiagaraComponent> SpawnTrailVFXComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Item|VFX")
	TObjectPtr<UNiagaraComponent> IdleAuraVFXComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Item|Interaction",
		meta = (ClampMin = "1.0", Units = "cm"))
	float InteractionRadius = 100.f;
	
private:
	// 서버 생성 시간 기준, 남은 Emergence Duration 계산 후 Timer 설정
	void ScheduleEmergenceCompletion();
	void CompleteEmergence();
	
	// WorldItemState에 맞는 표시 상태 적용
	void ApplyPresentationState();
	void RefreshRarityPresentation();
	void RefreshPresentationTransform();
	
	void UpdateEmergenceTransform();
	void UpdateHoverTransform();
	
	float GetSynchronizedWorldTime() const;
	float GetEmergenceDuration() const;
	float GetEmergenceArcHeight() const;
	float GetHoverAmplitude() const;
	float GetHoverFrequency() const;
	const UDRWorldItemPresentationProfile* GetPresentationProfile() const;
	
	bool bLoggedMissingPresentationProfile = false;
	
	UPROPERTY(ReplicatedUsing = OnRep_EmergenceData)
	FDRWorldItemEmergenceData EmergenceData;
	
	FTimerHandle EmergenceTimerHandle;
};


















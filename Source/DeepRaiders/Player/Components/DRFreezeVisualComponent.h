#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRFreezeVisualComponent.generated.h"

class UAbilitySystemComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UDRFreezeVisualProfile;

struct FOnAttributeChangeData;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRFreezeVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRFreezeVisualComponent();

	/**
	 * Character의 ASC가 준비됐을 때 호출한다.
	 *
	 * Dedicated Server에서는 아무 Presentation 작업도 하지 않는다.
	 */
	void BindAbilitySystem(UAbilitySystemComponent* InASC);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 현재 설정된 눈 파츠를 실제 StaticMeshComponent로 생성 */
	void CreateVisualParts();

	/** ASC Delegate 제거 */
	void UnbindAbilitySystem();

	/** 현재 ASC의 Gauge 값을 읽어 Target을 갱신 */
	void RefreshTargetFreezeAmount(bool bSnapImmediately);

	/** FreezeGauge 변경 이벤트 */
	void HandleFreezeGaugeChanged(const FOnAttributeChangeData& Data);

	/** MaxFreezeGauge 변경 이벤트 */
	void HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& Data);

	/** 실제 눈 Mesh들의 Visibility / Scale을 갱신 */
	void ApplyVisualFreezeAmount(float FreezeAmount);

	bool ShouldCreateVisuals() const;

private:
	// BP에서 설정할 눈 파츠 목록.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Freeze|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDRFreezeVisualProfile> VisualProfile;
	
	/** 런타임에 생성된 StaticMeshComponent */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RuntimePartComponents;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	FDelegateHandle FreezeGaugeChangedHandle;
	FDelegateHandle MaxFreezeGaugeChangedHandle;

	float TargetFreezeAmount = 0.f;
	float VisualFreezeAmount = 0.f;

	bool bVisualPartsCreated = false;
};

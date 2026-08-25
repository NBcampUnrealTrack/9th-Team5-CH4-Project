#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRFreezeVisualComponent.generated.h"

class UAbilitySystemComponent;
class UStaticMeshComponent;
class UDRFreezeVisualProfile;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
struct FGameplayTag;
struct FOnAttributeChangeData;

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRFreezeVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRFreezeVisualComponent();

	/**
	 * Character의 ASC가 준비됐을 때 호출.
	 * Gameplay 상태를 읽기 위한 연결만 수행한다.
	 */
	void BindAbilitySystem(UAbilitySystemComponent* InASC);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// -------------------------------------------------
	// Ability System
	// -------------------------------------------------

	void UnbindAbilitySystem();
	void HandleFreezeGaugeChanged(const FOnAttributeChangeData& Data);
	void HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& Data);
	void RefreshTargetFreezeAmount(bool bSnapImmediately);


	// -------------------------------------------------
	// Visual
	// -------------------------------------------------

	bool ShouldCreateVisuals() const;
	void ApplyVisualFreezeAmount(float FreezeAmount);


	// -------------------------------------------------
	// Attachments
	// -------------------------------------------------

	void CreateAttachmentVisuals();
	void DestroyAttachmentVisuals();
	void ApplyAttachmentVisuals(float FreezeAmount);


	// -------------------------------------------------
	// Surface Frost
	// -------------------------------------------------

	void CreateSurfaceFrostVisual();
	void ClearSurfaceFrostVisual();
	void ApplySurfaceFrostVisual(float FreezeAmount);


	// -------------------------------------------------
	// Niagara
	// -------------------------------------------------

	void CreateNiagaraVisual();
	void ClearNiagaraVisual();
	void ApplyNiagaraVisual(float FreezeAmount);

	// -------------------------------------------------
	// Frozen Shell
	// -------------------------------------------------

	void CreateFrozenShellVisual();
	void DestroyFrozenShellVisual();
	void SetFrozenShellVisible(bool bVisible);
	void HandleFrozenTagChanged(const FGameplayTag Tag, int32 NewCount);
	
private:
	// -------------------------------------------------
	// Configuration
	// -------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Freeze|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDRFreezeVisualProfile> VisualProfile;

	// -------------------------------------------------
	// Runtime Visuals
	// -------------------------------------------------

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RuntimePartComponents;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceFrostMID;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FreezeNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FrozenShellComponent;

	FDelegateHandle FrozenTagChangedHandle;

	// -------------------------------------------------
	// Ability System
	// -------------------------------------------------

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	FDelegateHandle FreezeGaugeChangedHandle;


	// -------------------------------------------------
	// Runtime State
	// -------------------------------------------------

	float TargetFreezeAmount = 0.f;
	float VisualFreezeAmount = 0.f;
};

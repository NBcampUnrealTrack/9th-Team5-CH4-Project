#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"
#include "DRCharacterShadowComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
struct FGameplayTag;
struct FHitResult;

/*
 * 캐릭터 아래의 지면을 추적하고 높이에 따라 투명도를 조절하는 로컬 표현 컴포넌트
 */
UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRCharacterShadowComponent : public UDecalComponent
{
	GENERATED_BODY()
	
public:
	UDRCharacterShadowComponent();
	
	// Character의 ASC가 준비됐을 때 호출한다.
	void BindAbilitySystem(UAbilitySystemComponent* InASC);
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	void UnbindAbilitySystem();
	void HandleStealthedTagChanged(const FGameplayTag Tag, int32 NewCount);
	
	bool ShouldCreateVisuals() const;
	bool TraceGround(FHitResult& OutHit, float& OutGroundDistance) const;
	
	void RefreshTrackingState();
	void UpdateShadow();
	void SetShadowVisible(bool NewVisible);
	
	float CalculateOpacity(float GroundDistance) const;
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow|Trace",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float TraceStartOffset = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow|Trace",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float TraceDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow|Trace",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float SurfaceOffset = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow|Fade",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float FullOpacityDistance = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow|Fade",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float FadeOutDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow|Fade",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacity = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow|Material", meta = (AllowPrivateAccess = "true"))
	FName OpacityParameterName = TEXT("Opacity");

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShadowMaterialInstance;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle StealthedTagChangedHandle;

	bool bStealthed = false;
};

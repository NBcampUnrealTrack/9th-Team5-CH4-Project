#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Core/Interaction/DRInteractionTypes.h"
#include "DRInteractionComponent.generated.h"

class AActor;
class APawn;
class UWorld;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDRFocusedInteractableChanged, AActor*, const FDRInteractionPromptData&);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UDRInteractionComponent();
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	AActor* GetFocusedTarget() const
	{
		return FocusedTarget.Get();
	}
	
	const FDRInteractionPromptData& GetFocusedPromptData() const
	{
		return FocusedPromptData;
	}
	
	EDRInteractionValidationResult ValidateInteractionAttempt(const FDRInteractionAttempt& Attempt) const;
	
	FDRFocusedInteractableChanged OnFocusedInteractableChanged;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxInteractionDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction",
		meta = (ClampMin = "0.1", ClampMax = "89.0", Units = "deg"))
	float MaxInteractionAngleDegrees = 6.f;
	
	// 현재 대상보다 새 대상이 이 각도 이상 조준점에 가까워야 포커스를 교체한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction",
		meta = (ClampMin = "0.0", Units = "deg"))
	float FocusSwitchAngleAdvantageDegrees = 1.f;
	
private:
	bool CanUpdateLocalFocus(APawn*& OutInteractor) const;
	
	AActor* FindBestInteractionTarget(APawn* Interactor, FDRInteractionPromptData& OutPromptData) const;
	
	EDRInteractionValidationResult EvaluateInteractionTarget(APawn* Interactor, AActor* Target,
		float& OutAimDot, float& OutDistanceSquared, FDRInteractionPromptData* OutPromptData) const;
	
	// 상호작용 대상이 다른 물체에 가려지지 않는지 체크
	bool HasClearLineOfSight(UWorld* World, APawn* Interactor, AActor* Target,
		const FVector& ViewLocation, const FVector& TargetLocation) const;
	
	void SetFocusedTarget(AActor* NewTarget, const FDRInteractionPromptData& NewPromptData);
	
	void ClearFocusedTarget();
	
	TWeakObjectPtr<AActor> FocusedTarget;
	FDRInteractionPromptData FocusedPromptData;	
};

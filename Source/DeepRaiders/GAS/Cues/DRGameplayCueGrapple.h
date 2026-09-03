#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "DRGameplayCueGrapple.generated.h"

class UCableComponent;
class UNiagaraComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRGameplayCueGrapple : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()
	
public:
	ADRGameplayCueGrapple(const FObjectInitializer& ObjectInitializer);
	
	virtual void Tick(float DeltaSeconds) override;
	
	virtual bool Recycle() override;
	virtual void ReuseAfterRecycle() override;
	
protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	// 성공한 케이블이 목표 지점에 도착한 프레임에 한 번만 호출된다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Grapple|Presentation", meta = (DisplayName = "Hook Attached"))
	void ReceiveHookAttached(const FVector& AttachLocation, const FVector& AttachNormal);
	
private:
	enum class EPresentationPhase : uint8
	{
		Inactive,
		Extending,
		Attached,
		Retracting,
	};
	
	bool BeginPresentation(AActor* Target, const FGameplayCueParameters& Parameters);
	
	bool ResolveStartAttachment(AActor* Target, USceneComponent*& OutComponent, FName& OutSocketName) const;
	
	FVector GetCurrentStartLocation() const;
	float CalculatePhaseDuration(const FVector& StartLocation, const FVector& EndLocation, float Speed) const;
	
	void UpdateHookLocation(const FVector& NewLocation);
	
	void EnterAttachedPhase();
	
	// 현재 훅 위치에서 발사 지점으로 돌아가는 단계를 시작
	void BeginRetraction();
	void FinishPresentation();
	void ResetPresentationState();
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PresentationRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> HookRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCableComponent> CableComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> HookMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> HookNiagaraComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Attachment", meta = (AllowPrivateAccess = "true"))
	FName LaunchSocketName = TEXT("VFXPoint");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Timing",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm/s"))
	float HookTravelSpeed = 6000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Timing",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm/s"))
	float HookRetractSpeed = 8000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Timing",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float MinimumPhaseDuration = 0.05f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Cable",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float CableLengthScale = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Target",
		meta = (AllowPrivateAccess = "true"))
	bool IsEffectCauserTrackingEnabled = true;

	TWeakObjectPtr<USceneComponent> StartComponent;
	TWeakObjectPtr<AActor> FollowTargetActor;
	
	FName StartSocketName = NAME_None;
		
	FVector LaunchLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FVector TargetNormal = FVector::ZeroVector;
	FVector RetractStartLocation = FVector::ZeroVector;
	bool bAttachmentFeedbackPlayed = false;
	
	float PhaseElapsedTime = 0.f;
	
	EPresentationPhase PresentationPhase = EPresentationPhase::Inactive;
	
	bool bRetractAfterExtension = false;
	
	float CurrentPhaseDuration = 0.f;
};









#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "DRGameplayCueAbsorb.generated.h"

class ADRPlayerCharacter;
class UNiagaraComponent;
class USceneComponent;
class UStaticMeshComponent;
class UDRRangedWeaponDefinition;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRGameplayCueAbsorb : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ADRGameplayCueAbsorb(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual bool Recycle() override;
	virtual void ReuseAfterRecycle() override;

protected:
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

private:
	bool BeginPresentation(AActor* MyTarget, const FGameplayCueParameters& Parameters);
	void StopPresentation(bool bPlayEndSound);
	void ResetPresentationState();
	bool ResolveBeamEndpoints(FVector& OutBeamStart, FVector& OutBeamEnd) const;
	void UpdateBeamEndpoints();
	FGameplayCueParameters BuildSoundParameters() const;
	void PlayStartAndLoopSounds();
	void StopLoopAndPlayEndSound(bool bPlayEndSound);

	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<USceneComponent> PresentationRoot;

	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<UNiagaraComponent> BeamNiagaraComponent;

	TWeakObjectPtr<ADRPlayerCharacter> TargetCharacter;
	TWeakObjectPtr<UStaticMeshComponent> StartComponent;
	TWeakObjectPtr<const UDRRangedWeaponDefinition> WeaponDefinition;

	FGameplayTag ActiveStartSoundCueTag;
	FGameplayTag ActiveLoopSoundCueTag;
	FGameplayTag ActiveEndSoundCueTag;
	FName StartSocketName = NAME_None;
	FName BeamStartParameterName = NAME_None;
	FName BeamEndParameterName = NAME_None;
	bool bPresentationActive = false;
};

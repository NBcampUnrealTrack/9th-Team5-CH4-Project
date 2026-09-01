#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameFramework/Actor.h"
#include "DRHotPackArea.generated.h"

class ADRPlayerCharacter;
class UAbilitySystemComponent;
class UGameplayEffect;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class DEEPRAIDERS_API ADRHotPackArea : public AActor
{
	GENERATED_BODY()

public:
	ADRHotPackArea();

	void Initialize(ADRPlayerCharacter* SourceCharacter);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hot Pack")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hot Pack")
	TObjectPtr<USphereComponent> RecoveryArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hot Pack")
	TObjectPtr<UStaticMeshComponent> HotPackMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hot Pack", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AreaRadius = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hot Pack", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float AreaDuration = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hot Pack")
	TSubclassOf<UGameplayEffect> RecoveryEffectClass;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool IsFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void ApplyRecovery(AActor* TargetActor);
	void RemoveRecovery(AActor* TargetActor);
	void RemoveAllRecoveries();
	UAbilitySystemComponent* GetTargetAbilitySystem(AActor* TargetActor) const;

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> ActiveRecoveryEffects;
	int32 SourceTeamId = INDEX_NONE;
};

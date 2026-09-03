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
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Initialize(
		ADRPlayerCharacter* SourceCharacter,
		float InAreaRadius,
		float InAreaDuration,
		TSubclassOf<UGameplayEffect> InRecoveryEffectClass,
		float InHealthRecoveryAmount,
		float InFreezeGaugeRecoveryAmount);

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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_AreaRadius, Category = "Hot Pack")
	float AreaRadius = 400.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hot Pack")
	float AreaDuration = 8.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hot Pack")
	TSubclassOf<UGameplayEffect> RecoveryEffectClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hot Pack")
	float HealthRecoveryAmount = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hot Pack")
	float FreezeGaugeRecoveryAmount = 0.0f;

private:
	void RefreshArea();

	UFUNCTION()
	void OnRep_AreaRadius();

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

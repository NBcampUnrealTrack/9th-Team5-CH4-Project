#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DeepRaiders/Combat/Projectile/DRProjectileTypes.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "GameFramework/Actor.h"
#include "DRTurret.generated.h"

struct FGameplayEffectSpecHandle;
class UAbilitySystemComponent;
class UDRProjectileWeaponItemDefinition;
class ADRPlayerState;
class ADRProjectile;
class APawn;
class UStaticMeshComponent;
struct FGameplayEffectSpecHandle;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRTurretWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon")
	TSubclassOf<ADRProjectile> ProjectileClass;

	/** 피격 VFX와 사운드 조회에만 사용하는 무기 Definition이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon")
	TObjectPtr<UDRProjectileWeaponItemDefinition> ProjectilePresentationDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon",
		meta = (ClampMin = "0.01", Units = "s"))
	float FireInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon",
		meta = (ClampMin = "1.0", Units = "cm"))
	float MaxAttackDistance = 10000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Detection",
		meta = (ClampMin = "0.05", Units = "s"))
	float DetectionInterval = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Detection",
		meta = (ClampMin = "0.0", Units = "deg"))
	float AimToleranceDegrees = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Detection",
		meta = (ClampMin = "0.1"))
	float RotationInterpSpeed = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon")
	float BreakableDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon")
	TArray<FDRGameplayEffectData> ImpactEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon")
	FDRProjectileWorldImpactData WorldImpactData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Weapon")
	FDRProjectileFalloffSettings FalloffSettings;
};

/** 서버에서 생성되어 모든 클라이언트에 복제되는 설치형 포탑의 기반 Actor다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRTurret : public AActor
{
	GENERATED_BODY()

public:
	ADRTurret();

	/** Deferred Spawn 중 설치 팀, 수명, 파괴 후 적용할 쿨다운 정보를 설정한다. */
	void InitializeTurret(ADRPlayerState* InInstallerPlayerState, int32 InOwnerTeamId, float InLifeSpan,
		UAbilitySystemComponent* InOwnerAbilitySystemComponent,
		FGameplayTag InCooldownTag, float InCooldownDuration,
		const FDRTurretWeaponSettings& InWeaponSettings);

	UFUNCTION(BlueprintPure, Category = "Turret")
	int32 GetOwnerTeamId() const { return OwnerTeamId; }

	UFUNCTION(BlueprintPure, Category = "Turret")
	bool IsInstalledBy(const ADRPlayerState* PlayerState) const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFirePresentation(
		UDRProjectileWeaponItemDefinition* PresentationDefinition,
		FVector_NetQuantize FireLocation,
		FRotator FireRotation);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 파생 Blueprint가 메시, 포신, 공격 컴포넌트를 붙일 기준점이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<USceneComponent> TurretRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Mesh")
	TObjectPtr<UStaticMeshComponent> TurretHolderMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Mesh")
	TObjectPtr<USceneComponent> TurretAimPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Mesh")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	/** 실제 투사체가 생성되는 발사 기준점이다. BP에서 위치와 회전을 조정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Mesh")
	TObjectPtr<USceneComponent> TurretMuzzle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Mesh")
	TObjectPtr<UStaticMeshComponent> TurretTankMesh;

	/** 포탑 상하 회전의 기준점이다. 메쉬 피벗에 맞춰 BP에서 조정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Mesh")
	FVector TurretAimPivotLocation = FVector(0.f, 0.f, 150.f);

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Turret")
	int32 OwnerTeamId = INDEX_NONE;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<ADRPlayerState> InstallerPlayerState;

	/** 터렛 탐지 반경을 디버그 구체로 표시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turret|Debug")
	bool IsDrawDetectionRange = false;

private:
	void ApplyOwnerCooldown() const;
	void UpdateTargetAndFire(float DeltaSeconds);
	APawn* FindNearestEnemy() const;
	bool FireAtTarget(APawn* TargetPawn);
	bool ResolveProjectileLaunchVelocity(
		const FVector& SpawnLocation,
		const FVector& AimPoint,
		FVector& OutLaunchVelocity) const;
	void BuildImpactEffectSpecs(TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;
	int32 GetCurrentOwnerTeamId() const;

	float ConfiguredLifeSpan = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> OwnerAbilitySystemComponent;

	FGameplayTag CooldownTag;
	float CooldownDuration = 0.f;

	FDRTurretWeaponSettings WeaponSettings;

	float FireTimer = 0.f;
	float DetectionTimer = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<APawn> CurrentTarget;

	UPROPERTY(ReplicatedUsing = OnRep_AimPitch)
	float AimPitch = 0.f;

	UFUNCTION()
	void OnRep_AimPitch();
};

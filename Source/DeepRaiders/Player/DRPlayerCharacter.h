#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "DRPlayerCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UAnimMontage;
class UDRItemDefinition;

class UVoxelNoClippingComponent;
class UDRCharacterMovementComponent;
class UDRMiningComponent;
class UDRTeleportComponent;
class UDRMeleeCombatComponent;
class UDRJetpackComponent;
class UDRPlayerLifecycleComponent;
class UDRHeldItemComponent;
class UAbilitySystemComponent;
class UGameplayEffect;
class USpringArmComponent;
class UDRPlayerAttributeSet;
class UDRItemAnimationSet;
class UDRFreezeVisualComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDROnPlayerCharacterDeath);

DECLARE_MULTICAST_DELEGATE_OneParam(FDROnAbilitySystemReady, UAbilitySystemComponent*);

/**
 * 플레이어 캐릭터의 이동 실행, 카메라와 장비 외형 표현을 담당한다.
 *
 * 입력 바인딩은 DRPlayerController가 담당하며,
 * 인벤토리, 퀵슬롯과 실제 장착 상태는 별도 컴포넌트가 관리한다.
 */
UCLASS()
class DEEPRAIDERS_API ADRPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADRPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser);
	
	float GetCurrentHealth() const;
	float GetMaxHealth() const;
	
	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetHealthRatio() const;
	
	UFUNCTION(BlueprintPure, Category = "Player|Health")
	bool IsDead() const;

	virtual void Landed(const FHitResult& Hit) override;

	/** 서버에서 기록한 가장 최근 착지 위치를 반환한다. */
	const FVector& GetLastLandedLocation() const
	{
		return LastLandedLocation;
	}

	void HandleJumpPressed();
	void HandleJumpReleased();
	
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

	/** 복제된 팀에 맞춰 캐릭터 머티리얼 색상을 갱신한다. */
	void RefreshTeamColor();

	/** 로컬 플레이어의 팀원이라면 지속 실루엣을 적용한다. */
	void RefreshTeamSilhouette();

	void ApplyHandEquipmentVisual(
		UStaticMesh* WorldMesh,
		const FTransform& WorldTransform);
		
	/** 현재 손 장비 외형을 제거한다. */
	void ClearHandEquipmentVisual();

	/** 등 소켓에 장비 외형을 적용한다. */
	void ApplyBackEquipmentVisual(UStaticMesh* BackMesh, const FTransform& BackTransform);

	/** 현재 등 장비 외형을 제거한다. */
	void ClearBackEquipmentVisual();

	/*
	 *  제트팩 외형을 적용한다
	 *  적용 시점은 아래와 같음
	 *  PossessedBy
	 *  OnRep_PlayerState
	 *  제트팩 획득 직후 서버
	 *  PlayerState 복제 수신 직후 클라이언트
	 */
	void RefreshJetpackVisual();

	void MoveInput(const FVector2D& MoveInput);
	void LookInput(const FVector2D& LookInput);

	UDRMeleeCombatComponent* GetMeleeCombatComponent() const
	{
		return MeleeCombatComponent;
	}

	UStaticMeshComponent* GetWorldHandEquipmentMesh() const
	{
		return WorldHandEquipmentMesh;
	}

	UDRJetpackComponent* GetJetpackComponent() const
	{
		return JetpackComponent;
	}

	/**
	 * HUD에서 사용할 제트팩 연료 비율.
	 * 소유 게스트는 서버 Fuel Snapshot의
	 * 보간 표시값을 사용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Player|Jetpack|UI")
	float GetDisplayedJetpackFuelRatio() const;

	/** PlayerState의 서버 연료값을 로컬 표시값에 반영한다. */
	void ReconcileJetpackFuelFromServer(float ServerFuel);

	FDROnPlayerCharacterDeath OnPlayerCharacterDeathDelegate;
	
	bool IsFrozen() const;
	
	UFUNCTION(BlueprintPure, Category = "Player|Animation")
	UDRItemAnimationSet* GetCurrentItemAnimationSet() const;

	FDROnAbilitySystemReady OnAbilitySystemReady;

	bool IsAbilitySystemReady() const
	{
		return bAbilitySystemReady;
	}
	
	UFUNCTION(BlueprintPure, Category = "Player|Aim")
	float GetAimPitchDegrees() const;

	float GetAimPitchMinDegrees() const
	{
		return AimPitchMinDegrees;
	}

	float GetAimPitchMaxDegrees() const
	{
		return AimPitchMaxDegrees;
	}
	
protected:
	virtual void BeginPlay() override;
	
	void InitializeAbilitySystem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Voxel")
	TObjectPtr<UVoxelNoClippingComponent> VoxelNoClippingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDRMeleeCombatComponent> MeleeCombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Jetpack", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDRJetpackComponent> JetpackComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Lifecycle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDRPlayerLifecycleComponent> PlayerLifecycleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Held Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDRHeldItemComponent> HeldItemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Freeze", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDRFreezeVisualComponent> FreezeVisualComponent;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> WorldHandEquipmentMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> WorldBackEquipmentMesh;

	
private:
	const UDRPlayerAttributeSet* GetPlayerAttributeSet() const;
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Respawn")
	TSubclassOf<UGameplayEffect> RespawnRestoreHealthEffectClass;
	
	void ApplySpawnAttributeReset();
	
	bool bAbilitySystemReady = false;

	TWeakObjectPtr<UAbilitySystemComponent> ReadyAbilitySystemComponent;

	FVector LastLandedLocation = FVector::ZeroVector;
	
	UPROPERTY(EditDefaultsOnly, Category = "Player|Aim")
	float AimPitchMinDegrees = -90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Aim")
	float AimPitchMaxDegrees = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Team")
	FName TeamColorParameterName = TEXT("Paint Tint");

	UPROPERTY(EditDefaultsOnly, Category = "Player|Team")
	FLinearColor Team0Color = FLinearColor::Red;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Team")
	FLinearColor Team1Color = FLinearColor::Blue;
	
#pragma region QuickSlot

public:
	void SetHeldItemDefinition(UDRItemDefinition* NewItemDefinition);

#pragma endregion

#pragma region Teleport

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Teleport", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDRTeleportComponent> TeleportComponent;
#pragma endregion
};

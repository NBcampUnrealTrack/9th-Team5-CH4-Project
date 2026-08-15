#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DeepRaiders/Item/DRItemActionTypes.h"
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
class UDRItemActionPresentationComponent;
class UDRHealthComponent;
class UDRPlayerLifecycleComponent;
class UDRHeldItemComponent;

class UAbilitySystemComponent;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDROnPlayerCharacterDeath);

/**
 * 플레이어 캐릭터의 이동 실행, 카메라와 장비 외형 표현을 담당한다.
 *
 * 입력 바인딩은 DRPlayerController가 담당하며,
 * 인벤토리, 퀵슬롯과 실제 장착 상태는 별도 컴포넌트가 관리한다.
 */
UCLASS()
class DEEPRAIDERS_API ADRPlayerCharacter 
    : public ACharacter
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ADRPlayerCharacter(const FObjectInitializer& ObjectInitializer);

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Test")
    TSubclassOf<UGameplayEffect> TestAddSnowEffect;
    
    virtual void Landed(const FHitResult& Hit) override;

    /** 지상에서는 점프, 공중에서는 제트팩 사용을 요청한다. */
    void HandleJumpPressed();

    /** 점프 입력과 제트팩 사용을 종료한다. */
    void HandleJumpReleased();

    /** 로컬 플레이어가 손에 든 아이템 던지기를 요청한다. */
    void RequestThrowHeldItem();
    
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_Controller() override;
    virtual void OnRep_PlayerState() override;

    /** 1인칭 및 월드 손 장비 외형을 적용한다. */
    void ApplyHandEquipmentVisual(
        UStaticMesh* FirstPersonMesh,
        UStaticMesh* WorldMesh,
        const FTransform& FirstPersonTransform,
        const FTransform& WorldTransform);

    /** 현재 손 장비 외형을 제거한다. */
    void ClearHandEquipmentVisual();

    /** 등 소켓에 장비 외형을 적용한다. */
    void ApplyBackEquipmentVisual(
        UStaticMesh* BackMesh,
        const FTransform& BackTransform);

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
    
    UFUNCTION(BlueprintPure, Category = "Player|Health")
    float GetCurrentHealth() const;

    UFUNCTION(BlueprintPure, Category = "Player|Health")
    float GetMaxHealth() const;

    UFUNCTION(BlueprintPure, Category = "Player|Health")
    float GetHealthRatio() const;

    UFUNCTION(BlueprintPure, Category = "Player|Health")
    bool IsDead() const;
    
    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser) override;
    
    /** 현재 장착 아이템의 Primary Action을 요청한다. */
    void RequestPrimaryItemAction(EDRItemActionTriggerEvent TriggerEvent);

    /** 현재 장착 아이템의 Secondary Action을 요청한다. */
    void RequestSecondaryItemAction(EDRItemActionTriggerEvent TriggerEvent);

    /**
     * 현재 장착 아이템에 해당 Action이 할당되어 있는지 확인한다.
     * 클라이언트 UX 검사와 서버 권한 검증 양쪽에서 사용한다.
     */
    bool HasHeldItemAction(EDRItemActionType ActionType) const;
    
    /** 서버에서의 땅파기 성공 여부 알려줌 */
    void NotifyMineConfirmedFromServer();
    
    UDRMeleeCombatComponent* GetMeleeCombatComponent() const
    {
        return MeleeCombatComponent;
    }

    UStaticMeshComponent* GetWorldHandEquipmentMesh() const
    {
        return WorldHandEquipmentMesh;
    }

    /**
     * CombatComponent가 서버에서 공격을 승인했을 때
     * 기존 Character Presentation을 실행한다.
     */
    void PlayMeleeWorldPresentationFromServer();

    /**
     * 서버에서 Melee Hit가 확정됐을 때
     * 기존 Sound / CameraShake 표현을 실행한다.
     */
    void PlayMeleeHitPresentationFromServer(
        ADRPlayerCharacter* HitPlayer,
        bool bKilled,
        const FVector& ImpactLocation);
    
    UDRJetpackComponent* GetJetpackComponent() const
    {
        return JetpackComponent;
    }
    
    /**
     * HUD에서 사용할 제트팩 연료 비율.
     * 소유 게스트는 서버 Fuel Snapshot의
     * 보간 표시값을 사용한다.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "Player|Jetpack|UI")
    float GetDisplayedJetpackFuelRatio() const;

    /** PlayerState의 서버 연료값을 로컬 표시값에 반영한다. */
    void ReconcileJetpackFuelFromServer(float ServerFuel);

    USceneComponent* GetFirstPersonEquipmentRoot() const
    {
        return FirstPersonEquipmentRoot;
    }
    
    UDRHealthComponent* GetHealthComponent() const
    {
        return HealthComponent;
    }
    
    UStaticMeshComponent* GetFirstPersonHandEquipmentMesh() const
    {
        return FirstPersonHandEquipmentMesh;
    }
    
    FDROnPlayerCharacterDeath OnPlayerCharacterDeathDelegate;
    
protected:
    virtual void BeginPlay() override;
    
    void InitializeAbilitySystem();
    
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Mining")
    TObjectPtr<UDRMiningComponent> MiningComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Voxel")
    TObjectPtr<UVoxelNoClippingComponent> VoxelNoClippingComponent;
    
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Combat",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDRMeleeCombatComponent> MeleeCombatComponent;
    
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Jetpack",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDRJetpackComponent> JetpackComponent;
    
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Item Action",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDRItemActionPresentationComponent> ItemActionPresentationComponent;
    
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Health",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDRHealthComponent> HealthComponent;
    
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Lifecycle",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDRPlayerLifecycleComponent> PlayerLifecycleComponent;
    
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Held Item",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDRHeldItemComponent> HeldItemComponent;
    
protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    /** 1인칭 장비 위치 및 사용 연출용 피벗 */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Equipment")
    TObjectPtr<USceneComponent> FirstPersonEquipmentRoot;

    /** 소유 플레이어에게만 보이는 1인칭 손 장비 */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Equipment")
    TObjectPtr<UStaticMeshComponent> FirstPersonHandEquipmentMesh;

    /** 다른 플레이어에게 보이는 월드 손 장비 */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Equipment")
    TObjectPtr<UStaticMeshComponent> WorldHandEquipmentMesh;

    /** 다른 플레이어에게 보이는 월드 등 장비 */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Equipment")
    TObjectPtr<UStaticMeshComponent> WorldBackEquipmentMesh;

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

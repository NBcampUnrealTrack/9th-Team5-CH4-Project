#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DeepRaiders/Item/DRItemActionTypes.h"
#include "DRPlayerCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class FLifetimeProperty;
class UDRMiningComponent;
class UDRTeleportComponent;
class UAnimMontage;
class UDRCharacterMovementComponent;
class UDRItemDefinition;
class UTimelineComponent;
class UCurveFloat;
class USoundBase;
class UAudioComponent;
class UCameraShakeBase;

USTRUCT(BlueprintType)
struct FDRFirstPersonSwingPresentation
{
    GENERATED_BODY()

    /** 스윙의 시간 흐름을 결정하는 Curve */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "First Person")
    TObjectPtr<UCurveFloat> Curve = nullptr;

    /** Curve 값이 1일 때 적용할 회전 오프셋 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "First Person")
    FRotator RotationOffset = FRotator::ZeroRotator;

    /** Curve 값이 1일 때 적용할 위치 오프셋 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "First Person")
    FVector LocationOffset = FVector::ZeroVector;
};

/**
 * 플레이어 캐릭터의 이동 실행, 카메라와 장비 외형 표현을 담당한다.
 *
 * 입력 바인딩은 DRPlayerController가 담당하며,
 * 인벤토리, 퀵슬롯과 실제 장착 상태는 별도 컴포넌트가 관리한다.
 */
UCLASS()
class DEEPRAIDERS_API ADRPlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADRPlayerCharacter(const FObjectInitializer& ObjectInitializer);

    virtual void Tick(float DeltaSeconds) override;
    virtual void Landed(const FHitResult& Hit) override;

    /** 지상에서는 점프, 공중에서는 제트팩 사용을 요청한다. */
    void HandleJumpPressed();

    /** 점프 입력과 제트팩 사용을 종료한다. */
    void HandleJumpReleased();
    
    /** 로컬 플레이어의 채굴 요청을 MiningComponent에 전달한다. */
    bool RequestMine();
    
    /** 로컬 플레이어가 근접 공격을 요청한다. */
    void RequestMeleeAttack();

    /** 로컬 플레이어가 손에 든 아이템 던지기를 요청한다. */
    void RequestThrowHeldItem();
    
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_Controller() override;
    virtual void OnRep_PlayerState() override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
    float GetCurrentHealth() const
    {
        return CurrentHealth;
    }

    UFUNCTION(BlueprintPure, Category = "Player|Health")
    float GetMaxHealth() const
    {
        return MaxHealth;
    }

    UFUNCTION(BlueprintPure, Category = "Player|Health")
    float GetHealthRatio() const
    {
        if (MaxHealth <= 0.f)
        {
            return 0.f;
        }

        return FMath::Clamp(
            CurrentHealth / MaxHealth,
            0.f,
            1.f);
    }

    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser) override;
    
    UFUNCTION(BlueprintPure, Category = "Player|Health")
    bool IsDead() const
    {
        return CurrentHealth <= KINDA_SMALL_NUMBER;
    }
    
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
    
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Melee|Debug")
    bool bIsMeleeAttackDrawDebug = true;
    
protected:
    virtual void BeginPlay() override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Mining")
    TObjectPtr<UDRMiningComponent> MiningComponent;
    
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Health")
    float MaxHealth = 100.f;

    UPROPERTY(
        ReplicatedUsing = OnRep_CurrentHealth,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Health")
    float CurrentHealth = 100.f;

    UFUNCTION()
    void OnRep_CurrentHealth();
    
private:
    void PrintNetworkState(const TCHAR* Context) const;

    UFUNCTION(Server, Reliable)
    void ServerStartJetpack();

    UFUNCTION(Server, Reliable)
    void ServerStopJetpack();

    /** 서버가 입력을 거절하거나 연료가 소진된 경우 로컬 예측을 취소한다. */
    UFUNCTION(Client, Reliable)
    void ClientRejectJetpack();

    UFUNCTION()
    void OnRep_JetpackActive();

    bool CanStartJetpack() const;

    UDRCharacterMovementComponent*
        GetDRCharacterMovementComponent() const;

    void StartJetpackFromServer();
    void StopJetpackFromServer();

    /** 서버에서 연료만 소비한다. 이동은 MovementComponent가 담당한다. */
    void UpdateJetpackFuel(float DeltaSeconds);

    void RefreshJetpackActivePresentation();
    
    // ===== Melee Attack =====

    /** 소유 플레이어의 1인칭 공격 표현을 실행한다. */
    void PlayOwnerMeleeAttackPresentation();

    /** 서버에서 공격 가능 여부를 검사한다. */
    bool CanStartMeleeAttack() const;

    /** 서버에서 실제 공격 판정을 수행한다. */
    void PerformMeleeHitCheck();

    /** 서버에서 공격 상태를 종료한다. */
    void FinishMeleeAttack();

    /** 소유 클라이언트의 공격 요청을 서버에서 처리한다. */
    UFUNCTION(Server, Reliable)
    void ServerRequestMeleeAttack();
    
    bool bIsMeleeAttacking = false;

    FTimerHandle MeleeHitTimerHandle;
    FTimerHandle MeleeFinishTimerHandle;
    
    // ===== Fall Damage =====

    /** 착지 속도를 기준으로 낙하 피해량을 계산한다. */
    float CalculateFallDamage(float LandingSpeed) const;

    /** 서버에서 낙하 피해를 적용한다. */
    void ApplyFallDamage(float LandingSpeed);
    
    /** 서버에서 사망 상태를 확정하고 진행 중인 기능을 정리한다. */
    void HandleDeath();

    /** 현재 인스턴스에서 래그돌 사망 표현을 적용한다. */
    void ApplyDeathRagdoll();
    
    /** 서버에서 리스폰 직전 래그돌 위치를 기준으로 새 Pawn을 생성한다. */
    void RespawnAtRagdollLocation();

    /** 서버 래그돌 주변에서 캐릭터 캡슐이 들어갈 수 있는 위치를 탐색한다. */
    bool TryFindRagdollRespawnTransform(FTransform& OutRespawnTransform) const;
    
    /** 새 Pawn이 Possess되었을 때 Controller 입력 제한을 해제한다. */
    void RestoreControllerInput();

    /** 동일 인스턴스에서 래그돌이 중복 적용되는 것을 방지한다. */
    bool bDeathRagdollApplied = false;

    FTimerHandle RespawnTimerHandle;
    
    void ExecuteHeldItemAction(EDRItemActionType ActionType);
    
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

    // ===== Jetpack =====
    
    UPROPERTY(
        EditDefaultsOnly,
        Category = "Player|Equipment|Jetpack")
        TObjectPtr<UStaticMesh> JetpackMesh;
    
    UPROPERTY(
        EditDefaultsOnly,
        Category = "Player|Equipment|Jetpack")
    FTransform JetpackRelativeTransform;
    
    /** 현재 Pawn이 실제로 제트팩을 분사 중인지 나타낸다. */
    UPROPERTY(
        ReplicatedUsing = OnRep_JetpackActive,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Jetpack")
    bool bIsJetpackActive = false;

    /** 초당 연료 소비량 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Jetpack",
        meta = (ClampMin = "0.0"))
    float JetpackFuelConsumptionPerSecond = 50.f;
    
    // ===== Melee Attack =====

    /** 공격 시작 후 실제 판정까지의 시간 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Combat",
        meta = (ClampMin = "0.0"))
    float MeleeAttackHitTime = 0.25f;

    /** 다음 공격이 가능해질 때까지의 시간 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Combat",
        meta = (ClampMin = "0.01"))
    float MeleeAttackDuration = 0.8f;

    /** 공격 피해량 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Combat",
        meta = (ClampMin = "0.0"))
    float MeleeAttackDamage = 10.f;

    /** 시선 정면으로 검사할 거리 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Combat",
        meta = (ClampMin = "0.0", Units = "cm"))
    float MeleeAttackRange = 200.f;
    
    // ===== Death / Respawn =====

    /** 사망 후 같은 위치에 다시 생성되기까지의 시간 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Respawn",
        meta = (ClampMin = "0.0", Units = "s"))
    float RespawnDelay = 3.f;
    
    /** 리스폰 위치를 가져올 래그돌 기준 본 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Respawn")
    FName RespawnRagdollBoneName = TEXT("pelvis");

    /** 래그돌 위치에서 아래쪽 바닥을 탐색할 거리 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Respawn",
        meta = (ClampMin = "0.0", Units = "cm"))
    float RespawnGroundTraceDistance = 2000.f;

    /** Capsule이 바닥에 박히지 않도록 추가로 띄우는 거리 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Respawn",
        meta = (ClampMin = "0.0", Units = "cm"))
    float RespawnGroundClearance = 5.f;
    
    /** 래그돌 위쪽에서 Capsule Sweep을 시작할 높이 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Respawn",
        meta = (ClampMin = "0.0", Units = "cm"))
    float RespawnSweepStartHeight = 300.f;

    /** 주변 리스폰 위치를 탐색할 때의 간격 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Respawn",
        meta = (ClampMin = "1.0", Units = "cm"))
    float RespawnSearchStep = 120.f;

    /** 래그돌 주변을 몇 단계까지 탐색할지 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Respawn",
        meta = (ClampMin = "0", ClampMax = "10"))
    int32 RespawnSearchRingCount = 3;
  
    // ===== Fall Damage =====

    /** 이 속도 이하로 착지하면 피해를 받지 않는다. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Fall Damage",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float MinFallDamageSpeed = 1000.f;

    /** 이 속도 이상으로 착지하면 최대 낙하 피해를 받는다. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Fall Damage",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float MaxFallDamageSpeed = 2500.f;

    /** 최대 체력에 대한 최대 낙하 피해 비율이다. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Fall Damage",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxFallDamageRatio = 0.8f;

    /**
     * 낙하 피해 증가 곡선의 지수다.
     *
     * 1.0: 선형
     * 2.0: 제곱 곡선
     * 3.0: 초반 피해가 더 완만하고 후반부에 급격히 증가
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Fall Damage",
        meta = (ClampMin = "0.01"))
    float FallDamageExponent = 2.f;
    
    // ===== First Person Item Action =====

    /** 1인칭 장비 스윙 Timeline */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Equipment|FirstPerson",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTimelineComponent> FirstPersonItemSwingTimeline;
    
    /** 채굴 시 사용하는 1인칭 연출 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Equipment|FirstPerson",
        meta = (AllowPrivateAccess = "true"))
    FDRFirstPersonSwingPresentation FirstPersonDigPresentation;

    /** 근접 공격 시 사용하는 1인칭 연출 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Equipment|FirstPerson",
        meta = (AllowPrivateAccess = "true"))
    FDRFirstPersonSwingPresentation FirstPersonMeleePresentation;

    /** 현재 재생 중인 스윙의 실제 오프셋 */
    FRotator ActiveFirstPersonSwingRotation =
        FRotator::ZeroRotator;

    FVector ActiveFirstPersonSwingLocation =
        FVector::ZeroVector;
    
    FTransform FirstPersonEquipmentRootBaseTransform;

    bool CanStartLocalItemAction() const;
    float GetItemActionCooldown(EDRItemActionType ActionType) const;

    void PlayFirstPersonItemSwing(
        const FDRFirstPersonSwingPresentation& Presentation);

    UFUNCTION()
    void UpdateFirstPersonItemSwing(float CurveValue);

    UFUNCTION()
    void FinishFirstPersonItemSwing();
    
    // ===== Item Action Presentation =====

    float NextLocalItemActionTime = 0.f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
    float DigActionCooldown = 0.6f;
    
    /** 로컬 1인칭에서 Action에 맞는 연출을 재생한다. */
    void PlayFirstPersonItemActionPresentation(
        EDRItemActionType ActionType);

    /** 다른 플레이어에게 보일 Action 연출을 재생한다. */
    void PlayWorldItemActionPresentation(
        EDRItemActionType ActionType);

    /** Action에 해당하는 3인칭 Montage를 반환한다. */
    UAnimMontage* ResolveWorldItemActionMontage(
        EDRItemActionType ActionType) const;

    /** 서버에서 확정된 아이템 Action 연출을 모든 클라이언트에 전달한다. */
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayWorldItemActionPresentation(
        EDRItemActionType ActionType);

    /**
     * 현재 Dig는 MiningComponent의 서버 처리와
     * Presentation RPC가 분리되어 있으므로 임시로 사용한다.
     */
    UFUNCTION(Server, Unreliable)
    void ServerRequestDigPresentation();

    /** 다른 플레이어에게 보이는 채굴 몽타주 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action|Presentation")
    TObjectPtr<UAnimMontage> WorldDigMontage;

    /** 다른 플레이어에게 보이는 근접 공격 몽타주 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action|Presentation")
    TObjectPtr<UAnimMontage> WorldMeleeAttackMontage;

    // ===== Item Action Sound =====

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action|Sound")
    TObjectPtr<USoundBase> DigSound;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action|Sound")
    TObjectPtr<USoundBase> MeleeSwingSound;
    
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Sound")
    TObjectPtr<USoundBase> JetpackSound;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> JetpackAudioComponent;
    
    UPROPERTY(EditDefaultsOnly, Category = "Player|Sound")
    TObjectPtr<USoundBase> FallSound;

    UPROPERTY(EditDefaultsOnly, Category = "Player|Sound")
    TObjectPtr<USoundBase> FallDamageSound;

    UPROPERTY(EditDefaultsOnly, Category = "Player|Sound")
    TObjectPtr<USoundBase> FallDeadSound;

    UFUNCTION(Client, Unreliable)
    void ClientPlayFallSound(
        bool bTookFallDamage,
        bool bDied);
    
    UPROPERTY(EditDefaultsOnly, Category = "Player|Sound")
    TObjectPtr<USoundBase> EquipSound;
    
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action|Sound")
    TObjectPtr<USoundBase> MeleeAirSound;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action|Sound")
    TObjectPtr<USoundBase> MeleeHitSound;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Item Action|Sound")
    TObjectPtr<USoundBase> MeleeKillSound;
    
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayMeleeImpactSound(
        bool bKilled,
        FVector_NetQuantize ImpactLocation);
    
    // ===== Camera Shake =====

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Camera|Shake")
    TSubclassOf<UCameraShakeBase> MeleeHitConfirmCameraShakeClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Camera|Shake")
    TSubclassOf<UCameraShakeBase> MeleeDamagedCameraShakeClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Camera|Shake")
    TSubclassOf<UCameraShakeBase> JetpackCameraShakeClass;
    
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Camera|Shake")
    TSubclassOf<UCameraShakeBase> FallDamageCameraShakeClass;
    
    UPROPERTY(Transient)
    TObjectPtr<UCameraShakeBase> JetpackCameraShakeInstance;
    
    void PlayLocalCameraShake(
        TSubclassOf<UCameraShakeBase> ShakeClass,
        float Scale = 1.f);
    
    UFUNCTION(Client, Unreliable)
    void ClientPlayMeleeHitFeedback(bool bKilled);

    UFUNCTION(Client, Unreliable)
    void ClientPlayMeleeDamagedFeedback(bool bKilled);
    
    UFUNCTION(Client, Unreliable)
    void ClientPlayDamagedCameraShake();
    
#pragma region QuickSlot
public:
    void SetHeldItemDefinition(UDRItemDefinition* NewItemDefinition);
    
protected:
    UPROPERTY(ReplicatedUsing = OnRep_HeldItemDefinition)
    TObjectPtr<UDRItemDefinition> HeldItemDefinition;
    
    UFUNCTION()
    void OnRep_HeldItemDefinition();
    
    void RefreshHeldItemVisual();
    void RefreshHeldItemMiningSettings();
#pragma endregion

#pragma region Teleport
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Teleport", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDRTeleportComponent> TeleportComponent;
#pragma endregion

};

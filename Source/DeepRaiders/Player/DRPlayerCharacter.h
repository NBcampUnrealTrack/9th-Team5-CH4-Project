#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "DRPlayerCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class FLifetimeProperty;
class UDRMiningComponent;

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
    ADRPlayerCharacter();

    virtual void Tick(float DeltaSeconds) override;
    virtual void Landed(const FHitResult& Hit) override;

    /** 지상에서는 점프, 공중에서는 제트팩 사용을 요청한다. */
    void HandleJumpPressed();

    /** 점프 입력과 제트팩 사용을 종료한다. */
    void HandleJumpReleased();
    
    /** 로컬 플레이어의 채굴 요청을 MiningComponent에 전달한다. */
    void RequestMine();
    
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

    // 임시 네트워크 테스트 진입점
    void RequestNetworkTest();
    
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

    // 임시 네트워크 복제 검증용
    void ApplyNetworkTestState();

    void PrintNetworkState(const TCHAR* Context) const;

    /** 소유 클라이언트의 요청을 서버에서 처리한다. */
    UFUNCTION(Server, Reliable)
    void ServerToggleNetworkTest();

    /** 복제된 테스트 상태를 클라이언트 외형에 반영한다. */
    UFUNCTION()
    void OnRep_NetworkTestActive();

    UFUNCTION(Server, Reliable)
    void ServerStartJetpack();

    UFUNCTION(Server, Reliable)
    void ServerStopJetpack();

    UFUNCTION()
    void OnRep_JetpackActive();

    bool CanStartJetpack() const;

    void StartJetpackFromServer();
    void StopJetpackFromServer();

    void UpdateJetpack(float DeltaSeconds);

    void RefreshJetpackActivePresentation();
    
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

    /** 서버가 관리하며, 클라이언트에서는 RepNotify로 외형을 갱신한다. */
    UPROPERTY(
        ReplicatedUsing = OnRep_NetworkTestActive,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Player|Network Test")
    bool bNetworkTestActive = false;
    
    /** F키 눌르면 테스트 해볼수있음 */
    UPROPERTY(
    EditDefaultsOnly,
    Category = "Player|Equipment|Test")
    TObjectPtr<UStaticMesh> EquipmentTestMesh;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Player|Equipment|Test")
    FTransform TestFirstPersonTransform;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Player|Equipment|Test")
    FTransform TestWorldHandTransform;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Player|Equipment|Test")
    FTransform TestWorldBackTransform;
    
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

    /** 초당 상승 가속도 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Jetpack",
        meta = (ClampMin = "0.0", Units = "cm/s^2"))
    float JetpackAcceleration = 2500.f;

    /** 제트팩 사용 중 최대 상승 속도 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Jetpack",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float MaxJetpackRiseSpeed = 900.f;

    /** 초당 연료 소비량 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Jetpack",
        meta = (ClampMin = "0.0"))
    float JetpackFuelConsumptionPerSecond = 20.f;
};
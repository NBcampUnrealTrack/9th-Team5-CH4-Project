#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "DRPlayerCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UInputAction;
class UInputMappingContext;
class FLifetimeProperty;

/**
 * 플레이어 입력, 카메라와 장비 외형 표현을 담당한다.
 *
 * 인벤토리, 퀵슬롯, 실제 장착 상태는 별도 컴포넌트가 관리하고
 * 이 클래스는 전달받은 메시를 1인칭 및 월드에 표시한다.
 */
UCLASS()
class DEEPRAIDERS_API ADRPlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADRPlayerCharacter();

    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_Controller() override;
    virtual void PawnClientRestart() override;

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

protected:
    virtual void BeginPlay() override;

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    // 임시 네트워크 복제 검증용
    void HandleNetworkTest(const FInputActionValue& Value);
    void ApplyNetworkTestState();

    void PrintNetworkState(const TCHAR* Context) const;

    /** 소유 클라이언트의 요청을 서버에서 처리한다. */
    UFUNCTION(Server, Reliable)
    void ServerToggleNetworkTest();

    /** 복제된 테스트 상태를 클라이언트 외형에 반영한다. */
    UFUNCTION()
    void OnRep_NetworkTestActive();

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

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Player|Input")
    TObjectPtr<UInputAction> NetworkTestAction;

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
};
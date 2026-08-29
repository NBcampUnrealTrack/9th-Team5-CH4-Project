#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DRCharacterMovementComponent.generated.h"

class FSavedMove_DRCharacter;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

UENUM()
enum class EDRCustomMovementMode : uint8
{
    None = 0,
    
    // 특정 액션 이름이 아닌 외부 이동 액션을 처리하는 모드
    MovementAction = 1,
};

UCLASS()
class DEEPRAIDERS_API UDRCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UDRCharacterMovementComponent();

    void BindAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);

    /** 소유 클라이언트 및 서버가 사용할 제트팩 입력 상태 */
    void SetWantsJetpack(bool bNewWantsJetpack);

    bool WantsJetpack() const
    {
        return bWantsJetpack;
    }

    /** SavedMove에서 받은 입력 플래그를 서버 이동에 복원한다. */
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;

    virtual void SetBase(
        UPrimitiveComponent* NewBase,
        const FName BoneName = NAME_None,
        bool bNotifyActor = true) override;

    /** 커스텀 SavedMove를 생성하는 예측 데이터를 반환한다. */
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    
    // 외부 이동 액션이 사용할 공통 커스텀 이동 모드 설정 함수
    void SetCustomMovementMode(EDRCustomMovementMode NewMode);
    
    // None이면 바닥 상태를 확인한 뒤 일반 이동 모드로 복귀한다.
    void ExitCustomMovementMode();
    
    bool IsCustomMovementModeActive(EDRCustomMovementMode Mode) const;
    
protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 낙하 물리 안에서 예측 가능한 제트팩 추진력을 적용한다. */
    virtual void PhysFalling(
        float DeltaTime,
        int32 Iterations) override;

    virtual void PhysCustom(float deltaTime, int32 Iterations) override;
    
private:
    void UnbindAbilitySystem();
    void HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& Data);
    void ApplyMoveSpeedMultiplier(float Multiplier);

    void PhysMovementAction(float DeltaTime, int32 Iterations);
    UDRMovementActionComponent* GetMovementActionComponent() const;
    
    // 커스텀 이동이 끝났을 때 Walking 또는 Falling으로 복귀
    void RestoreDefaultMovementMode();
    
    TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
    FDelegateHandle MoveSpeedChangedDelegateHandle;
    float BaseWalkSpeed = 0.f;

    bool CanApplyJetpackThrust() const;

    /** 로컬 입력 또는 서버가 복원한 입력 상태 */
    uint8 bWantsJetpack : 1;

    /** 현재 출력 상승 진행 시간 */
    float JetpackSpoolElapsed = 0.f;

    /** 제트팩 작동 직후의 초기 추진 가속도 */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Jetpack",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.0",
            Units = "cm/s^2"))
    float InitialJetpackAcceleration = 1200.f;

    /** 출력 상승이 끝난 뒤의 최대 추진 가속도 */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Jetpack",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.0",
            Units = "cm/s^2"))
    float MaxJetpackAcceleration = 3200.f;

    /** 초기 출력에서 최대 출력까지 도달하는 시간 */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Jetpack",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.01",
            Units = "s"))
    float JetpackSpoolUpTime = 0.65f;

    /** 출력 증가 곡선의 지수 */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Jetpack",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.01"))
    float JetpackThrustExponent = 1.7f;

    /** 제트팩 사용 중 최대 상승 속도 */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Jetpack",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.0",
            Units = "cm/s"))
    float MaxJetpackRiseSpeed = 900.f;

    friend class FSavedMove_DRCharacter;
};

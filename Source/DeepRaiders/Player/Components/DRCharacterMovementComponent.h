#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DRCharacterMovementComponent.generated.h"

class FSavedMove_DRCharacter;
class AVoxelWorld;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FDRCharacterMovementUpdated, float, const FVector&, const FVector&);

UENUM()
enum class EDRCustomMovementMode : uint8
{
	None = 0,

	// 특정 액션 이름이 아닌 외부 이동 액션을 처리하는 모드
	MovementAction = 1,

	// 복셀 데이터가 캡슐 내부를 충분히 채워 collision 갱신 전에 움직임을 멈춘 상태
	VoxelContained = 2,
};

UCLASS()
class DEEPRAIDERS_API UDRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDRCharacterMovementComponent();

	/** 이동 갱신 직후 필요한 후처리 컴포넌트에 전달한다. */
	FDRCharacterMovementUpdated OnCharacterMovementUpdated;

	void BindAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);

	/** 슈퍼점프 체공 중에만 공중 조작력을 높이고, 착지 시 원래 값으로 복원한다. */
	void ActivateSuperJumpAirControl(float NewAirControl);

	/** 소유 클라이언트 및 서버가 사용할 제트팩 입력 상태 */
	void SetWantsJetpack(bool bNewWantsJetpack);

	bool WantsJetpack() const
	{
		return bWantsJetpack;
	}

	/** Zipline attach 보정 Velocity와 분리해 관리하는 축 방향 gameplay 속도. */
	void SetZiplineRailSpeed(float NewRailSpeed)
	{
		ZiplineRailSpeed = NewRailSpeed;
	}

	float GetZiplineRailSpeed() const
	{
		return ZiplineRailSpeed;
	}

	/** Manual Zipline 탑승 시 이전 walking 입력이 첫 custom tick에 남지 않도록 초기화한다. */
	void ResetManualZiplineInputState();

	/** SavedMove에서 받은 입력 플래그를 서버 이동에 복원한다. */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor = true) override;

	virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;

	/** 커스텀 SavedMove를 생성하는 예측 데이터를 반환한다. */
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	// 명시적으로 보존 중인 공중 관성만 일반 Falling 최대 속도보다 우선한다.
	virtual float GetMaxSpeed() const override;

	// 이동 액션 종료 순간의 횡방향 속도를 Falling 최대 속도의 임시 하한으로 저장한다.
	void BeginAirborneMomentumPreservation();

	// Dash나 새 이동 액션처럼 현재 관성을 명시적으로 대체하는 동작에서 호출한다.
	void ClearAirborneMomentumPreservation();

	bool IsAirborneMomentumPreservationActive() const
	{
		return bAirborneMomentumPreservationActive;
	}
	
	// 외부 이동 액션이 사용할 공통 커스텀 이동 모드 설정 함수
	void SetCustomMovementMode(EDRCustomMovementMode NewMode);

	// None이면 바닥 상태를 확인한 뒤 일반 이동 모드로 복귀한다.
	void ExitCustomMovementMode();

	bool IsCustomMovementModeActive(EDRCustomMovementMode Mode) const;

	/** 복셀 매몰 판정 컴포넌트가 요청한 이동 정지 상태에 진입한다. */
	void EnterVoxelContainedMode();

	/** 복셀 매몰 판정 컴포넌트가 요청한 이동 정지 상태를 해제한다. */
	void ExitVoxelContainedMode();

protected:
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	virtual bool CheckFall(
		const FFindFloorResult& OldFloor,
		const FHitResult& Hit,
		const FVector& Delta,
		const FVector& OldLocation,
		float RemainingTime,
		float TimeTick,
		int32 Iterations,
		bool bMustJump) override;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 낙하 물리 안에서 예측 가능한 제트팩 추진력을 적용한다. */
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

private:
	void UnbindAbilitySystem();
	void HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& Data);
	void ApplyMoveSpeedMultiplier(float Multiplier);

	void PhysMovementAction(float DeltaTime, int32 Iterations);
	void UpdateZiplineFacing(const FDRMovementActionState& State, float DeltaTime);
	UDRMovementActionComponent* GetMovementActionComponent() const;

	// 네트워크 보정으로 ActionState와 MovementMode가 어긋났을 때 다음 이동 갱신에서 복구한다.
	void ReconcileMovementActionMode();
	
	// 커스텀 이동이 끝났을 때 Walking 또는 Falling으로 복귀
	void RestoreDefaultMovementMode();
	bool ShouldKeepVoxelFloor(const FFindFloorResult& OldFloor, const FVector& OldLocation) const;

	bool TryHandleZiplineBlockingCollision(const FHitResult& Hit);
	
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	TWeakObjectPtr<AVoxelWorld> LastVoxelFloorWorld;
	FDelegateHandle MoveSpeedChangedDelegateHandle;
	float BaseWalkSpeed = 0.f;
	float AirControlBeforeSuperJump = 0.f;

	bool bSuperJumpAirControlActive = false;

	bool CanApplyJetpackThrust() const;

	/** 로컬 입력 또는 서버가 복원한 입력 상태 */
	uint8 bWantsJetpack : 1;

	/**
	 * Manual Zipline 입력의 네트워크 복원 상태.
	 * -1 / 0 / +1은 현재 Manual control mode의 기준 축에 대한 signed input이다.
	 *
	 * 소유 클라이언트의 SavedMove가 FLAG_Custom_1/2로 서버에 전달하고,
	 * Dedicated Server의 PhysMovementAction이 이 값을 사용한다.
	 */
	int8 ManualZiplineInput = 0;

	/**
	 * Zipline의 실제 축 방향 gameplay 속도.
	 * Rope에 붙기 위한 lateral / clamp 보정 Velocity와 분리한다.
	 * Manual ViewRelative에서는 이 값의 부호가 몸 Facing 전환 시점도 결정한다.
	 */
	float ZiplineRailSpeed = 0.f;

	/*
 	* 그래플 종료 이후부터 착지 전까지만 사용하는 속도 상한 상태다.
 	* 그래플 자체가 아니라 해당 이동 액션이 명시적으로 요청한 경우에만 활성화된다.
 	*/
	bool bAirborneMomentumPreservationActive = false;
	float PreservedLateralSpeed = 0.f;
	
	/** 현재 출력 상승 진행 시간 */
	float JetpackSpoolElapsed = 0.f;

	/** VoxelWorld 하단 경계 직전에서 floor가 사라져도 자연 낙하로 전환하지 않는 여유 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float VoxelLowerBoundaryTolerance = 2.f;

	/** 제트팩 작동 직후의 초기 추진 가속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jetpack", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s^2"))
	float InitialJetpackAcceleration = 1200.f;

	/** 출력 상승이 끝난 뒤의 최대 추진 가속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jetpack", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s^2"))
	float MaxJetpackAcceleration = 3200.f;

	/** 초기 출력에서 최대 출력까지 도달하는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jetpack", meta = ( AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float JetpackSpoolUpTime = 0.65f;

	/** 출력 증가 곡선의 지수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jetpack", meta = ( AllowPrivateAccess = "true", ClampMin = "0.01"))
	float JetpackThrustExponent = 1.7f;

	/** 제트팩 사용 중 최대 상승 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jetpack", meta = ( AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
	float MaxJetpackRiseSpeed = 900.f;

	friend class FSavedMove_DRCharacter;
};

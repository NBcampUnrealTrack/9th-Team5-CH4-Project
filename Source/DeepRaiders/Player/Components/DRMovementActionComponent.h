#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMovementActionComponent.generated.h"

UENUM(BlueprintType)
enum class EDRMovementActionType : uint8
{
	None UMETA(DisplayName = "None"),
	Grapple UMETA(DisplayName = "Grapple"),
};

UENUM(BlueprintType)
enum class EDRMovementActionEndReason : uint8
{
	Completed UMETA(DisplayName = "Completed"),
	Cancelled UMETA(DisplayName = "Cancelled"),
	Invalidated UMETA(DisplayName = "Invalidated"),
	OwnerDeath UMETA(DisplayName = "Owner Death"),
};

/*
 * 이동 액션이 실행되는 동안 유지되는 런타임 상태
 * 실제 설정값은 이후 아이템 또는 스킬 Definition에서 채워 전달한다.
 */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRMovementActionState
{
	GENERATED_BODY()
	
public:
	bool IsActive() const
	{
		return bActive;
	}
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	uint8 bActive:1 = false;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	EDRMovementActionType ActionType = EDRMovementActionType::None;

	// 이동 액션의 서버 검증, 로컬 예측값을 구분하기 위한 Id
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	int32 SessionId = 0;

	// 그래플링에서는 훅 위치로 사용한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	FVector_NetQuantize ReferenceLocation = FVector::ZeroVector;

	// 액션이 제공하는 방향성 가속도 크기다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float ActionAcceleration = 0.f;

	// 액션 중 허용할 최대 속도다. 0 이하이면 제한하지 않는다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float MaxSpeed = 0.f;
	
	// 기존 입력 가속도를 액션 중 얼마나 반영할지 결정한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	float ControlScale = 1.f;
	
	// 비활성 상태로 복제될 때 마지막 종료 이유를 전달한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement Action")
	EDRMovementActionEndReason LastEndReason = EDRMovementActionEndReason::Invalidated;
};

/**
 * 이동 컴포넌트가 액션 이동 계산에 제공하는 입력이다.
 * 위치 이동이나 충돌 처리는 포함하지 않는다.
 */
struct FDRMovementActionSimulationInput
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector InputAcceleration = FVector::ZeroVector;
	FVector Gravity = FVector::ZeroVector;
	float DeltaTime = 0.f;
};

/**
 * 액션 컴포넌트가 CharacterMovementComponent에 반환하는 이동 기여도다.
 */
struct FDRMovementActionSimulationOutput
{
	FVector AdditionalAcceleration = FVector::ZeroVector;
	float MaxSpeed = 0.f;
	bool bApplyGravity = true;
};

/**
 * 이동이 끝난 뒤 GA가 종료 조건을 판단할 수 있도록 전달하는 결과다.
 */
struct FDRMovementActionSimulationResult
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDRMovementActionEnded, EDRMovementActionEndReason);
DECLARE_MULTICAST_DELEGATE_OneParam(FDRMovementActionSimulated, const FDRMovementActionSimulationResult&);

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRMovementActionComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UDRMovementActionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 소유 클라이언트가 서버 응답 전에 이동을 예측할 때 사용한다.
	bool StartPredictedMovementAction(const FDRMovementActionState& NewState);
	
	// 서버가 검증한 이동 액션 상태를 시작한다.
	bool StartAuthoritativeMovementAction(const FDRMovementActionState& NewState);
	
	// 현재 권한에 맞는 이동 액션 상태를 종료
	void EndMovementAction(EDRMovementActionEndReason EndReason);
	
	// 현재 상태를 기반으로 이번 프레임의 이동 기여도를 계산한다.
	void EvaluateMovementContribution(const FDRMovementActionSimulationInput& Input,
		FDRMovementActionSimulationOutput& OutOutput) const;
	
	// 이동 계산 이후 현재 위치와 속도를 GA에 전달한다.
	void ReportMovementSimulation(const FVector& Location, const FVector& Velocity);
	
	bool IsMovementActionActive() const;
	
	const FDRMovementActionState& GetSimulationActionState() const;
	
	FDRMovementActionEnded OnMovementActionEnded;
	FDRMovementActionSimulated OnMovementActionSimulated;
	
protected:
	UFUNCTION()
	void OnRep_AuthoritativeActionState();
	
private:
	bool IsLocallyControlledOwner() const;
	
	void ClearPredictedActionState();
	
	void EvaluateGrappleContribution(const FDRMovementActionState& State, const FDRMovementActionSimulationInput& Input,
		FDRMovementActionSimulationOutput& OutOutput)  const;
	
	void RequestReplicationUpdate() const;
	
	UPROPERTY(ReplicatedUsing = OnRep_AuthoritativeActionState, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Movement Action", meta = (AllowPrivateAccess = "true"))
	FDRMovementActionState AuthoritativeActionState;	
	
	// 클라이언트에서만 사용하는 예측 상태
	FDRMovementActionState PredictedActionState;	
};


























// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRLeashComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRLeashTargetChanged, AActor*, PreviousTarget, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRLeashFollowingChanged, bool, bNewFollowing);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEEPRAIDERS_API UDRLeashComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDRLeashComponent();
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 추적 대상을 명시적으로 교체
	// nullptr 비허용, 추적 해제는 TryRelease 사용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category= "Leash")
	bool TrySetLeashTarget(AActor* NewTarget);
	
	// 추적 대상이 없는 경우, 최초 상호작용 대상이 연결됨.
	// 이미 대상이 존재하는 경우 실패, 같은 대상이면 성공
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leash")
	bool TryClaimLeashTarget(AActor* NewTarget);
	
	// 추적 대상 해제
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leash")
	bool TryReleaseLeashTarget();
	
	UFUNCTION(BlueprintPure, Category = "Leash")
	AActor* GetLeashTarget() const
	{
		return LeashTarget.Get();
	}
	
	UFUNCTION(BlueprintPure, Category = "Leash")
	bool IsFollowing() const
	{
		return bIsFollowing;
	}

	UPROPERTY(BlueprintAssignable, Category = "Leash")
	FDRLeashTargetChanged OnLeashTargetChangedDelegate;
	
	UPROPERTY(BlueprintAssignable, Category = "Leash")
	FDRLeashFollowingChanged OnLeashFollowingChangedDelegate;
	
protected:
	UFUNCTION()
	void OnRep_LeashTarget(AActor* PreviousTarget);
	
	UFUNCTION()
	void OnRep_IsFollowing();
	
protected:
	// 이 거리보다 멀어지면 추적을 시작
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leash|Distance", meta = (ClampMin = "0.0", Units = "cm"))
	float FollowStartDistance = 300.0f;
	
	// 이 거리 안으로 들어오면 감속 후 추적을 종료
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leash|Distance", meta = (ClampMin = "0.0", Units = "cm"))
	float FollowStopDistance = 200.0f;
	
	// 최대 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leash|Movement", meta = (ClampMin = "1.0", Units = "cm/s"))
	float MaxFollowSpeed = 500.0f;
	
	// 속도 보간 변화량, 작을 수록 lag가 커짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leash|Movement", meta = (ClampMin = "0.01"))
	float FollowVelocityResponse = 10.0f;
	
	// Target 기준 로컬 오프셋
	// ex) 플레이어 어깨 위에 떠다니는 펫
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leash|Movement")
	FVector FollowOffset = FVector::ZeroVector;
	
	// 추적 대상
	UPROPERTY(ReplicatedUsing = OnRep_LeashTarget, VisibleInstanceOnly, BlueprintReadOnly,  Category = "Leash")
	TObjectPtr<AActor> LeashTarget;
	
	UPROPERTY(ReplicatedUsing = OnRep_IsFollowing, VisibleInstanceOnly, BlueprintReadOnly, Category = "Leash")
	bool bIsFollowing = false;
	
private:
	bool HasLeashAuthority() const;
	
	bool IsValidLeashTarget( const AActor* Target) const;
	
	void UpdateLeashMovement(float DeltaSeconds);
	
	void SetFollowingState(bool bNewFollowing);
	void RequestReplicationUpdate() const;
	
	void ClearLeashTarget();
	
private:
	FVector CurrentVelocity = FVector::ZeroVector;	
};

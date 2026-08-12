#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "DRTeleportPoint.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class APawn;
class FLifetimeProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRTeleportPointPawnSignature, APawn*, Interactor);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRTeleportPointSimpleSignature);

UENUM(BlueprintType)
enum class EDRTeleportAccessType : uint8
{
	// 모든 팀이 등록할 수 있는 공용 텔레포트.
	Public UMETA(DisplayName = "Public"),

	// 먼저 등록한 팀이 소유권을 가져가는 선점형 텔레포트.
	Claimable UMETA(DisplayName = "Claimable"),

	// 배치 시점부터 OwnerTeamId 팀만 등록할 수 있는 팀 고정 텔레포트.
	TeamOwned UMETA(DisplayName = "Team Owned")
};

UCLASS()
class DEEPRAIDERS_API ADRTeleportPoint : public AActor, public IDRInteractableInterface
{
	GENERATED_BODY()

public:
	ADRTeleportPoint();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	bool IsRegistered() const { return bRegistered; }
	
	UFUNCTION(BlueprintCallable, Category = "Teleport")
	FText GetTeleportDisplayName() const;
	
	UFUNCTION(BlueprintPure, Category = "Teleport")
	int32 GetOwnerTeamId() const { return OwnerTeamId; }

	UFUNCTION(BlueprintPure, Category = "Teleport")
	EDRTeleportAccessType GetAccessType() const { return AccessType; }

	UFUNCTION(BlueprintPure, Category = "Teleport")
	bool IsRegisteredForTeam(int32 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	FTransform GetTeleportArrivalTransform() const;

	bool CanRegisterForTeam(int32 TeamId, APawn* Interactor) const;
	bool TryRegisterForTeam(int32 TeamId, APawn* Interactor);

	// BP에서 임시 MI 교체 등을 바인딩할 수 있는 상태 변화 이벤트다.
	UPROPERTY(BlueprintAssignable, Category = "Teleport|Event")
	FDRTeleportPointPawnSignature OnTeleportOccupied;

	UPROPERTY(BlueprintAssignable, Category = "Teleport|Event")
	FDRTeleportPointSimpleSignature OnTeleportEmptied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Teleport|Event")
	void BP_OnTeleportOccupied(APawn* Interactor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Teleport|Event")
	void BP_OnTeleportEmptied();

private:
	UFUNCTION()
	void HandleInteractionVolumeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool IsFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleInteractionVolumeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	bool IsInteractorInRange(APawn* Interactor) const;
	void NotifyTeleportOccupied(APawn* Interactor);
	void NotifyTeleportEmptied();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlatformMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	EDRTeleportAccessType AccessType = EDRTeleportAccessType::Public;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	FText TeleportDisplayName = FText::FromString("Teleport Point");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	FVector TeleportArrivalOffset = FVector(0.f, 0.f, 120.f);
	
	// false인 텔레포트는 이후 단계에서 즉시 사용 가능한 포인트로 다룬다.
	// 등록 여부만 복제하고, 이동/목록 UI는 이후 단계에서 붙인다.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	bool bRegistered = false;

	// Claimable은 등록 순간 설정되고, TeamOwned는 에디터에서 미리 지정한다.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Teleport", meta = (AllowPrivateAccess = "true"))
	int32 OwnerTeamId = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<APawn>> OverlappingInteractors;
	
#pragma region Interact
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* Interactor) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(APawn* Interactor);
#pragma endregion
};

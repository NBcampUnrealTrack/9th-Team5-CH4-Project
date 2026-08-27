#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTeamSelectionPad.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/** 올라온 플레이어를 지정된 팀으로 변경하는 발판이다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRTeamSelectionPad : public AActor
{
	GENERATED_BODY()

public:
	ADRTeamSelectionPad();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Team")
	void SetTeamId(int32 NewTeamId);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/** 내부 TeamId 0은 1팀, 1은 2팀이다. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_TeamId,
		Category = "Team",
		meta = (ClampMin = "0", ClampMax = "1"))
	int32 TeamId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team|Visual")
	FName TeamColorParameterName = TEXT("ColorBase");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team|Visual")
	FLinearColor Team0Color = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team|Visual")
	FLinearColor Team1Color = FLinearColor::Blue;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_TeamId();

	void RefreshTeamColor();
};

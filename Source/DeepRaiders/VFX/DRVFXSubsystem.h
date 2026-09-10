#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRVFXSubsystem.generated.h"

class AActor;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class UDRVFXLibrary;
struct FDRVFXDefinition;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRVFXRequest
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "VFX")
	TObjectPtr<AActor> TargetActor = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "VFX", meta = (Categories = "GameplayCue.VFX"))
	FGameplayTag VFXTag;

	UPROPERTY(BlueprintReadWrite, Category = "VFX")
	float RawMagnitude = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VFX")
	FVector Direction = FVector::ZeroVector;
};

UCLASS()
class DEEPRAIDERS_API UDRVFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Deinitialize() override;
	
	UNiagaraComponent* PlayApplicationVFX(const UDRVFXLibrary* Library, const FDRVFXRequest& Request);
	UNiagaraComponent* StartPersistentVFX(const UDRVFXLibrary* Library, const FDRVFXRequest& Request);
	bool StopPersistentVFX(const FDRVFXRequest& Request);
	bool QueuePersistentVFXRemoval(const UDRVFXLibrary* Library, const FDRVFXRequest& Request);
	
	UNiagaraComponent* PlayRemovalVFX(const UDRVFXLibrary* Library, const FDRVFXRequest& Request);
	
private:
	struct FPersistentVFXKey
	{
		TWeakObjectPtr<AActor> TargetActor;
		FGameplayTag  VFXTag;
		
		bool operator==(const FPersistentVFXKey& Other) const
		{
			return TargetActor == Other.TargetActor && VFXTag == Other.VFXTag;	
		}
		
		friend uint32 GetTypeHash(const FPersistentVFXKey& Key)
		{
			return HashCombine(GetTypeHash(Key.TargetActor), GetTypeHash(Key.VFXTag));
		}
	};

	struct FPendingPersistentVFXRemoval
	{
		TWeakObjectPtr<UDRVFXLibrary> Library;
		FDRVFXRequest Request;
	};
	
	// VFXDefinition 탐색 후 반환
	const FDRVFXDefinition* ResolveDefinition(const UDRVFXLibrary* Library, const FDRVFXRequest& Request) const;
	
	// 요청에 맞춰 VFX 생성 후 부착
	UNiagaraComponent* SpawnAttachedSystem(const FDRVFXDefinition& Definition, UNiagaraSystem* NiagaraSystem,
		const FDRVFXRequest& Request, bool bPersistent) const;
	
	// 부착할 위치 탐색 후 반환
	USceneComponent* ResolveAttachComponent(const FDRVFXDefinition& Definition, AActor* TargetActor) const;
	
	static void ApplyUserParameters(UNiagaraComponent* NiagaraComponent, const FDRVFXDefinition& Definition,
		const FDRVFXRequest& Request);
	
	// VFX 정리
	void CleanupInvalidPersistentVFX();
	void FinalizePersistentVFXRemoval(FPersistentVFXKey Key);
	
	TMap<FPersistentVFXKey, TWeakObjectPtr<UNiagaraComponent>> ActivePersistentVFX;
	TMap<FPersistentVFXKey, FPendingPersistentVFXRemoval> PendingPersistentVFXRemovals;
};

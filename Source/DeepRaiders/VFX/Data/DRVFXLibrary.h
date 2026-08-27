#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DRVFXLibrary.generated.h"

class UNiagaraSystem;

UENUM(BlueprintType)
enum class EDRVFXAttachTarget : uint8
{
	ActorRoot,
	SkeletalMesh
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRVFXDefinition
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> ApplicationSystem = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> PersistentSystem = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> RemovalSystem = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	EDRVFXAttachTarget AttachTarget = EDRVFXAttachTarget::SkeletalMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	FName AttachSocketName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ShowOnlyInnerProperties))
	FTransform RelativeTransform = FTransform::Identity;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX|Parameters")
	TMap<FName, float> FloatParameters;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX|Parameters")
	TMap<FName, FLinearColor> ColorParameters;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX|Parameters")
	TMap<FName, FVector> VectorParameters;
	
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRVFXLibrary : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	const FDRVFXDefinition* FindDefinition(const FGameplayTag& VFXTag) const;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (Categories = "GameplayCue.VFX"))
	TMap<FGameplayTag, FDRVFXDefinition> Definitions;
};


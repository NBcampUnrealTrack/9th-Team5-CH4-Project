#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "DRGameplayCueVFX.generated.h"

class UDRVFXLibrary;
class UDRVFXSubsystem;
struct FDRVFXRequest;

UCLASS(Blueprintable)
class DEEPRAIDERS_API UDRGameplayCueVFX : public UGameplayCueNotify_Static
{
	GENERATED_BODY()
	
public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UDRVFXLibrary> VFXLibrary = nullptr;
	
private:
	bool BuildRequest(AActor* Target, const FGameplayCueParameters& Parameters,
		FDRVFXRequest& OutRequest, UDRVFXSubsystem*& OutSubsystem) const;
	
	FGameplayTag ResolveVFXTag(const FGameplayCueParameters& Parameters) const;	
};

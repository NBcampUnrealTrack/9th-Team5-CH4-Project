
#include "DRGameplayCueVFX.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "DRVFXSubsystem.h"
#include "Data/DRVFXLibrary.h"
#include "NiagaraComponent.h"

bool UDRGameplayCueVFX::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	FDRVFXRequest Request;
	UDRVFXSubsystem* VFXSubsystem = nullptr;
	
	return BuildRequest(MyTarget, Parameters, Request, VFXSubsystem) 
		&& IsValid(VFXSubsystem->PlayApplicationVFX(VFXLibrary, Request));
}

bool UDRGameplayCueVFX::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	FDRVFXRequest Request;
	UDRVFXSubsystem* VFXSubsystem = nullptr;
	
	return BuildRequest(MyTarget, Parameters, Request, VFXSubsystem) 
		&& IsValid(VFXSubsystem->PlayApplicationVFX(VFXLibrary, Request));
}

bool UDRGameplayCueVFX::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	FDRVFXRequest Request;
	UDRVFXSubsystem* VFXSubsystem = nullptr;
	
	return BuildRequest(MyTarget, Parameters, Request, VFXSubsystem) 
		&& IsValid(VFXSubsystem->StartPersistentVFX(VFXLibrary, Request));
}

bool UDRGameplayCueVFX::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	FDRVFXRequest Request;
	UDRVFXSubsystem* VFXSubsystem = nullptr;
	
	if (!BuildRequest(MyTarget, Parameters, Request, VFXSubsystem))
	{
		return false;
	}
	
	const UAbilitySystemComponent* AbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MyTarget);
	// 같은 Cue 태그를 사용하는 다른 지속 효과가 남아 있으면 공용 Persistent VFX를 제거하지 않는다.
	if (IsValid(AbilitySystem) && AbilitySystem->GetGameplayTagCount(Request.VFXTag) > 0)
	{
		return true;
	}

	return VFXSubsystem->QueuePersistentVFXRemoval(VFXLibrary, Request);
}

bool UDRGameplayCueVFX::BuildRequest(AActor* Target, const FGameplayCueParameters& Parameters,
	FDRVFXRequest& OutRequest, UDRVFXSubsystem*& OutSubsystem) const
{
	OutRequest = FDRVFXRequest();
	OutSubsystem = nullptr;

	if (!IsValid(Target) 
		|| !IsValid(VFXLibrary) 
		|| !IsValid(Target->GetWorld()))
	{
		return false;
	}

	const FGameplayTag VFXTag = ResolveVFXTag(Parameters);
	if (!VFXTag.IsValid())
	{
		return false;
	}

	OutSubsystem = Target->GetWorld()->GetSubsystem<UDRVFXSubsystem>();
	if (!IsValid(OutSubsystem))
	{
		return false;
	}

	OutRequest.TargetActor = Target;
	OutRequest.VFXTag = VFXTag;
	OutRequest.RawMagnitude = Parameters.RawMagnitude;

	return true;
}

FGameplayTag UDRGameplayCueVFX::ResolveVFXTag(const FGameplayCueParameters& Parameters) const
{
	return Parameters.OriginalTag.IsValid()	? Parameters.OriginalTag : GameplayCueTag;
}

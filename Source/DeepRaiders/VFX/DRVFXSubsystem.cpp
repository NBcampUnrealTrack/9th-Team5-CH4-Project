
#include "DRVFXSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Data/DRVFXLibrary.h"

void UDRVFXSubsystem::Deinitialize()
{
	for (const TPair<FPersistentVFXKey, TWeakObjectPtr<UNiagaraComponent>>& Pair : ActivePersistentVFX)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->DeactivateImmediate();
			Pair.Value->DestroyComponent();
		}
	}
	
	ActivePersistentVFX.Reset();
	
	Super::Deinitialize();
}

UNiagaraComponent* UDRVFXSubsystem::PlayApplicationVFX(const UDRVFXLibrary* Library, const FDRVFXRequest& Request)
{
	const FDRVFXDefinition* Definition = ResolveDefinition(Library, Request);
	
	return Definition ?
		SpawnAttachedSystem(*Definition, Definition->ApplicationSystem, Request, false) : nullptr;
}

UNiagaraComponent* UDRVFXSubsystem::StartPersistentVFX(const UDRVFXLibrary* Library, const FDRVFXRequest& Request)
{
	CleanupInvalidPersistentVFX();
	
	if (!IsValid(Request.TargetActor)
		|| !Request.VFXTag.IsValid())
	{
		return nullptr;
	}
	
	const FPersistentVFXKey Key{Request.TargetActor, Request.VFXTag};
	
	if (const TWeakObjectPtr<UNiagaraComponent>* ExistingComponent = ActivePersistentVFX.Find(Key))
	{
		if (ExistingComponent->IsValid())
		{
			return ExistingComponent->Get();
		}
		
		ActivePersistentVFX.Remove(Key);
	}
	
	const FDRVFXDefinition* Definition = ResolveDefinition(Library, Request);
	if (!Definition)
	{
		return nullptr;
	}
	
	UNiagaraComponent* NiagaraComponent = SpawnAttachedSystem(*Definition, Definition->PersistentSystem, Request, true);
	
	if (IsValid(NiagaraComponent))
	{
		ActivePersistentVFX.Add(Key, NiagaraComponent);
	}

	return NiagaraComponent;
}

bool UDRVFXSubsystem::StopPersistentVFX(const FDRVFXRequest& Request)
{
	if (!IsValid(Request.TargetActor)
		|| !Request.VFXTag.IsValid())
	{
		return false;
	}

	const FPersistentVFXKey Key{Request.TargetActor, Request.VFXTag};
	TWeakObjectPtr<UNiagaraComponent> NiagaraComponent;
	
	if (!ActivePersistentVFX.RemoveAndCopyValue(Key, NiagaraComponent)
		|| !NiagaraComponent.IsValid())
	{
		return false;
	}
	
	NiagaraComponent->DeactivateImmediate();
	NiagaraComponent->DestroyComponent();
	
	return true;	
}

UNiagaraComponent* UDRVFXSubsystem::PlayRemovalVFX(const UDRVFXLibrary* Library, const FDRVFXRequest& Request)
{
	const FDRVFXDefinition* Definition = ResolveDefinition(Library, Request);
	
	return Definition ? SpawnAttachedSystem(*Definition, Definition->RemovalSystem, Request, false) : nullptr;
}

const FDRVFXDefinition* UDRVFXSubsystem::ResolveDefinition(const UDRVFXLibrary* Library,
	const FDRVFXRequest& Request) const
{
	if (!IsValid(Library)
		|| !IsValid(Request.TargetActor)
		|| !Request.VFXTag.IsValid())
	{
		return nullptr;
	}
	
	const FDRVFXDefinition* Definition = Library->FindDefinition(Request.VFXTag);
	if (!Definition)
	{
		UE_LOG(LogTemp,	Warning, TEXT("[VFXSubsystem] Definition not found: %s"), *Request.VFXTag.ToString());
	}

	return Definition;
}

UNiagaraComponent* UDRVFXSubsystem::SpawnAttachedSystem(const FDRVFXDefinition& Definition,
	UNiagaraSystem* NiagaraSystem, const FDRVFXRequest& Request, bool bPersistent) const
{
	UWorld* World = GetWorld();
	
	if (!IsValid(World)
		|| World->GetNetMode() == NM_DedicatedServer
		|| !IsValid(NiagaraSystem)
		|| !IsValid(Request.TargetActor))
	{
		return nullptr;
	}
	
	USceneComponent* AttachComponent = ResolveAttachComponent(Definition, Request.TargetActor);
	if (!IsValid(AttachComponent))
	{
		return nullptr;
	}
	
	FName AttachSocketName = Definition.AttachSocketName;
	if (!AttachSocketName.IsNone() 
		&& !AttachComponent->DoesSocketExist(AttachSocketName))
	{
		AttachSocketName = NAME_None;
	}
	
	const FVector RelativeLocation = Definition.RelativeTransform.GetLocation();
	const FRotator RelativeRotation = Definition.RelativeTransform.Rotator();
	const FVector RelativeScale = Definition.RelativeTransform.GetScale3D();
	
	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSystem, AttachComponent, AttachSocketName, RelativeLocation, RelativeRotation, RelativeScale,
		EAttachLocation::KeepRelativeOffset, !bPersistent, ENCPoolMethod::None, false, true);
	
	if (!IsValid(NiagaraComponent))
	{
		return nullptr;
	}
	
	ApplyUserParameters(NiagaraComponent, Definition);
	NiagaraComponent->Activate(true);
	
	return NiagaraComponent;	
}

USceneComponent* UDRVFXSubsystem::ResolveAttachComponent(const FDRVFXDefinition& Definition, AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return nullptr;
	}

	if (Definition.AttachTarget == EDRVFXAttachTarget::SkeletalMesh)
	{
		if (USkeletalMeshComponent* SkeletalMesh = TargetActor->FindComponentByClass<USkeletalMeshComponent>())
		{
			return SkeletalMesh;
		}
	}

	return TargetActor->GetRootComponent();
}

void UDRVFXSubsystem::ApplyUserParameters(UNiagaraComponent* NiagaraComponent, const FDRVFXDefinition& Definition)
{
	if (!IsValid(NiagaraComponent))
	{
		return;
	}

	for (const TPair<FName, float>& Pair : Definition.FloatParameters)
	{
		NiagaraComponent->SetVariableFloat(Pair.Key, Pair.Value);
	}

	for (const TPair<FName, FLinearColor>& Pair : Definition.ColorParameters)
	{
		NiagaraComponent->SetVariableLinearColor(Pair.Key, Pair.Value);
	}

	for (const TPair<FName, FVector>& Pair : Definition.VectorParameters)
	{
		NiagaraComponent->SetVariableVec3(Pair.Key, Pair.Value);
	}
}

void UDRVFXSubsystem::CleanupInvalidPersistentVFX()
{
	for (auto Iterator = ActivePersistentVFX.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Key().TargetActor.IsValid() 
			|| !Iterator.Value().IsValid())
		{
			if (Iterator.Value().IsValid())
			{
				Iterator.Value()->DeactivateImmediate();
				Iterator.Value()->DestroyComponent();
			}

			Iterator.RemoveCurrent();
		}
	}
}

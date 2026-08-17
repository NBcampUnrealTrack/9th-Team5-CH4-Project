// Fill out your copyright notice in the Description page of Project Settings.


#include "DRItemAbilitySet.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"

void FDRItemAbilitySet_GrantedHandles::AddAbilitySpecHandle(FGameplayAbilitySpecHandle Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.AddUnique(Handle);
	}
}

void FDRItemAbilitySet_GrantedHandles::AddGameplayEffectHandle(FActiveGameplayEffectHandle Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.AddUnique(Handle);
	}
}

void FDRItemAbilitySet_GrantedHandles::TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent)
		|| !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}
	
	// 부여된 Ability 회수
	for (const FGameplayAbilitySpecHandle Handle : AbilitySpecHandles)
	{
		if (AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) == nullptr)
		{
			continue;
		}
		
		AbilitySystemComponent->CancelAbilityHandle(Handle);
		AbilitySystemComponent->ClearAbility(Handle);
	}
	
	for (const FActiveGameplayEffectHandle Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}
	
	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}

bool FDRItemAbilitySet_GrantedHandles::IsEmpty() const
{
	return AbilitySpecHandles.IsEmpty() && GameplayEffectHandles.IsEmpty();
}

// 등록된 Ability와 Effect를 ASC에 부여
// 현재 SourceObject는 ItemDefinition, 추후 수정 가능성 농후
void UDRItemAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent,
	FDRItemAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	if (!IsValid(AbilitySystemComponent)
		|| !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}
	
	// Ability 부여
	for (const FDRItemAbilitySet_GameplayAbility& AbilityToGrant : GrantedAbilities)
	{
		if (!AbilityToGrant.Ability)
		{
			continue;
		}
		
		FGameplayAbilitySpec AbilitySpec(
			AbilityToGrant.Ability
			, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;
		
		AbilitySpec.InputID = static_cast<int32>(AbilityToGrant.InputID);
		
		const FGameplayAbilitySpecHandle AbilityHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		
		if (OutGrantedHandles != nullptr)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilityHandle);
		}
	}
	
	// Effect 부여
	for (const FDRItemAbilitySet_GameplayEffect& EffectToGrant : GrantedEffects)
	{
		if (!EffectToGrant.GameplayEffect)
		{
			continue;
		}
		
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		
		if (IsValid(SourceObject))
		{
			EffectContext.AddSourceObject(SourceObject);
		}
		
		FGameplayEffectSpecHandle EffectSpec = AbilitySystemComponent->MakeOutgoingSpec(
			EffectToGrant.GameplayEffect
			, EffectToGrant.EffectLevel
			, EffectContext);
		
		if (!EffectSpec.IsValid())
		{
			continue;
		}
		
		const FActiveGameplayEffectHandle EffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
			*EffectSpec.Data.Get());
		
		if (OutGrantedHandles != nullptr)
		{
			OutGrantedHandles->AddGameplayEffectHandle(EffectHandle);
		}
	}
	
}

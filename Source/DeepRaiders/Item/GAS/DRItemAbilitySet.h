// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "DRItemAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EDRAbilityInputID : uint8
{
	Primary = 0,
	Secondary = 1,
	
};

// ItemAbilitySet이 부여할 하나의 GameplayAbility 항목
USTRUCT(BlueprintType)
struct FDRItemAbilitySet_GameplayAbility
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> Ability = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = 1, UIMin = 1))
	int32 AbilityLevel = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	EDRAbilityInputID InputID = EDRAbilityInputID::Primary;
};

// 아이템이 장착된 동안 적용할 GameplayEffect 항목
USTRUCT(BlueprintType)
struct FDRItemAbilitySet_GameplayEffect
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = 0.0, UIMin = 0.0))
	float EffectLevel = 1.0f;	
};

// ItemAbilitySet이 제공한 Handle 모음
struct DEEPRAIDERS_API FDRItemAbilitySet_GrantedHandles
{
public:
	void AddAbilitySpecHandle(FGameplayAbilitySpecHandle Handle);

	void AddGameplayEffectHandle(FActiveGameplayEffectHandle Handle);
	
	// 이 ItemAbilitySet이 부여한 Ability와 Effect만 제거
	void TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);

	bool IsEmpty() const;

private:
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

// 아이템 장착 시 ASC에 부여할 Ability와 지속 Effect의 정적 정의
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRItemAbilitySet : public UDataAsset
{
	GENERATED_BODY()
public:
	// 서버 ASC에 Ability와 Effect를 부여
	// SourceObject는 ItemDefinition 전달 (추후 수정될 가능성 농후)
	void GiveToAbilitySystem(
		UAbilitySystemComponent* AbilitySystemComponent
		, FDRItemAbilitySet_GrantedHandles* OutGrantedHandles
		, UObject* SourceObject) const;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Set")
	TArray<FDRItemAbilitySet_GameplayAbility> GrantedAbilities;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Set")
	TArray<FDRItemAbilitySet_GameplayEffect> GrantedEffects;
	
};

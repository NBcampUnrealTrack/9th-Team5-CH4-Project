#pragma once

#include "NativeGameplayTags.h"

namespace DRGameplayTags
{
	// State
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Frozen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Absorbing);

	// Ability
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Ranged);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Snow_Absorb);

	// Data
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Snow_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Freeze_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
}
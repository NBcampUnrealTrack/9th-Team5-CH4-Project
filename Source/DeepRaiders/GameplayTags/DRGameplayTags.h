#pragma once

#include "NativeGameplayTags.h"

namespace DRGameplayTags
{
	// State
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Frozen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Absorbing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_UI_InventoryOpen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_UI_ShopOpen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_UI_TeleportOpen);

	// Ability
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Ranged);
	// UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Ranged_Projectile);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Ranged_Sprayer);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Snow_Absorb);

	// Cooldown
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Ranged);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_One);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Two);
	
	// Data
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Snow_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Freeze_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Health_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cooldown_Duration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Effect_MoveSpeed);

	// Effect Policy
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Policy_PersistThroughDeath);
	
	// Gameplay Cue - VFX
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_VFX);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_VFX_Effect_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_VFX_Effect_MoveSpeed);
	
	// Weapon Effect
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Sprayer_Active);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Cannon_Explosion);
	
	// UI Screen
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_HUD);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_QuickSlot);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Inventory_Player);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Inventory_Storage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Teleport);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Shop);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_StartingSelection);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Scoreboard);
	
	// Gameplay Cue - Sound
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Attack_Miss);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Attack_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Attack_Kill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Attack_Swing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Dig);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Item_PickedUp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Ore_Dropped);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Ore_Discovered);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_Land);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_FallDamage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_FallDeath);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Rifle_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Sprayer_Active);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Cannon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Cannon_Explosion);
}

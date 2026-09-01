#pragma once

#include "NativeGameplayTags.h"

namespace DRGameplayTags
{
	// State
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Frozen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Absorbing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_BlinkRecovery);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_UI_InventoryOpen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_UI_ShopOpen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_UI_TeleportOpen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_MovementAction_Active);

	// Ability
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Ranged);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Snow_Absorb);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Item_Throw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_MovementAction);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Item_Grapple);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_SecondaryCancel);
	
	// Skill
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_Blink);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_ForwardDash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_SuperJump);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_CombatRoll);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_Search);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_HotPack);

	// Perk
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Perk_Skill_Charges);
	
	// Event
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Ability_Throw_Release);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_MovementAction_Cancel);
	
	// Cooldown
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Ranged);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_One);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Two);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Blink);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_ForwardDash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_SuperJump);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_CombatRoll);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Search);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_HotPack);
	
	// Data
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Snow_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Freeze_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Health_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cooldown_Duration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_BlinkRecovery_Duration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Effect_MoveSpeed);

	// Effect Policy
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Policy_PersistThroughDeath);
	
	// Gameplay Cue - Throwable
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Item_Throwable_Throw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Item_Throwable_Impact);
	
	// Gameplay Cue - VFX
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_VFX);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_VFX_Effect_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_VFX_Effect_MoveSpeed);
	
	// Weapon Effect
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Projectile_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Projectile_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Sprayer_Active);
	
	// UI Screen
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_HUD);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_QuickSlot);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Inventory_Player);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Inventory_Storage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Teleport);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Shop);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_StartingSelection);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Scoreboard);
	
	// Gameplay Cue - Sound - Melee
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Melee_Attack_Miss);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Melee_Attack_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Melee_Attack_Kill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Melee_Attack_Swing);

	// Gameplay Cue - Sound - Player
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_Snowball_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_Land);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_FallDamage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_FallDeath);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_Frozen_Enter);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Player_Frozen_Death);

	// Gameplay Cue - Sound - Item
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Item_Throwable_Throw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Item_Throwable_Impact);

	// Gameplay Cue - Sound - World / Etc
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Dig);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Item_PickedUp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Ore_Dropped);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Ore_Discovered);

	// Gameplay Cue - Sound - Weapon
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Rifle_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Rifle_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Shotgun_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Shotgun_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Cannon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Cannon_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Weapon_Sprayer_Active);
	
	// Gameplay Cue - Player
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Player_Hit);
	
	// Gameplay Cue - Movement Action
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_MovementAction_Grapple_Active);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_MovementAction_Grapple_Failed);
}

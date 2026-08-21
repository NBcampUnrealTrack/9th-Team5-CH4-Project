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
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Snow_Absorb);

	// Data
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Snow_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Freeze_Amount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	// UI Screen
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_HUD);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_QuickSlot);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Inventory_Player);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Inventory_Storage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Teleport);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Screen_Shop);

	// Gameplay Cue - Sound
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Attack_Miss);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Attack_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Attack_Kill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Dig);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Item_PickedUp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Ore_Dropped);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sound_Ore_Discovered);
}

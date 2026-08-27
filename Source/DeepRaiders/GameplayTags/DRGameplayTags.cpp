#include "DRGameplayTags.h"

namespace DRGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Frozen,
		"State.Frozen",
		"Player is frozen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Dead,
		"State.Dead",
		"Player is dead.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Absorbing,
		"State.Absorbing",
		"Player is absorbing snow.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_UI_InventoryOpen,
		"State.UI.InventoryOpen",
		"Local player's inventory UI is open.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_UI_ShopOpen,
		"State.UI.ShopOpen",
		"Local player's shop UI is open.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_UI_TeleportOpen,
		"State.UI.TeleportOpen",
		"Local player's teleport UI is open.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack,
		"Ability.Attack",
		"Parent tag for attack abilities.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Ranged,
		"Ability.Attack.Ranged",
		"Ranged attack ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Melee,
		"Ability.Attack.Melee",
		"Melee attack ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Snow_Absorb,
		"Ability.Snow.Absorb",
		"Snow absorption ability.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Weapon_Ranged,
		"Cooldown.Weapon.Ranged",
		"Shared fire interval cooldown for ranged weapons.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_One,
		"Cooldown.Skill.One",
		"Cooldown for the first character skill slot.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Two,
		"Cooldown.Skill.Two",
		"Cooldown for the second character skill slot.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Effect_Policy_PersistThroughDeath,
		"Effect.Policy.PersistThroghDeath",
		"Active GameplayEffect persists through death and respawn.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX,
		"GameplayCue.VFX",
		"Parent GameplayCue tag for data-driven visual effects");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Effect_Heal,
		"GameplayCue.VFX.Effect.Heal",
		"Heal effect visual effects");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_VFX_Effect_MoveSpeed,
		"GameplayCue.VFX.Effect.MoveSpeed",
		"MoveSpeed effect visual effects");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Snow_Amount,
		"Data.Snow.Amount",
		"Snow amount passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Freeze_Amount,
		"Data.Freeze.Amount",
		"Freeze amount passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Damage,
		"Data.Damage",
		"Damage amount passed through GAS.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Health_Heal,
		"Data.Health.Heal",
		"Health Heal amount passed through GAS.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Cooldown_Duration,
		"Data.Cooldown.Duration",
		"Cooldown duration passed through GAS.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_HUD,
		"UI.Screen.HUD",
		"Main HUD screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_QuickSlot,
		"UI.Screen.QuickSlot",
		"Quick slot screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Inventory_Player,
		"UI.Screen.Inventory.Player",
		"Player inventory screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Inventory_Storage,
		"UI.Screen.Inventory.Storage",
		"Storage inventory screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Teleport,
		"UI.Screen.Teleport",
		"Teleport selection screen.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Shop,
		"UI.Screen.Shop",
		"Shop screen.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Scoreboard,
		"UI.Screen.Scoreboard",
		"Match scoreboard screen.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Attack_Miss,
		"GameplayCue.Sound.Attack.Miss",
		"Attack missed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Attack_Hit,
		"GameplayCue.Sound.Attack.Hit",
		"Attack hit.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Attack_Kill,
		"GameplayCue.Sound.Attack.Kill",
		"Attack killed the target.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Attack_Swing,
		"GameplayCue.Sound.Attack.Swing",
		"Melee weapon swing.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Dig,
		"GameplayCue.Sound.Dig",
		"Digging sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Item_PickedUp,
		"GameplayCue.Sound.Item.PickedUp",
		"Item picked up.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Ore_Dropped,
		"GameplayCue.Sound.Ore.Dropped",
		"Ore dropped.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Ore_Discovered,
		"GameplayCue.Sound.Ore.Discovered",
		"Ore discovered.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	GameplayCue_Sound_Player_Land,
	"GameplayCue.Sound.Player.Land",
	"Player landed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_FallDamage,
		"GameplayCue.Sound.Player.FallDamage",
		"Player took fall damage.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_FallDeath,
		"GameplayCue.Sound.Player.FallDeath",
		"Player died from fall damage.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Rifle_Fire,
		"GameplayCue.Sound.Weapon.Rifle.Fire",
		"Rifle fired.");
}

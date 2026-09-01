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
		State_BlinkRecovery,
		"State.BlinkRecovery",
		"Player may move after blinking but cannot attack or use skills.");

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
		State_MovementAction_Active,
		"State.MovementAction.Active",
		"Player is currently executing a movement action.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack,
		"Ability.Attack",
		"Parent tag for attack abilities.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Ranged,
		"Ability.Attack.Ranged",
		"Ranged attack ability.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Weapon_Projectile_Fire,
		"GameplayCue.Weapon.Projectile.Fire",
		"Data-driven projectile weapon fire presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Weapon_Projectile_Impact,
		"GameplayCue.Weapon.Projectile.Impact",
		"Data-driven projectile weapon impact presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Weapon_Sprayer_Active,
		"GameplayCue.Weapon.Sprayer.Active",
		"Data-driven active sprayer presentation.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Item_Throwable_Throw,
		"GameplayCue.Item.Throwable.Throw",
		"Throwable release presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Item_Throwable_Impact,
		"GameplayCue.Item.Throwable.Impact",
		"Throwable impact presentation.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Melee,
		"Ability.Attack.Melee",
		"Melee attack ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Snow_Absorb,
		"Ability.Snow.Absorb",
		"Snow absorption ability.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Item_Throw,
		"Ability.Item.Throw",
		"Item-based throwable ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill,
		"Ability.Skill",
		"Parent tag for character skills.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Blink,
		"Ability.Skill.Blink",
		"Blink skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_ForwardDash,
		"Ability.Skill.ForwardDash",
		"Forward dash skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_SuperJump,
		"Ability.Skill.SuperJump",
		"Super jump skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_CombatRoll,
		"Ability.Skill.CombatRoll",
		"Combat roll skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_Search,
		"Ability.Skill.Search",
		"Search skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Skill_HotPack,
		"Ability.Skill.HotPack",
		"Hot pack skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Perk_Skill_Charges,
		"Perk.Skill.Charges",
		"Changes the equipped skill to use rechargeable charges.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_MovementAction,
		"Ability.MovementAction",
		"Parent tag for movement action abilities.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Item_Grapple,
		"Ability.Item.Grapple",
		"Item-based grappling ability.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Input_SecondaryCancel,
		"Ability.Input.SecondaryCancel",
		"Active movement ability receives Secondary input as a cancel request.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Event_Ability_Throw_Release,
		"Event.Ability.Throw.Release",
		"Throwable montage release point.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Event_MovementAction_Cancel,
		"Event.MovementAction.Cancel",
		"Requests cancellation of the active movement action.");
	
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
		Cooldown_Skill_Blink,
		"Cooldown.Skill.Blink",
		"Cooldown for the blink skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_ForwardDash,
		"Cooldown.Skill.ForwardDash",
		"Cooldown for the forward dash skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_SuperJump,
		"Cooldown.Skill.SuperJump",
		"Cooldown for the super jump skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_CombatRoll,
		"Cooldown.Skill.CombatRoll",
		"Cooldown for the combat roll skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_Search,
		"Cooldown.Skill.Search",
		"Cooldown for the search skill.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Skill_HotPack,
		"Cooldown.Skill.HotPack",
		"Cooldown for the hot pack skill.");
	
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
		Data_BlinkRecovery_Duration,
		"Data.BlinkRecovery.Duration",
		"Blink recovery duration passed through GAS.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Effect_MoveSpeed,
		"Data.Effect.MoveSpeed",
		"Effect MoveSpeed passed through GAS.");
	
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
		UI_Screen_StartingSelection,
		"UI.Screen.StartingSelection",
		"Starting weapon and skill selection screen.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		UI_Screen_Scoreboard,
		"UI.Screen.Scoreboard",
		"Match scoreboard screen.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Miss,
		"GameplayCue.Sound.Melee.Attack.Miss",
		"Melee attack missed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Hit,
		"GameplayCue.Sound.Melee.Attack.Hit",
		"Melee attack hit.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Kill,
		"GameplayCue.Sound.Melee.Attack.Kill",
		"Melee attack killed the target.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Melee_Attack_Swing,
		"GameplayCue.Sound.Melee.Attack.Swing",
		"Melee weapon swing.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Snowball_Impact,
		"GameplayCue.Sound.Player.Snowball.Impact",
		"Snow projectile impact on a player.");

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
		GameplayCue_Sound_Player_Frozen_Enter,
		"GameplayCue.Sound.Player.Frozen.Enter",
		"Player entered frozen state.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Player_Frozen_Death,
		"GameplayCue.Sound.Player.Frozen.Death",
		"Frozen player died.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Item_Throwable_Throw,
		"GameplayCue.Sound.Item.Throwable.Throw",
		"Throwable release sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Item_Throwable_Impact,
		"GameplayCue.Sound.Item.Throwable.Impact",
		"Throwable impact sound.");
	
	// =====================================================
	// Gameplay Cue - Sound - Weapon
	// =====================================================
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Rifle_Fire,
		"GameplayCue.Sound.Weapon.Rifle.Fire",
		"Rifle fire sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Rifle_Impact,
		"GameplayCue.Sound.Weapon.Rifle.Impact",
		"Rifle impact sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Shotgun_Fire,
		"GameplayCue.Sound.Weapon.Shotgun.Fire",
		"Shotgun fire sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Shotgun_Impact,
		"GameplayCue.Sound.Weapon.Shotgun.Impact",
		"Shotgun impact sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Cannon_Fire,
		"GameplayCue.Sound.Weapon.Cannon.Fire",
		"Cannon fire sound.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Cannon_Impact,
		"GameplayCue.Sound.Weapon.Cannon.Impact",
		"Cannon impact sound.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Sound_Weapon_Sprayer_Active,
		"GameplayCue.Sound.Weapon.Sprayer.Active",
		"Active sprayer loop sound.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Player_Hit,
		"GameplayCue.Player.Hit",
		"Player hit presentation.");
}

#pragma once

UENUM(BlueprintType)
enum class EDRAbilityInputId : uint8
{
	Primary = 0,
	Secondary = 1,
	Interaction = 2,
	MatchData = 3,
	Inventory = 4,
	Drop = 5,
	Skill1 = 6,
	Skill2 = 7,

	// 이 Ability는 플레이어 입력 슬롯에 바인딩하지 않고 다른 로직이나 이벤트로 발동한다.
	Unbound = 8,

	// Space 입력의 일반 점프와 조건부 보조 Ability가 공유하는 논리 입력 슬롯이다.
	Jump = 9,

	// 직렬화 호환을 위해 이후 새 입력 ID는 반드시 이 아래에만 추가한다.
};

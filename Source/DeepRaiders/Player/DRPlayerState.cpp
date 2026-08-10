#include "DRPlayerState.h"

#include "DRPlayerCharacter.h"
#include "Net/UnrealNetwork.h"

void ADRPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRPlayerState, bHasDeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, DeepestDigLocation);

	// 제트팩 보유 여부는 다른 플레이어도 알아야 한다.
	DOREPLIFETIME(
		ADRPlayerState,
		bHasJetpack);

	// 연료는 해당 플레이어 자신에게만 보내도 된다.
	DOREPLIFETIME_CONDITION(
		ADRPlayerState,
		CurrentJetpackFuel,
		COND_OwnerOnly);
}

bool ADRPlayerState::UpdateDeepestDigLocation(const FVector& Location)
{
	if (!HasAuthority() ||
		(bHasDeepestDigLocation && Location.Z >= DeepestDigLocation.Z))
	{
		return false;
	}

	bHasDeepestDigLocation = true;
	DeepestDigLocation = Location;
	ForceNetUpdate();
	return true;
}

void ADRPlayerState::GrantJetpack()
{
	if (!HasAuthority())
	{
		return;
	}

	bHasJetpack = true;
	CurrentJetpackFuel = MaxJetpackFuel;

	/*
	 * 서버에서는 RepNotify가 자동 호출되지 않으므로
	 * 리슨 서버 화면을 위해 직접 외형을 갱신한다.
	 */
	RefreshJetpackVisualOnPawn();

	// 일회성 획득 상태를 빠르게 전송하도록 요청한다.
	ForceNetUpdate();
}

bool ADRPlayerState::ConsumeJetpackFuel(float Amount)
{
	if (!HasAuthority() ||
		!bHasJetpack ||
		Amount <= 0.f ||
		CurrentJetpackFuel <= 0.f)
	{
		return false;
	}

	CurrentJetpackFuel = FMath::Max(
		0.f,
		CurrentJetpackFuel - Amount);

	// 이번 호출에서 연료 소비가 실행되었다는 의미
	return true;
}

bool ADRPlayerState::RefillJetpackFuel()
{
	if (!HasAuthority() || !bHasJetpack)
	{
		return false;
	}

	// 이미 가득 차 있으면 값을 다시 변경하지 않는다.
	if (FMath::IsNearlyEqual(
			CurrentJetpackFuel,
			MaxJetpackFuel))
	{
		return false;
	}

	CurrentJetpackFuel = MaxJetpackFuel;

	// 착지는 일회성 이벤트이므로 즉시 복제를 요청한다.
	ForceNetUpdate();

	return true;
}

void ADRPlayerState::OnRep_HasJetpack()
{
	RefreshJetpackVisualOnPawn();
}

void ADRPlayerState::OnRep_JetpackFuel()
{
	/*
	 * 추후 HUD 연료 게이지 갱신용.
	 *
	 * 현재 UI가 없다면 비어 있어도 동작에는 문제가 없지만,
	 * 아무 처리도 계속 하지 않을 거면 ReplicatedUsing 대신
	 * Replicated로 바꿔도 된다.
	 */
}

void ADRPlayerState::RefreshJetpackVisualOnPawn()
{
	ADRPlayerCharacter* PlayerCharacter =
		GetPawn<ADRPlayerCharacter>();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RefreshJetpackVisual();
}

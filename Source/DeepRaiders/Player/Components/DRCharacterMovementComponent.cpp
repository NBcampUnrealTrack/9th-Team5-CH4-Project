#include "DRCharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"

class FSavedMove_DRCharacter : public FSavedMove_Character
{
public:
    typedef FSavedMove_Character Super;

    uint8 bSavedWantsJetpack : 1;
    float SavedJetpackSpoolElapsed = 0.f;

    virtual void Clear() override
    {
        Super::Clear();

        bSavedWantsJetpack = false;
        SavedJetpackSpoolElapsed = 0.f;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Result = Super::GetCompressedFlags();

        if (bSavedWantsJetpack)
        {
            Result |= FLAG_Custom_0;
        }

        return Result;
    }

    virtual bool CanCombineWith(
        const FSavedMovePtr& NewMove,
        ACharacter* InCharacter,
        float MaxDelta) const override
    {
        const FSavedMove_DRCharacter* NewDRMove = static_cast<const FSavedMove_DRCharacter*>(NewMove.Get());

        if (bSavedWantsJetpack != NewDRMove->bSavedWantsJetpack)
        {
            return false;
        }

        /*
         * 출력 증가 중인 이동을 합치면 프레임 단위 곡선 계산이
         * 달라질 수 있으므로 제트팩 사용 중에는 합치지 않는다.
         */
        if (bSavedWantsJetpack)
        {
            return false;
        }

        return Super::CanCombineWith(
            NewMove,
            InCharacter,
            MaxDelta);
    }

    virtual void SetMoveFor(
        ACharacter* Character,
        float InDeltaTime,
        const FVector& NewAccel,
        FNetworkPredictionData_Client_Character& ClientData) override
    {
        Super::SetMoveFor(
            Character,
            InDeltaTime,
            NewAccel,
            ClientData);

        const UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());

        if (!IsValid(Movement))
        {
            return;
        }

        bSavedWantsJetpack = Movement->bWantsJetpack;

        SavedJetpackSpoolElapsed = Movement->JetpackSpoolElapsed;
    }

    virtual void PrepMoveFor(
        ACharacter* Character) override
    {
        Super::PrepMoveFor(Character);

        UDRCharacterMovementComponent* Movement =
            Cast<UDRCharacterMovementComponent>(
                Character->GetCharacterMovement());

        if (!IsValid(Movement))
        {
            return;
        }

        Movement->bWantsJetpack =
            bSavedWantsJetpack;

        Movement->JetpackSpoolElapsed =
            SavedJetpackSpoolElapsed;
    }
};

class FNetworkPredictionData_Client_DRCharacter : public FNetworkPredictionData_Client_Character
{
public:
    explicit FNetworkPredictionData_Client_DRCharacter(const UCharacterMovementComponent& ClientMovement)
        : FNetworkPredictionData_Client_Character(ClientMovement)
    {
    }

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_DRCharacter());
    }
};

UDRCharacterMovementComponent::UDRCharacterMovementComponent()
    : bWantsJetpack(false)
{
    GravityScale = 1.5f;
}

void UDRCharacterMovementComponent::SetWantsJetpack(
    bool bNewWantsJetpack)
{
    if (bWantsJetpack == bNewWantsJetpack)
    {
        return;
    }

    bWantsJetpack = bNewWantsJetpack;
    JetpackSpoolElapsed = 0.f;
}

void UDRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    const bool bNewWantsJetpack =
        (Flags &
         FSavedMove_Character::FLAG_Custom_0) != 0;

    SetWantsJetpack(bNewWantsJetpack);
}

FNetworkPredictionData_Client* UDRCharacterMovementComponent::GetPredictionData_Client() const
{
    if (ClientPredictionData == nullptr)
    {
        UDRCharacterMovementComponent* MutableThis =
            const_cast<
                UDRCharacterMovementComponent*>(this);

        MutableThis->ClientPredictionData =
            new FNetworkPredictionData_Client_DRCharacter(
                *this);
    }

    return ClientPredictionData;
}

bool UDRCharacterMovementComponent::CanApplyJetpackThrust() const
{
    if (!bWantsJetpack ||
        !IsFalling() ||
        !IsValid(CharacterOwner))
    {
        return false;
    }

    const ADRPlayerCharacter* PlayerCharacter =
        Cast<ADRPlayerCharacter>(CharacterOwner);

    if (!IsValid(PlayerCharacter) ||
        PlayerCharacter->IsDead())
    {
        return false;
    }

    const ADRPlayerState* PlayerState =
        PlayerCharacter
            ->GetPlayerState<ADRPlayerState>();

    if (!IsValid(PlayerState))
    {
        return false;
    }

    return PlayerState->HasJetpack() &&
        PlayerState->GetJetpackFuel() >
            KINDA_SMALL_NUMBER;
}

void UDRCharacterMovementComponent::PhysFalling(
    float DeltaTime,
    int32 Iterations)
{
    if (CanApplyJetpackThrust())
    {
        const float SafeSpoolUpTime =
            FMath::Max(
                JetpackSpoolUpTime,
                KINDA_SMALL_NUMBER);

        JetpackSpoolElapsed =
            FMath::Min(
                JetpackSpoolElapsed + DeltaTime,
                SafeSpoolUpTime);

        const float SpoolAlpha =
            FMath::Clamp(
                JetpackSpoolElapsed /
                    SafeSpoolUpTime,
                0.f,
                1.f);

        const float ThrustAlpha =
            FMath::Pow(
                SpoolAlpha,
                FMath::Max(
                    JetpackThrustExponent,
                    0.01f));

        const float CurrentAcceleration =
            FMath::Lerp(
                InitialJetpackAcceleration,
                MaxJetpackAcceleration,
                ThrustAlpha);

        Velocity.Z =
            FMath::Min(
                Velocity.Z +
                    CurrentAcceleration * DeltaTime,
                MaxJetpackRiseSpeed);
    }
    else
    {
        JetpackSpoolElapsed = 0.f;
    }

    /*
     * 중력, 충돌, 낙하 이동은 기존 CharacterMovement 로직에 맡긴다.
     */
    Super::PhysFalling(
        DeltaTime,
        Iterations);
}
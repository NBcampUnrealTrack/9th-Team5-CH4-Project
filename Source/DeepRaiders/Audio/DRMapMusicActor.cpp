#include "DRMapMusicActor.h"

#include "Components/AudioComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

ADRMapMusicActor::ADRMapMusicActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	MusicComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicComponent"));
	SetRootComponent(MusicComponent);
	MusicComponent->bAutoActivate = false;
	MusicComponent->bAllowSpatialization = false;
	MusicComponent->bIsUISound = true;

	static ConstructorHelpers::FObjectFinder<USoundClass> MusicSoundClass(
		TEXT("/Game/DeepRaiders/Sound/SoundClass/SC_Music.SC_Music"));
	if (MusicSoundClass.Succeeded())
	{
		MusicComponent->SoundClassOverride = MusicSoundClass.Object;
	}
}

void ADRMapMusicActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	MusicComponent->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleMusicFinished);
	MiningGameState = GetWorld()->GetGameState<ADRMiningGameStateBase>();
	if (!IsValid(MiningGameState))
	{
		SetMusicPhase(EMusicPhase::Map);
		return;
	}

	MiningGameState->OnGameTimerChanged.AddDynamic(
		this,
		&ThisClass::HandleGameTimerChanged);
	HandleGameTimerChanged(
		MiningGameState->GetGameRemainingSeconds(),
		MiningGameState->IsGameStarted(),
		MiningGameState->IsGameEnded());
}

void ADRMapMusicActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MiningGameState))
	{
		MiningGameState->OnGameTimerChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGameTimerChanged);
	}

	if (IsValid(MusicComponent))
	{
		MusicComponent->OnAudioFinished.RemoveDynamic(this, &ThisClass::HandleMusicFinished);
		MusicComponent->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

void ADRMapMusicActor::HandleGameTimerChanged(int32, bool bGameStarted, bool bGameEnded)
{
	if (bGameEnded)
	{
		// 종료 사운드는 GameState의 공통 Cue로 재생한다.
		Destroy();
		return;
	}

	SetMusicPhase(bGameStarted ? EMusicPhase::Game : EMusicPhase::Map);
}

void ADRMapMusicActor::HandleMusicFinished()
{
	if (bChangingMusicPhase)
	{
		return;
	}

	PlayNextMusic();
}

void ADRMapMusicActor::SetMusicPhase(EMusicPhase NewPhase)
{
	if (MusicPhase == NewPhase)
	{
		return;
	}

	MusicPhase = NewPhase;
	MusicIndex = INDEX_NONE;
	bChangingMusicPhase = true;
	if (IsValid(MusicComponent))
	{
		MusicComponent->Stop();
	}
	PlayNextMusic();
	bChangingMusicPhase = false;
}

void ADRMapMusicActor::PlayNextMusic()
{
	if (!IsValid(MusicComponent))
	{
		return;
	}

	const TArray<TObjectPtr<USoundBase>>& Playlist = GetCurrentPlaylist();
	for (int32 Attempt = 0; Attempt < Playlist.Num(); ++Attempt)
	{
		MusicIndex = (MusicIndex + 1) % Playlist.Num();
		USoundBase* Music = Playlist[MusicIndex];
		if (!IsValid(Music))
		{
			continue;
		}

		MusicComponent->SetSound(Music);
		if (FadeInDuration > 0.f)
		{
			MusicComponent->FadeIn(FadeInDuration);
		}
		else
		{
			MusicComponent->Play();
		}
		return;
	}

	MusicComponent->Stop();
}

const TArray<TObjectPtr<USoundBase>>& ADRMapMusicActor::GetCurrentPlaylist() const
{
	switch (MusicPhase)
	{
	case EMusicPhase::Game:
		return GameMusicPlaylist;
	case EMusicPhase::Map:
	case EMusicPhase::None:
	default:
		return MapMusicPlaylist;
	}
}

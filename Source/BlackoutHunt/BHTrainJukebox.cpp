// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainJukebox.h"
#include "BHCharacter.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

namespace
{
	// CC0 lofi loops. Whichever resolve at runtime become the per-person jukebox playlist, so importing more CC0
	// loops later auto-adds them with no code change. SW_LofiTrain ships; the others are downloaded CC0 tracks.
	struct FJukeTrack { const TCHAR* Path; const TCHAR* Name; };
	const FJukeTrack BHJukeTracks[] = {
		{ TEXT("/Game/BlackoutHunt/Audio/SW_LofiTrain.SW_LofiTrain"),           TEXT("Lofi - Train") },
		{ TEXT("/Game/BlackoutHunt/Audio/SW_LofiChill.SW_LofiChill"),           TEXT("Lofi - Chill") },
		{ TEXT("/Game/BlackoutHunt/Audio/SW_LofiHipHop.SW_LofiHipHop"),         TEXT("Lofi - Hip Hop") },
		{ TEXT("/Game/BlackoutHunt/Audio/SW_LofiSlowStride.SW_LofiSlowStride"), TEXT("Lofi - Slow Stride") },
	};
}

ABHTrainJukebox::ABHTrainJukebox()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.08f;
	bReplicates = true;
	InteractionLabel = FText::FromString(TEXT("Jukebox"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Arch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Arch"));
	Arch->SetupAttachment(SceneRoot);
	Speaker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Speaker"));
	Speaker->SetupAttachment(SceneRoot);

	MusicAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudio"));
	MusicAudio->SetupAttachment(SceneRoot);
	MusicAudio->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	MusicAudio->bAutoActivate = false;
	MusicAudio->bOverrideAttenuation = true;
	MusicAudio->AttenuationOverrides.bAttenuate = true;
	MusicAudio->AttenuationOverrides.FalloffDistance = 1800.0f;
	MusicAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(450.0f, 0.0f, 0.0f);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	Glow->SetRelativeLocation(FVector(0.0f, -30.0f, 150.0f));
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(360.0f);
	Glow->SetLightColor(FLinearColor(0.9f, 0.4f, 0.9f, 1.0f));
	Glow->SetCastShadows(false);

	ApplyVisuals();
}

void ABHTrainJukebox::BeginPlay()
{
	Super::BeginPlay();
	BuildTrackList();
	ApplyAudio();
}

void ABHTrainJukebox::BuildTrackList()
{
	if (Tracks.Num() > 0)
	{
		return;
	}
	for (const FJukeTrack& Candidate : BHJukeTracks)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Candidate.Path))
		{
			// Loop the bed at runtime (matches how the menu ambient sets bLooping in BHPlayerController).
			if (USoundWave* Wave = Cast<USoundWave>(Sound))
			{
				Wave->bLooping = true;
			}
			Tracks.Add(Sound);
			TrackNames.Add(Candidate.Name);
		}
	}
}

bool ABHTrainJukebox::IsInLobby() const
{
	const UWorld* W = GetWorld();
	const ABHGameState* BHGS = W ? W->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby;
}

int32 ABHTrainJukebox::GetLocalSelectedTrack() const
{
	// Lobby-only: the jukebox is silent anywhere but the train Lobby.
	if (!IsInLobby())
	{
		return -1;
	}
	const UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;   // this client's local player
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return -1;
	}
	const int32 Idx = BHPS->JukeboxTrackIndex;
	return (Idx >= 0 && Idx < Tracks.Num()) ? Idx : -1;
}

void ABHTrainJukebox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Poll the LOCAL player's per-person selection each tick and play it on this client only.
	ApplyAudio();
	if (Glow)
	{
		const bool bPlaying = (GetLocalSelectedTrack() >= 0);
		const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		Glow->SetIntensity(bPlaying ? (55.0f + 25.0f * FMath::Sin(T * 5.0f)) : 0.0f);
	}
}

bool ABHTrainJukebox::CanInteract_Implementation(ABHCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHTrainJukebox::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	BuildTrackList();
	const int32 Count = Tracks.Num();
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (Count <= 0 || !BHPS)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(TEXT("The jukebox has no records loaded."), 2.0f);
		}
		return;
	}
	// PER-PERSON: cycle only the interacting player's own selection (off -> 0 -> 1 -> ... -> last -> off). It is
	// owner-only replicated, so only their client plays it; everyone else is unaffected.
	const int32 Cur = BHPS->JukeboxTrackIndex;
	BHPS->JukeboxTrackIndex = (Cur + 1 >= Count) ? -1 : (Cur + 1);
	BHPS->ForceNetUpdate();

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		const int32 Now = BHPS->JukeboxTrackIndex;
		const FString Label = (Now >= 0 && TrackNames.IsValidIndex(Now)) ? TrackNames[Now] : FString(TEXT("Off"));
		PC->ClientShowStatusMessage(FString::Printf(TEXT("Jukebox (your track): %s"), *Label), 2.5f);
	}
}

void ABHTrainJukebox::ApplyAudio()
{
	if (!MusicAudio)
	{
		return;
	}
	BuildTrackList();
	const int32 Sel = GetLocalSelectedTrack();   // lobby-gated + bounds-checked
	if (Sel == AppliedTrackIndex && (Sel < 0 || MusicAudio->IsPlaying()))
	{
		return;   // nothing changed since last apply
	}
	AppliedTrackIndex = Sel;
	if (Sel >= 0 && Tracks.IsValidIndex(Sel) && Tracks[Sel])
	{
		MusicAudio->SetSound(Tracks[Sel]);
		MusicAudio->Play();
	}
	else if (MusicAudio->IsPlaying())
	{
		MusicAudio->Stop();
	}
}

FText ABHTrainJukebox::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	(void)Character;
	return FText::FromString(GetLocalSelectedTrack() >= 0 ? TEXT("Next track / off") : TEXT("Play your music"));
}

FBHInteractionPromptInfo ABHTrainJukebox::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(TEXT("JUKEBOX"));
	}
	return Info;
}

void ABHTrainJukebox::GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const
{
	(void)PlayerId;
	OutLines.Reset();
	OutColor = FLinearColor(0.9f, 0.6f, 1.0f, 1.0f);
	OutLines.Add(TEXT("JUKEBOX"));
	const int32 Sel = GetLocalSelectedTrack();
	const FString Now = (Sel >= 0 && TrackNames.IsValidIndex(Sel)) ? TrackNames[Sel] : FString(TEXT("(off)"));
	OutLines.Add(FString::Printf(TEXT("Your track: %s"), *Now));
	OutLines.Add(TEXT("Tap to change your own music."));
}

void ABHTrainJukebox::ApplyVisuals()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetRelativeScale3D(FVector(0.7f, 0.5f, 1.5f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	}
	BHPropVisuals::ConfigurePart(Body, BHPropVisuals::CubeMesh(), BHPropVisuals::WoodMaterial(), FVector(0.0f, 0.0f, 75.0f), FRotator::ZeroRotator, FVector(0.7f, 0.5f, 1.5f), false);
	BHPropVisuals::ConfigurePart(Arch, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -26.0f, 120.0f), FRotator::ZeroRotator, FVector(0.62f, 0.04f, 0.55f), false);
	BHPropVisuals::ConfigurePart(Speaker, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -26.0f, 55.0f), FRotator::ZeroRotator, FVector(0.55f, 0.04f, 0.5f), false);

	BHPropVisuals::TintPart(Body, FLinearColor(0.35f, 0.16f, 0.10f, 1.0f));
	BHPropVisuals::TintPart(Arch, FLinearColor(0.85f, 0.35f, 0.85f, 1.0f), 0.7f);
	BHPropVisuals::TintPart(Speaker, FLinearColor(0.05f, 0.05f, 0.06f, 1.0f), 0.0f);
}

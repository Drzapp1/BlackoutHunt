// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

// Prop Hunt (opt-in, reversible) game-mode partial. This file is one of the ABHGameMode partial .cpp files (alongside
// BHGameModeBotServices.cpp, BHGameModeTrainFlow.cpp, etc.); it adds the prop-hunt round services WITHOUT forking the
// standard hide-and-seek flow. Prop Hunt is gated entirely by bPropHuntMode (parsed from ?BHPropHunt=1 or the
// bh.PropHunt cvar in BuildRuntimeFacility); when off, none of this runs and the game behaves exactly as before.
//
// Design: the SEEKER is the Hunter, the PROPS are the Survivors. The existing role assignment already produces one
// Hunter + the rest Survivors, and the existing capture / "all survivors caught" resolution already gives the seeker
// the win when every prop is found. The only two behavioural deltas live here + at two tiny guarded hooks in
// BHGameMode.cpp: (1) a "taunt" director that periodically forces every hidden prop to make a sound so the seeker has a
// fair chance, and (2) the Hunt-timeout result flips to a PROPS win (they survived). Caught props are kept OUT (the
// Hall-Monitor conversion in NotifySurvivorCaptured is suppressed for prop hunt) so "props found" math stays simple.

#include "BHGameMode.h"

#include "BHCharacter.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropHuntLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

// Master opt-in fallback toggle (the URL option ?BHPropHunt=1 is the primary path). A host can flip this in the console
// before starting and every level for the rest of the session reads it (the cvar is global and survives ServerTravel).
static TAutoConsoleVariable<int32> CVarBHPropHunt(
	TEXT("bh.PropHunt"),
	0,
	TEXT("Prop Hunt mode: when non-zero, the live Hunt becomes a hide-as-a-prop game (Seeker=Hunter, Props=Survivors). ")
	TEXT("Opt-in and reversible; also settable per-session with the ?BHPropHunt=1 launch option."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntTauntBase(
	TEXT("bh.PropHuntTauntBase"),
	30.0f,
	TEXT("Prop Hunt: seconds between forced prop taunts at the START of the Hunt (loosest cadence)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntTauntMin(
	TEXT("bh.PropHuntTauntMin"),
	10.0f,
	TEXT("Prop Hunt: seconds between forced prop taunts at the END of the Hunt (tightest cadence)."),
	ECVF_Default);

// Free accessor so BuildRuntimeFacility (in BHGameMode.cpp) can fold the cvar into the parsed bPropHuntMode without
// the cvar object leaking out of this translation unit. Declared in BHGameMode.h.
bool BHIsPropHuntCVarEnabled()
{
	return CVarBHPropHunt.GetValueOnGameThread() != 0;
}

bool ABHGameMode::IsPropHuntMode() const
{
	return bPropHuntMode;
}

// Count the props (role == Survivor) and how many are still hidden (alive). The Hall-Monitor conversion is suppressed
// for prop hunt, so a caught prop keeps the Survivor role but drops to a non-alive life state -- which makes
// "found = total - remaining" exact without any extra bookkeeping.
void ABHGameMode::CountPropHuntProps(int32& OutTotal, int32& OutRemaining) const
{
	OutTotal = 0;
	OutRemaining = 0;
	if (!GameState)
	{
		return;
	}
	for (const APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || BHPS->PlayerRole != EBHPlayerRole::Survivor)
		{
			continue;
		}
		++OutTotal;
		if (BHPS->LifeState == EBHPlayerLifeState::Alive)
		{
			++OutRemaining;
		}
	}
}

void ABHGameMode::RefreshPropHuntGameState()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
	{
		return;
	}
	int32 Total = 0;
	int32 Remaining = 0;
	CountPropHuntProps(Total, Remaining);
	// NextTaunt is purely a HUD countdown hint; it is recomputed authoritatively in TickPropHunt.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Base = CVarBHPropHuntTauntBase.GetValueOnGameThread();
	const float Min = CVarBHPropHuntTauntMin.GetValueOnGameThread();
	const float Elapsed = (HuntSeconds > 0) ? static_cast<float>(HuntSeconds - BHGS->RemainingTime) / static_cast<float>(HuntSeconds) : 0.0f;
	const float Interval = BHPropHunt::TauntIntervalSeconds(Elapsed, Base, Min);
	const float NextTaunt = (PropHuntLastTauntServerTime > -100.0f) ? PropHuntLastTauntServerTime + Interval : Now + Interval;
	BHGS->SetPropHuntState(true, Remaining, Total, NextTaunt);
}

void ABHGameMode::BeginPropHuntHunt()
{
	if (!bPropHuntMode)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	// Seed the first taunt one full (loose) interval into the Hunt so props get a beat to settle first.
	PropHuntLastTauntServerTime = Now;
	RefreshPropHuntGameState();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!PC || !BHPS)
		{
			continue;
		}
		if (BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			PC->ClientShowStatusMessage(TEXT("PROP HUNT. You are the SEEKER - find every disguised prop before time runs out. Q scans, Mouse1 swings."), 6.0f);
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			PC->ClientShowStatusMessage(TEXT("PROP HUNT. Look at a prop and press Z to become it. Middle-mouse locks you still, [ and ] rotate. Survive the timer!"), 6.0f);
		}
	}
}

// Drives the taunt cadence + the replicated HUD counters. Called once per second from TickRoundTimer's Hunt branch,
// guarded by bPropHuntMode.
void ABHGameMode::TickPropHunt()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Base = CVarBHPropHuntTauntBase.GetValueOnGameThread();
	const float Min = CVarBHPropHuntTauntMin.GetValueOnGameThread();
	const float Elapsed = (HuntSeconds > 0) ? static_cast<float>(HuntSeconds - BHGS->RemainingTime) / static_cast<float>(HuntSeconds) : 0.0f;
	const float Interval = BHPropHunt::TauntIntervalSeconds(Elapsed, Base, Min);

	if (Now - PropHuntLastTauntServerTime >= Interval)
	{
		ForcePropHuntTaunt();
		PropHuntLastTauntServerTime = Now;
	}

	RefreshPropHuntGameState();
}

// Every hidden prop makes a sound at its location (so the seeker gets a fair directional hint), and everyone gets a
// short on-screen cue. Reuses the existing noise + atmosphere stimulus plumbing -- no new assets.
void ABHGameMode::ForcePropHuntTaunt()
{
	if (!GetWorld())
	{
		return;
	}

	int32 TauntedProps = 0;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Prop = *It;
		const ABHPlayerState* BHPS = Prop ? Prop->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BHPS || BHPS->PlayerRole != EBHPlayerRole::Survivor || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}
		const FVector PropLocation = Prop->GetActorLocation();
		NotifyLoudNoise(PropLocation, TEXT("prop_taunt"));
		ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Noise, PropLocation, Prop, nullptr, 1.0f, TEXT("prop_taunt"));
		++TauntedProps;
		if (ABHPlayerController* PropPC = Cast<ABHPlayerController>(Prop->GetController()))
		{
			PropPC->ClientShowStatusMessage(TEXT("TAUNT! Your prop just gave off a sound - move or re-hide."), 2.0f);
		}
	}

	if (TauntedProps <= 0)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (PC && BHPS && BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			PC->ClientShowStatusMessage(TEXT("A taunt rings out - the props just gave themselves away. Listen!"), 2.0f);
		}
	}
}

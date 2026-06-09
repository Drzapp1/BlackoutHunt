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

#include "BHBreaker.h"
#include "BHCharacter.h"
#include "BHEscapeStationManager.h"
#include "BHExitGate.h"
#include "BHGameInstance.h"
#include "BHGameState.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropHuntLibrary.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

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

static TAutoConsoleVariable<int32> CVarBHPropHuntHideSeconds(
	TEXT("bh.PropHuntHideSeconds"),
	30,
	TEXT("Prop Hunt: length of the HIDE phase (the seeker is frozen + screen-blacked while props disguise)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBHPropHuntSeekSeconds(
	TEXT("bh.PropHuntSeekSeconds"),
	240,
	TEXT("Prop Hunt: length of the SEEK phase (the seeker hunts; props survive the clock to win)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBHPropHuntArenaSpawns(
	TEXT("bh.PropHuntArenaSpawns"),
	12,
	TEXT("Prop Hunt: target spawn-point count on an arena map. Placed APlayerStarts are used first; the floor-trace ")
	TEXT("scatter fills the remainder (12 covers a full 10-player lobby with slack)."),
	ECVF_Default);

// --- Seeker kit (P4) -------------------------------------------------------------------------------------------

static TAutoConsoleVariable<float> CVarBHPropHuntScanRadius(
	TEXT("bh.PropHuntScanRadius"),
	1500.0f,
	TEXT("Prop Hunt: radius (cm) of the seeker's Q sonar -- every alive prop inside it is pinged through walls."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntScanCooldown(
	TEXT("bh.PropHuntScanCooldown"),
	8.0f,
	TEXT("Prop Hunt: seconds between seeker sonar pings."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntPulseBase(
	TEXT("bh.PropHuntPulseBase"),
	45.0f,
	TEXT("Prop Hunt: seconds between auto reveal pulses at the START of the seek (loosest cadence)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntPulseMin(
	TEXT("bh.PropHuntPulseMin"),
	12.0f,
	TEXT("Prop Hunt: seconds between auto reveal pulses at the END of the seek (tightest cadence)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntMissSlowBase(
	TEXT("bh.PropHuntMissSlowBase"),
	0.6f,
	TEXT("Prop Hunt: wrong-hit self-slow duration (s) on the seeker's FIRST consecutive miss."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntMissSlowPerMiss(
	TEXT("bh.PropHuntMissSlowPerMiss"),
	0.35f,
	TEXT("Prop Hunt: extra self-slow seconds per additional consecutive miss."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntMissSlowMax(
	TEXT("bh.PropHuntMissSlowMax"),
	2.0f,
	TEXT("Prop Hunt: hard cap (s) on the wrong-hit self-slow."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntMissSlowFactor(
	TEXT("bh.PropHuntMissSlowFactor"),
	0.55f,
	TEXT("Prop Hunt: movement-speed multiplier while the wrong-hit slow is active (0.55 = a heavy but escapable stumble)."),
	ECVF_Default);

// --- Match wrapper (P6) ----------------------------------------------------------------------------------------

static TAutoConsoleVariable<int32> CVarBHPropHuntRoundCount(
	TEXT("bh.PropHuntRoundCount"),
	3,
	TEXT("Prop Hunt: rounds per match (best-of-N). The ?BHPropHuntRounds= travel option overrides per session."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBHPropHuntSeekers(
	TEXT("bh.PropHuntSeekers"),
	1,
	TEXT("Prop Hunt: STARTING seeker count per round (infection adds more as props are caught)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBHPropHuntPointsPerSecondAlive(
	TEXT("bh.PropHuntPointsPerSecondAlive"),
	1,
	TEXT("Prop Hunt: score a hidden prop earns per second alive during the seek."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBHPropHuntPointsPerCatch(
	TEXT("bh.PropHuntPointsPerCatch"),
	75,
	TEXT("Prop Hunt: score a seeker earns per prop caught."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBHPropHuntSurviveBonus(
	TEXT("bh.PropHuntSurviveBonus"),
	100,
	TEXT("Prop Hunt: bonus for every prop still hidden when the seek timer expires (the props-win payoff)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBHPropHuntSeekerBasePoints(
	TEXT("bh.PropHuntSeekerBasePoints"),
	25,
	TEXT("Prop Hunt: flat score for STARTING a round as the seeker, so the rotation is never a points penalty."),
	ECVF_Default);

// --- Taunts + audio (P5) ---------------------------------------------------------------------------------------

static TAutoConsoleVariable<int32> CVarBHPropHuntTauntBonusPoints(
	TEXT("bh.PropHuntTauntBonusPoints"),
	25,
	TEXT("Prop Hunt: score bonus a prop earns for a MANUAL taunt (deliberately giving away its position)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHPropHuntManualTauntCooldown(
	TEXT("bh.PropHuntManualTauntCooldown"),
	8.0f,
	TEXT("Prop Hunt: per-prop seconds between manual taunts (keeps the bonus a deliberate risk, not a farm)."),
	ECVF_Default);

namespace
{
	// First-guess stingers from the SoundsOfHorror pack (the only licensed SFX library in the tree). They are wired
	// for FUNCTION, not final feel -- swap the paths freely after an ear pass in the playtest (P9). Captions are
	// carried by the status-message channel instead so multi-cue moments don't spam the caption line.
	constexpr const TCHAR* BHPropHuntKlaxonSoundPath = TEXT("/Game/SoundsOfHorror/Impacts/CUE/CUE_SOH_IP_03.CUE_SOH_IP_03");
	constexpr const TCHAR* BHPropHuntTauntHornSoundPath = TEXT("/Game/SoundsOfHorror/Clues/CUE/CUE_SOH_Clue_05.CUE_SOH_Clue_05");
	constexpr const TCHAR* BHPropHuntFoundStingSoundPath = TEXT("/Game/SoundsOfHorror/Impacts/CUE/CUE_SOH_IP_07.CUE_SOH_IP_07");
	constexpr const TCHAR* BHPropHuntWrongHitClangSoundPath = TEXT("/Game/SoundsOfHorror/Impacts/CUE/CUE_SOH_IP_02.CUE_SOH_IP_02");
	constexpr const TCHAR* BHPropHuntPulseWhooshSoundPath = TEXT("/Game/SoundsOfHorror/Tension/CUE/CUE_SOH_TS_03.CUE_SOH_TS_03");
	constexpr const TCHAR* BHPropHuntPropsWinSoundPath = TEXT("/Game/SoundsOfHorror/BuildUps/CUE/CUE_SOH_BU_02.CUE_SOH_BU_02");
	constexpr const TCHAR* BHPropHuntSeekersWinSoundPath = TEXT("/Game/SoundsOfHorror/BuildUps/CUE/CUE_SOH_BU_06.CUE_SOH_BU_06");

	FBHGameplayAudioCue BHMakePropHuntAudioCue(const TCHAR* ObjectPath, const FVector& Location, float Volume, float Pitch, float MaxDistance, bool bSpatial)
	{
		FBHGameplayAudioCue Cue;
		Cue.AudioAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(ObjectPath));
		Cue.Location = Location;
		Cue.Volume = Volume;
		Cue.Pitch = Pitch;
		Cue.MaxAudibleDistance = MaxDistance;
		Cue.bSpatial = bSpatial;
		return Cue;
	}
}

// Free accessor so BuildRuntimeFacility (in BHGameMode.cpp) can fold the cvar into the parsed bPropHuntMode without
// the cvar object leaking out of this translation unit. Declared in BHGameMode.h.
bool BHIsPropHuntCVarEnabled()
{
	return CVarBHPropHunt.GetValueOnGameThread() != 0;
}

// Seeker-kit accessors for the pawn code in BHCharacter.cpp (declared in BHGameMode.h; cvars stay file-local).
float BHPropHuntScanRadius()
{
	return FMath::Max(200.0f, CVarBHPropHuntScanRadius.GetValueOnGameThread());
}

float BHPropHuntScanCooldownSeconds()
{
	return FMath::Max(1.0f, CVarBHPropHuntScanCooldown.GetValueOnGameThread());
}

float BHPropHuntMissSlowSecondsFor(int32 ConsecutiveMisses)
{
	return BHPropHunt::SeekerMissSlowSeconds(
		ConsecutiveMisses,
		CVarBHPropHuntMissSlowBase.GetValueOnGameThread(),
		CVarBHPropHuntMissSlowPerMiss.GetValueOnGameThread(),
		CVarBHPropHuntMissSlowMax.GetValueOnGameThread());
}

float BHPropHuntMissSlowFactor()
{
	return FMath::Clamp(CVarBHPropHuntMissSlowFactor.GetValueOnGameThread(), 0.10f, 1.0f);
}

bool ABHGameMode::IsPropHuntMode() const
{
	return bPropHuntMode;
}

void ABHGameMode::StripPropHuntObjectives()
{
	if (!bPropHuntMode)
	{
		return;
	}

	int32 Destroyed = 0;
	auto DestroyAll = [&Destroyed](auto& Array)
	{
		for (const auto& Ptr : Array)
		{
			if (AActor* Actor = Ptr.Get())
			{
				Actor->Destroy();
				++Destroyed;
			}
		}
		Array.Reset();
	};
	DestroyAll(ObjectiveStations);
	DestroyAll(BreakerActors);
	DestroyAll(ExitGates);
	DestroyAll(EscapeStationManagers);

	ActiveBreakerCount = 0;
	ActiveSideObjectiveCount = 0;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBreakerCounts(0, 0);
		BHGS->SetSideObjectiveCounts(0, 0);
		BHGS->SetExitUnlocked(false);
	}
	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt PropHunt: stripped %d classroom objective actor(s) from the arena."), Destroyed);
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

// HIDE phase (rides the base Prep phase). The seeker is frozen + (client-side) screen-blacked; props (Survivors) move
// freely and disguise; capture is off for everyone. The base Prep->Hunt timer transition (RemainingTime<=0 ->
// StartHuntPhase) is the hide->seek release.
void ABHGameMode::BeginPropHuntHidePhase()
{
	if (!bPropHuntMode)
	{
		return;
	}
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (BHGS)
	{
		// bHunterInputFrozen freezes the seeker AND blocks their capture (BHCharacter.cpp); props stay free.
		BHGS->SetIntermissionLocks(/*bCaptureDisabled*/true, /*bPlayerInputFrozen*/false, /*bHunterInputFrozen*/true);
		BHGS->SetRemainingTime(FMath::Max(3, CVarBHPropHuntHideSeconds.GetValueOnGameThread()));
	}
	// Suppress taunts during hide (TickPropHunt only runs in the seek/Hunt phase anyway; this is belt-and-suspenders).
	PropHuntLastTauntServerTime = 1.0e9f;
	RefreshPropHuntGameState();

	// Match wrapper (P6): publish the best-of-N readout (rounds completed live in the GameInstance, surviving the
	// per-round travel) and snapshot every player's score so the round MVP is the biggest DELTA this round.
	const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	const int32 RoundIndex = (BHGI ? BHGI->GetPropHuntRoundsPlayed() : 0) + 1;
	if (BHGS)
	{
		BHGS->SetPropHuntMatchState(RoundIndex, GetPropHuntRoundCount(), false);
	}
	PropHuntRoundStartScores.Reset();
	if (GameState)
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			if (ABHPlayerState* ScorePS = Cast<ABHPlayerState>(RawPS))
			{
				PropHuntRoundStartScores.Add(ScorePS, ScorePS->PropHuntScore);
			}
		}
	}

	RecordPlaytestTelemetryMarker(TEXT("ph_round_start"), HunterSpawn, FString::Printf(TEXT("round=%d"), RoundIndex));

	// Bot orchestration (P8): a light 2 Hz service covering the hide AND the seek (it early-outs by phase).
	PropHuntBotNextActionTime.Reset();
	if (bBotMode)
	{
		GetWorldTimerManager().SetTimer(PropHuntBotServiceHandle, this, &ABHGameMode::ServicePropHuntBots, 0.5f, true, 1.0f);
	}

	const int32 HideSeconds = BHGS ? BHGS->RemainingTime : CVarBHPropHuntHideSeconds.GetValueOnGameThread();
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
			PC->ClientShowStatusMessage(FString::Printf(TEXT("PROP HUNT. You are the SEEKER - eyes closed for %ds while the props hide..."), HideSeconds), 6.0f);
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("HIDE! Look at a prop and press Z to become it. The seeker is blind for %ds. [ and ] rotate, middle-mouse locks."), HideSeconds), 6.0f);
		}
	}
}

// SEEK phase (rides the base Hunt phase). Called from StartHuntPhase: release the seeker + start the seek clock + taunts.
void ABHGameMode::BeginPropHuntHunt()
{
	if (!bPropHuntMode)
	{
		return;
	}
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (BHGS)
	{
		// Release everyone: seeker un-frozen, capture on.
		BHGS->SetIntermissionLocks(false, false, false);
		BHGS->SetRemainingTime(FMath::Max(10, CVarBHPropHuntSeekSeconds.GetValueOnGameThread()));
	}
	PropHuntLastTauntServerTime = Now;
	PropHuntLastPulseServerTime = Now; // first reveal pulse lands a full interval after release
	RefreshPropHuntGameState();

	// Release klaxon: one global (non-spatial) hit so everyone knows the hunt is on.
	BroadcastGameplayAudioCue(BHMakePropHuntAudioCue(BHPropHuntKlaxonSoundPath, FVector::ZeroVector, 0.85f, 1.12f, 0.0f, /*bSpatial*/ false));
	int32 HiddenAtRelease = 0;
	int32 TotalProps = 0;
	CountPropHuntProps(TotalProps, HiddenAtRelease);
	RecordPlaytestTelemetryMarker(TEXT("ph_hide_end"), HunterSpawn, FString::Printf(TEXT("props=%d"), HiddenAtRelease));

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
			PC->ClientShowStatusMessage(TEXT("SEEK! Hunt down every disguised prop. Q scans, Mouse1 swings."), 5.0f);
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			PC->ClientShowStatusMessage(TEXT("The seeker is loose! Hold still and survive the timer."), 5.0f);
		}
	}
}

// A caught prop joins the seekers (infection). Called from NotifySurvivorCaptured's prop-hunt branch. If this was the
// LAST hidden prop, the seekers win the round instead.
void ABHGameMode::HandlePropHuntCapture(ABHCharacter* Survivor, ABHCharacter* CapturingHunter)
{
	ABHPlayerState* PS = Survivor ? Survivor->GetPlayerState<ABHPlayerState>() : nullptr;
	AController* Ctrl = Survivor ? Survivor->GetController() : nullptr;
	if (!PS)
	{
		return;
	}
	const FVector Loc = Survivor->GetActorLocation();
	ABHPlayerState* CatcherPS = CapturingHunter ? CapturingHunter->GetPlayerState<ABHPlayerState>() : nullptr;
	RecordPlaytestTelemetryMarker(TEXT("ph_catch"), Loc, TEXT("prophunt"), CatcherPS, PS);

	const int32 CatchPoints = FMath::Max(0, CVarBHPropHuntPointsPerCatch.GetValueOnGameThread());
	if (CatcherPS && CatchPoints > 0)
	{
		CatcherPS->AddPropHuntScore(CatchPoints);
	}
	if (ABHPlayerController* CatcherPC = Cast<ABHPlayerController>(CapturingHunter ? CapturingHunter->GetController() : nullptr))
	{
		CatcherPC->ClientShowStatusMessage(CatchPoints > 0
			? FString::Printf(TEXT("Caught a prop! +%d points."), CatchPoints)
			: TEXT("Caught a prop!"), 2.5f);
	}

	Survivor->MarkCaptured(); // drops the disguise + any lock, sets Captured/out-of-play/hidden
	ApplyPresenceSpike(Loc, 60.0f, TEXT("A prop was found."));
	BroadcastStatus(FString::Printf(TEXT("%s was found!"), *PS->GetPlayerName()), 3.0f);
	// Found sting: a global hit so the whole lobby feels the count drop (P5 audio).
	BroadcastGameplayAudioCue(BHMakePropHuntAudioCue(BHPropHuntFoundStingSoundPath, Loc, 0.7f, 1.0f, 0.0f, /*bSpatial*/ false));

	// Infection: props remain -> the caught prop joins the seeker team. Otherwise that was the last prop -> seekers win.
	if (CountAliveSurvivors() > 0 && Ctrl)
	{
		PS->SetRole(EBHPlayerRole::Hunter);
		PS->SetLifeState(EBHPlayerLifeState::Alive);
		PS->SetHiddenInLocker(false);
		BroadcastStatus(FString::Printf(TEXT("%s joined the hunt!"), *PS->GetPlayerName()), 3.0f);
		RestartPlayer(Ctrl);
	}
	else
	{
		EndRound(EBHRoundPhase::HunterWin);
	}
	RefreshPropHuntGameState();
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

	// Survival trickle (P6): every second hidden is worth bh.PropHuntPointsPerSecondAlive. Rides this 1 Hz tick, so
	// it only accrues during the seek (this function early-outs outside the Hunt phase).
	const int32 PerSecond = CVarBHPropHuntPointsPerSecondAlive.GetValueOnGameThread();
	if (PerSecond > 0 && GameState)
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			ABHPlayerState* TrickleP = Cast<ABHPlayerState>(RawPS);
			if (TrickleP && TrickleP->PlayerRole == EBHPlayerRole::Survivor && TrickleP->LifeState == EBHPlayerLifeState::Alive)
			{
				TrickleP->AddPropHuntScore(PerSecond);
			}
		}
	}

	// Auto reveal pulse (P4): same tightening-lerp shape as the taunt cadence, on its own (slower) clock. Guarantees
	// a passive seeker still closes the round out -- by the final stretch every prop flashes up every ~PulseMin s.
	const float PulseInterval = BHPropHunt::TauntIntervalSeconds(
		Elapsed,
		CVarBHPropHuntPulseBase.GetValueOnGameThread(),
		CVarBHPropHuntPulseMin.GetValueOnGameThread());
	if (Now - PropHuntLastPulseServerTime >= PulseInterval)
	{
		ForcePropHuntRevealPulse();
		PropHuntLastPulseServerTime = Now;
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
		// Taunt horn at the prop's position -- far-audible so the seeker can walk the sound down (P5 audio).
		BroadcastGameplayAudioCue(BHMakePropHuntAudioCue(BHPropHuntTauntHornSoundPath, PropLocation + FVector(0.0f, 0.0f, 60.0f), 0.9f, 1.18f, 5200.0f, /*bSpatial*/ true));
		// Bot props (P8): a taunt just exposed everyone -- some bots take the cue to scurry somewhere fresh.
		if (BHPS->IsABot() && Prop->IsPropLockedInPlace() && FMath::FRand() < 0.35f)
		{
			PropHuntBotNextActionTime.FindOrAdd(Prop) = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + FMath::FRandRange(0.5f, 2.5f);
		}
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

// Briefly surface EVERY alive prop's position to every seeker, range-unlimited -- the fairness valve that keeps a
// stalled endgame moving. Marker delivery + the screen flash are client-side (ClientReceivePropHuntSonar with
// bIsPulse); props get a warning so the reveal feels fair rather than psychic.
void ABHGameMode::ForcePropHuntRevealPulse()
{
	if (!GetWorld())
	{
		return;
	}

	TArray<FVector> PropLocations;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Prop = *It;
		const ABHPlayerState* BHPS = Prop ? Prop->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BHPS || BHPS->PlayerRole != EBHPlayerRole::Survivor || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}
		PropLocations.Add(Prop->GetActorLocation());
		if (ABHPlayerController* PropPC = Cast<ABHPlayerController>(Prop->GetController()))
		{
			PropPC->ClientShowStatusMessage(TEXT("REVEAL PULSE - the seekers just glimpsed your position!"), 2.5f);
		}
	}
	if (PropLocations.Num() == 0)
	{
		return;
	}

	int32 PulsedSeekers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		ABHCharacter* SeekerChar = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		if (!SeekerChar || !BHPS || BHPS->PlayerRole != EBHPlayerRole::Hunter || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}
		SeekerChar->ClientReceivePropHuntSonar(PropLocations, /*RevealSeconds*/ 1.6f, /*bIsPulse*/ true, /*CooldownSeconds*/ 0.0f);
		// A soft whoosh sells the pulse on the seeker's side only (props get the text warning instead).
		PC->ClientPlayGameplayAudioCue(BHMakePropHuntAudioCue(BHPropHuntPulseWhooshSoundPath, FVector::ZeroVector, 0.5f, 1.0f, 0.0f, /*bSpatial*/ false));
		++PulsedSeekers;
	}
	if (PulsedSeekers > 0)
	{
		RecordPlaytestTelemetryMarker(TEXT("ph_pulse"), PropLocations[0], FString::Printf(TEXT("props=%d seekers=%d"), PropLocations.Num(), PulsedSeekers));
	}
}

// Manual taunt (P5): the risk/reward flush. A live prop deliberately emits the taunt cue at its position for
// bh.PropHuntTauntBonusPoints, on a personal cooldown -- and the AUTO taunt clock restarts, so one bold prop's risk
// buys every prop a fresh quiet window (the fairness debt is paid either way).
void ABHGameMode::HandlePropHuntManualTaunt(ABHCharacter* Prop)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	ABHPlayerState* PropPS = Prop ? Prop->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!bPropHuntMode || !BHGS || !PropPS || !GetWorld())
	{
		return;
	}
	if (PropPS->PlayerRole != EBHPlayerRole::Survivor || PropPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return;
	}
	ABHPlayerController* PropPC = Cast<ABHPlayerController>(Prop->GetController());
	if (BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		if (PropPC)
		{
			PropPC->ClientShowStatusMessage(TEXT("Taunting unlocks when the seek begins."), 2.5f);
		}
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float Cooldown = FMath::Max(1.0f, CVarBHPropHuntManualTauntCooldown.GetValueOnGameThread());
	if (Now - Prop->LastManualTauntServerTime < Cooldown)
	{
		if (PropPC)
		{
			PropPC->ClientShowStatusMessage(FString::Printf(TEXT("Taunt recharging: %ds."), FMath::CeilToInt(Cooldown - (Now - Prop->LastManualTauntServerTime))), 2.0f);
		}
		return;
	}
	Prop->LastManualTauntServerTime = Now;

	const FVector PropLocation = Prop->GetActorLocation();
	NotifyLoudNoise(PropLocation, TEXT("prop_taunt_manual"));
	ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Noise, PropLocation, Prop, nullptr, 1.0f, TEXT("prop_taunt_manual"));
	// Slightly higher pitch than the auto horn so regulars learn to tell a bold prop from the forced chorus.
	BroadcastGameplayAudioCue(BHMakePropHuntAudioCue(BHPropHuntTauntHornSoundPath, PropLocation + FVector(0.0f, 0.0f, 60.0f), 0.95f, 1.35f, 5200.0f, /*bSpatial*/ true));

	const int32 Bonus = FMath::Max(0, CVarBHPropHuntTauntBonusPoints.GetValueOnGameThread());
	PropPS->AddPropHuntScore(Bonus);
	// Pay the fairness debt: the auto-taunt clock restarts for everyone.
	PropHuntLastTauntServerTime = Now;
	RefreshPropHuntGameState();

	if (PropPC)
	{
		PropPC->ClientShowStatusMessage(FString::Printf(TEXT("TAUNT! +%d points - now move before the seeker arrives."), Bonus), 3.0f);
	}
	RecordPlaytestTelemetryMarker(TEXT("ph_taunt_manual"), PropLocation, TEXT("prophunt"), PropPS);
}

// Wrong-hit clang (P5 audio): a spatial metal hit at the whiffing seeker, audible to anyone nearby (a hiding prop
// HEARS the seeker flailing one aisle over -- free intel, the other half of the wrong-hit penalty).
void ABHGameMode::PlayPropHuntWrongHitClang(const FVector& Location)
{
	if (!bPropHuntMode)
	{
		return;
	}
	BroadcastGameplayAudioCue(BHMakePropHuntAudioCue(BHPropHuntWrongHitClangSoundPath, Location, 0.8f, 0.86f, 2400.0f, /*bSpatial*/ true));
}

// Round-end sting (P5 audio): one global win/lose hit. Called from EndRound, which is idempotent, so every end path
// (all caught, timer expiry, seeker disconnect) plays it exactly once.
void ABHGameMode::PlayPropHuntRoundEndSting(EBHRoundPhase ResultPhase)
{
	if (!bPropHuntMode || (ResultPhase != EBHRoundPhase::SurvivorsWin && ResultPhase != EBHRoundPhase::HunterWin))
	{
		return;
	}
	const bool bPropsWon = ResultPhase == EBHRoundPhase::SurvivorsWin;
	BroadcastGameplayAudioCue(BHMakePropHuntAudioCue(
		bPropsWon ? BHPropHuntPropsWinSoundPath : BHPropHuntSeekersWinSoundPath,
		FVector::ZeroVector, 0.8f, 1.0f, 0.0f, /*bSpatial*/ false));
}

int32 ABHGameMode::GetPropHuntRoundCount() const
{
	const int32 Configured = PropHuntRoundCountOption > 0
		? PropHuntRoundCountOption
		: CVarBHPropHuntRoundCount.GetValueOnGameThread();
	return FMath::Clamp(Configured, 1, 9);
}

// Rotation pick (P6): everyone seeks before anyone seeks twice. The per-player counts/scores persist across the
// round travel (FBHTravelPlayerProgress), so leavers simply drop out of the candidate set and rejoiners keep
// their history. Picks bh.PropHuntSeekers players, fewest-seeks-first, ties to the lowest score then join order.
TArray<ABHPlayerState*> ABHGameMode::ChoosePropHuntStartingSeekers(const TArray<ABHPlayerState*>& Players)
{
	TArray<ABHPlayerState*> Candidates;
	for (ABHPlayerState* Candidate : Players)
	{
		if (Candidate)
		{
			Candidates.Add(Candidate);
		}
	}

	const int32 SeekerCount = FMath::Clamp(CVarBHPropHuntSeekers.GetValueOnGameThread(), 1, FMath::Max(1, Candidates.Num() - 1));
	TArray<ABHPlayerState*> Chosen;
	while (Chosen.Num() < SeekerCount && Candidates.Num() > 0)
	{
		TArray<int32> TimesSeeker;
		TArray<int32> Scores;
		for (const ABHPlayerState* Candidate : Candidates)
		{
			TimesSeeker.Add(Candidate->PropHuntTimesSeeker);
			Scores.Add(Candidate->PropHuntScore);
		}
		const int32 PickIndex = BHPropHunt::PickStartingSeekerIndex(TimesSeeker, Scores);
		if (!Candidates.IsValidIndex(PickIndex))
		{
			break;
		}
		Chosen.Add(Candidates[PickIndex]);
		Candidates.RemoveAt(PickIndex);
	}

	const int32 SeekerBase = FMath::Max(0, CVarBHPropHuntSeekerBasePoints.GetValueOnGameThread());
	for (ABHPlayerState* Seeker : Chosen)
	{
		++Seeker->PropHuntTimesSeeker;
		if (SeekerBase > 0)
		{
			Seeker->AddPropHuntScore(SeekerBase);
		}
	}
	return Chosen;
}

// Match wrapper (P6), on the idempotent EndRound seam: survive bonuses, the round-MVP banner, the persisted round
// counter, arena rotation, and -- after the final round -- the champion broadcast + the return-to-lobby flag that
// ResetRoundByTravel consumes.
void ABHGameMode::HandlePropHuntRoundEnd(EBHRoundPhase ResultPhase)
{
	if (!bPropHuntMode || (ResultPhase != EBHRoundPhase::SurvivorsWin && ResultPhase != EBHRoundPhase::HunterWin))
	{
		return;
	}

	// Props that outlasted the seek earn the survive bonus (every alive prop survived; with infection the last
	// prop standing is usually alone by now).
	const int32 SurviveBonus = FMath::Max(0, CVarBHPropHuntSurviveBonus.GetValueOnGameThread());
	if (ResultPhase == EBHRoundPhase::SurvivorsWin && SurviveBonus > 0 && GameState)
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			ABHPlayerState* BonusPS = Cast<ABHPlayerState>(RawPS);
			if (BonusPS && BonusPS->PlayerRole == EBHPlayerRole::Survivor && BonusPS->LifeState == EBHPlayerLifeState::Alive)
			{
				BonusPS->AddPropHuntScore(SurviveBonus);
			}
		}
	}

	// Round MVP = the biggest score DELTA this round (snapshot taken at hide start).
	ABHPlayerState* RoundMvp = nullptr;
	int32 BestDelta = 0;
	if (GameState)
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			ABHPlayerState* DeltaPS = Cast<ABHPlayerState>(RawPS);
			if (!DeltaPS)
			{
				continue;
			}
			const int32* StartScore = PropHuntRoundStartScores.Find(DeltaPS);
			const int32 Delta = DeltaPS->PropHuntScore - (StartScore ? *StartScore : 0);
			if (Delta > BestDelta)
			{
				BestDelta = Delta;
				RoundMvp = DeltaPS;
			}
		}
	}
	if (RoundMvp)
	{
		BroadcastStatus(FString::Printf(TEXT("Round MVP: %s (+%d pts)"), *RoundMvp->GetPlayerName(), BestDelta), 5.0f);
	}

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	const int32 RoundsPlayed = BHGI ? BHGI->IncrementPropHuntRoundsPlayed() : 1;
	const int32 RoundCount = GetPropHuntRoundCount();
	ABHGameState* BHGS = GetGameState<ABHGameState>();

	if (RoundsPlayed >= RoundCount)
	{
		// Match over: crown the champion (top cumulative score) and flag the post-round travel back to the lobby.
		if (BHGS)
		{
			BHGS->SetPropHuntMatchState(RoundsPlayed, RoundCount, true);
		}
		bPropHuntMatchCompletePendingTravel = true;
		ABHPlayerState* Champion = nullptr;
		if (GameState)
		{
			for (APlayerState* RawPS : GameState->PlayerArray)
			{
				ABHPlayerState* ChampPS = Cast<ABHPlayerState>(RawPS);
				if (ChampPS && (!Champion || ChampPS->PropHuntScore > Champion->PropHuntScore))
				{
					Champion = ChampPS;
				}
			}
		}
		BroadcastStatus(Champion
			? FString::Printf(TEXT("MATCH OVER - champion: %s with %d points! Returning to the lobby."), *Champion->GetPlayerName(), Champion->PropHuntScore)
			: TEXT("MATCH OVER. Returning to the lobby."), 6.0f);
		RecordPlaytestTelemetryMarker(TEXT("ph_match_end"), HunterSpawn, FString::Printf(TEXT("rounds=%d"), RoundsPlayed), Champion);
	}
	else
	{
		if (BHGS)
		{
			BHGS->SetPropHuntMatchState(RoundsPlayed, RoundCount, false);
		}
		// Arena rotation: a host map route (?BHMapRoute=ContainersHouse,RuinedCrypt,...) cycles one arena per
		// round; without a route the match replays the same arena.
		if (RuntimeMapRoute.Num() > 0)
		{
			NextRuntimeLevelName = RuntimeMapRoute[RoundsPlayed % RuntimeMapRoute.Num()];
		}
		BroadcastStatus(FString::Printf(TEXT("Round %d of %d done - next round shortly."), RoundsPlayed, RoundCount), 4.0f);
	}
	RecordPlaytestTelemetryMarker(TEXT("ph_round_end"), HunterSpawn, FString::Printf(TEXT("round=%d/%d"), RoundsPlayed, RoundCount));
}

// Bot orchestration (P8). Bot PROPS: wander on their normal survivor brain for a while, then disguise as the
// nearest valid mesh where they happen to stand, lock, and hold -- with an occasional post-taunt relocation
// (scheduled by ForcePropHuntTaunt) so they don't fossilize. Bot SEEKERS: the normal hunter brain does the
// roaming/chasing/swinging (the bot-vision gate makes still disguises invisible to it, so it hunts by noise);
// this service just fires the sonar on a human-ish cadence, which doubles as an audible "seeker was here" tell.
void ABHGameMode::ServicePropHuntBots()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	UWorld* World = GetWorld();
	if (!bPropHuntMode || !World || !BHGS)
	{
		return;
	}
	const bool bHidePhase = BHGS->RoundPhase == EBHRoundPhase::Prep;
	const bool bSeekPhase = BHGS->RoundPhase == EBHRoundPhase::Hunt;
	if (!bHidePhase && !bSeekPhase)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float HideSeconds = FMath::Max(3.0f, static_cast<float>(CVarBHPropHuntHideSeconds.GetValueOnGameThread()));
	for (TActorIterator<ABHCharacter> It(World); It; ++It)
	{
		ABHCharacter* Bot = *It;
		ABHPlayerState* BotPS = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Bot || !BotPS || !BotPS->IsABot() || BotPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		float& NextActionTime = PropHuntBotNextActionTime.FindOrAdd(Bot, -1.0f);
		if (BotPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			if (!Bot->IsDisguisedAsProp())
			{
				if (NextActionTime < 0.0f)
				{
					// First sight of this bot prop: let the survivor brain wander a stretch of the hide first, so
					// bots spread into spots instead of freezing at spawn. Mid-seek (an infection round restart or
					// a relocation window) hides again quickly.
					NextActionTime = Now + (bHidePhase ? FMath::FRandRange(0.30f, 0.65f) * HideSeconds : FMath::FRandRange(2.0f, 5.0f));
				}
				else if (Now >= NextActionTime)
				{
					if (Bot->BotPickNearbyPropDisguiseAuthority())
					{
						Bot->SetPropLockedAuthority(true);
						NextActionTime = Now + 1.0e9f; // hold until a taunt schedules a relocation look
					}
					else
					{
						NextActionTime = Now + 2.0f; // blocked (no-hide zone?) -- keep wandering, retry shortly
					}
				}
			}
			else if (!Bot->IsPropLockedInPlace() && Now >= NextActionTime)
			{
				// End of a relocation wander: settle wherever the brain walked us, possibly as a new prop.
				if (Bot->BotPickNearbyPropDisguiseAuthority())
				{
					Bot->SetPropLockedAuthority(true);
				}
				else
				{
					Bot->SetPropLockedAuthority(true); // keep the current disguise; locking still beats drifting
				}
				NextActionTime = Now + 1.0e9f;
			}
			else if (Bot->IsPropLockedInPlace() && Now >= NextActionTime && NextActionTime < 1.0e8f)
			{
				// A taunt marked this bot for a relocation look: unfreeze and let the survivor brain scurry.
				Bot->SetPropLockedAuthority(false);
				NextActionTime = Now + FMath::FRandRange(3.5f, 7.0f);
			}
		}
		else if (BotPS->PlayerRole == EBHPlayerRole::Hunter && bSeekPhase)
		{
			if (NextActionTime < 0.0f || NextActionTime > 1.0e8f)
			{
				NextActionTime = Now + FMath::FRandRange(4.0f, 9.0f);
			}
			else if (Now >= NextActionTime)
			{
				Bot->BotUseScan(); // the prop sonar branch: loud at the bot, fear-pings nearby props
				NextActionTime = Now + BHPropHuntScanCooldownSeconds() + FMath::FRandRange(2.0f, 6.0f);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// P3: the arena loader. Prop-hunt arenas are the prop-rich imported pack maps (ContainersHouse warehouse, RuinedCrypt,
// ...) wired up by NAME through the BHPropHunt::FArenaSpec registry, not re-authored: the pack demo .umap ships its own
// geometry/lighting, and everything BlackoutHunt needs on top -- spawn points, runtime navmesh, containment, void
// recovery -- is derived from the level's blocking static geometry at load. Hand-placing APlayerStarts (one tagged
// "Hunter") in an authored Arena_<Name> bake (Tools/Setup-PropHuntArena.py) refines the spawns; nothing requires it.
// ---------------------------------------------------------------------------------------------------------------------

FString BHResolvePropHuntArenaPackage(const FString& Token)
{
	const BHPropHunt::FArenaSpec* Spec = BHPropHunt::FindArenaSpec(Token);
	if (!Spec)
	{
		return FString();
	}
	if (FPackageName::DoesPackageExist(Spec->AuthoredPackage))
	{
		return Spec->AuthoredPackage;
	}
	if (FPackageName::DoesPackageExist(Spec->SourcePackage))
	{
		return Spec->SourcePackage;
	}
	UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt PropHunt: arena '%s' is registered but neither %s nor %s exists (not on disk / not cooked)."),
		Spec->LogicalName, Spec->AuthoredPackage, Spec->SourcePackage);
	return FString();
}

namespace
{
	// Does the loaded world's map short-name match this arena spec (either the authored bake or the raw pack demo)?
	// GetMapName() keeps the PIE prefix ("UEDPIE_0_Map_ContainersHouse_Demo"); StreamingLevelsPrefix strips it.
	bool BHWorldMatchesArenaPackage(const UWorld* World, const BHPropHunt::FArenaSpec& Spec)
	{
		if (!World)
		{
			return false;
		}
		FString MapShort = World->GetMapName();
		MapShort.RemoveFromStart(World->StreamingLevelsPrefix);
		return MapShort.Equals(FPackageName::GetShortName(FString(Spec.AuthoredPackage)), ESearchCase::IgnoreCase)
			|| MapShort.Equals(FPackageName::GetShortName(FString(Spec.SourcePackage)), ESearchCase::IgnoreCase);
	}
}

// The blocking static-mesh bounds of the loaded map. Pawn-blocking, registered components only, with sky spheres /
// world-sized shells excluded (their bounds would inflate the play space to nonsense) and door-handle-sized slivers
// ignored. This is the frame every other arena system (spawn scatter, nav volume, containment, void recovery) hangs off.
FBox ABHGameMode::ComputePropHuntArenaBounds() const
{
	FBox Bounds(ForceInit);
	UWorld* World = GetWorld();
	if (!World)
	{
		return Bounds;
	}

	constexpr float MaxComponentExtent = 50000.0f; // 500 m: anything larger is a skybox/backdrop, not play space
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsA<APawn>())
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Actor);
		for (const UStaticMeshComponent* Component : MeshComponents)
		{
			if (!Component || !Component->IsRegistered())
			{
				continue;
			}
			if (Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision
				|| Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
			{
				continue;
			}
			const FVector Extent = Component->Bounds.BoxExtent;
			if (Extent.GetMax() > MaxComponentExtent || Extent.GetMax() <= 2.0f)
			{
				continue;
			}
			Bounds += Component->Bounds.GetBox();
		}
	}
	return Bounds;
}

// Deterministic spawn scatter for arena maps that ship too few PlayerStarts. Halton (2,3) candidates spread over the
// bounds; each is floor-traced TOP-DOWN THROUGH the geometry collecting every standable surface (so a warehouse roof
// does not shadow the interior floor), then the LOWEST standable point with pawn-capsule clearance wins. Points keep
// MinSpacing apart so the props do not spawn in a clump.
TArray<FVector> ABHGameMode::ScatterPropHuntSpawnPoints(UWorld* World, const FBox& Bounds, int32 DesiredCount, int32 Salt)
{
	TArray<FVector> Points;
	if (!World || !Bounds.IsValid || DesiredCount <= 0)
	{
		return Points;
	}

	constexpr float EdgeMargin = 250.0f;      // keep clear of the containment walls
	constexpr float MinSpacing = 500.0f;      // spread the spawn cloud out
	constexpr float CapsuleRadius = 42.0f;    // matches BHResolveSpawnLocation's clearance capsule
	constexpr float CapsuleHalfHeight = 98.0f;
	constexpr int32 MaxFloorHops = 6;         // roof -> mezzanine -> floor is plenty

	const FVector BoundsSize = Bounds.GetSize();
	const float SpanX = FMath::Max(100.0f, BoundsSize.X - 2.0f * EdgeMargin);
	const float SpanY = FMath::Max(100.0f, BoundsSize.Y - 2.0f * EdgeMargin);
	const float TraceTop = Bounds.Max.Z + 500.0f;
	const float TraceBottom = Bounds.Min.Z - 500.0f;
	const int32 MaxCandidates = FMath::Max(DesiredCount * 12, 64);

	FCollisionQueryParams TraceParams(FName(TEXT("BHPropHuntSpawnScatter")), /*bInTraceComplex*/ false);
	const FCollisionShape ClearanceCapsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	for (int32 Candidate = 0; Candidate < MaxCandidates && Points.Num() < DesiredCount; ++Candidate)
	{
		const FVector2D UV = BHPropHunt::ArenaScatterUV(Candidate, Salt);
		const float X = Bounds.Min.X + EdgeMargin + UV.X * SpanX;
		const float Y = Bounds.Min.Y + EdgeMargin + UV.Y * SpanY;

		// Spacing first (cheap) against already-accepted points.
		bool bTooClose = false;
		for (const FVector& Existing : Points)
		{
			if (FVector::DistSquared2D(Existing, FVector(X, Y, 0.0f)) < MinSpacing * MinSpacing)
			{
				bTooClose = true;
				break;
			}
		}
		if (bTooClose)
		{
			continue;
		}

		// Collect every standable surface under this XY by repeated down-traces through each hit.
		TArray<FVector, TInlineAllocator<MaxFloorHops>> StandableHits;
		FVector TraceStart(X, Y, TraceTop);
		for (int32 Hop = 0; Hop < MaxFloorHops && TraceStart.Z > TraceBottom; ++Hop)
		{
			FHitResult FloorHit;
			if (!World->LineTraceSingleByChannel(FloorHit, TraceStart, FVector(X, Y, TraceBottom), ECC_Visibility, TraceParams))
			{
				break;
			}
			if (FloorHit.ImpactNormal.Z >= 0.72f)
			{
				StandableHits.Add(FloorHit.ImpactPoint);
			}
			TraceStart = FloorHit.ImpactPoint - FVector(0.0f, 0.0f, 30.0f);
		}

		// Lowest standable surface with capsule headroom wins (interior floor beats crate-shadowed slab beats roof).
		for (int32 HitIndex = StandableHits.Num() - 1; HitIndex >= 0; --HitIndex)
		{
			const FVector SpawnPoint = StandableHits[HitIndex] + FVector(0.0f, 0.0f, CapsuleHalfHeight + 12.0f);
			if (!World->OverlapBlockingTestByChannel(SpawnPoint, FQuat::Identity, ECC_Pawn, ClearanceCapsule, TraceParams))
			{
				Points.Add(SpawnPoint);
				break;
			}
		}
	}
	return Points;
}

bool ABHGameMode::TryBuildPropHuntArena()
{
	UWorld* World = GetWorld();
	// The sandboxes/lobby/train never run on a pack map; checking here keeps the dispatch call site clean.
	if (!World || bLobbyLevel || bTrainIntermissionLevel || bPracticeMode || bTestMode)
	{
		return false;
	}

	int32 ArenaCount = 0;
	const BHPropHunt::FArenaSpec* Arenas = BHPropHunt::ArenaSpecs(ArenaCount);
	const BHPropHunt::FArenaSpec* LoadedArena = nullptr;
	for (int32 Index = 0; Index < ArenaCount; ++Index)
	{
		if (BHWorldMatchesArenaPackage(World, Arenas[Index]))
		{
			LoadedArena = &Arenas[Index];
			break;
		}
	}
	if (!LoadedArena)
	{
		return false;
	}

	// A loaded arena map IS prop hunt: force the mode for a direct `open <ArenaPackage>` (no URL option, no cvar) so
	// the classroom generator can never build the procedural Facility inside a pack map. BeginPlay publishes the flag
	// to the GameState right after BuildRuntimeFacility returns, so clients pick it up as usual.
	if (!bPropHuntMode)
	{
		bPropHuntMode = true;
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt PropHunt: arena map '%s' loaded without ?BHPropHunt=1; enabling prop hunt for it."), LoadedArena->LogicalName);
	}

	RuntimeLevelName = LoadedArena->LogicalName;
	NextRuntimeLevelName = LoadedArena->LogicalName; // replay the same arena after the round (P6 adds rotation)

	const FBox ArenaBounds = ComputePropHuntArenaBounds();
	if (!ArenaBounds.IsValid)
	{
		// A registered map with zero blocking static meshes -- nothing to stand on. Build a flat emergency pen at
		// the origin so the session is still playable, and say so loudly (this is a registry/map mistake).
		UE_LOG(LogTemp, Error, TEXT("BlackoutHunt PropHunt: arena '%s' has no blocking static geometry; building an emergency holding pen at the origin."), LoadedArena->LogicalName);
		SpawnBlock(FVector(0.0f, 0.0f, -15.0f), FVector(40.0f, 40.0f, 0.30f), FLinearColor(0.20f, 0.22f, 0.25f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
		SurvivorSpawns = { FVector(-600.0f, -600.0f, 120.0f), FVector(600.0f, -600.0f, 120.0f), FVector(-600.0f, 600.0f, 120.0f), FVector(600.0f, 600.0f, 120.0f) };
		HunterSpawn = FVector(1500.0f, 0.0f, 120.0f);
		AddMapContainment(2000.0f, 2000.0f);
		BuildRuntimeNavigationBounds(FVector(0.0f, 0.0f, 200.0f), FVector(4200.0f, 4200.0f, 700.0f));
		return true;
	}

	// --- Spawns: hand-placed PlayerStarts win (the authored-bake path); the floor scatter fills the remainder. ---
	SurvivorSpawns.Reset();
	bool bFoundHunterStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}
		if (Start->PlayerStartTag == FName(TEXT("Hunter")))
		{
			HunterSpawn = Start->GetActorLocation();
			bFoundHunterStart = true;
		}
		else
		{
			SurvivorSpawns.Add(Start->GetActorLocation());
		}
	}
	const int32 PlacedSpawnCount = SurvivorSpawns.Num();

	const int32 DesiredSpawns = FMath::Clamp(CVarBHPropHuntArenaSpawns.GetValueOnGameThread(), 4, 32);
	if (SurvivorSpawns.Num() < DesiredSpawns)
	{
		const int32 Needed = DesiredSpawns - SurvivorSpawns.Num() + (bFoundHunterStart ? 0 : 1);
		SurvivorSpawns.Append(ScatterPropHuntSpawnPoints(World, ArenaBounds, Needed, /*Salt*/ 0));
	}

	// No tagged seeker hold: take the spawn farthest from the cloud's centroid (the edge of the play space), so the
	// frozen, screen-blacked seeker waits away from the middle of the hiding props.
	if (!bFoundHunterStart && SurvivorSpawns.Num() > 0)
	{
		const int32 HoldIndex = BHPropHunt::PickSeekerHoldIndex(SurvivorSpawns);
		if (SurvivorSpawns.IsValidIndex(HoldIndex))
		{
			HunterSpawn = SurvivorSpawns[HoldIndex];
			if (SurvivorSpawns.Num() > 1)
			{
				SurvivorSpawns.RemoveAt(HoldIndex);
			}
		}
	}
	if (SurvivorSpawns.Num() == 0)
	{
		// Scatter found nothing standable (broken collision?). Drop everyone at the bounds centre, high, and let the
		// fall + void recovery sort it out -- degraded but playable, and loud in the log for the per-map checklist.
		UE_LOG(LogTemp, Error, TEXT("BlackoutHunt PropHunt: arena '%s' spawn scatter found no standable floor; using the bounds centre. Check the map's collision."), LoadedArena->LogicalName);
		SurvivorSpawns.Add(FVector(ArenaBounds.GetCenter().X, ArenaBounds.GetCenter().Y, ArenaBounds.Max.Z + 150.0f));
		if (!bFoundHunterStart)
		{
			HunterSpawn = SurvivorSpawns[0] + FVector(300.0f, 0.0f, 0.0f);
		}
	}

	// --- Bounds-driven failsafes: void recovery, containment, runtime navmesh. ---
	VoidRecoveryZThreshold = ArenaBounds.Min.Z - 600.0f;

	const FVector BoundsCenter = ArenaBounds.GetCenter();
	const FVector BoundsSize = ArenaBounds.GetSize();
	constexpr float ContainPad = 400.0f;
	const float WallHeightMeters = FMath::Clamp((BoundsSize.Z + 600.0f) / 100.0f, 6.0f, 60.0f);
	AddMapContainmentAt(FVector(BoundsCenter.X, BoundsCenter.Y, ArenaBounds.Min.Z - 100.0f),
		BoundsSize.X * 0.5f + ContainPad, BoundsSize.Y * 0.5f + ContainPad, WallHeightMeters);

	// Nav band: dip below the lowest floor (Recast skips a surface sitting on the volume's bottom voxel -- see the
	// tutorial note in BuildRuntimeNavigation) and stop a little over the roof. A roof navmesh patch may generate on
	// enclosed maps; players don't path-follow, and the P8 bot pass owns trimming it if it ever misleads bots.
	const float NavZMin = ArenaBounds.Min.Z - 100.0f;
	const float NavZMax = ArenaBounds.Max.Z + 300.0f;
	BuildRuntimeNavigationBounds(
		FVector(BoundsCenter.X, BoundsCenter.Y, (NavZMin + NavZMax) * 0.5f),
		FVector(BoundsSize.X + 2.0f * ContainPad, BoundsSize.Y + 2.0f * ContainPad, NavZMax - NavZMin));

	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt PropHunt: arena '%s' built -- bounds %s, %d placed + %d scattered spawns, seeker hold %s (%s), void-Z %.0f."),
		LoadedArena->LogicalName,
		*ArenaBounds.ToString(),
		PlacedSpawnCount,
		SurvivorSpawns.Num() - PlacedSpawnCount,
		*HunterSpawn.ToCompactString(),
		bFoundHunterStart ? TEXT("tagged PlayerStart") : TEXT("farthest spawn"),
		VoidRecoveryZThreshold);
	return true;
}

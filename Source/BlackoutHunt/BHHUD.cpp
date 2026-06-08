// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHHUD.h"
#include "BHAccountSubsystem.h"
#include "BHBreakableGlassPane.h"
#include "BHCharacter.h"
#include "BHBreaker.h"
#include "BHDiagramRenderer.h"
#include "BHExitGate.h"
#include "BHGameState.h"
#include "BHInteractableInterface.h"
#include "BHJumpscareMonster.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHCosmeticUnlocks.h"
#include "BHRevisionQuestionBank.h"
#include "BHSecurityMonitor.h"
#include "BHTrainBlackjackTable.h"
#include "BHTrainChessTable.h"
#include "BHTrainTicTacToeTable.h"
#include "BHTrainConnectFourTable.h"
#include "BHTrainSlotMachine.h"
#include "BHTrainOthelloTable.h"
#include "BHTrainDartboard.h"
#include "BHTrainBonusQuestionTerminal.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// Survivor stress warning thresholds (fear/dread are 0..100 meters). These cutoffs
	// drive the on-screen stress hint lines shown under the vitals readout.
	constexpr float HudFearPanicHintThreshold = 74.0f;
	constexpr float HudDreadPanicHintThreshold = 82.0f;
	constexpr float HudDreadHintThreshold = 62.0f;
	constexpr float HudFearHintThreshold = 55.0f;

	// Meter warning cutoffs shared by DrawProgressBar / DrawRawMeter and the presence pill.
	// "High" meters (fear/dread/presence) warn at/above the high threshold; the teacher
	// signal bar uses a lower cutoff. "Low" meters (battery/stamina) warn at/below the low
	// threshold.
	constexpr float HudHighMeterWarningThreshold = 72.0f;
	constexpr float HudTeacherSignalWarningThreshold = 58.0f;
	constexpr float HudLowMeterWarningThreshold = 24.0f;

	// Maximum range (in cm) over which a teacher/hunter signal is surfaced on the HUD,
	// used both for proximity scanning and for the visible-hunter arrow distance falloff.
	constexpr float HudTeacherSignalRangeCm = 6000.0f;

	FString FormatClock(const int32 TotalSeconds)
	{
		const int32 ClampedSeconds = FMath::Max(0, TotalSeconds);
		return FString::Printf(TEXT("%02d:%02d"), ClampedSeconds / 60, ClampedSeconds % 60);
	}

	FString ClampHudLine(const FString& Text, int32 MaxCharacters)
	{
		const int32 SafeMax = FMath::Max(8, MaxCharacters);
		if (Text.Len() <= SafeMax)
		{
			return Text;
		}
		return Text.Left(SafeMax - 3) + TEXT("...");
	}

	FString TrainPhaseLabel(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("ARRIVAL");
		case EBHTrainPhase::Recap:
			return TEXT("RECAP");
		case EBHTrainPhase::BonusQuestion:
			return TEXT("BONUS QUESTIONS");
		case EBHTrainPhase::Shop:
			return TEXT("SHOP");
		case EBHTrainPhase::StationStop:
			return TEXT("STATION STOP");
		case EBHTrainPhase::Departing:
			return TEXT("DEPARTING");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("TRAIN");
		}
	}

	FString TrainPhaseObjective(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("NEXT: Board the train before recap starts.");
	case EBHTrainPhase::Recap:
		return TEXT("NEXT: Read recap boards; snacks, drinks, and minigames are optional.");
	case EBHTrainPhase::BonusQuestion:
		return TEXT("NEXT: Use the bonus terminal with 1-4, or play optional minigames.");
	case EBHTrainPhase::Shop:
		return TEXT("NEXT: Buy role-eligible upgrades; train activities stay open.");
		case EBHTrainPhase::StationStop:
			return TEXT("NEXT: Doors are open. Board now.");
		case EBHTrainPhase::Departing:
			return TEXT("NEXT: Doors closed. Hold position for travel.");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("NEXT: Await train route data.");
		}
	}

	FString TrainPhaseNextBeat(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("Next: recap boards.");
		case EBHTrainPhase::Recap:
			return TEXT("Next: bonus terminal.");
		case EBHTrainPhase::BonusQuestion:
			return TEXT("Next: shop carriage.");
		case EBHTrainPhase::Shop:
			return TEXT("Next: station stop.");
		case EBHTrainPhase::StationStop:
			return TEXT("Next: departure.");
		case EBHTrainPhase::Departing:
			return TEXT("Next: route loads.");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("Next: waiting for route data.");
		}
	}

	FString TrainDoorHudStatus(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
		case EBHTrainPhase::StationStop:
			return TEXT("Doors open");
		case EBHTrainPhase::Departing:
			return TEXT("Doors closed for departure");
		case EBHTrainPhase::Recap:
		case EBHTrainPhase::BonusQuestion:
		case EBHTrainPhase::Shop:
			return TEXT("Doors closed while moving");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("Doors pending");
		}
	}

	FString SpectatorRolePreferenceLabel(EBHPlayerRole Role)
	{
		switch (Role)
		{
		case EBHPlayerRole::Hunter:
			return TEXT("TEACHER");
		case EBHPlayerRole::Survivor:
			return TEXT("SURVIVOR");
		case EBHPlayerRole::FakeHunter:
			return TEXT("MONITOR");
		case EBHPlayerRole::Unassigned:
		default:
			return TEXT("NONE");
		}
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= FMath::Clamp(Alpha, 0.0f, 1.0f);
		return Color;
	}

	bool IsAlivePathThreat(const ABHPlayerState* PlayerState)
	{
		return PlayerState
			&& PlayerState->LifeState == EBHPlayerLifeState::Alive
			&& (PlayerState->PlayerRole == EBHPlayerRole::Hunter
				|| PlayerState->PlayerRole == EBHPlayerRole::FakeHunter
				|| PlayerState->PlayerRole == EBHPlayerRole::Tester);
	}

	uint8 ObjectiveBeatRoleBit(EBHPlayerRole Role)
	{
		const int32 RoleIndex = static_cast<int32>(Role);
		return RoleIndex >= 0 && RoleIndex < 8 ? static_cast<uint8>(1u << RoleIndex) : 0;
	}

	bool IsObjectiveBeatVisibleTo(const FBHObjectiveBeat& Beat, const ABHPlayerState* ViewerState)
	{
		if (Beat.AudienceRoleMask == 0)
		{
			return true;
		}
		if (!ViewerState)
		{
			return false;
		}
		if (ViewerState->PlayerRole == EBHPlayerRole::Tester)
		{
			return true;
		}
		return (Beat.AudienceRoleMask & ObjectiveBeatRoleBit(ViewerState->PlayerRole)) != 0;
	}

	bool IsAliveTeacherThreat(const ABHPlayerState* PlayerState)
	{
		return PlayerState
			&& PlayerState->LifeState == EBHPlayerLifeState::Alive
			&& (PlayerState->PlayerRole == EBHPlayerRole::Hunter
				|| PlayerState->PlayerRole == EBHPlayerRole::Tester);
	}

	bool IsRevisionParticipant(const ABHPlayerState* PlayerState)
	{
		return PlayerState
			&& PlayerState->LifeState == EBHPlayerLifeState::Alive
			&& (PlayerState->PlayerRole == EBHPlayerRole::Survivor
				|| PlayerState->PlayerRole == EBHPlayerRole::FakeHunter
				|| PlayerState->PlayerRole == EBHPlayerRole::Tester);
	}

	bool AllowsClassroomWarmupObjectives(const ABHGameState* GameState)
	{
		return GameState
			&& GameState->RoundPhase == EBHRoundPhase::Prep
			&& !GameState->bPracticeMode
			&& !GameState->bTestMode;
	}

	int32 CountRevisionParticipants(const ABHGameState* GameState)
	{
		if (!GameState)
		{
			return 0;
		}

		int32 Count = 0;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (IsRevisionParticipant(Cast<ABHPlayerState>(PlayerState)))
			{
				++Count;
			}
		}
		return Count;
	}

	int32 EstimateRevisionContributionTarget(const ABHGameState* GameState)
	{
		if (GameState && GameState->RevisionContributionTarget > 0)
		{
			return FMath::Clamp(GameState->RevisionContributionTarget, 1, 6);
		}

		// Fallback only until the server replicates the frozen target: mirror the authority formula in
		// ABHGameMode::ComputeLiveRevisionContributionTarget (5, plus one more on later stages) so the HUD
		// never under-promises a contribution gate the server actually enforces at 5-6. (Previously this
		// estimated from questions-per-node and capped at 4, which disagreed with the real requirement.)
		const int32 ParticipantCount = CountRevisionParticipants(GameState);
		(void)ParticipantCount;
		const int32 StageIndex = GameState ? FMath::Clamp(GameState->TrainStageIndex, 0, 2) : 0;
		return FMath::Clamp(5 + FMath::Clamp(StageIndex, 0, 1), 1, 6);
	}

	float GuidanceDistanceMeters(float DistanceCm)
	{
		return FMath::Max(0.0f, DistanceCm) / 100.0f;
	}

	FString FormatGuidanceDistance(float DistanceCm)
	{
		return FString::Printf(TEXT("%.0fm"), GuidanceDistanceMeters(DistanceCm));
	}

	bool IsCloserObjectiveCandidate(const AActor* Actor, const FVector& Origin, float& InOutBestDistanceSq)
	{
		if (!Actor)
		{
			return false;
		}

		const float DistanceSq = FVector::DistSquared2D(Actor->GetActorLocation(), Origin);
		if (DistanceSq >= InOutBestDistanceSq)
		{
			return false;
		}

		InOutBestDistanceSq = DistanceSq;
		return true;
	}

	template <typename ActorType>
	float ResolveGuidanceDistanceCm(const ActorType* Actor, const FVector& Origin)
	{
		return Actor ? FMath::Sqrt(FVector::DistSquared2D(Actor->GetActorLocation(), Origin)) : 0.0f;
	}

	const ABHBreaker* FindNearestActiveBreaker(UWorld* World, const ABHCharacter* Character, float& OutDistanceCm)
	{
		OutDistanceCm = 0.0f;
		if (!World || !Character)
		{
			return nullptr;
		}

		const FVector Origin = Character->GetActorLocation();
		float BestDistanceSq = TNumericLimits<float>::Max();
		const ABHBreaker* BestBreaker = nullptr;
		for (TActorIterator<ABHBreaker> It(World); It; ++It)
		{
			const ABHBreaker* Breaker = *It;
			if (Breaker && Breaker->IsDirectorActive() && !Breaker->IsRepaired() && IsCloserObjectiveCandidate(Breaker, Origin, BestDistanceSq))
			{
				BestBreaker = Breaker;
			}
		}

		OutDistanceCm = ResolveGuidanceDistanceCm(BestBreaker, Origin);
		return BestBreaker;
	}

	const ABHObjectiveStation* FindNearestActiveStation(UWorld* World, const ABHCharacter* Character, float& OutDistanceCm)
	{
		OutDistanceCm = 0.0f;
		if (!World || !Character)
		{
			return nullptr;
		}

		const FVector Origin = Character->GetActorLocation();
		float BestDistanceSq = TNumericLimits<float>::Max();
		const ABHObjectiveStation* BestStation = nullptr;
		for (TActorIterator<ABHObjectiveStation> It(World); It; ++It)
		{
			const ABHObjectiveStation* Station = *It;
			if (Station
				&& Station->IsDirectorActive()
				&& !Station->IsCompleted()
				&& !Station->IsTeacherMirrorTrapNode()
				&& IsCloserObjectiveCandidate(Station, Origin, BestDistanceSq))
			{
				BestStation = Station;
			}
		}

		OutDistanceCm = ResolveGuidanceDistanceCm(BestStation, Origin);
		return BestStation;
	}

	const ABHExitGate* FindNearestActiveExitGate(UWorld* World, const ABHCharacter* Character, float& OutDistanceCm)
	{
		OutDistanceCm = 0.0f;
		if (!World || !Character)
		{
			return nullptr;
		}

		const FVector Origin = Character->GetActorLocation();
		float BestDistanceSq = TNumericLimits<float>::Max();
		const ABHExitGate* BestExitGate = nullptr;
		for (TActorIterator<ABHExitGate> It(World); It; ++It)
		{
			const ABHExitGate* ExitGate = *It;
			if (ExitGate && ExitGate->IsDirectorActive() && IsCloserObjectiveCandidate(ExitGate, Origin, BestDistanceSq))
			{
				BestExitGate = ExitGate;
			}
		}

		OutDistanceCm = ResolveGuidanceDistanceCm(BestExitGate, Origin);
		return BestExitGate;
	}

	FString BuildStationGuidanceText(const ABHObjectiveStation* Station, float DistanceCm, bool bRevisionMode)
	{
		const FString DistanceText = FormatGuidanceDistance(DistanceCm);
		if (!Station)
		{
			return bRevisionMode
				? FString(TEXT("Find an active class node. Answer with 1-4, then hold E after unlock."))
				: FString(TEXT("Find an active station. Answer with 1-4, then hold E after unlock."));
		}

		const bool bNeedsAnswer = !Station->IsQuestionSolved() && Station->GetQuestionChoiceCount() > 0;
		if (bNeedsAnswer)
		{
			return bRevisionMode
				? FString::Printf(TEXT("Nearest class node %s: answer with 1-4, then hold E."), *DistanceText)
				: FString::Printf(TEXT("Nearest task %s: answer with 1-4, then hold E."), *DistanceText);
		}

		return bRevisionMode
			? FString::Printf(TEXT("Unlocked class node %s: hold E to finish the team task."), *DistanceText)
			: FString::Printf(TEXT("Unlocked task %s: hold E to finish it."), *DistanceText);
	}

	FString BuildSurvivorHuntActionLine(UWorld* World, const ABHGameState* GameState, const ABHPlayerState* PlayerState, const ABHCharacter* Character)
	{
		if (!GameState)
		{
			return TEXT("Find your role objective.");
		}

		if (GameState->bExitUnlocked)
		{
			float ExitDistanceCm = 0.0f;
			if (FindNearestActiveExitGate(World, Character, ExitDistanceCm))
			{
				return FString::Printf(TEXT("Exit open. Nearest active gate %s: press E to escape."), *FormatGuidanceDistance(ExitDistanceCm));
			}
			return TEXT("Exit open. Reach an active gate and press E.");
		}

		const int32 RemainingBreakers = FMath::Max(0, GameState->BreakersRequired - GameState->BreakersCompleted);
		const int32 RemainingStations = FMath::Max(0, GameState->SideObjectivesRequired - GameState->SideObjectivesCompleted);
		float BreakerDistanceCm = 0.0f;
		float StationDistanceCm = 0.0f;
		const ABHBreaker* NearestBreaker = RemainingBreakers > 0 && !GameState->bRevisionMode
			? FindNearestActiveBreaker(World, Character, BreakerDistanceCm)
			: nullptr;
		const ABHObjectiveStation* NearestStation = RemainingStations > 0
			? FindNearestActiveStation(World, Character, StationDistanceCm)
			: nullptr;

		if (GameState->bRevisionMode)
		{
			if (RemainingStations > 0)
			{
				return BuildStationGuidanceText(NearestStation, StationDistanceCm, true);
			}

			const int32 ContributionTarget = EstimateRevisionContributionTarget(GameState);
			if (PlayerState && PlayerState->RevisionStats.MasteryPercent < GameState->RevisionIndividualThreshold)
			{
				return FString::Printf(TEXT("Exit locked. Correct missed topics to reach %.0f%% mastery."), GameState->RevisionIndividualThreshold);
			}
			if (PlayerState && PlayerState->RevisionStats.ContributionCount < ContributionTarget)
			{
				return FString::Printf(TEXT("Exit locked. Contribute at class nodes (%d/%d)."), PlayerState->RevisionStats.ContributionCount, ContributionTarget);
			}
			return TEXT("Class gates are checking mastery. Help classmates finish weak topics.");
		}

		if (NearestBreaker && (!NearestStation || BreakerDistanceCm <= StationDistanceCm))
		{
			return RemainingStations > 0
				? FString::Printf(TEXT("Nearest power %s: hold E to repair. Exit also needs %d task%s."),
					*FormatGuidanceDistance(BreakerDistanceCm),
					RemainingStations,
					RemainingStations == 1 ? TEXT("") : TEXT("s"))
				: FString::Printf(TEXT("Nearest power %s: hold E until repaired."), *FormatGuidanceDistance(BreakerDistanceCm));
		}

		if (NearestStation)
		{
			FString StationLine = BuildStationGuidanceText(NearestStation, StationDistanceCm, false);
			if (RemainingBreakers > 0)
			{
				StationLine += FString::Printf(TEXT(" Exit also needs %d breaker%s."), RemainingBreakers, RemainingBreakers == 1 ? TEXT("") : TEXT("s"));
			}
			return ClampHudLine(StationLine, 118);
		}

		if (RemainingBreakers > 0)
		{
			return FString::Printf(TEXT("Find a live breaker. Hold E to repair %d power node%s."), RemainingBreakers, RemainingBreakers == 1 ? TEXT("") : TEXT("s"));
		}
		if (RemainingStations > 0)
		{
			return FString::Printf(TEXT("Find an active station. Finish %d task%s to unlock the exit."), RemainingStations, RemainingStations == 1 ? TEXT("") : TEXT("s"));
		}
		return TEXT("All objectives are done. Find the active exit gate.");
	}

	FString BuildTrainActionLine(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
	{
		if (!GameState)
		{
			return TEXT("Wait for train status.");
		}

		switch (GameState->TrainPhase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("Train arrived. Board now; snacks, drinks, and minigames are optional once aboard.");
		case EBHTrainPhase::Recap:
			return TEXT("Read recap boards, grab food/drink, or play optional social-car minigames.");
		case EBHTrainPhase::BonusQuestion:
			return PlayerState && PlayerState->IsAliveSurvivor()
				? FString(TEXT("Answer the bonus terminal with 1-4, or play minigames for small points."))
				: FString(TEXT("Bonus phase live. Snacks, drinks, and minigames are optional."));
		case EBHTrainPhase::Shop:
			return PlayerState && PlayerState->IsAliveHunter()
				? FString(TEXT("Buy Teacher upgrades with capture points, or play social-car minigames."))
				: FString(TEXT("Buy role-eligible powerups; food, drinks, and games stay open."));
		case EBHTrainPhase::StationStop:
			return TEXT("Doors open. Board before departure; outside players are pulled aboard.");
		case EBHTrainPhase::Departing:
			return TEXT("Boarding closed. Hold position while the next route loads.");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("Wait for the next train instruction.");
		}
	}

	FString BuildFinalEscapeActionLine(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
	{
		if (!GameState)
		{
			return TEXT("Reach the evacuation train.");
		}

		if (GameState->FinalEscapeState == EBHFinalEscapeState::Cutscene)
		{
			return TEXT("Evacuation train unlocking. Control returns soon; Teacher release is delayed.");
		}
		if (GameState->FinalEscapeState == EBHFinalEscapeState::Departed)
		{
			return TEXT("Evacuation train departed. Round is resolving.");
		}
		if (GameState->FinalEscapeState == EBHFinalEscapeState::Failed)
		{
			return TEXT("Evacuation failed. Wait for the result screen.");
		}
		if (PlayerState && PlayerState->IsAliveHunter())
		{
			return TEXT("Final escape active. Stop students, but door camping disrupts capture pressure.");
		}
		if (PlayerState && PlayerState->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			return TEXT("Final escape active. Pressure routes; Hall Monitors cannot board.");
		}
		return TEXT("Final escape active. Board any open evacuation train door.");
	}

	FString BuildHudActionLine(UWorld* World, const ABHGameState* GameState, const ABHPlayerState* PlayerState, const ABHCharacter* Character)
	{
		if (!GameState)
		{
			return TEXT("Host or join a classroom session.");
		}

		if (PlayerState)
		{
			if (PlayerState->LifeState == EBHPlayerLifeState::Escaped)
			{
				return TEXT("You escaped. Watch the remaining route until the next phase.");
			}
			if (PlayerState->PlayerRole == EBHPlayerRole::Spectator)
			{
				return GameState->RoundPhase == EBHRoundPhase::Lobby
					? FString(TEXT("Spectator lobby. Queue a next-round role or wait for host assignment."))
					: FString(TEXT("Spectator support: H encourages; T/Y/U request next-round roles."));
			}
			if (PlayerState->LifeState == EBHPlayerLifeState::Captured && PlayerState->PlayerRole != EBHPlayerRole::FakeHunter)
			{
				return TEXT("Caught. Wait for Hall Monitor return or the next lobby.");
			}
		}

		switch (GameState->RoundPhase)
		{
		case EBHRoundPhase::Lobby:
			return PlayerState && PlayerState->bReady
				? FString(TEXT("Ready set. Wait for every player and the host start."))
				: FString(TEXT("Press Enter to ready up for the classroom round."));
		case EBHRoundPhase::Prep:
		{
			// Guided warmup checklist: a short, role-specific "N/M - next step" line computed from
			// the player's tried-actions mask (EBHWarmupStep). Replaces the old static strings so a
			// new student always sees the next thing to try, and a clear "you're ready" when done.
			FString Line;
			if (!PlayerState)
			{
				Line = TEXT("Warmup: try your controls. The live Hunt resets everyone.");
			}
			else
			{
				int32 Done = 0;
				int32 Total = 0;
				BHWarmupProgress(PlayerState->PlayerRole, PlayerState->WarmupChecklistMask, Done, Total);
				if (Total <= 0)
				{
					Line = TEXT("Warmup: get ready. The live Hunt resets everyone.");
				}
				else if (Done >= Total)
				{
					Line = TEXT("Warmup complete - you're ready. Wait for the host to start the Hunt.");
				}
				else
				{
					const FString NextStep = BHWarmupNextStepLabel(PlayerState->PlayerRole, PlayerState->WarmupChecklistMask);
					Line = FString::Printf(TEXT("Warmup %d/%d - %s. Hunt start resets everyone."), Done, Total, *NextStep);
				}
			}
			// Final handoff: count the last few seconds down so the safe->live switch is unmistakable.
			if (GameState->RemainingTime > 0 && GameState->RemainingTime <= 5)
			{
				return FString::Printf(TEXT("Hunt starts in %ds. %s"), GameState->RemainingTime, *Line);
			}
			return Line;
		}
		case EBHRoundPhase::Intermission:
			return BuildTrainActionLine(GameState, PlayerState);
		case EBHRoundPhase::FinalEscape:
			return BuildFinalEscapeActionLine(GameState, PlayerState);
		case EBHRoundPhase::SurvivorsWin:
			return TEXT("Survivors escaped. Returning to lobby shortly.");
		case EBHRoundPhase::HunterWin:
			return TEXT("Teacher wins. Returning to lobby shortly.");
		case EBHRoundPhase::Hunt:
		default:
			break;
		}

		if (!PlayerState || PlayerState->PlayerRole == EBHPlayerRole::Unassigned)
		{
			return TEXT("Wait for role assignment.");
		}

		if (PlayerState->PlayerRole == EBHPlayerRole::Tester && PlayerState->LifeState == EBHPlayerLifeState::Alive)
		{
			return TEXT("Tester tools active. E handles objectives; Left Mouse captures; shortcuts stay in the role panel.");
		}

		if (PlayerState->IsAliveHunter())
		{
			if (Character && Character->GetTeacherCaptureCooldownRemaining() > 0.0f)
			{
				return FString::Printf(TEXT("Axe recovering %ds. Use Q scan, listen for noise, and cut off routes."),
					FMath::CeilToInt(Character->GetTeacherCaptureCooldownRemaining()));
			}
			return GameState->bExitUnlocked
				? FString(TEXT("Exit open. Patrol active gates; capture visible students away from doors."))
				: FString(TEXT("Use Q for rough heartbeat, watch noise/CCTV pings, and capture visible students."));
		}

		if (PlayerState->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			if (GameState->bRevisionMode)
			{
				const int32 ContributionTarget = EstimateRevisionContributionTarget(GameState);
				if (PlayerState->RevisionStats.ContributionCount < ContributionTarget)
				{
					return FString::Printf(TEXT("Hall Monitor tools locked. Answer at stations to contribute (%d/%d)."),
						PlayerState->RevisionStats.ContributionCount,
						ContributionTarget);
				}
				return TEXT("Hall Monitor tools ready. G places traps; Q real hint; R false marker.");
			}
			return TEXT("Hall Monitor: misdirect with traps and hints. You cannot capture.");
		}

		return BuildSurvivorHuntActionLine(World, GameState, PlayerState, Character);
	}

	FString BuildHudDetailLine(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
	{
		if (!GameState)
		{
			return TEXT("");
		}

		if (GameState->RoundPhase == EBHRoundPhase::Intermission)
		{
			const float ServerNow = GameState->GetServerWorldTimeSeconds();
			const int32 Countdown = FMath::Max(0, FMath::CeilToInt(GameState->TrainPhaseEndServerTime - ServerNow));
			const FString PointsText = PlayerState
				? (PlayerState->IsAliveHunter()
					? FString::Printf(TEXT("CAPTURE %d"), PlayerState->HunterPoints)
					: FString::Printf(TEXT("POINTS %d"), PlayerState->QuestionPoints))
				: FString(TEXT("POINTS --"));
			return FString::Printf(TEXT("%s / %s / %s / %s / %s"),
				*TrainPhaseLabel(GameState->TrainPhase),
				*FormatClock(Countdown),
				GameState->TrainDestinationName.IsEmpty() ? TEXT("NEXT STOP") : *GameState->TrainDestinationName.ToUpper(),
				*PointsText,
				*TrainDoorHudStatus(GameState->TrainPhase).ToUpper());
		}

		if (GameState->RoundPhase == EBHRoundPhase::FinalEscape)
		{
			const float ServerNow = GameState->GetServerWorldTimeSeconds();
			const int32 EscapeCountdown = FMath::Max(0, FMath::CeilToInt(GameState->FinalEscapeEndServerTime - ServerNow));
			if (GameState->FinalEscapeState == EBHFinalEscapeState::Cutscene)
			{
				const int32 CutsceneCountdown = FMath::Max(0, FMath::CeilToInt(GameState->FinalEscapeCutsceneEndServerTime - ServerNow));
				const int32 HunterReleaseCountdown = FMath::Max(0, FMath::CeilToInt(GameState->HunterReleaseServerTime - ServerNow));
				return FString::Printf(TEXT("CONTROL %s / TEACHER RELEASE %s / TRAIN %s"),
					*FormatClock(CutsceneCountdown),
					*FormatClock(HunterReleaseCountdown),
					*FormatClock(EscapeCountdown));
			}
			const int32 HunterReleaseCountdown = FMath::Max(0, FMath::CeilToInt(GameState->HunterReleaseServerTime - ServerNow));
			return HunterReleaseCountdown > 0
				? FString::Printf(TEXT("TRAIN DEPARTS %s / TEACHER RELEASE %s"), *FormatClock(EscapeCountdown), *FormatClock(HunterReleaseCountdown))
				: FString::Printf(TEXT("TRAIN DEPARTS %s / TEACHER RELEASED"), *FormatClock(EscapeCountdown));
		}

		if (GameState->bRevisionMode)
		{
			const int32 ContributionTarget = EstimateRevisionContributionTarget(GameState);
			const FString PersonalText = PlayerState && IsRevisionParticipant(PlayerState)
				? FString::Printf(TEXT("YOU %.0f/%.0f CONTRIB %d/%d"),
					PlayerState->RevisionStats.MasteryPercent,
					GameState->RevisionIndividualThreshold,
					PlayerState->RevisionStats.ContributionCount,
					ContributionTarget)
				: FString(TEXT("OBSERVE"));
			return FString::Printf(TEXT("CLASS %.0f/%.0f  %s"),
				GameState->RevisionClassMasteryAverage,
				GameState->RevisionClassThreshold,
				*PersonalText);
		}

		const FString BreakerReadout = GameState->BreakersRequired > 0
			? FString::Printf(TEXT("%d/%d"), GameState->BreakersCompleted, GameState->BreakersRequired)
			: FString(TEXT("CLEAR"));
		const FString StationReadout = GameState->SideObjectivesRequired > 0
			? FString::Printf(TEXT("%d/%d"), GameState->SideObjectivesCompleted, GameState->SideObjectivesRequired)
			: FString(TEXT("CLEAR"));
		const FString ModifierText = GameState->RoundModifier == EBHRoundModifier::None ? FString(TEXT("NO MODIFIER")) : GameState->GetRoundModifierText().ToUpper();
		return FString::Printf(TEXT("POWER %s  TASKS %s  PRESENCE %.0f  %s"),
			*BreakerReadout,
			*StationReadout,
			FMath::Clamp(GameState->PresenceLevel, 0.0f, 100.0f),
			*ModifierText);
	}

	FString BuildHudAuxLine(const ABHGameState* GameState)
	{
		if (!GameState)
		{
			return TEXT("");
		}

		if (GameState->bRevisionMode)
		{
			return GameState->RevisionReviewTimeRemaining > 0
				? FString::Printf(TEXT("REVIEW %ds / %s"), GameState->RevisionReviewTimeRemaining, *GameState->RevisionReviewText)
				: FString::Printf(TEXT("WEAK TOPIC %s / %s"), *FBHRevisionQuestionBank::TopicToString(GameState->RevisionWeakTopic), *GameState->PresenceText);
		}

		if (GameState->RoundPhase == EBHRoundPhase::Intermission && !GameState->TrainAnnouncement.IsEmpty())
		{
			return ClampHudLine(FString::Printf(TEXT("%s / %s"), *GameState->TrainAnnouncement, *TrainPhaseNextBeat(GameState->TrainPhase)), 116);
		}

		if (GameState->RoundPhase == EBHRoundPhase::FinalEscape)
		{
			const FString Line = GameState->FinalEscapeState == EBHFinalEscapeState::Cutscene
				? FString(TEXT("Evacuation doors unlocking. Get ready to sprint to any green doorway."))
				: FString(TEXT("Use any green train door. Teacher camping near a door weakens capture pressure."));
			return ClampHudLine(Line, 116);
		}

		return TEXT("");
	}

	bool HasNearbyPathThreat(UWorld* World, const ABHCharacter* Character, float& OutThreatAlpha)
	{
		OutThreatAlpha = 0.0f;
		if (!World || !Character)
		{
			return false;
		}

		const ABHPlayerState* LocalPS = Character->GetBHPlayerState();
		if (!LocalPS || !LocalPS->IsAliveSurvivor())
		{
			return false;
		}

		FVector ViewLocation = Character->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
		FRotator ViewRotation = Character->GetActorRotation();
		Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);

		const FVector CharacterLocation = Character->GetActorLocation();
		constexpr float NearbyHunterRange = 1650.0f;
		constexpr float VisibleHunterRange = 2600.0f;
		constexpr float MonsterRange = 3200.0f;
		bool bFoundThreat = false;

		auto RegisterThreat = [&](float Distance, float Range)
		{
			bFoundThreat = true;
			OutThreatAlpha = FMath::Max(OutThreatAlpha, 1.0f - FMath::Clamp(Distance / FMath::Max(1.0f, Range), 0.0f, 1.0f));
		};

		auto HasLocalSightTo = [&](const ABHCharacter* OtherCharacter)
		{
			if (!OtherCharacter)
			{
				return false;
			}

			const FVector ThreatLocation = OtherCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDPathThreatLOS), false);
			Params.AddIgnoredActor(Character);
			Params.AddIgnoredActor(OtherCharacter);

			FHitResult Hit;
			return !World->LineTraceSingleByChannel(Hit, ViewLocation, ThreatLocation, ECC_Visibility, Params);
		};

		for (TActorIterator<ABHCharacter> It(World); It; ++It)
		{
			const ABHCharacter* OtherCharacter = *It;
			const ABHPlayerState* OtherPS = OtherCharacter ? OtherCharacter->GetBHPlayerState() : nullptr;
			if (!OtherCharacter || OtherCharacter == Character || !IsAlivePathThreat(OtherPS))
			{
				continue;
			}

			const float Distance = FVector::Dist2D(OtherCharacter->GetActorLocation(), CharacterLocation);
			const bool bVeryNear = Distance <= NearbyHunterRange;
			const bool bVisibleNear = Distance <= VisibleHunterRange && HasLocalSightTo(OtherCharacter);
			if (bVeryNear || bVisibleNear)
			{
				RegisterThreat(Distance, bVeryNear ? NearbyHunterRange : VisibleHunterRange);
			}
		}

		for (TActorIterator<ABHJumpscareMonster> It(World); It; ++It)
		{
			const ABHJumpscareMonster* Monster = *It;
			if (!Monster)
			{
				continue;
			}

			const float Distance = FVector::Dist2D(Monster->GetActorLocation(), CharacterLocation);
			if (Distance <= MonsterRange)
			{
				RegisterThreat(Distance, MonsterRange);
			}
		}

		if (bFoundThreat)
		{
			OutThreatAlpha = FMath::Clamp(OutThreatAlpha, 0.22f, 1.0f);
		}
		return bFoundThreat;
	}

	struct FBHTeacherProximityReadout
	{
		bool bFound = false;
		bool bLineOfSight = false;
		FVector TeacherLocation = FVector::ZeroVector;
		float DistanceCm = 0.0f;
		float ProximityPercent = 0.0f;
	};

	FBHTeacherProximityReadout FindTeacherProximity(UWorld* World, const ABHCharacter* Character)
	{
		FBHTeacherProximityReadout Readout;
		if (!World || !Character)
		{
			return Readout;
		}

		const ABHPlayerState* LocalPS = Character->GetBHPlayerState();
		if (!LocalPS || !LocalPS->IsAliveSurvivor())
		{
			return Readout;
		}

		FVector ViewLocation = Character->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
		FRotator ViewRotation = Character->GetActorRotation();
		Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);

		const FVector CharacterLocation = Character->GetActorLocation();
		constexpr float TeacherSignalRange = HudTeacherSignalRangeCm;
		float BestDistance = TeacherSignalRange;
		bool bBestLineOfSight = false;

		auto HasLineOfSightTo = [&](const ABHCharacter* OtherCharacter)
		{
			if (!OtherCharacter)
			{
				return false;
			}

			const FVector ThreatLocation = OtherCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDTeacherProximityLOS), false);
			Params.AddIgnoredActor(Character);
			Params.AddIgnoredActor(OtherCharacter);

			FHitResult Hit;
			return !World->LineTraceSingleByChannel(Hit, ViewLocation, ThreatLocation, ECC_Visibility, Params);
		};

		for (TActorIterator<ABHCharacter> It(World); It; ++It)
		{
			const ABHCharacter* OtherCharacter = *It;
			const ABHPlayerState* OtherPS = OtherCharacter ? OtherCharacter->GetBHPlayerState() : nullptr;
			if (!OtherCharacter || OtherCharacter == Character || !IsAliveTeacherThreat(OtherPS))
			{
				continue;
			}

			const float Distance = FVector::Dist2D(OtherCharacter->GetActorLocation(), CharacterLocation);
			if (Distance <= BestDistance)
			{
				BestDistance = Distance;
				bBestLineOfSight = HasLineOfSightTo(OtherCharacter);
				Readout.TeacherLocation = OtherCharacter->GetActorLocation();
				Readout.bFound = true;
			}
		}

		if (Readout.bFound)
		{
			const float BaseSignal = 1.0f - FMath::Clamp(BestDistance / TeacherSignalRange, 0.0f, 1.0f);
			Readout.bLineOfSight = bBestLineOfSight;
			Readout.DistanceCm = BestDistance;
			Readout.ProximityPercent = FMath::Clamp((BaseSignal + (bBestLineOfSight ? 0.12f : 0.0f)) * 100.0f, 4.0f, 100.0f);
		}
		return Readout;
	}
}

ABHHUD::ABHHUD()
{
	LastSeenPhase = EBHRoundPhase::Lobby;
	bHasSeenPhase = false;
	PhaseBannerEndTime = 0.0f;
	bTaughtFear = false;
	bTaughtStamina = false;
	bTaughtTeacherNear = false;
	bTaughtQuestionFormat = false;
	bTaughtGuideAccess = false;
	bTaughtSpectator = false;
	LastSeenPresencePulse = 0;
	PresencePulseEndTime = 0.0f;
	bHasVisibleHunterCue = false;
	LastVisibleHunterLocation = FVector::ZeroVector;
	LastVisibleHunterDistanceCm = 0.0f;
	VisibleHunterCueUntilTime = 0.0f;
	SmoothedVisibleHunterArrowX = 0.0f;
}

void ABHHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const APlayerController* PC = PlayerOwner;
	const ABHPlayerController* BHPC = PC ? Cast<ABHPlayerController>(PC) : nullptr;
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	ABHCharacter* Character = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
	const bool bShowSurvivorWarnings = BHPS && BHPS->IsAliveSurvivor();
	const bool bHighContrastHud = BHPC && BHPC->IsHighContrastHudEnabled();

	// Resolve cached UI preferences (palette, scale, opacity, element toggles) once per frame.
	RefreshHudPreferences(BHPC, BHGS);

	// In-session Guide discoverability (#8): the how-to-play Guide is already reachable in-match
	// (Esc -> Guide tab), but a brand-new student may not know it exists. Point them to it once, in
	// the Lobby before the round starts. One client-side status line; never during a chase.
	if (!bTaughtGuideAccess && BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby)
	{
		bTaughtGuideAccess = true;
		if (ABHPlayerController* TeachPC = Cast<ABHPlayerController>(PlayerOwner))
		{
			TeachPC->ShowLocalStatusMessage(TEXT("New here? Press Esc and open the GUIDE tab for a 60-second how-to-play."), 6.0f);
		}
	}

	// Spectator / late-joiner orientation (#14): someone watching a live round (died, or joined
	// mid-round) has no context. Orient them once - what to watch and how to ask for a role next time.
	if (!bTaughtSpectator && BHPS && BHPS->PlayerRole == EBHPlayerRole::Spectator && BHGS && BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		bTaughtSpectator = true;
		if (ABHPlayerController* TeachPC = Cast<ABHPlayerController>(PlayerOwner))
		{
			TeachPC->ShowLocalStatusMessage(TEXT("You're spectating. Watch how survivors work stations and break the Teacher's line of sight. H cheers; T/Y/U request a role next round."), 6.0f);
		}
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (BHGS)
	{
		if (!bHasSeenPhase)
		{
			PhaseBannerEndTime = BHGS->RoundPhase == EBHRoundPhase::Lobby ? 0.0f : Now + 3.2f;
			LastSeenPhase = BHGS->RoundPhase;
			bHasSeenPhase = true;
		}
		else if (LastSeenPhase != BHGS->RoundPhase)
		{
			LastSeenPhase = BHGS->RoundPhase;
			PhaseBannerEndTime = Now + 3.6f;
		}

		if (LastSeenPresencePulse != BHGS->PresencePulse)
		{
			LastSeenPresencePulse = BHGS->PresencePulse;
			PresencePulseEndTime = Now + 0.75f;
		}
	}

	// Always run the horror overlay: the explicit scare cues (flash, blink, face image, directive banner)
	// must render for whoever a cue targeted, even a hunter/tester firing scares from Test Commands. The
	// fear/dread ambient vignette inside is gated to alive survivors.
	DrawHorrorOverlay(Character, BHGS);

	const float SafePad = FMath::Max(18.0f, Canvas->ClipX * 0.014f);
	// Independent HUD sizes: TextScale grows readable text/labels; WidgetScale grows the chrome (meter
	// bars, minimap, equipment) plus the readout row spacing so stacked lines never overlap when enlarged.
	const float TextScale = HudTextScale;
	const float WidgetScale = HudWidgetScale;
	const float ReadoutW = FMath::Clamp(Canvas->ClipX * 0.38f, 280.0f, 560.0f) * TextScale;
	if (BHGS && BHGS->bTutorialMode)
	{
		// Tutorial: the DrawTutorialPrompt banner is the sole instruction channel, so suppress the generic match
		// telemetry (timer/exit pill + action/detail/aux lines). It otherwise contradicts the guided lesson - e.g.
		// "FIND A LIVE BREAKER / HOLD E" during the flashlight step, or "EXIT OPEN" while the lesson is still at a
		// station. The taught vitals stack (battery/stamina/fear/dread) is drawn separately and stays.
	}
	else if (BHGS)
	{
		const FString TimerText = BHGS->bTestMode ? FString(TEXT("TEST LOOP")) : (BHGS->bPracticeMode ? FString(TEXT("PRACTICE")) : FString::Printf(TEXT("T-%s"), *FormatClock(BHGS->RemainingTime)));
		const FString ExitText = BHGS->bExitUnlocked ? FString(TEXT("EXIT OPEN")) : FString(TEXT("EXIT LOCKED"));
		const FString ActionLine = BuildHudActionLine(GetWorld(), BHGS, BHPS, Character).ToUpper();
		const FString DetailLine = BuildHudDetailLine(BHGS, BHPS).ToUpper();
		const FString AuxLine = BuildHudAuxLine(BHGS).ToUpper();
		const FLinearColor ExitColor = BHGS->bExitUnlocked ? ActivePalette.Good : ActivePalette.Bad;
		DrawHudText(FString::Printf(TEXT("%s / %s"), *TimerText, *ExitText), SafePad, SafePad, ExitColor, GEngine->GetSmallFont(), 0.88f * TextScale);
		DrawWrappedHudText(ActionLine, SafePad, SafePad + 22.0f * TextScale, ReadoutW, bHighContrastHud ? FLinearColor(0.92f, 0.98f, 0.90f, 1.0f) : FLinearColor(0.84f, 0.80f, 0.70f, 0.90f), GEngine->GetSmallFont(), 0.70f * TextScale, 13.0f * TextScale, 2);
		DrawWrappedHudText(DetailLine, SafePad, SafePad + 55.0f * TextScale, ReadoutW, bHighContrastHud ? FLinearColor(0.78f, 0.92f, 0.88f, 0.96f) : FLinearColor(0.72f, 0.68f, 0.60f, 0.92f), GEngine->GetSmallFont(), 0.60f * TextScale, 12.0f * TextScale, 1);
		if (!AuxLine.IsEmpty())
		{
			DrawWrappedHudText(AuxLine, SafePad, SafePad + 73.0f * TextScale, ReadoutW, bHighContrastHud ? FLinearColor(0.84f, 0.86f, 0.68f, 0.92f) : FLinearColor(0.80f, 0.70f, 0.60f, 0.88f), GEngine->GetSmallFont(), 0.56f * TextScale, 12.0f * TextScale, 1);
		}
	}
	else
	{
		DrawHudText(TEXT("NO SIGNAL"), SafePad, SafePad, FLinearColor(0.96f, 0.24f, 0.16f, 0.92f), GEngine->GetSmallFont(), 0.90f * TextScale);
		DrawHudText(TEXT("HOST OR JOIN"), SafePad, SafePad + 21.0f * TextScale, FLinearColor(0.72f, 0.68f, 0.60f, 0.88f), GEngine->GetSmallFont(), 0.64f * TextScale);
	}

	if (BHPS)
	{
		const UEnum* RoleEnum = StaticEnum<EBHPlayerRole>();
		const UEnum* LifeEnum = StaticEnum<EBHPlayerLifeState>();
		FString RoleName = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(BHPS->PlayerRole)) : TEXT("Unassigned");
		if (BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			RoleName = TEXT("Teacher");
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			RoleName = TEXT("Hall Monitor");
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			RoleName = TEXT("Tester");
		}
		const FString LifeName = LifeEnum ? LifeEnum->GetNameStringByValue(static_cast<int64>(BHPS->LifeState)) : TEXT("Alive");
		const FString ReadyText = (BHGS && BHGS->bTestMode) ? TEXT("TEST") : ((BHGS && BHGS->bPracticeMode) ? TEXT("LAB") : (BHPS->bReady ? TEXT("READY") : TEXT("NOT READY")));
		DrawRightAlignedText(RoleName.ToUpper(), Canvas->ClipX - SafePad, SafePad, FLinearColor(0.88f, 0.84f, 0.74f, 0.90f), GEngine->GetSmallFont(), 0.82f * TextScale);
		DrawRightAlignedText(FString::Printf(TEXT("%s / %s / AV%02d"), *LifeName.ToUpper(), *ReadyText, BHPS->AvatarIndex + 1), Canvas->ClipX - SafePad, SafePad + 20.0f * TextScale, FLinearColor(0.66f, 0.62f, 0.56f, 0.90f), GEngine->GetSmallFont(), 0.58f * TextScale);
		if ((BHGS && BHGS->bTestMode) || BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			// Right-pinned wrap box: widen by text size, then recompute X from the scaled width so it stays on-screen.
			const float ShortcutW = FMath::Clamp(Canvas->ClipX * 0.34f, 310.0f, 540.0f) * TextScale;
			DrawWrappedHudText(TEXT("TEST KEYS  INS RESET  HOME TRAIN  PGUP PHASE  END STATION  PGDN ESCAPE  DEL RECAP"),
				Canvas->ClipX - SafePad - ShortcutW,
				SafePad + 42.0f * TextScale,
				ShortcutW,
				FLinearColor(0.95f, 0.86f, 0.42f, 0.80f),
				GEngine->GetSmallFont(),
				0.50f * TextScale,
				11.0f * TextScale,
				2);
		}
		else if (Character && BHPS->IsAliveHunter())
		{
			FString CaptureReadout = TEXT("AXE READY");
			FLinearColor CaptureColor(0.68f, 0.92f, 0.62f, 0.88f);
			if (Character->IsTeacherCaptureAttackActive())
			{
				CaptureReadout = Character->IsTeacherCaptureAttackInWindup() ? TEXT("AXE WINDUP") : TEXT("AXE RECOVERY");
				CaptureColor = Character->IsTeacherCaptureAttackInWindup()
					? FLinearColor(1.0f, 0.62f, 0.24f, 0.94f)
					: FLinearColor(0.96f, 0.42f, 0.30f, 0.88f);
			}
			else if (Character->GetTeacherCaptureCooldownRemaining() > 0.0f)
			{
				CaptureReadout = FString::Printf(TEXT("AXE %ds"), FMath::CeilToInt(Character->GetTeacherCaptureCooldownRemaining()));
				CaptureColor = FLinearColor(0.82f, 0.62f, 0.42f, 0.82f);
			}

			DrawRightAlignedText(CaptureReadout, Canvas->ClipX - SafePad, SafePad + 42.0f * TextScale, CaptureColor, GEngine->GetSmallFont(), 0.66f * TextScale);
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter && !(BHGS && BHGS->bTutorialMode))
		{
			// In the Monitor tutorial the prompt channel narrates the tools step-by-step ("Press Q to send a REAL
			// hint", etc.); the persistent "Q REAL R MARK G TRAP" pill would contradict the "revise to unlock tools"
			// framing, so suppress it and let the lesson drive.
			const float MonitorW = FMath::Clamp(Canvas->ClipX * 0.30f, 260.0f, 440.0f) * TextScale;
			FString MonitorLine;
			if (BHGS && BHGS->bRevisionMode)
			{
				const int32 ContributionTarget = EstimateRevisionContributionTarget(BHGS);
				MonitorLine = BHPS->RevisionStats.ContributionCount < ContributionTarget
					? FString::Printf(TEXT("TOOLS LOCKED / JOIN ANSWER TEAM %d/%d"), BHPS->RevisionStats.ContributionCount, ContributionTarget)
					: FString::Printf(TEXT("Q REAL  R FALSE MARK  G TRAP / CONTRIBUTIONS %d/%d"), BHPS->RevisionStats.ContributionCount, ContributionTarget);
			}
			else
			{
				MonitorLine = TEXT("Q REAL  R AIMED MARK  G TRAP / NO CAPTURE");
			}
			DrawWrappedHudText(MonitorLine,
				Canvas->ClipX - SafePad - MonitorW,
				SafePad + 42.0f * TextScale,
				MonitorW,
				FLinearColor(0.95f, 0.70f, 0.32f, 0.82f),
				GEngine->GetSmallFont(),
				0.50f * TextScale,
				11.0f * TextScale,
				2);
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Spectator && BHGS && BHGS->RoundPhase != EBHRoundPhase::Lobby)
		{
			DrawSpectatorSupportPanel(BHGS, BHPS);
		}
	}

	// Lobby-only: show who is in (humans + bots, ready state) so a small group can fill out the
	// roster with bots and start once everyone's ready.
	if (BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby)
	{
		DrawLobbyRoster(BHGS, BHPS);
	}

	if (Character)
	{
		// Easter egg ("the faculty remembers"): a one-time, client-local nod when the local player's name
		// matches a famous physicist. Pure cosmetic status line; never affects gameplay. Gated by bh.EasterEggs.
		if (!bShownPhysicistGreeting && BHAreEasterEggsEnabled())
		{
			const ABHPlayerState* GreetPS = Character->GetBHPlayerState();
			const FString LowerName = GreetPS ? GreetPS->GetPlayerName().ToLower() : FString();
			if (!LowerName.IsEmpty())
			{
				bShownPhysicistGreeting = true; // resolved once the name has replicated; never re-scans
				FString PhysicistLine;
				if (LowerName == TEXT("newton")) { PhysicistLine = TEXT("Somewhere, an apple falls in your honour."); }
				else if (LowerName == TEXT("einstein")) { PhysicistLine = TEXT("Time runs a little strangely around you."); }
				else if (LowerName == TEXT("curie")) { PhysicistLine = TEXT("You glow faintly in the dark. (It's fine.)"); }
				else if (LowerName == TEXT("tesla")) { PhysicistLine = TEXT("The lights flicker when you arrive. They like you."); }
				else if (LowerName == TEXT("feynman")) { PhysicistLine = TEXT("There's plenty of room at the bottom -- of that locker."); }
				else if (LowerName == TEXT("bohr")) { PhysicistLine = TEXT("Classically, your exact position is uncertain."); }
				else if (LowerName == TEXT("schrodinger")) { PhysicistLine = TEXT("You are both hidden and found until someone opens the locker."); }
				else if (LowerName == TEXT("hawking")) { PhysicistLine = TEXT("Even the dark radiates, given enough time."); }
				else if (LowerName == TEXT("galileo")) { PhysicistLine = TEXT("And yet, the exit moves."); }
				else if (LowerName == TEXT("faraday")) { PhysicistLine = TEXT("You induce a quiet current of dread in the Teacher."); }
				else if (LowerName == TEXT("maxwell")) { PhysicistLine = TEXT("A small demon sorts the fast students from the slow."); }
				else if (LowerName == TEXT("planck")) { PhysicistLine = TEXT("Reality is grainier than it looks. So is this building."); }
				else if (LowerName == TEXT("lovelace")) { PhysicistLine = TEXT("You compute the escape before the Teacher takes a step."); }
				else if (LowerName == TEXT("noether")) { PhysicistLine = TEXT("Every symmetry hides something the dark can't take from you."); }
				else if (LowerName == TEXT("hertz")) { PhysicistLine = TEXT("Your footsteps oscillate at a frequency only the building hears."); }
				else if (LowerName == TEXT("joule")) { PhysicistLine = TEXT("Every step you take does honest work."); }
				else if (LowerName == TEXT("ohm")) { PhysicistLine = TEXT("You resist. The current of fear flows around you."); }
				else if (LowerName == TEXT("volta")) { PhysicistLine = TEXT("There's a potential between you and the exit. Close it."); }
				else if (LowerName == TEXT("ampere")) { PhysicistLine = TEXT("A steady current of nerve runs through you."); }
				else if (LowerName == TEXT("kelvin")) { PhysicistLine = TEXT("Absolute zero is colder. Tonight, not by much."); }
				else if (LowerName == TEXT("rutherford")) { PhysicistLine = TEXT("Most of this building is empty space. Use it."); }
				else if (LowerName == TEXT("heisenberg")) { PhysicistLine = TEXT("The surer you are of where you're going, the less of how fast."); }
				else if (LowerName == TEXT("pascal")) { PhysicistLine = TEXT("The pressure is equal in every direction. Even here."); }
				else if (LowerName == TEXT("doppler")) { PhysicistLine = TEXT("Footsteps drop in pitch as the Teacher passes you by."); }
				else if (LowerName == TEXT("42") || LowerName == TEXT("adams")) { PhysicistLine = TEXT("The answer is 42. The question is which locker."); }
				else if (LowerName == TEXT("pi")) { PhysicistLine = TEXT("You go on forever and never quite repeat."); }
				if (!PhysicistLine.IsEmpty())
				{
					if (ABHPlayerController* GreetPC = Cast<ABHPlayerController>(PlayerOwner))
					{
						GreetPC->ShowLocalStatusMessage(PhysicistLine, 5.0f);
					}
					// Cosmetic achievement: unlocks the "Chalk" tint.
					if (UWorld* World = GetWorld())
					{
						if (UBHAccountSubsystem* Account = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
						{
							Account->UnlockAchievement(FName(TEXT("honorary_faculty")));
						}
					}
				}
			}
		}

		// Achievement: hid in a locker (a gameplay achievement, so NOT gated by the cosmetic easter-egg toggle).
		// UnlockAchievement is idempotent, so the per-frame call no-ops after the first hide.
		if (Character->IsHiddenInLocker())
		{
			if (UWorld* World = GetWorld())
			{
				if (UBHAccountSubsystem* Account = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
				{
					Account->UnlockAchievement(FName(TEXT("spelunker")));
				}
			}
		}

		// Easter egg ("scratched into the locker"): while hidden, a faint message someone left in the wood.
		// Stable per-locker (hashed from the spot you're hiding in) so each locker keeps its own "graffiti".
		// Cosmetic; visible only to you, only while concealed. Gated by bh.EasterEggs.
		if (Character->IsHiddenInLocker() && BHAreEasterEggsEnabled())
		{
			static const TCHAR* LockerScratches[] = {
				TEXT("scratched here: \"g = 9.81. it still pulls, even down here.\""),
				TEXT("carved deep: \"sound needs a medium. screams travel anyway.\""),
				TEXT("a faded note: \"the quieter you are, the louder it gets.\""),
				TEXT("etched small: \"F = ma. fear has mass too.\""),
				TEXT("shaky letters: \"I counted to 96. don't.\""),
				TEXT("someone wrote: \"revise. it's the only way out.\""),
				TEXT("a student carved: \"the train always comes. eventually.\""),
				TEXT("barely visible: \"light has no mass, but it still leaves.\""),
				TEXT("cut with a key: \"the answer is 42. find the question.\""),
				TEXT("tiny letters: \"Schrodinger hid here. or did he?\""),
				TEXT("worn smooth: \"energy is conserved. courage isn't.\""),
				TEXT("fresh marks: \"you are not the first to wait in the dark.\""),
				TEXT("deep grooves: \"momentum is conserved. so are grudges.\""),
				TEXT("a tally of 13: \"period 5. never came back.\""),
				TEXT("pencil, faint: \"the exit is a wave. catch it at the crest.\""),
				TEXT("scratched twice: \"P = IV. pay the voltage, pass the test.\"")
			};
			const FVector HideSpot = Character->GetActorLocation();
			const int32 ScratchIndex = FMath::Abs(FMath::FloorToInt(HideSpot.X) * 73 + FMath::FloorToInt(HideSpot.Y) * 31) % static_cast<int32>(UE_ARRAY_COUNT(LockerScratches));
			DrawWrappedHudText(LockerScratches[ScratchIndex], Canvas->ClipX * 0.5f - 230.0f * TextScale, Canvas->ClipY * 0.60f, 460.0f * TextScale, FLinearColor(0.62f, 0.58f, 0.52f, 0.42f), GEngine->GetSmallFont(), 0.52f * TextScale, 12.0f * TextScale, 2);
		}

		// Probe teacher proximity once -- it feeds both the vitals meter and the threat arrow,
		// so it stays outside the per-element visibility gates below.
		const FBHTeacherProximityReadout TeacherProximity = FindTeacherProximity(GetWorld(), Character);

		if (bShowVitals)
		{
			// Bar widths follow widget size; the stack height (anchor + row steps) follows max(widget,text)
			// so the bottom-left panel stays fully on-screen and rows clear the taller of bar-or-label.
			const float MeterStackScale = FMath::Max(WidgetScale, TextScale);
			// Bar width follows max(widget,text) so the text-scaled label + readout that share the line above
			// each bar always have room (prevents collision when text is large but widget is small).
			const float MeterW = FMath::Clamp(Canvas->ClipX * 0.22f, 210.0f, 310.0f) * MeterStackScale;
			const float VitalsY = Canvas->ClipY - SafePad - 132.0f * MeterStackScale;
			const FString VitalsTitle = Character->IsDetentionMarked()
				? FString::Printf(TEXT("MARKED %.0fs"), Character->GetDetentionMarkRemaining())
				: (Character->IsHiddenInLocker() ? FString(TEXT("CONCEALED")) : FString(TEXT("STATUS")));
			DrawHudText(VitalsTitle.ToUpper(), SafePad, VitalsY - 19.0f * MeterStackScale, Character->IsDetentionMarked() ? FLinearColor(1.0f, 0.20f, 0.12f, 0.96f) : FLinearColor(0.76f, 0.72f, 0.64f, 0.84f), GEngine->GetSmallFont(), 0.66f * TextScale);
			// BATTERY + STAMINA are relevant to every role (the Teacher has a flashlight/blackout and sprints),
			// but TEACHER proximity / FEAR / DREAD are survivor-only and sat dead at 0 for the Teacher / Hall
			// Monitor (most visible in their tutorials). Gate those three by survivor role and re-flow the panel
			// with a running Y so a non-survivor sees a compact BATTERY+STAMINA panel with no empty gaps. A
			// survivor still gets the exact original layout.
			DrawProgressBar(TEXT("BATTERY"), Character->GetFlashlightBattery(), SafePad, VitalsY, MeterW, FLinearColor(0.80f, 0.82f, 0.70f, 0.88f));
			float MeterY = VitalsY + 32.0f * MeterStackScale;
			if (bShowSurvivorWarnings)
			{
				const FString TeacherText = TeacherProximity.bFound
					? FString::Printf(TEXT("%s %.0fm"), TeacherProximity.bLineOfSight ? TEXT("VISIBLE") : TEXT("NEAR"), TeacherProximity.DistanceCm / 100.0f)
					: FString(TEXT("CLEAR"));
				DrawProgressBar(TEXT("TEACHER"), TeacherProximity.ProximityPercent, SafePad, MeterY, MeterW, FLinearColor(0.90f, 0.36f, 0.22f, 0.90f), TeacherText);
				MeterY += 36.0f * MeterStackScale;
			}
			DrawRawMeter(TEXT("STAMINA"), Character->GetStaminaPercent(), SafePad, MeterY, MeterW, FLinearColor(0.75f, 0.83f, 0.54f, 0.88f), false);
			MeterY += 18.0f * MeterStackScale;
			if (bShowSurvivorWarnings)
			{
				DrawRawMeter(TEXT("FEAR"), Character->GetFear(), SafePad, MeterY, MeterW, FLinearColor(0.92f, 0.28f, 0.20f, 0.88f), true);
				MeterY += 18.0f * MeterStackScale;
				DrawRawMeter(TEXT("DREAD"), Character->GetDread(), SafePad, MeterY, MeterW, FLinearColor(0.84f, 0.18f, 0.14f, 0.90f), true);
			}
			FString StressHint;
			if (bShowSurvivorWarnings && (Character->GetFear() >= HudFearPanicHintThreshold || Character->GetDread() >= HudDreadPanicHintThreshold))
			{
				StressHint = TEXT("PANIC NOISE: SLOW DOWN OR BREAK LINE OF SIGHT");
			}
			else if (bShowSurvivorWarnings && Character->GetDread() >= HudDreadHintThreshold)
			{
				StressHint = TEXT("DREAD: TASKS SLOW AND HIDING GETS RISKY");
			}
			else if (bShowSurvivorWarnings && Character->GetFear() >= HudFearHintThreshold)
			{
				StressHint = TEXT("FEAR: SPRINTING GETS LOUDER");
			}
			if (!StressHint.IsEmpty())
			{
				DrawWrappedHudText(StressHint, SafePad, VitalsY + 122.0f * MeterStackScale, MeterW, FLinearColor(0.96f, 0.42f, 0.30f, 0.88f), GEngine->GetSmallFont(), 0.48f * TextScale, 10.0f * TextScale, 1);
			}

			// First-time "why" coaching (SHARED-D): the first time each meter is *noticed* (a low
			// threshold, well below the panic cutoffs above), explain the cause->effect once, then
			// defer to the steady-state StressHint. Suppressed while the Teacher is visible so it
			// never adds reading load mid-chase. Pure client-side; fires one short status line.
			if (bShowSurvivorWarnings && !(TeacherProximity.bFound && TeacherProximity.bLineOfSight))
			{
				if (ABHPlayerController* TeachPC = Cast<ABHPlayerController>(PlayerOwner))
				{
					if (!bTaughtTeacherNear && TeacherProximity.bFound && TeacherProximity.ProximityPercent >= 34.0f)
					{
						bTaughtTeacherNear = true;
						TeachPC->ShowLocalStatusMessage(TEXT("The TEACHER bar shows how close the Teacher is. When it climbs, hide in a locker or break line of sight."), 4.5f);
					}
					else if (!bTaughtFear && Character->GetFear() >= 30.0f)
					{
						bTaughtFear = true;
						TeachPC->ShowLocalStatusMessage(TEXT("FEAR rises near danger - and high Fear makes you louder when you sprint. Move calmly to stay quiet."), 4.5f);
					}
					else if (!bTaughtStamina && Character->GetStaminaPercent() <= 34.0f)
					{
						bTaughtStamina = true;
						TeachPC->ShowLocalStatusMessage(TEXT("STAMINA drains when you sprint or vault. Let it recover so you can run when it counts."), 4.5f);
					}
				}
			}
		}

		if (!bShowSurvivorWarnings)
		{
			bHasVisibleHunterCue = false;
			SmoothedVisibleHunterArrowX = 0.0f;
		}
		else if (TeacherProximity.bFound && TeacherProximity.bLineOfSight)
		{
			bHasVisibleHunterCue = true;
			LastVisibleHunterLocation = TeacherProximity.TeacherLocation;
			LastVisibleHunterDistanceCm = TeacherProximity.DistanceCm;
			VisibleHunterCueUntilTime = Now + 0.34f;
		}

		if (bHasVisibleHunterCue)
		{
			const float CueStrength = FMath::Clamp((VisibleHunterCueUntilTime - Now) / 0.34f, 0.0f, 1.0f);
			if (CueStrength > 0.02f)
			{
				if (bShowThreatArrow)
				{
					DrawVisibleHunterArrow(Character, LastVisibleHunterLocation, LastVisibleHunterDistanceCm, CueStrength);
				}
			}
			else
			{
				bHasVisibleHunterCue = false;
				SmoothedVisibleHunterArrowX = 0.0f;
			}
		}
	}

	float PathThreatAlpha = 0.0f;
	const bool bPathDetected = bShowSurvivorWarnings && Character && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && HasNearbyPathThreat(GetWorld(), Character, PathThreatAlpha);

	// The minimap toggle hides the heat sensor, but a detected path threat still forces it
	// up as a safety reveal regardless of the preference.
	if (Character && BHGS && BHGS->RoundPhase != EBHRoundPhase::Lobby && BHPC && ((BHPC->IsHudMapVisible() && bShowMinimap) || bPathDetected))
	{
		// Minimap box follows widget size. MapW and the bottom reserve mirror DrawHeatSensor's internal
		// PanelW/PanelH (same widget-scaled clamps) so the right-edge anchor and the bottom margin track the
		// real scaled panel and it stays fully on-screen.
		const float MapW = FMath::Clamp(Canvas->ClipX * 0.26f, 280.0f, 380.0f) * WidgetScale;
		const float MapPanelH = FMath::Clamp(Canvas->ClipY * 0.38f, 250.0f, 360.0f) * WidgetScale;
		const float MapX = Canvas->ClipX - SafePad - MapW;
		const float MaxMapY = FMath::Max(SafePad + 44.0f, Canvas->ClipY - SafePad - MapPanelH);
		const float MapY = FMath::Clamp(Canvas->ClipY * 0.17f, SafePad + 44.0f, MaxMapY);
		DrawHeatSensor(Character, BHGS, MapX, MapY);
	}

	DrawCCTVRevealMarker(Character, BHPC);
	DrawNodeMarker(Character, BHPC);
	if (bShowObjectiveBeats)
	{
		DrawObjectiveBeats(Character, BHGS);
	}

	const float WarningLevel = (bShowSurvivorWarnings && bShowCrosshairDanger) ? FMath::Max(Character ? Character->GetDread() : 0.0f, BHGS ? BHGS->PresenceLevel : 0.0f) : 0.0f;
	const float DangerAlpha = FMath::Clamp(WarningLevel / 100.0f, 0.0f, 1.0f);
	DrawCrosshair(DangerAlpha);
	if (bShowNameplates)
	{
		DrawNearbyNameTags(Character);
	}
	DrawInteractionPrompt(Character);

	if (bPathDetected)
	{
		const FString PulseText = TEXT("PATH DETECTED");
		const float PulseScale = FMath::Lerp(1.02f, 1.22f, PathThreatAlpha) * TextScale;
		const float PulseY = Canvas->ClipY * 0.53f;
		const FLinearColor PulseColor(1.0f, 0.18f, 0.12f, FMath::Lerp(0.80f, 0.98f, PathThreatAlpha));
		float PulseW = 0.0f;
		float PulseH = 0.0f;
		Canvas->TextSize(GEngine->GetSmallFont(), PulseText, PulseW, PulseH, PulseScale, PulseScale);
		const float PulseX = (Canvas->ClipX - PulseW) * 0.5f;
		DrawHudText(PulseText, PulseX + 1.0f, PulseY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), GEngine->GetSmallFont(), PulseScale);
		DrawHudText(PulseText, PulseX, PulseY, PulseColor, GEngine->GetSmallFont(), PulseScale);
		DrawLine(PulseX + PulseW * 0.10f, PulseY + PulseH + 4.0f, PulseX + PulseW * 0.90f, PulseY + PulseH + 5.0f, FLinearColor(1.0f, 0.10f, 0.05f, FMath::Lerp(0.45f, 0.85f, PathThreatAlpha)), 2.0f);
	}

	if (BHPC)
	{
		if (BHPC->HasActiveStatusMessage())
		{
			const FString& Status = BHPC->GetStatusMessage();
			const float StatusAlpha = BHPC->GetStatusMessageAlpha();
			// Centred text panel: scale box + text together by max(widget,text). Measure at ToastScale (not the
			// 0.92 draw scale) so at defaults (ToastScale==1) the box matches the original 1.0-measured width
			// pixel-for-pixel, and it still grows proportionally to fit enlarged text.
			const float ToastScale = FMath::Max(WidgetScale, TextScale);
			const float ToastTextScale = 0.92f * ToastScale;
			float TextW = 0.0f;
			float TextH = 0.0f;
			Canvas->TextSize(GEngine->GetSmallFont(), Status, TextW, TextH, ToastScale, ToastScale);
			const float ToastW = FMath::Clamp(TextW + 48.0f * ToastScale, 260.0f * ToastScale, Canvas->ClipX * 0.62f);
			const float ToastX = (Canvas->ClipX - ToastW) * 0.5f;
			const bool bLongStatus = TextW > ToastW - 48.0f * ToastScale;
			const float ToastH = (bLongStatus ? 66.0f : 48.0f) * ToastScale;
			const float ToastY = Canvas->ClipY * 0.72f + (1.0f - StatusAlpha) * 10.0f;
			const FLinearColor ToastFill = bHighContrastHud ? FLinearColor(0.0f, 0.0f, 0.0f, 0.92f) : FLinearColor(0.020f, 0.020f, 0.018f, 0.84f);
			const FLinearColor ToastAccent = bHighContrastHud ? FLinearColor(1.0f, 0.96f, 0.42f, 1.0f) : FLinearColor(0.96f, 0.74f, 0.36f, 0.94f);
			const FLinearColor ToastText = bHighContrastHud ? FLinearColor(1.0f, 0.98f, 0.82f, 1.0f) : FLinearColor(0.96f, 0.87f, 0.62f, 1.0f);
			DrawPanel(ToastX, ToastY, ToastW, ToastH, WithAlpha(ToastFill, StatusAlpha), WithAlpha(ToastAccent, StatusAlpha));
			DrawWrappedHudText(Status, ToastX + 24.0f * ToastScale, ToastY + 15.0f * ToastScale, ToastW - 48.0f * ToastScale, WithAlpha(ToastText, StatusAlpha), GEngine->GetSmallFont(), ToastTextScale, 17.0f * ToastScale, 2);
		}
	}

	DrawMinigameStatus(Character);
	DrawRoofPrompt(Character);

	DrawPhaseBanner(BHGS, Character);
	DrawRoleIntroCard(BHPC, BHGS);
	DrawDiagramPreview();
	// Tutorial guidance gets its own top banner so the shared status toast (noise/round alerts) can't cut it off.
	DrawTutorialPrompt(BHPC);
	// Drawn last so the transition snapshot sits on top of the whole HUD.
	DrawTutorialCard(BHPC);
}

void ABHHUD::DrawMinigameStatus(ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !Character)
	{
		return;
	}
	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	if (!BHPS)
	{
		return;
	}
	AActor* Table = BHPS->GetActiveMinigameTable();
	if (!Table)
	{
		return;
	}
	// Hide the readout once the player walks away from the table they last sat at.
	if (FVector::Dist(Character->GetActorLocation(), Table->GetActorLocation()) > 750.0f)
	{
		return;
	}

	TArray<FString> Lines;
	FLinearColor Accent(0.70f, 0.85f, 1.0f, 1.0f);
	if (ABHTrainBlackjackTable* Blackjack = Cast<ABHTrainBlackjackTable>(Table))
	{
		Blackjack->GetHudLinesForPlayer(BHPS->GetPlayerId(), Lines, Accent);
	}
	else if (ABHTrainChessTable* Chess = Cast<ABHTrainChessTable>(Table))
	{
		Chess->GetHudBoardLines(BHPS->GetPlayerId(), Lines, Accent);
	}
	else if (ABHTrainTicTacToeTable* TicTacToe = Cast<ABHTrainTicTacToeTable>(Table))
	{
		TicTacToe->GetHudLinesForPlayer(BHPS->GetPlayerId(), Lines, Accent);
	}
	else if (ABHTrainConnectFourTable* ConnectFour = Cast<ABHTrainConnectFourTable>(Table))
	{
		ConnectFour->GetHudLinesForPlayer(BHPS->GetPlayerId(), Lines, Accent);
	}
	else if (ABHTrainSlotMachine* Slots = Cast<ABHTrainSlotMachine>(Table))
	{
		Slots->GetHudLinesForPlayer(BHPS->GetPlayerId(), Lines, Accent);
	}
	else if (ABHTrainOthelloTable* Othello = Cast<ABHTrainOthelloTable>(Table))
	{
		Othello->GetHudLinesForPlayer(BHPS->GetPlayerId(), Lines, Accent);
	}
	else if (ABHTrainDartboard* Dartboard = Cast<ABHTrainDartboard>(Table))
	{
		Dartboard->GetHudLinesForPlayer(BHPS->GetPlayerId(), Lines, Accent);
	}
	if (Lines.Num() == 0)
	{
		return;
	}

	// Monospace layout (fixed per-character advance) so the chess board columns align under a proportional font.
	const UFont* Font = GEngine->GetSmallFont();
	const float Scale = HudTextScale;
	float CharW = 0.0f;
	float CharH = 0.0f;
	Canvas->TextSize(Font, TEXT("W"), CharW, CharH, Scale, Scale);
	if (CharW <= 0.0f)
	{
		return;
	}
	const float LineH = CharH * 1.15f;

	int32 MaxLen = 0;
	for (const FString& Line : Lines)
	{
		MaxLen = FMath::Max(MaxLen, Line.Len());
	}

	const float Pad = 16.0f * Scale;
	const float SafePad = FMath::Max(18.0f, Canvas->ClipX * 0.014f);
	const float PanelW = MaxLen * CharW + Pad * 2.0f;
	const float PanelH = Lines.Num() * LineH + Pad * 2.0f;
	const float PanelX = SafePad;
	const float PanelY = Canvas->ClipY * 0.96f - PanelH;   // bottom-left, clear of the centre crosshair/prompt

	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.02f, 0.03f, 0.05f, 0.86f), FLinearColor(Accent.R, Accent.G, Accent.B, 0.95f));

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		const FString& Line = Lines[LineIndex];
		const FLinearColor Color = (LineIndex == 0) ? Accent : FLinearColor(0.92f, 0.95f, 0.99f, 1.0f);
		const float Y = PanelY + Pad + LineIndex * LineH;
		for (int32 CharIndex = 0; CharIndex < Line.Len(); ++CharIndex)
		{
			DrawHudText(FString::Chr(Line[CharIndex]), PanelX + Pad + CharIndex * CharW, Y, Color, Font, Scale);
		}
	}
}

void ABHHUD::DrawRoofPrompt(ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !Character)
	{
		return;
	}
	// The walkable roof slab top is Z=316 and a standing player inside is ~212, so Z>320 cleanly means "on the
	// roof". O is already bound to ResetToTrain (-> RequestResetToTrainInterior), which teleports back inside.
	if (Character->GetActorLocation().Z <= 320.0f)
	{
		return;
	}

	const FString Message = TEXT("Press  O  to return to the cabin");
	const UFont* Font = GEngine->GetLargeFont() ? GEngine->GetLargeFont() : GEngine->GetSmallFont();
	const float Scale = FMath::Max(HudWidgetScale, HudTextScale);
	float TextW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(Font, Message, TextW, TextH, Scale, Scale);

	const float PanelW = TextW + 56.0f * Scale;
	const float PanelH = TextH + 28.0f * Scale;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY * 0.16f;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.03f, 0.04f, 0.06f, 0.88f), FLinearColor(1.0f, 0.82f, 0.34f, 0.96f));
	DrawHudText(Message, PanelX + 28.0f * Scale, PanelY + 14.0f * Scale, FLinearColor(1.0f, 0.94f, 0.62f, 1.0f), Font, Scale);
}

FLinearColor ABHHUD::MainText() const
{
	return ActivePalette.TextMain;
}

FLinearColor ABHHUD::MutedText() const
{
	return ActivePalette.TextMuted;
}

void ABHHUD::RefreshHudPreferences(const ABHPlayerController* PlayerController, const ABHGameState* GameState)
{
	// Defaults keep the HUD unchanged when no controller/preferences are available.
	HudWidgetScale = 1.0f;
	HudTextScale = 1.0f;
	HudPanelOpacity = 1.0f;
	bColorblindPalette = false;
	bShowMinimap = true;
	bShowNameplates = true;
	bShowVitals = true;
	bShowObjectiveBeats = true;
	bShowThreatArrow = true;
	bShowCrosshairDanger = true;

	bool bHighContrast = false;
	if (PlayerController)
	{
		HudWidgetScale = FMath::Clamp(PlayerController->GetHudScale(), 0.75f, 1.5f);
		HudTextScale = FMath::Clamp(PlayerController->GetHudTextScale(), 0.80f, 1.6f);
		HudPanelOpacity = FMath::Clamp(PlayerController->GetHudPanelOpacity(), 0.35f, 1.0f);
		bColorblindPalette = PlayerController->IsColorblindHudEnabled();
		bHighContrast = PlayerController->IsHighContrastHudEnabled();
		bShowMinimap = PlayerController->IsHudMinimapVisible();
		bShowNameplates = PlayerController->IsHudNameplatesVisible();
		bShowVitals = PlayerController->IsHudVitalsVisible();
		bShowObjectiveBeats = PlayerController->IsHudObjectiveBeatsVisible();
		bShowThreatArrow = PlayerController->IsHudThreatArrowVisible();
		bShowCrosshairDanger = PlayerController->IsHudCrosshairDangerVisible();
	}

	const FString LevelName = (GameState && GameState->GetWorld())
		? GameState->GetWorld()->GetMapName()
		: (GetWorld() ? GetWorld()->GetMapName() : FString());
	// Strip the PIE prefix (e.g. "UEDPIE_0_") so map-name matching works in editor play.
	FString CleanLevel = LevelName;
	int32 PrefixEnd = INDEX_NONE;
	if (CleanLevel.StartsWith(TEXT("UEDPIE_")) && CleanLevel.FindChar(TEXT('_'), PrefixEnd))
	{
		// Skip the "UEDPIE_<n>_" prefix entirely.
		const int32 SecondUnderscore = CleanLevel.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromStart, PrefixEnd + 1);
		if (SecondUnderscore != INDEX_NONE)
		{
			CleanLevel = CleanLevel.RightChop(SecondUnderscore + 1);
		}
	}

	const FLinearColor MapAccent = BHHudTheme::MapAccentTintForLevel(CleanLevel);
	const BHHudTheme::EPaletteMode Mode = bColorblindPalette
		? BHHudTheme::EPaletteMode::Colorblind
		: BHHudTheme::EPaletteMode::Standard;
	ActivePalette = BHHudTheme::ResolveActivePalette(Mode, MapAccent, bHighContrast);
}

void ABHHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& FillColorIn, const FLinearColor& AccentColorIn)
{
	if (!Canvas || W <= 1.0f || H <= 1.0f)
	{
		return;
	}

	// Panel opacity preference scales the fill/accent alpha once; every derived shade below
	// is expressed as a fraction of these, so they fade together while text stays readable.
	const FLinearColor FillColor = WithAlpha(FillColorIn, HudPanelOpacity);
	const FLinearColor AccentColor = WithAlpha(AccentColorIn, HudPanelOpacity);

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.42f), X + 6.0f, Y + 7.0f, W, H);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.26f), X + 2.0f, Y + 2.0f, W, H);
	DrawRect(FillColor, X, Y, W, H);

	const float HeaderH = FMath::Min(24.0f, H * 0.30f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, FillColor.A * 0.045f), X + 2.0f, Y + 1.0f, W - 4.0f, HeaderH);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.24f), X + 2.0f, Y + HeaderH, W - 4.0f, 1.0f);
	DrawRect(FLinearColor(0.84f, 0.96f, 0.93f, FillColor.A * 0.18f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.40f), X, Y + H - 1.0f, W, 1.0f);
	DrawRect(AccentColor, X, Y, 3.0f, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, AccentColor.A * 0.18f), X + 3.0f, Y, 16.0f, H);

	for (float ScanY = Y + HeaderH + 8.0f; ScanY < Y + H - 4.0f; ScanY += 9.0f)
	{
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.055f), X + 5.0f, ScanY, W - 10.0f, 1.0f);
	}

	DrawCornerBrackets(X + 5.0f, Y + 5.0f, W - 10.0f, H - 10.0f, FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, AccentColor.A * 0.75f), 12.0f, 1.25f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.52f), X + W - 9.0f, Y + 7.0f, 3.0f, 3.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.52f), X + W - 9.0f, Y + H - 10.0f, 3.0f, 3.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, FillColor.A * 0.18f), X + W - 8.0f, Y + 8.0f, 1.0f, 1.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, FillColor.A * 0.18f), X + W - 8.0f, Y + H - 9.0f, 1.0f, 1.0f);
}

void ABHHUD::DrawCornerBrackets(float X, float Y, float W, float H, const FLinearColor& Color, float Length, float Thickness)
{
	if (!Canvas || W <= 1.0f || H <= 1.0f)
	{
		return;
	}

	const float ClampedLength = FMath::Clamp(Length, 4.0f, FMath::Min(W, H) * 0.45f);
	DrawLine(X, Y, X + ClampedLength, Y, Color, Thickness);
	DrawLine(X, Y, X, Y + ClampedLength, Color, Thickness);
	DrawLine(X + W, Y, X + W - ClampedLength, Y, Color, Thickness);
	DrawLine(X + W, Y, X + W, Y + ClampedLength, Color, Thickness);
	DrawLine(X, Y + H, X + ClampedLength, Y + H, Color, Thickness);
	DrawLine(X, Y + H, X, Y + H - ClampedLength, Color, Thickness);
	DrawLine(X + W, Y + H, X + W - ClampedLength, Y + H, Color, Thickness);
	DrawLine(X + W, Y + H, X + W, Y + H - ClampedLength, Color, Thickness);
}

void ABHHUD::DrawCircle(float CenterX, float CenterY, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	if (!Canvas || Radius <= 0.0f)
	{
		return;
	}

	const int32 ClampedSegments = FMath::Clamp(Segments, 8, 96);
	FVector2D Previous(CenterX + Radius, CenterY);
	for (int32 Index = 1; Index <= ClampedSegments; ++Index)
	{
		const float Angle = (static_cast<float>(Index) / static_cast<float>(ClampedSegments)) * 2.0f * PI;
		const FVector2D Current(CenterX + FMath::Cos(Angle) * Radius, CenterY + FMath::Sin(Angle) * Radius);
		DrawLine(Previous.X, Previous.Y, Current.X, Current.Y, Color, Thickness);
		Previous = Current;
	}
}

void ABHHUD::DrawKeyBox(const FString& Key, float X, float Y, float W, float H, const FLinearColor& AccentColor, bool bLit)
{
	if (!Canvas || !GEngine || Key.IsEmpty())
	{
		return;
	}

	const FLinearColor BackColor = bLit ? FLinearColor(0.045f, 0.058f, 0.060f, 0.92f) : FLinearColor(0.025f, 0.030f, 0.033f, 0.76f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.54f), X + 2.0f, Y + 2.0f, W, H);
	DrawRect(BackColor, X, Y, W, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.74f : 0.26f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.50f : 0.18f), X, Y + H - 1.0f, W, 1.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f), X, Y, 1.0f, H);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f), X + W - 1.0f, Y, 1.0f, H);

	// The key cap letter tracks the box, which callers size with the uniform popup scale max(widget,text);
	// centering uses the measured size so it stays centred at any scale.
	const float KeyScale = 0.76f * FMath::Max(HudWidgetScale, HudTextScale);
	float TextW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(GEngine->GetSmallFont(), Key, TextW, TextH, KeyScale, KeyScale);
	DrawHudText(Key, X + (W - TextW) * 0.5f, Y + (H - TextH) * 0.5f - 1.0f, bLit ? ActivePalette.KeyBoxLit : ActivePalette.KeyBoxDim, GEngine->GetSmallFont(), KeyScale);
}

void ABHHUD::DrawStatusPill(const FString& Label, float X, float Y, float W, const FLinearColor& AccentColor, bool bLit)
{
	if (!Canvas || !GEngine || Label.IsEmpty())
	{
		return;
	}

	// Pill box scales with widget size; the label scales with text size (independent axes).
	const float GS = HudWidgetScale;
	const float T = HudTextScale;
	const float H = 21.0f * GS;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.34f), X + 2.0f * GS, Y + 2.0f * GS, W, H);
	DrawRect(FLinearColor(0.028f, 0.034f, 0.035f, 0.88f), X, Y, W, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.28f : 0.10f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.90f : 0.34f), X + 8.0f * GS, Y + 7.0f * GS, 7.0f * GS, 7.0f * GS);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.18f : 0.05f), X + 6.0f * GS, Y + 5.0f * GS, 11.0f * GS, 11.0f * GS);
	DrawWrappedHudText(Label, X + 24.0f * GS, Y + 5.0f * GS, W - 29.0f * GS, bLit ? MainText() : ActivePalette.TextMuted, GEngine->GetSmallFont(), 0.60f * T, 10.0f * T, 1);
}

void ABHHUD::DrawHudText(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font, float Scale) const
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}

	const UFont* DrawFont = Font ? Font : (GEngine ? GEngine->GetSmallFont() : nullptr);
	if (!DrawFont)
	{
		return;
	}

	Canvas->SetDrawColor(FLinearColor(0.0f, 0.0f, 0.0f, Color.A * 0.72f).ToFColor(true));
	Canvas->DrawText(DrawFont, Text, X + 1.0f, Y + 1.0f, Scale, Scale);
	Canvas->SetDrawColor(Color.ToFColor(true));
	Canvas->DrawText(DrawFont, Text, X, Y, Scale, Scale);
}

void ABHHUD::DrawRightAlignedText(const FString& Text, float RightX, float Y, const FLinearColor& Color, const UFont* Font, float Scale) const
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}

	const UFont* DrawFont = Font ? Font : (GEngine ? GEngine->GetSmallFont() : nullptr);
	if (!DrawFont)
	{
		return;
	}

	float TextW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(DrawFont, Text, TextW, TextH, Scale, Scale);
	DrawHudText(Text, RightX - TextW, Y, Color, DrawFont, Scale);
}

float ABHHUD::DrawWrappedHudText(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, const UFont* Font, float Scale, float LineHeight, int32 MaxLines, bool bCenterEachLine) const
{
	if (!Canvas || Text.IsEmpty() || MaxWidth <= 1.0f || MaxLines <= 0)
	{
		return 0.0f;
	}

	const UFont* DrawFont = Font ? Font : (GEngine ? GEngine->GetSmallFont() : nullptr);
	if (!DrawFont)
	{
		return 0.0f;
	}

	FString Sanitized = Text;
	Sanitized.ReplaceInline(TEXT("\r"), TEXT(" "));
	Sanitized.ReplaceInline(TEXT("\n"), TEXT(" "));

	TArray<FString> Words;
	Sanitized.ParseIntoArray(Words, TEXT(" "), true);
	if (Words.IsEmpty())
	{
		return 0.0f;
	}

	TArray<FString> Lines;
	FString CurrentLine;
	bool bTruncated = false;
	for (int32 WordIndex = 0; WordIndex < Words.Num(); ++WordIndex)
	{
		const FString Candidate = CurrentLine.IsEmpty() ? Words[WordIndex] : CurrentLine + TEXT(" ") + Words[WordIndex];
		float CandidateW = 0.0f;
		float CandidateH = 0.0f;
		Canvas->TextSize(DrawFont, Candidate, CandidateW, CandidateH, Scale, Scale);
		if (CandidateW <= MaxWidth || CurrentLine.IsEmpty())
		{
			CurrentLine = Candidate;
			continue;
		}

		Lines.Add(CurrentLine);
		CurrentLine = Words[WordIndex];
		if (Lines.Num() >= MaxLines)
		{
			bTruncated = true;
			break;
		}
	}

	if (!bTruncated && !CurrentLine.IsEmpty() && Lines.Num() < MaxLines)
	{
		Lines.Add(CurrentLine);
	}
	else if (!CurrentLine.IsEmpty() && Lines.Num() >= MaxLines)
	{
		bTruncated = true;
	}

	if (bTruncated && !Lines.IsEmpty() && !Lines.Last().EndsWith(TEXT("...")))
	{
		FString& LastLine = Lines.Last();
		while (LastLine.Len() > 3)
		{
			const FString Candidate = LastLine + TEXT("...");
			float CandidateW = 0.0f;
			float CandidateH = 0.0f;
			Canvas->TextSize(DrawFont, Candidate, CandidateW, CandidateH, Scale, Scale);
			if (CandidateW <= MaxWidth)
			{
				LastLine = Candidate;
				break;
			}
			LastLine = LastLine.LeftChop(1);
		}
	}

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		float LineX = X;
		if (bCenterEachLine && Canvas)
		{
			// Centre each wrapped line within [X, X+MaxWidth] so multi-line captions read as a tidy centred block
			// instead of a ragged left-aligned stack.
			float LineW = 0.0f;
			float LineH = 0.0f;
			Canvas->TextSize(DrawFont, Lines[LineIndex], LineW, LineH, Scale, Scale);
			LineX = X + FMath::Max(0.0f, (MaxWidth - LineW) * 0.5f);
		}
		DrawHudText(Lines[LineIndex], LineX, Y + LineIndex * LineHeight, Color, DrawFont, Scale);
	}

	return Lines.Num() * LineHeight;
}

void ABHHUD::DrawProgressBar(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, const FString& ValueText)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	// Bar geometry scales with widget size; the label/readout scale with text size (independent axes).
	const float GS = HudWidgetScale;
	const float T = HudTextScale;
	const float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	const float BarH = 9.0f * GS;
	const float BarY = Y + 13.0f * GS;
	const float FillW = W * (ClampedValue / 100.0f);
	const bool bTeacherSignal = Label.Contains(TEXT("TEACHER"));
	const bool bTeacherVisible = bTeacherSignal && ValueText.Contains(TEXT("VISIBLE"));
	const bool bWarnLow = (Label.Contains(TEXT("BATTERY")) || Label.Contains(TEXT("STAMINA"))) && ClampedValue <= HudLowMeterWarningThreshold;
	const bool bWarnHigh = (Label.Contains(TEXT("FEAR")) || Label.Contains(TEXT("DREAD")) || Label.Contains(TEXT("PRESENCE")) || bTeacherSignal) && (bTeacherVisible || ClampedValue >= (bTeacherSignal ? HudTeacherSignalWarningThreshold : HudHighMeterWarningThreshold));
	const bool bWarning = bWarnLow || bWarnHigh;
	const ABHPlayerController* BHPC = PlayerOwner ? Cast<ABHPlayerController>(PlayerOwner) : nullptr;
	const bool bHighContrast = BHPC && BHPC->IsHighContrastHudEnabled();
	const FString RightText = ValueText.IsEmpty() ? FString::Printf(TEXT("%.0f%%"), ClampedValue) : ValueText;
	const FLinearColor LabelColor = bHighContrast ? FLinearColor(0.90f, 0.98f, 0.94f, 1.0f) : MutedText();
	const FLinearColor ReadoutColor = bWarning ? ActivePalette.WarnHot : LabelColor;
	DrawHudText(Label, X, Y - 4.0f, LabelColor, GEngine->GetSmallFont(), 0.66f * T);
	DrawRightAlignedText(RightText, X + W, Y - 4.0f, ReadoutColor, GEngine->GetSmallFont(), 0.66f * T);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, bHighContrast ? 0.64f : 0.42f), X, BarY + 1.0f, W, BarH);
	DrawRect(FLinearColor(0.020f, 0.026f, 0.028f, bHighContrast ? 1.0f : 0.94f), X, BarY, W, BarH);
	DrawRect(FLinearColor(0.82f, 0.95f, 0.90f, bHighContrast ? 0.22f : 0.10f), X, BarY, W, 1.0f);
	if (FillW > 0.5f)
	{
		const FLinearColor EffectiveFill = bWarning ? FLinearColor(1.0f, 0.28f, 0.18f, 0.96f) : FillColor;
		DrawRect(FLinearColor(EffectiveFill.R, EffectiveFill.G, EffectiveFill.B, EffectiveFill.A * 0.20f), X, BarY - 2.0f, FillW, BarH + 4.0f);
		DrawRect(EffectiveFill, X, BarY, FillW, BarH);
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, EffectiveFill.A * 0.28f), X, BarY, FillW, 1.0f);
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, EffectiveFill.A * 0.38f), X + FMath::Max(0.0f, FillW - 2.0f), BarY - 1.0f, 2.0f, BarH + 2.0f);
	}
	for (int32 Segment = 1; Segment < 10; ++Segment)
	{
		const float SegmentX = X + W * (static_cast<float>(Segment) / 10.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.34f), SegmentX, BarY, 1.0f, BarH);
	}
	if (bWarnLow || bWarnHigh)
	{
		const float Pulse = GetWorld() ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 7.0f) : 1.0f;
		DrawRect(FLinearColor(1.0f, 0.18f, 0.10f, 0.22f + Pulse * 0.20f), X, BarY - 2.0f, W, BarH + 4.0f);
	}
}

void ABHHUD::DrawVisibleHunterArrow(const ABHCharacter* Character, const FVector& HunterLocation, float DistanceCm, float CueStrength)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character)
	{
		return;
	}

	const float Strength = FMath::Clamp(CueStrength, 0.0f, 1.0f);
	if (Strength <= 0.01f)
	{
		return;
	}

	FVector2D ScreenPosition(Canvas->ClipX * 0.5f, 0.0f);
	const FVector MarkerLocation = HunterLocation + FVector(0.0f, 0.0f, 124.0f);
	const bool bProjectedOnScreen = PlayerOwner->ProjectWorldLocationToScreen(MarkerLocation, ScreenPosition, true)
		&& ScreenPosition.X >= 0.0f
		&& ScreenPosition.X <= Canvas->ClipX
		&& ScreenPosition.Y >= 0.0f
		&& ScreenPosition.Y <= Canvas->ClipY;
	if (!bProjectedOnScreen)
	{
		const FVector ToHunter = (HunterLocation - Character->GetActorLocation()).GetSafeNormal2D();
		const float Side = FVector::DotProduct(ToHunter, Character->GetActorRightVector());
		ScreenPosition.X = Canvas->ClipX * (0.5f + FMath::Clamp(Side, -1.0f, 1.0f) * 0.34f);
	}

	const float EdgePadX = FMath::Min(FMath::Clamp(Canvas->ClipX * 0.18f, 130.0f, 260.0f), Canvas->ClipX * 0.42f);
	const float TargetArrowX = FMath::Clamp(ScreenPosition.X, EdgePadX, Canvas->ClipX - EdgePadX);
	if (SmoothedVisibleHunterArrowX <= 0.0f || SmoothedVisibleHunterArrowX < EdgePadX || SmoothedVisibleHunterArrowX > Canvas->ClipX - EdgePadX || FMath::Abs(SmoothedVisibleHunterArrowX - TargetArrowX) > Canvas->ClipX * 0.36f)
	{
		SmoothedVisibleHunterArrowX = TargetArrowX;
	}
	else
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 1.0f / 60.0f;
		SmoothedVisibleHunterArrowX = FMath::FInterpTo(SmoothedVisibleHunterArrowX, TargetArrowX, DeltaSeconds, 14.0f);
	}
	const float ArrowX = SmoothedVisibleHunterArrowX;
	const float DistanceAlpha = 1.0f - FMath::Clamp(DistanceCm / HudTeacherSignalRangeCm, 0.0f, 1.0f);
	const float Pulse = GetWorld() ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.0f) : 1.0f;
	const float CueAlpha = FMath::Lerp(0.74f, 0.98f, DistanceAlpha) * Strength;
	const float ArrowY = FMath::Max(17.0f, Canvas->ClipY * 0.020f) + Pulse * 1.5f;
	const float ArrowH = FMath::Clamp(Canvas->ClipY * 0.025f, 17.0f, 26.0f);
	const float ArrowW = ArrowH * 0.62f;
	const float TailH = FMath::Clamp(Canvas->ClipY * 0.011f, 7.0f, 11.0f);

	const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.76f * Strength);
	const FLinearColor ArrowColor(1.0f, 0.11f, 0.05f, CueAlpha);
	const FLinearColor HotColor(1.0f, 0.32f, 0.18f, FMath::Clamp(CueAlpha + Pulse * 0.10f * Strength, 0.0f, 1.0f));

	if (bProjectedOnScreen && ScreenPosition.Y > Canvas->ClipY * 0.10f && ScreenPosition.Y < Canvas->ClipY * 0.88f)
	{
		const float BracketW = FMath::Lerp(20.0f, 34.0f, DistanceAlpha);
		const float BracketH = FMath::Lerp(24.0f, 44.0f, DistanceAlpha);
		const float MarkerX = FMath::Clamp(ScreenPosition.X - BracketW * 0.5f, 8.0f, Canvas->ClipX - BracketW - 8.0f);
		const float MarkerY = FMath::Clamp(ScreenPosition.Y - BracketH * 0.42f, Canvas->ClipY * 0.10f, Canvas->ClipY - BracketH - 18.0f);
		const float MarkerPulse = 0.5f + 0.5f * FMath::Sin((GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) * 6.5f);
		DrawCornerBrackets(MarkerX + 1.0f, MarkerY + 1.0f, BracketW, BracketH, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 8.0f, 2.4f);
		DrawCornerBrackets(MarkerX, MarkerY, BracketW, BracketH, FLinearColor(1.0f, 0.10f, 0.04f, CueAlpha), 8.0f, 1.8f);
		DrawCircle(ScreenPosition.X, ScreenPosition.Y, FMath::Lerp(6.0f, 10.0f, DistanceAlpha) + MarkerPulse * 1.5f, FLinearColor(1.0f, 0.12f, 0.05f, (0.18f + MarkerPulse * 0.12f) * Strength), 1.2f, 24);
		DrawLine(MarkerX + BracketW * 0.28f, MarkerY + BracketH + 4.0f, MarkerX + BracketW * 0.72f, MarkerY + BracketH + 4.0f, FLinearColor(1.0f, 0.16f, 0.08f, (0.42f + MarkerPulse * 0.20f) * Strength), 1.6f);
	}

	DrawRect(FLinearColor(1.0f, 0.04f, 0.02f, (0.12f + Pulse * 0.10f) * Strength), ArrowX - ArrowW * 2.1f, 0.0f, ArrowW * 4.2f, 3.0f);
	DrawLine(ArrowX - ArrowW * 1.65f, ArrowY - 2.0f, ArrowX - ArrowW * 0.65f, ArrowY - 2.0f, FLinearColor(1.0f, 0.08f, 0.04f, 0.30f * Strength), 1.5f);
	DrawLine(ArrowX + ArrowW * 0.65f, ArrowY - 2.0f, ArrowX + ArrowW * 1.65f, ArrowY - 2.0f, FLinearColor(1.0f, 0.08f, 0.04f, 0.30f * Strength), 1.5f);
	DrawLine(ArrowX + 1.0f, ArrowY + ArrowH + 1.0f, ArrowX - ArrowW + 1.0f, ArrowY + 1.0f, ShadowColor, 4.5f);
	DrawLine(ArrowX + 1.0f, ArrowY + ArrowH + 1.0f, ArrowX + ArrowW + 1.0f, ArrowY + 1.0f, ShadowColor, 4.5f);
	DrawLine(ArrowX + 1.0f, ArrowY + ArrowH + 1.0f, ArrowX + 1.0f, ArrowY + ArrowH + TailH + 1.0f, ShadowColor, 3.5f);
	DrawLine(ArrowX, ArrowY + ArrowH, ArrowX - ArrowW, ArrowY, ArrowColor, 3.0f);
	DrawLine(ArrowX, ArrowY + ArrowH, ArrowX + ArrowW, ArrowY, ArrowColor, 3.0f);
	DrawLine(ArrowX, ArrowY + ArrowH, ArrowX, ArrowY + ArrowH + TailH, HotColor, 2.0f);

	const FString Label = FString::Printf(TEXT("TEACHER VISIBLE %.0fm"), DistanceCm / 100.0f);
	float TextW = 0.0f;
	float TextH = 0.0f;
	const float TextScale = 0.58f * HudTextScale;
	Canvas->TextSize(GEngine->GetSmallFont(), Label, TextW, TextH, TextScale, TextScale);
	const float TextMaxX = FMath::Max(12.0f, Canvas->ClipX - TextW - 12.0f);
	const float TextX = FMath::Clamp(ArrowX - TextW * 0.5f, 12.0f, TextMaxX);
	const float TextY = ArrowY + ArrowH + TailH + 5.0f;
	DrawHudText(Label, TextX + 1.0f, TextY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f * Strength), GEngine->GetSmallFont(), TextScale);
	DrawHudText(Label, TextX, TextY, FLinearColor(1.0f, 0.34f, 0.22f, CueAlpha), GEngine->GetSmallFont(), TextScale);
}

void ABHHUD::DrawCCTVRevealMarker(const ABHCharacter* Character, const ABHPlayerController* PlayerController)
{
	if (!Canvas || !GEngine || !PlayerOwner || !PlayerController || !PlayerController->HasActiveCCTVReveal())
	{
		return;
	}

	const float Alpha = PlayerController->GetCCTVRevealAlpha();
	if (Alpha <= 0.01f)
	{
		return;
	}

	const FVector RevealLocation = PlayerController->GetCCTVRevealLocation();
	FVector2D ScreenPosition(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const bool bProjected = PlayerOwner->ProjectWorldLocationToScreen(RevealLocation, ScreenPosition, true);
	const bool bOnScreen = bProjected
		&& ScreenPosition.X >= 0.0f
		&& ScreenPosition.X <= Canvas->ClipX
		&& ScreenPosition.Y >= 0.0f
		&& ScreenPosition.Y <= Canvas->ClipY;

	if (!bOnScreen)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector ToReveal = (RevealLocation - ViewLocation).GetSafeNormal();
		const FRotationMatrix ViewMatrix(ViewRotation);
		float Side = FVector::DotProduct(ToReveal, ViewMatrix.GetScaledAxis(EAxis::Y));
		float Vertical = FVector::DotProduct(ToReveal, ViewMatrix.GetScaledAxis(EAxis::Z));
		const float Forward = FVector::DotProduct(ToReveal, ViewMatrix.GetScaledAxis(EAxis::X));
		if (Forward < 0.0f)
		{
			Side = -Side;
			Vertical = -Vertical;
		}
		ScreenPosition.X = Canvas->ClipX * (0.5f + FMath::Clamp(Side, -1.0f, 1.0f) * 0.42f);
		ScreenPosition.Y = Canvas->ClipY * (0.5f - FMath::Clamp(Vertical, -0.85f, 0.85f) * 0.36f);
	}

	const float EdgePad = 24.0f;
	ScreenPosition.X = FMath::Clamp(ScreenPosition.X, EdgePad, Canvas->ClipX - EdgePad);
	ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y, EdgePad + 18.0f, Canvas->ClipY - EdgePad);

	const float DistanceCm = Character ? FVector::Dist(Character->GetActorLocation(), RevealLocation) : 0.0f;
	const float DistanceAlpha = Character ? 1.0f - FMath::Clamp(DistanceCm / 8000.0f, 0.0f, 1.0f) : 0.35f;
	const float Pulse = GetWorld() ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 7.5f) : 0.5f;
	float BracketW = FMath::Lerp(28.0f, 42.0f, DistanceAlpha);
	float BracketH = FMath::Lerp(28.0f, 46.0f, DistanceAlpha);
	if (bOnScreen)
	{
		const ABHCharacter* RevealTarget = PlayerController->GetCCTVRevealTarget();
		if (IsValid(RevealTarget))
		{
			const FBox TargetBounds = RevealTarget->GetComponentsBoundingBox(true);
			if (TargetBounds.IsValid)
			{
				const FVector BoundsCenter = TargetBounds.GetCenter();
				const FVector TopLocation(BoundsCenter.X, BoundsCenter.Y, TargetBounds.Max.Z);
				const FVector BottomLocation(BoundsCenter.X, BoundsCenter.Y, TargetBounds.Min.Z);
				FVector2D TopScreen = ScreenPosition;
				FVector2D BottomScreen = ScreenPosition;
				if (PlayerOwner->ProjectWorldLocationToScreen(TopLocation, TopScreen, true)
					&& PlayerOwner->ProjectWorldLocationToScreen(BottomLocation, BottomScreen, true))
				{
					const float BodyScreenH = FMath::Abs(BottomScreen.Y - TopScreen.Y);
					const float BodyWorldH = FMath::Max(1.0f, TargetBounds.GetSize().Z);
					const float BodyWorldW = FMath::Max(TargetBounds.GetSize().X, TargetBounds.GetSize().Y);
					if (BodyScreenH > 12.0f)
					{
						BracketH = FMath::Clamp(BodyScreenH + 18.0f, 38.0f, Canvas->ClipY * 0.54f);
						BracketW = FMath::Clamp(BodyScreenH * FMath::Clamp(BodyWorldW / BodyWorldH, 0.28f, 0.76f) + 18.0f, 30.0f, 112.0f);
						ScreenPosition.X = FMath::Clamp((TopScreen.X + BottomScreen.X) * 0.5f, EdgePad, Canvas->ClipX - EdgePad);
						ScreenPosition.Y = FMath::Clamp((TopScreen.Y + BottomScreen.Y) * 0.5f, EdgePad + 18.0f, Canvas->ClipY - EdgePad);
					}
				}
			}
		}
	}
	const float MarkerX = FMath::Clamp(ScreenPosition.X - BracketW * 0.5f, 8.0f, Canvas->ClipX - BracketW - 8.0f);
	const float MarkerY = FMath::Clamp(ScreenPosition.Y - BracketH * 0.5f, 44.0f, Canvas->ClipY - BracketH - 18.0f);
	const FString RawTargetName = PlayerController->GetCCTVRevealTargetName();
	const bool bUnverifiedMotion = RawTargetName.Equals(TEXT("MOTION"), ESearchCase::IgnoreCase)
		|| RawTargetName.Contains(TEXT("ECHO"), ESearchCase::IgnoreCase)
		|| RawTargetName.Contains(TEXT("FALSE"), ESearchCase::IgnoreCase);
	const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.68f * Alpha);
	const FLinearColor BaseMarkerColor = bUnverifiedMotion
		? FLinearColor(1.0f, 0.68f, 0.24f, 1.0f)
		: FLinearColor(0.42f, 0.92f, 0.86f, 1.0f);
	const FLinearColor MarkerColor(BaseMarkerColor.R, BaseMarkerColor.G, BaseMarkerColor.B, (0.72f + Pulse * 0.20f) * Alpha);
	const FLinearColor SoftColor(BaseMarkerColor.R, BaseMarkerColor.G, BaseMarkerColor.B, (0.16f + Pulse * 0.10f) * Alpha);

	DrawCornerBrackets(MarkerX + 1.0f, MarkerY + 1.0f, BracketW, BracketH, ShadowColor, 9.0f, 2.4f);
	DrawCornerBrackets(MarkerX, MarkerY, BracketW, BracketH, MarkerColor, 9.0f, 1.8f);
	DrawCircle(ScreenPosition.X, ScreenPosition.Y, 7.0f + Pulse * 2.0f, SoftColor, 1.2f, 24);

	const FString TargetName = RawTargetName.IsEmpty() ? FString(TEXT("SURVIVOR")) : RawTargetName.ToUpper();
	FString Label;
	if (Character)
	{
		Label = bUnverifiedMotion
			? FString::Printf(TEXT("CCTV MOTION %.0fm"), DistanceCm / 100.0f)
			: FString::Printf(TEXT("CCTV LOCK %s %.0fm"), *TargetName.Left(18), DistanceCm / 100.0f);
	}
	else
	{
		Label = bUnverifiedMotion
			? FString(TEXT("CCTV MOTION"))
			: FString::Printf(TEXT("CCTV LOCK %s"), *TargetName.Left(18));
	}
	float TextW = 0.0f;
	float TextH = 0.0f;
	const float TextScale = 0.58f * HudTextScale;
	Canvas->TextSize(GEngine->GetSmallFont(), Label, TextW, TextH, TextScale, TextScale);
	const float TextX = FMath::Clamp(ScreenPosition.X - TextW * 0.5f, 10.0f, FMath::Max(10.0f, Canvas->ClipX - TextW - 10.0f));
	const float TextY = FMath::Clamp(MarkerY + BracketH + 7.0f, 46.0f, Canvas->ClipY - TextH - 12.0f);
	DrawHudText(Label, TextX + 1.0f, TextY + 1.0f, ShadowColor, GEngine->GetSmallFont(), TextScale);
	DrawHudText(Label, TextX, TextY, MarkerColor, GEngine->GetSmallFont(), TextScale);
}

void ABHHUD::DrawNodeMarker(const ABHCharacter* Character, const ABHPlayerController* PlayerController)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character || !PlayerController || !GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= PlayerController->NodeMarkerUntilTime)
	{
		return;
	}
	const float Alpha = FMath::Clamp((PlayerController->NodeMarkerUntilTime - Now) / 4.0f, 0.20f, 1.0f);
	const FLinearColor Color(0.72f, 0.92f, 0.74f, Alpha);
	const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.62f * Alpha);
	FVector2D ScreenLocation;
	if (!PlayerOwner->ProjectWorldLocationToScreen(PlayerController->NodeMarkerLocation + FVector(0.0f, 0.0f, 115.0f), ScreenLocation, true))
	{
		return;
	}
	const float DistanceCm = FVector::Dist2D(PlayerController->NodeMarkerLocation, Character->GetActorLocation());
	const float X = FMath::Clamp(ScreenLocation.X, 34.0f, Canvas->ClipX - 34.0f);
	const float Y = FMath::Clamp(ScreenLocation.Y, 80.0f, Canvas->ClipY - 96.0f);
	DrawLine(X - 7.0f, Y, X + 7.0f, Y, Color, 1.0f);
	DrawLine(X, Y - 7.0f, X, Y + 7.0f, Color, 1.0f);
	const FString Text = FString::Printf(TEXT("NODE %.0fm"), DistanceCm / 100.0f);
	const float Scale = 0.56f * HudTextScale;
	float TextW = 0.0f, TextH = 0.0f;
	Canvas->TextSize(GEngine->GetSmallFont(), Text, TextW, TextH, Scale, Scale);
	const float TextX = FMath::Clamp(X - TextW * 0.5f, 14.0f, Canvas->ClipX - TextW - 14.0f);
	DrawHudText(Text, TextX + 1.0f, Y + 11.0f, ShadowColor, GEngine->GetSmallFont(), Scale);
	DrawHudText(Text, TextX, Y + 10.0f, Color, GEngine->GetSmallFont(), Scale);
}

void ABHHUD::DrawRawMeter(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, bool bHighIsBad)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	// Bar geometry scales with widget size; the inline label column widens by max(widget,text) so the
	// text-size label always fits before the bar; the label itself scales with text size.
	const float GS = HudWidgetScale;
	const float T = HudTextScale;
	const float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	const float LabelW = 58.0f * FMath::Max(GS, T);
	const float BarX = X + LabelW + 8.0f * GS;
	const float BarW = FMath::Max(1.0f, W - LabelW - 8.0f * GS);
	const float BarH = 9.0f * GS;
	const float FillW = BarW * (ClampedValue / 100.0f);
	const bool bWarning = bHighIsBad ? ClampedValue >= HudHighMeterWarningThreshold : ClampedValue <= HudLowMeterWarningThreshold;
	const ABHPlayerController* BHPC = PlayerOwner ? Cast<ABHPlayerController>(PlayerOwner) : nullptr;
	const bool bHighContrast = BHPC && BHPC->IsHighContrastHudEnabled();
	const FLinearColor TextColor = bWarning ? FLinearColor(1.0f, 0.42f, 0.30f, 0.98f) : (bHighContrast ? FLinearColor(0.90f, 0.98f, 0.94f, 1.0f) : FLinearColor(0.62f, 0.70f, 0.68f, 0.88f));
	const FLinearColor EffectiveFill = bWarning ? FLinearColor(1.0f, 0.28f, 0.18f, 0.96f) : FillColor;

	DrawHudText(Label, X, Y - 2.0f, TextColor, GEngine->GetSmallFont(), 0.56f * T);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, bHighContrast ? 0.64f : 0.44f), BarX, Y + 1.0f, BarW, BarH);
	DrawRect(FLinearColor(0.016f, 0.020f, 0.022f, bHighContrast ? 1.0f : 0.90f), BarX, Y, BarW, BarH);
	DrawRect(FLinearColor(0.82f, 0.95f, 0.90f, bHighContrast ? 0.22f : 0.10f), BarX, Y, BarW, 1.0f);
	if (FillW > 0.5f)
	{
		DrawRect(FLinearColor(EffectiveFill.R, EffectiveFill.G, EffectiveFill.B, EffectiveFill.A * 0.18f), BarX, Y - 1.0f, FillW, BarH + 2.0f);
		DrawRect(EffectiveFill, BarX, Y, FillW, BarH);
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, EffectiveFill.A * 0.28f), BarX, Y, FillW, 1.0f);
	}
	for (int32 Segment = 1; Segment < 5; ++Segment)
	{
		const float SegmentX = BarX + BarW * (static_cast<float>(Segment) / 5.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.32f), SegmentX, Y, 1.0f, BarH);
	}
}

void ABHHUD::DrawCrosshair(float DangerAlpha)
{
	if (!Canvas)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const FLinearColor CalmColor(0.78f, 0.76f, 0.66f, 0.46f);
	const FLinearColor ThreatColor(0.96f, 0.16f, 0.10f, 0.84f);
	const float ClampedDanger = FMath::Clamp(DangerAlpha, 0.0f, 1.0f);
	const FLinearColor CrosshairColor = FLinearColor::LerpUsingHSV(CalmColor, ThreatColor, ClampedDanger);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const ABHPlayerController* BHPC = PlayerOwner ? Cast<ABHPlayerController>(PlayerOwner) : nullptr;
	const int32 Style = BHPC ? FMath::Clamp(BHPC->GetCrosshairStyle(), 0, 3) : 0;
	const float JitterX = (FMath::Sin(Now * 23.0f) + FMath::Sin(Now * 7.0f) * 0.35f) * ClampedDanger * 1.85f;
	const float JitterY = (FMath::Cos(Now * 19.0f) + FMath::Sin(Now * 11.0f) * 0.25f) * ClampedDanger * 1.55f;
	const float DrawCenterX = CenterX + JitterX;
	const float DrawCenterY = CenterY + JitterY;
	const float Tremor = FMath::Sin(Now * 37.0f) * ClampedDanger;
	const float Thickness = FMath::Lerp(0.85f, 1.45f, ClampedDanger);
	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.38f);

	auto ScratchLine = [&](float X1, float Y1, float X2, float Y2, float Offset)
	{
		DrawLine(X1 + 1.0f, Y1 + 1.0f, X2 + 1.0f, Y2 + 1.0f, Shadow, Thickness + 0.45f);
		DrawLine(X1, Y1, X2, Y2, CrosshairColor, Thickness);
		if (ClampedDanger > 0.58f)
		{
			DrawLine(X1 + Offset, Y1 - Offset, X2 - Offset * 0.35f, Y2 + Offset * 0.30f, FLinearColor(ThreatColor.R, ThreatColor.G, ThreatColor.B, 0.20f + ClampedDanger * 0.24f), 0.7f);
		}
	};

	switch (Style)
	{
	case 1:
	{
		const float Size = FMath::Lerp(2.0f, 3.5f, ClampedDanger);
		DrawRect(Shadow, DrawCenterX - Size + 1.0f, DrawCenterY - Size + 1.0f, Size * 2.0f, Size * 2.0f);
		DrawRect(CrosshairColor, DrawCenterX - Size * 0.5f, DrawCenterY - Size * 0.5f, Size, Size);
		ScratchLine(DrawCenterX - 8.0f, DrawCenterY + 8.0f + Tremor, DrawCenterX - 3.0f, DrawCenterY + 8.5f, 1.0f);
		break;
	}
	case 2:
	{
		const float Gap = FMath::Lerp(6.0f, 10.0f, ClampedDanger);
		const float Reach = FMath::Lerp(14.0f, 22.0f, ClampedDanger);
		ScratchLine(DrawCenterX - Reach, DrawCenterY - 1.0f, DrawCenterX - Gap, DrawCenterY + Tremor, 1.4f);
		ScratchLine(DrawCenterX + Gap, DrawCenterY - Tremor, DrawCenterX + Reach, DrawCenterY + 1.0f, 1.1f);
		ScratchLine(DrawCenterX - 1.0f, DrawCenterY - Reach, DrawCenterX + Tremor, DrawCenterY - Gap, 0.9f);
		ScratchLine(DrawCenterX + Tremor, DrawCenterY + Gap, DrawCenterX - 1.0f, DrawCenterY + Reach, 1.3f);
		break;
	}
	case 3:
		if (ClampedDanger > 0.28f)
		{
			ScratchLine(DrawCenterX - 5.0f, DrawCenterY, DrawCenterX + 5.0f, DrawCenterY + Tremor, 0.8f);
			ScratchLine(DrawCenterX, DrawCenterY - 5.0f, DrawCenterX + Tremor, DrawCenterY + 5.0f, 0.8f);
		}
		else
		{
			DrawRect(FLinearColor(CrosshairColor.R, CrosshairColor.G, CrosshairColor.B, 0.34f), DrawCenterX - 0.5f, DrawCenterY - 0.5f, 1.0f, 1.0f);
		}
		break;
	case 0:
	default:
	{
		const float Reach = FMath::Lerp(8.0f, 14.0f, ClampedDanger);
		ScratchLine(DrawCenterX - Reach, DrawCenterY - 1.0f, DrawCenterX - 2.0f, DrawCenterY + Tremor, 1.1f);
		ScratchLine(DrawCenterX + 2.0f, DrawCenterY - Tremor, DrawCenterX + Reach * 0.72f, DrawCenterY + 1.0f, 0.9f);
		ScratchLine(DrawCenterX + Tremor, DrawCenterY - Reach * 0.86f, DrawCenterX - 1.0f, DrawCenterY - 2.0f, 1.0f);
		DrawRect(FLinearColor(CrosshairColor.R, CrosshairColor.G, CrosshairColor.B, CrosshairColor.A * 0.62f), DrawCenterX - 1.0f, DrawCenterY - 1.0f, 2.0f, 2.0f);
		break;
	}
	}
}

void ABHHUD::DrawHorrorOverlay(const ABHCharacter* Character, const ABHGameState* GameState)
{
	if (!Canvas)
	{
		return;
	}

	const ABHPlayerState* LocalPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
	// The fear/dread/presence ambient vignette is survivor-only, but the explicit scare cues below
	// (flash, blink, face image, directive banner) render for whoever the cue targeted — including a
	// hunter or tester firing scares from the Test Commands menu.
	const bool bAliveSurvivor = LocalPS && LocalPS->IsAliveSurvivor();

	const float FearAlpha = (bAliveSurvivor && Character) ? FMath::Clamp(Character->GetFear() / 100.0f, 0.0f, 1.0f) : 0.0f;
	const float DreadAlpha = (bAliveSurvivor && Character) ? FMath::Clamp(Character->GetDread() / 100.0f, 0.0f, 1.0f) : 0.0f;
	const float PresenceAlpha = (bAliveSurvivor && GameState) ? FMath::Clamp(GameState->PresenceLevel / 100.0f, 0.0f, 1.0f) : 0.0f;
	const ABHPlayerController* BHPC = Cast<ABHPlayerController>(PlayerOwner);
	const float HorrorFlashAlpha = BHPC ? BHPC->GetHorrorCueFlashAlpha() : 0.0f;
	const float HorrorBlinkAlpha = BHPC ? BHPC->GetHorrorCueBlinkAlpha() : 0.0f;
	UTexture2D* HorrorFaceImage = BHPC ? BHPC->GetHorrorCueFaceImage() : nullptr;
	const float HorrorFaceImageAlpha = BHPC ? BHPC->GetHorrorCueFaceImageAlpha() : 0.0f;
	const FString HorrorDirective = BHPC ? BHPC->GetHorrorDirectiveText() : FString();
	const float HorrorDirectiveAlpha = BHPC ? BHPC->GetHorrorDirectiveAlpha() : 0.0f;
	const bool bHasFaceImage = HorrorFaceImage != nullptr && HorrorFaceImageAlpha > 0.01f;
	const bool bHasDirective = !HorrorDirective.IsEmpty() && HorrorDirectiveAlpha > 0.01f;
	// The tutorial teaches hiding without punishing it: a freshly-hidden student should not feel like the
	// lights "died" on them. Soften the fear/dread/presence vignette to a gentle hint there (and cap it low)
	// so the screen stays readable while they learn lockers, the crawl duct, and the breaker.
	const bool bTutorialSoften = GameState && GameState->bTutorialMode;
	const float OverlayScale = bTutorialSoften ? 0.12f : 0.34f;
	const float OverlayCap = bTutorialSoften ? 0.12f : 0.34f;
	const float OverlayAlpha = FMath::Clamp(FMath::Max(FMath::Max(FearAlpha, DreadAlpha), PresenceAlpha) * OverlayScale, 0.0f, OverlayCap);
	if (OverlayAlpha <= 0.01f && HorrorFlashAlpha <= 0.01f && HorrorBlinkAlpha <= 0.01f && !bHasFaceImage && !bHasDirective)
	{
		return;
	}

	if (OverlayAlpha > 0.01f)
	{
		const float EdgeW = FMath::Clamp(Canvas->ClipX * (0.055f + OverlayAlpha * 0.16f), 42.0f, 210.0f);
		const float EdgeH = FMath::Clamp(Canvas->ClipY * (0.050f + OverlayAlpha * 0.18f), 32.0f, 150.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha), 0.0f, 0.0f, Canvas->ClipX, EdgeH);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha), 0.0f, Canvas->ClipY - EdgeH, Canvas->ClipX, EdgeH);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha * 0.85f), 0.0f, 0.0f, EdgeW, Canvas->ClipY);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha * 0.85f), Canvas->ClipX - EdgeW, 0.0f, EdgeW, Canvas->ClipY);
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float PresencePulseAlpha = FMath::Clamp((PresencePulseEndTime - Now) / 0.75f, 0.0f, 1.0f);
	if (PresencePulseAlpha > 0.0f)
	{
		DrawRect(FLinearColor(0.95f, 0.12f, 0.05f, PresencePulseAlpha * 0.10f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	if (HorrorFlashAlpha > 0.01f && BHPC)
	{
		FLinearColor FlashColor = BHPC->GetHorrorCueFlashColor();
		FlashColor.A = FMath::Clamp(HorrorFlashAlpha * 0.55f, 0.0f, 0.55f);
		DrawRect(FlashColor, 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	// Full-screen "PNG in your face": cover the whole view with the still (centre-cropped, no
	// letterboxing) so it reads as a face slammed into the camera. Scanlines/grain below layer over it.
	if (bHasFaceImage)
	{
		const float ScreenW = Canvas->ClipX;
		const float ScreenH = Canvas->ClipY;
		const float TexW = FMath::Max(1, HorrorFaceImage->GetSizeX());
		const float TexH = FMath::Max(1, HorrorFaceImage->GetSizeY());
		const float CoverScale = FMath::Max(ScreenW / TexW, ScreenH / TexH);
		const float DrawW = TexW * CoverScale;
		const float DrawH = TexH * CoverScale;
		const float DrawX = (ScreenW - DrawW) * 0.5f;
		const float DrawY = (ScreenH - DrawH) * 0.5f;
		// Black backdrop first so any transparent texture edges still read as a full takeover.
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FMath::Clamp(HorrorFaceImageAlpha, 0.0f, 1.0f)), 0.0f, 0.0f, ScreenW, ScreenH);
		DrawTexture(HorrorFaceImage, DrawX, DrawY, DrawW, DrawH, 0.0f, 0.0f, 1.0f, 1.0f,
			FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(HorrorFaceImageAlpha, 0.0f, 1.0f)));
	}

	// Hard black blink drawn last so it briefly punches through everything on the moment of impact.
	if (HorrorBlinkAlpha > 0.01f)
	{
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FMath::Clamp(HorrorBlinkAlpha * 0.92f, 0.0f, 0.92f)), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	if (PresenceAlpha >= 0.55f || DreadAlpha >= 0.65f)
	{
		const float PulseAlpha = FMath::Clamp(FMath::Max(PresenceAlpha, DreadAlpha) * 0.12f, 0.0f, 0.12f);
		DrawRect(FLinearColor(0.72f, 0.02f, 0.02f, PulseAlpha), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	const float ScanlineAlpha = FMath::Clamp(OverlayAlpha * 0.22f + PresencePulseAlpha * 0.035f, 0.0f, 0.09f);
	if (ScanlineAlpha > 0.01f)
	{
		const float Drift = FMath::Fmod(Now * 18.0f, 46.0f);
		for (float Y = 18.0f + Drift; Y < Canvas->ClipY; Y += 46.0f)
		{
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, ScanlineAlpha), 0.0f, Y, Canvas->ClipX, 1.0f);
		}
	}

	const float GrainAlpha = FMath::Clamp(OverlayAlpha * 0.32f + PresencePulseAlpha * 0.05f, 0.0f, 0.12f);
	if (GrainAlpha > 0.012f)
	{
		for (int32 Index = 0; Index < 18; ++Index)
		{
			const float SeedA = FMath::Frac(FMath::Abs(FMath::Sin(Index * 12.9898f + Now * 3.7f)) * 43758.5453f);
			const float SeedB = FMath::Frac(FMath::Abs(FMath::Sin(Index * 78.233f + Now * 2.1f)) * 16421.371f);
			const float SpeckW = 1.0f + FMath::Frac(SeedA * 17.0f) * 3.0f;
			const float SpeckH = 1.0f + FMath::Frac(SeedB * 11.0f) * 18.0f;
			DrawRect(FLinearColor(0.92f, 1.0f, 0.96f, GrainAlpha * (0.20f + SeedB * 0.55f)), SeedA * Canvas->ClipX, SeedB * Canvas->ClipY, SpeckW, SpeckH);
		}
	}

	// Large centered directive banner ("SOMETHING IS BEHIND YOU", "DON'T TURN AROUND", ...), drawn on top
	// of everything. Used by the directive/behind-you scares; pulses red and fades with its own alpha.
	if (bHasDirective)
	{
		const UFont* DirectiveFont = GEngine ? GEngine->GetLargeFont() : nullptr;
		const float DirectiveScale = FMath::Clamp(Canvas->ClipX / 1280.0f, 1.0f, 2.4f) * 1.7f;
		float TextW = 0.0f;
		float TextH = 0.0f;
		Canvas->TextSize(DirectiveFont, HorrorDirective, TextW, TextH, DirectiveScale, DirectiveScale);
		const float TextX = (Canvas->ClipX - TextW) * 0.5f;
		const float TextY = Canvas->ClipY * 0.70f;
		const float DirectivePulse = 0.80f + 0.20f * FMath::Sin(Now * 7.0f);
		DrawHudText(HorrorDirective, TextX + 2.0f, TextY + 2.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.85f * HorrorDirectiveAlpha), DirectiveFont, DirectiveScale);
		DrawHudText(HorrorDirective, TextX, TextY, FLinearColor(0.96f, 0.06f, 0.05f, HorrorDirectiveAlpha * DirectivePulse), DirectiveFont, DirectiveScale);
	}
}

void ABHHUD::DrawHeatSensor(const ABHCharacter* Character, const ABHGameState* GameState, float X, float Y)
{
	if (!Canvas || !GEngine || !Character || !GameState || !GetWorld())
	{
		return;
	}

	// Minimap box + blips scale with widget size; the MAP / range labels scale with text size. PanelW/PanelH
	// mirror the widget-scaled size the caller used for the right-edge anchor so it stays on-screen.
	const float GS = HudWidgetScale;
	const float T = HudTextScale;
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.26f, 280.0f, 380.0f) * GS;
	const float PanelH = FMath::Clamp(Canvas->ClipY * 0.38f, 250.0f, 360.0f) * GS;
	const float MapX = X + 12.0f * GS;
	const float MapY = Y + 31.0f * GS;
	const float MapW = PanelW - 24.0f * GS;
	const float MapH = PanelH - 47.0f * GS;
	const float CenterX = MapX + MapW * 0.5f;
	const float CenterY = MapY + MapH * 0.5f;
	const float MapRadius = FMath::Min(MapW, MapH) * 0.47f;
	const float SensorRange = 6500.0f;
	const float Now = GetWorld()->GetTimeSeconds();
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.52f), X + 4.0f, Y + 5.0f, PanelW, PanelH);
	DrawRect(FLinearColor(0.014f, 0.014f, 0.012f, 0.74f), X, Y, PanelW, PanelH);
	DrawLine(X, Y, X + PanelW * 0.72f, Y + 1.0f, FLinearColor(0.58f, 0.52f, 0.42f, 0.42f), 1.0f);
	DrawLine(X + PanelW, Y + 7.0f, X + PanelW - 1.0f, Y + PanelH, FLinearColor(0.58f, 0.16f, 0.10f, 0.38f), 1.0f);
	DrawLine(X + 9.0f, Y + PanelH, X + PanelW, Y + PanelH - 2.0f, FLinearColor(0.58f, 0.52f, 0.42f, 0.32f), 1.0f);
	DrawHudText(TEXT("MAP"), X + 12.0f * GS, Y + 10.0f * GS, FLinearColor(0.75f, 0.69f, 0.57f, 0.80f), GEngine->GetSmallFont(), 0.64f * T);
	DrawRightAlignedText(TEXT("65M"), X + PanelW - 13.0f * GS, Y + 10.0f * GS, FLinearColor(0.55f, 0.50f, 0.43f, 0.70f), GEngine->GetSmallFont(), 0.56f * T);
	DrawRect(FLinearColor(0.020f, 0.022f, 0.019f, 0.86f), MapX, MapY, MapW, MapH);
	DrawLine(CenterX, MapY + 6.0f, CenterX, MapY + MapH - 6.0f, FLinearColor(0.65f, 0.62f, 0.52f, 0.08f), 1.0f);
	DrawLine(MapX + 6.0f, CenterY, MapX + MapW - 6.0f, CenterY, FLinearColor(0.65f, 0.62f, 0.52f, 0.08f), 1.0f);
	DrawCircle(CenterX, CenterY, MapRadius * 0.48f, FLinearColor(0.65f, 0.62f, 0.52f, 0.08f), 1.0f, 36);
	DrawCircle(CenterX, CenterY, MapRadius, FLinearColor(0.65f, 0.62f, 0.52f, 0.12f), 1.0f, 44);
	const float SweepAngle = FMath::Fmod(Now * 1.55f, 2.0f * PI) - PI * 0.5f;
	for (int32 Trail = 0; Trail < 2; ++Trail)
	{
		const float TrailAngle = SweepAngle - Trail * 0.19f;
		const float TrailAlpha = 0.16f / static_cast<float>(Trail + 1);
		DrawLine(CenterX, CenterY, CenterX + FMath::Cos(TrailAngle) * MapRadius, CenterY + FMath::Sin(TrailAngle) * MapRadius, FLinearColor(0.80f, 0.12f, 0.08f, TrailAlpha), 1.0f);
	}

	auto ProjectLocation = [&](const FVector& WorldLocation)
	{
		const FVector Delta = WorldLocation - Character->GetActorLocation();
		const float LocalRight = FVector::DotProduct(Delta, Character->GetActorRightVector());
		const float LocalForward = FVector::DotProduct(Delta, Character->GetActorForwardVector());
		const float PX = CenterX + FMath::Clamp(LocalRight / SensorRange, -1.0f, 1.0f) * MapRadius;
		const float PY = CenterY - FMath::Clamp(LocalForward / SensorRange, -1.0f, 1.0f) * MapRadius;
		return FVector2D(PX, PY);
	};

	auto DrawDot = [&](const FVector& WorldLocation, const FLinearColor& Color, float Size)
	{
		Size *= GS;
		const FVector2D P = ProjectLocation(WorldLocation);
		const float Pulse = 0.75f + FMath::Sin(Now * 4.0f + Size) * 0.25f;
		DrawRect(FLinearColor(Color.R, Color.G, Color.B, Color.A * 0.16f * Pulse), P.X - Size, P.Y - Size, Size * 2.0f, Size * 2.0f);
		DrawRect(Color, P.X - Size * 0.45f, P.Y - Size * 0.45f, Size * 0.9f, Size * 0.9f);
	};

	for (TActorIterator<ABHBreaker> It(GetWorld()); It; ++It)
	{
		const ABHBreaker* Breaker = *It;
		if (Breaker && Breaker->IsDirectorActive())
		{
			DrawDot(Breaker->GetActorLocation(), FLinearColor(0.26f, 0.95f, 0.82f, 0.96f), 5.0f);
		}
	}

	for (TActorIterator<ABHObjectiveStation> It(GetWorld()); It; ++It)
	{
		const ABHObjectiveStation* Station = *It;
		if (Station && Station->IsDirectorActive() && !Station->IsCompleted())
		{
			DrawDot(Station->GetActorLocation(), Station->IsQuestionSolved() ? FLinearColor(0.54f, 0.76f, 1.0f, 0.94f) : FLinearColor(1.0f, 0.62f, 0.22f, 0.98f), 5.0f);
		}
	}

	for (TActorIterator<ABHExitGate> It(GetWorld()); It; ++It)
	{
		const ABHExitGate* ExitGate = *It;
		if (ExitGate && ExitGate->IsDirectorActive())
		{
			DrawDot(ExitGate->GetActorLocation(), GameState->bExitUnlocked ? FLinearColor(0.40f, 1.0f, 0.62f, 1.0f) : FLinearColor(0.72f, 0.72f, 0.66f, 0.72f), 7.0f);
		}
	}

	for (TActorIterator<ABHJumpscareMonster> It(GetWorld()); It; ++It)
	{
		const ABHJumpscareMonster* Monster = *It;
		if (Monster)
		{
			DrawDot(Monster->GetActorLocation(), FLinearColor(1.0f, 0.05f, 0.02f, 1.0f), 9.0f);
		}
	}

	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* OtherCharacter = *It;
		const ABHPlayerState* OtherPS = OtherCharacter ? OtherCharacter->GetBHPlayerState() : nullptr;
		if (!OtherCharacter || !OtherPS || OtherCharacter == Character || OtherPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		const FLinearColor HeatColor = OtherPS->PlayerRole == EBHPlayerRole::Hunter
			? FLinearColor(1.0f, 0.14f, 0.08f, 0.98f)
			: (OtherPS->PlayerRole == EBHPlayerRole::FakeHunter ? FLinearColor(1.0f, 0.48f, 0.14f, 0.94f) : FLinearColor(0.42f, 0.94f, 0.56f, 0.86f));
		DrawDot(OtherCharacter->GetActorLocation(), HeatColor, 6.0f);
	}

	DrawLine(CenterX - 5.0f, CenterY, CenterX + 5.0f, CenterY, FLinearColor(0.82f, 0.78f, 0.66f, 0.90f), 1.2f);
	DrawLine(CenterX, CenterY - 7.0f, CenterX, CenterY + 5.0f, FLinearColor(0.82f, 0.78f, 0.66f, 0.90f), 1.2f);
	DrawRect(FLinearColor(0.26f, 0.78f, 0.68f, 0.76f), X + 12.0f * GS, Y + PanelH - 13.0f * GS, 4.0f * GS, 4.0f * GS);
	DrawRect(FLinearColor(0.86f, 0.50f, 0.18f, 0.80f), X + 42.0f * GS, Y + PanelH - 13.0f * GS, 4.0f * GS, 4.0f * GS);
	DrawRect(FLinearColor(0.92f, 0.08f, 0.04f, 0.86f), X + 72.0f * GS, Y + PanelH - 13.0f * GS, 4.0f * GS, 4.0f * GS);
}

void ABHHUD::DrawObjectiveBeats(const ABHCharacter* Character, const ABHGameState* GameState)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character || !GameState || GameState->ObjectiveBeats.IsEmpty())
	{
		return;
	}

	const float ServerNow = GameState->GetServerWorldTimeSeconds();
	const ABHPlayerState* ViewerState = PlayerOwner->GetPlayerState<ABHPlayerState>();
	for (const FBHObjectiveBeat& Beat : GameState->ObjectiveBeats)
	{
		if (!IsObjectiveBeatVisibleTo(Beat, ViewerState))
		{
			continue;
		}
		if (Beat.Label.IsEmpty())
		{
			continue;
		}
		if (Beat.ExpireServerTime > 0.0f && ServerNow > Beat.ExpireServerTime)
		{
			continue;
		}

		const float DistanceCm = FVector::Dist2D(Beat.Location, Character->GetActorLocation());
		const float MaxDisplayDistance = Beat.bPrimary ? 9000.0f : FMath::Max(900.0f, Beat.Radius);
		if (DistanceCm > MaxDisplayDistance)
		{
			continue;
		}

		FVector2D ScreenLocation;
		if (!PlayerOwner->ProjectWorldLocationToScreen(Beat.Location + FVector(0.0f, 0.0f, 115.0f), ScreenLocation, true))
		{
			continue;
		}

		const float X = FMath::Clamp(ScreenLocation.X, 34.0f, Canvas->ClipX - 34.0f);
		const float Y = FMath::Clamp(ScreenLocation.Y, 80.0f, Canvas->ClipY - 96.0f);
		const float Alpha = Beat.ExpireServerTime > 0.0f
			? FMath::Clamp((Beat.ExpireServerTime - ServerNow) / 4.0f, 0.20f, 1.0f)
			: 0.78f;
		const FLinearColor Color = Beat.bDanger
			? FLinearColor(1.0f, 0.16f, 0.08f, Alpha)
			: (Beat.bPrimary ? FLinearColor(0.72f, 0.92f, 0.74f, Alpha) : FLinearColor(0.82f, 0.78f, 0.66f, Alpha));
		const FString Text = FString::Printf(TEXT("%s %.0fm"), *Beat.Label.ToUpper(), DistanceCm / 100.0f);
		float TextW = 0.0f;
		float TextH = 0.0f;
		const float Scale = (Beat.bPrimary ? 0.56f : 0.48f) * HudTextScale;
		Canvas->TextSize(GEngine->GetSmallFont(), Text, TextW, TextH, Scale, Scale);
		const float TextX = FMath::Clamp(X - TextW * 0.5f, 14.0f, Canvas->ClipX - TextW - 14.0f);
		DrawLine(X - 7.0f, Y, X + 7.0f, Y, Color, 1.0f);
		DrawLine(X, Y - 7.0f, X, Y + 7.0f, Color, 1.0f);
		DrawHudText(Text, TextX + 1.0f, Y + 11.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.62f), GEngine->GetSmallFont(), Scale);
		DrawHudText(Text, TextX, Y + 10.0f, Color, GEngine->GetSmallFont(), Scale);
	}
}

void ABHHUD::DrawInteractionPrompt(ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character || !GetWorld())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDInteractPrompt), false, Character);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * 620.0f;
	TArray<FHitResult> Hits;
	if (!GetWorld()->LineTraceMultiByChannel(Hits, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		return;
	}

	AActor* Target = nullptr;
	float TargetDistanceCm = 0.0f;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (Candidate && Candidate->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
		{
			Target = Candidate;
			TargetDistanceCm = Hit.Distance;
			break;
		}

		if (Hit.bBlockingHit)
		{
			break;
		}
	}

	if (!Target || !Target->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
	{
		return;
	}

	FBHInteractionPromptInfo PromptInfo = IBHInteractableInterface::Execute_GetInteractionPromptInfo(Target, Character);
	if (!PromptInfo.bUsePromptInfo)
	{
		PromptInfo.Label = IBHInteractableInterface::Execute_GetInteractionLabel(Target, Character);
		PromptInfo.bCanInteract = IBHInteractableInterface::Execute_CanInteract(Target, Character);
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const ABHPlayerController* BHPC = Cast<ABHPlayerController>(PlayerOwner);
	const bool bHighContrastHud = BHPC && BHPC->IsHighContrastHudEnabled();
	const ABHObjectiveStation* PromptStation = Cast<ABHObjectiveStation>(Target);
	const bool bCanViewStationQuestion = BHPS
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& (BHPS->IsAliveSurvivor() || (BHGS && (BHGS->bRevisionMode || BHGS->bTutorialMode) && BHPS->PlayerRole == EBHPlayerRole::FakeHunter));
	const bool bStationQuestionPrompt = PromptStation
		&& PromptStation->IsDirectorActive()
		&& !PromptStation->IsCompleted()
		&& !PromptStation->IsQuestionSolved()
		&& PromptStation->GetQuestionChoiceCount() > 0
		&& bCanViewStationQuestion
		&& BHGS
		&& (BHGS->RoundPhase == EBHRoundPhase::Hunt || AllowsClassroomWarmupObjectives(BHGS));
	const bool bBonusAnswerPrompt = Cast<ABHTrainBonusQuestionTerminal>(Target) && PromptInfo.bCanInteract;

	if (PromptInfo.RiskText.IsEmpty())
	{
		if (Cast<ABHBreaker>(Target))
		{
			PromptInfo.RiskText = FText::FromString(TEXT("NOISY"));
			PromptInfo.bNoisy = true;
			PromptInfo.HoldSeconds = FMath::Max(PromptInfo.HoldSeconds, 1.0f);
		}
		else if (Cast<ABHSecurityMonitor>(Target))
		{
			PromptInfo.RiskText = FText::FromString(TEXT("EXPOSED"));
			PromptInfo.bDangerous = true;
		}
		else if (PromptStation)
		{
			PromptInfo.RiskText = FText::FromString(bStationQuestionPrompt ? TEXT("ANSWER WITH 1-4") : TEXT("LISTENING"));
			PromptInfo.bDangerous = true;
		}
		else if (Cast<ABHBreakableGlassPane>(Target))
		{
			PromptInfo.RiskText = FText::FromString(TEXT("LOUD"));
			PromptInfo.bNoisy = true;
		}
	}

	if (!PromptInfo.bCanInteract && PromptInfo.DisabledReason.IsEmpty())
	{
		if (PromptStation)
		{
			if (!PromptStation->IsDirectorActive())
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("INACTIVE THIS ROUND"));
			}
			else if (PromptStation->IsCompleted())
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("DONE"));
			}
			else if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("READY UP FIRST"));
			}
			else if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("HUNT PHASE ONLY"));
			}
			else if (!bCanViewStationQuestion && !BHPS->IsAliveSurvivor())
			{
				PromptInfo.DisabledReason = FText::FromString(BHPS->PlayerRole == EBHPlayerRole::FakeHunter ? TEXT("SURVIVORS FINISH THIS NODE") : TEXT("SURVIVOR OBJECTIVE"));
			}
			else if (!PromptStation->IsQuestionSolved() && PromptStation->GetQuestionChoiceCount() > 0)
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("ANSWER WITH 1-4 FIRST"));
			}
			else
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("LOCKED"));
			}
		}
		else if (Cast<ABHExitGate>(Target))
		{
			const int32 RemainingBreakers = BHGS ? FMath::Max(0, BHGS->BreakersRequired - BHGS->BreakersCompleted) : 0;
			const int32 RemainingStations = BHGS ? FMath::Max(0, BHGS->SideObjectivesRequired - BHGS->SideObjectivesCompleted) : 0;
			if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("HALL MONITORS CANNOT ESCAPE"));
			}
			else if (BHPS && BHPS->IsAliveHunter())
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("TEACHER CANNOT ESCAPE"));
			}
			else if (BHGS && BHGS->bRevisionMode)
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("MASTERY OR CONTRIBUTION GATE"));
			}
			else if (RemainingBreakers > 0 || RemainingStations > 0)
			{
				PromptInfo.DisabledReason = FText::FromString(FString::Printf(TEXT("FINISH POWER %d / TASKS %d"), RemainingBreakers, RemainingStations));
			}
			else
			{
				PromptInfo.DisabledReason = FText::FromString(TEXT("EXIT LOCKED"));
			}
		}
	}

	const bool bCanInteract = PromptInfo.bCanInteract || bStationQuestionPrompt || bBonusAnswerPrompt;
	FString Prompt = PromptInfo.Label.IsEmpty() ? FString(TEXT("INTERACT")) : PromptInfo.Label.ToString().ToUpper();
	if (bStationQuestionPrompt)
	{
		Prompt.ReplaceInline(TEXT(": PRESS 1-4"), TEXT(""));
		Prompt.ReplaceInline(TEXT(" PRESS 1-4"), TEXT(""));
	}
	const FString DistanceText = TargetDistanceCm > 1.0f ? FString::Printf(TEXT(" / %.1fm"), TargetDistanceCm / 100.0f) : FString();
	const FString KeyText = (bStationQuestionPrompt || bBonusAnswerPrompt) ? TEXT("1-4") : (PromptInfo.HoldSeconds > 0.05f ? TEXT("HOLD E") : TEXT("E"));
	const FString PromptLine = bCanInteract
		? FString::Printf(TEXT("%s / %s%s"), *KeyText, *Prompt, *DistanceText)
		: FString::Printf(TEXT("NO / %s%s"), *Prompt, *DistanceText);

	// While the question panel is on screen (a survivor or monitor reading a node's question), suppress this
	// centered interaction prompt: it otherwise renders behind the opaque panel (worse at higher HUD scale)
	// and the panel already carries its own "press 1-4 / Tab" controls hint. Every other interactable (and the
	// solved/blocked node states, which show no panel) keep their prompt and disabled-reason line.
	if (!bStationQuestionPrompt)
	{
		float TextW = 0.0f;
		float TextH = 0.0f;
		const float Scale = (bCanInteract ? 0.68f : 0.62f) * HudTextScale;
		Canvas->TextSize(GEngine->GetSmallFont(), PromptLine, TextW, TextH, Scale, Scale);

		const float PromptX = (Canvas->ClipX - TextW) * 0.5f;
		const float PromptY = Canvas->ClipY * 0.5f + 29.0f;
		const FLinearColor PromptColor = bHighContrastHud
			? (bCanInteract ? FLinearColor(0.94f, 1.0f, 0.86f, 1.0f) : FLinearColor(0.82f, 0.84f, 0.82f, 0.92f))
			: (bCanInteract ? FLinearColor(0.82f, 0.78f, 0.66f, 0.88f) : FLinearColor(0.54f, 0.50f, 0.45f, 0.72f));
		DrawHudText(PromptLine, PromptX + 1.0f, PromptY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.42f), GEngine->GetSmallFont(), Scale);
		DrawHudText(PromptLine, PromptX, PromptY, PromptColor, GEngine->GetSmallFont(), Scale);
		if (bCanInteract)
		{
			const float ScratchY = PromptY + TextH + 3.0f;
			const FLinearColor ScratchColor = PromptInfo.bDangerous || PromptInfo.bNoisy ? FLinearColor(0.92f, 0.14f, 0.08f, 0.62f) : FLinearColor(0.82f, 0.18f, 0.12f, 0.44f);
			DrawLine(PromptX + TextW * 0.18f, ScratchY, PromptX + TextW * 0.82f, ScratchY + 1.0f, ScratchColor, 1.0f);
			if (PromptInfo.Progress > 0.01f)
			{
				DrawLine(PromptX + TextW * 0.18f, ScratchY + 4.0f, PromptX + TextW * FMath::Lerp(0.18f, 0.82f, PromptInfo.Progress), ScratchY + 4.0f, FLinearColor(0.78f, 0.88f, 0.62f, 0.76f), 2.0f);
			}
		}

		const FText DetailText = bCanInteract ? PromptInfo.RiskText : PromptInfo.DisabledReason;
		if (!DetailText.IsEmpty())
		{
			const FString DetailLine = DetailText.ToString().ToUpper();
			float DetailW = 0.0f;
			float DetailH = 0.0f;
			const float DetailScale = 0.50f * HudTextScale;
			Canvas->TextSize(GEngine->GetSmallFont(), DetailLine, DetailW, DetailH, DetailScale, DetailScale);
			const float DetailX = (Canvas->ClipX - DetailW) * 0.5f;
			const float DetailY = PromptY + TextH + 8.0f;
			const FLinearColor DetailColor = bCanInteract
				? (PromptInfo.bNoisy || PromptInfo.bDangerous
					? (bHighContrastHud ? FLinearColor(1.0f, 0.42f, 0.24f, 1.0f) : FLinearColor(0.96f, 0.28f, 0.14f, 0.84f))
					: (bHighContrastHud ? FLinearColor(0.82f, 0.94f, 0.86f, 0.96f) : FLinearColor(0.66f, 0.72f, 0.66f, 0.88f)))
				: (bHighContrastHud ? FLinearColor(0.92f, 0.74f, 0.52f, 0.96f) : FLinearColor(0.70f, 0.64f, 0.56f, 0.88f));
			DrawHudText(DetailLine, DetailX + 1.0f, DetailY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.46f), GEngine->GetSmallFont(), DetailScale);
			DrawHudText(DetailLine, DetailX, DetailY, DetailColor, GEngine->GetSmallFont(), DetailScale);
		}
	}

	if (const ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target); Station && bCanViewStationQuestion)
	{
		DrawQuestionPanel(Station);
	}
}

namespace
{
	// Colour for an earned nameplate emblem badge (index into EBHCosmeticCategory::Emblem; 0 = none).
	FLinearColor BHNameplateEmblemColor(int32 EmblemIndex)
	{
		switch (EmblemIndex)
		{
		case 1:  return FLinearColor(0.90f, 0.92f, 0.86f); // Chalk Star
		case 2:  return FLinearColor(0.20f, 0.92f, 0.45f); // Exit Sign
		case 3:  return FLinearColor(0.95f, 0.80f, 0.30f); // Crown
		case 4:  return FLinearColor(0.45f, 0.88f, 0.95f); // Halo
		case 5:  return FLinearColor(1.00f, 0.52f, 0.22f); // Ember
		default: return FLinearColor(0.72f, 0.74f, 0.70f);
		}
	}
}

void ABHHUD::DrawNearbyNameTags(const ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character || !GetWorld())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);

	constexpr float MaxNameTagDistance = 1500.0f;
	constexpr float MaxNameTagDistanceSq = MaxNameTagDistance * MaxNameTagDistance;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* OtherCharacter = *It;
		if (!OtherCharacter || OtherCharacter == Character || OtherCharacter->IsHidden())
		{
			continue;
		}

		const ABHPlayerState* OtherPS = OtherCharacter->GetBHPlayerState();
		if (!OtherPS || OtherPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Character->GetActorLocation(), OtherCharacter->GetActorLocation());
		if (DistanceSq > MaxNameTagDistanceSq)
		{
			continue;
		}

		const FVector NameLocation = OtherCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDNameTagLOS), false);
		Params.AddIgnoredActor(Character);
		Params.AddIgnoredActor(OtherCharacter);

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, NameLocation, ECC_Visibility, Params))
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerOwner->ProjectWorldLocationToScreen(NameLocation, ScreenPosition, true))
		{
			continue;
		}
		if (ScreenPosition.X < 0.0f || ScreenPosition.X > Canvas->ClipX || ScreenPosition.Y < 0.0f || ScreenPosition.Y > Canvas->ClipY)
		{
			continue;
		}

		// Social emote bubble, drawn just above the nameplate when the player recently emoted (press X).
		FString EmoteLabel;
		if (OtherCharacter->GetActiveEmote(EmoteLabel))
		{
			const float EmoteScale = 1.1f * HudTextScale;
			float EmoteW = 0.0f;
			float EmoteH = 0.0f;
			Canvas->TextSize(GEngine->GetLargeFont(), EmoteLabel, EmoteW, EmoteH, EmoteScale, EmoteScale);
			DrawHudText(EmoteLabel, ScreenPosition.X - EmoteW * 0.5f, ScreenPosition.Y - 36.0f * HudTextScale, FLinearColor(1.0f, 0.95f, 0.5f, 1.0f), GEngine->GetLargeFont(), EmoteScale);
		}

		FString Label = OtherPS->GetPlayerName().IsEmpty() ? FString(TEXT("PLAYER")) : OtherPS->GetPlayerName().ToUpper();
		const bool bTeacherRole = OtherPS->PlayerRole == EBHPlayerRole::Hunter || OtherPS->PlayerRole == EBHPlayerRole::Tester;
		const bool bHallMonitorRole = OtherPS->PlayerRole == EBHPlayerRole::FakeHunter;
		const bool bThreatRole = bTeacherRole || bHallMonitorRole;
		if (bTeacherRole)
		{
			Label = TEXT("TEACHER");
		}
		else if (bHallMonitorRole)
		{
			Label = TEXT("HALL MONITOR");
		}

		const float DistanceAlpha = 1.0f - FMath::Clamp(FMath::Sqrt(DistanceSq) / MaxNameTagDistance, 0.0f, 1.0f);
		const float Alpha = FMath::Lerp(0.48f, 0.96f, DistanceAlpha);
		const float Scale = (bThreatRole ? 0.98f : 0.90f) * HudTextScale;
		const FLinearColor TextColor = bTeacherRole
			? FLinearColor(0.96f, 0.18f, 0.12f, Alpha)
			: (bHallMonitorRole
				? FLinearColor(1.0f, 0.56f, 0.18f, Alpha)
				: FLinearColor(0.82f, 0.78f, 0.66f, Alpha));

		float TextW = 0.0f;
		float TextH = 0.0f;
		Canvas->TextSize(GEngine->GetSmallFont(), Label, TextW, TextH, Scale, Scale);
		// If this survivor shows an emblem badge to the left of the name, reserve its footprint and shift the
		// name (and its title caption) right by half of it, so the badge+name group stays centred over the
		// character instead of hanging lopsided to the left.
		const bool bShowEmblem = !bThreatRole && OtherPS->SelectedEmblemIndex > 0;
		const float EmblemBadgeSize = bShowEmblem ? FMath::Max(8.0f, TextH * 0.55f) : 0.0f;
		const float EmblemFootprint = bShowEmblem ? EmblemBadgeSize + 5.0f : 0.0f;
		const float X = ScreenPosition.X - TextW * 0.5f + EmblemFootprint * 0.5f;
		const float Y = ScreenPosition.Y - TextH * 0.5f;
		DrawHudText(Label, X + 1.0f, Y + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, Alpha * 0.70f), GEngine->GetSmallFont(), Scale);
		DrawHudText(Label, X, Y, TextColor, GEngine->GetSmallFont(), Scale);
		DrawLine(X + TextW * 0.18f, Y + TextH + 3.0f, X + TextW * 0.82f, Y + TextH + 3.0f, FLinearColor(TextColor.R, TextColor.G, TextColor.B, Alpha * 0.58f), 2.0f);

		// Earned nameplate flair (survivors only -- threats show TEACHER / HALL MONITOR and stay anonymous):
		// a small emblem badge to the left of the name, and an earned title underneath it. Cosmetic only.
		if (!bThreatRole)
		{
			if (bShowEmblem)
			{
				const FLinearColor EmblemColor = BHNameplateEmblemColor(OtherPS->SelectedEmblemIndex);
				// Sits just left of the name's left edge (X already includes the half-footprint shift) and is
				// vertically centred on the name.
				DrawPanel(X - EmblemFootprint, ScreenPosition.Y - EmblemBadgeSize * 0.5f, EmblemBadgeSize, EmblemBadgeSize,
					FLinearColor(EmblemColor.R, EmblemColor.G, EmblemColor.B, Alpha),
					FLinearColor(0.0f, 0.0f, 0.0f, Alpha * 0.55f));
			}
			if (OtherPS->SelectedTitleIndex > 0)
			{
				const FString TitleText = BHCosmeticItemName(EBHCosmeticCategory::Title, OtherPS->SelectedTitleIndex);
				const float TitleScale = Scale * 0.78f;
				float TitleW = 0.0f;
				float TitleH = 0.0f;
				Canvas->TextSize(GEngine->GetSmallFont(), TitleText, TitleW, TitleH, TitleScale, TitleScale);
				// Centre the title under the NAME (which may be shifted right by the emblem), not the raw
				// character point, so name + title read as one column; drop it just below the underline.
				const float TitleX = (X + TextW * 0.5f) - TitleW * 0.5f;
				const float TitleY = Y + TextH + 8.0f;
				DrawHudText(TitleText, TitleX + 1.0f, TitleY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, Alpha * 0.55f), GEngine->GetSmallFont(), TitleScale);
				DrawHudText(TitleText, TitleX, TitleY, FLinearColor(0.86f, 0.78f, 0.52f, Alpha * 0.92f), GEngine->GetSmallFont(), TitleScale);
			}
		}
	}
}

void ABHHUD::DrawEquipmentStrip(const ABHGameState* GameState)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	// Strip chrome follows widget size (matching DrawStatusPill's internal box scale); pill text follows text size.
	const float GS = HudWidgetScale;
	const float SafePad = FMath::Max(22.0f, Canvas->ClipX * 0.018f);
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.44f, 430.0f, 680.0f) * GS;
	const float PanelH = 42.0f * GS;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY - SafePad - PanelH;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.008f, 0.011f, 0.012f, 0.58f), FLinearColor(0.45f, 0.70f, 0.68f, 0.64f));

	const FString ModeText = !GameState ? FString(TEXT("OFFLINE")) : (GameState->bTestMode ? FString(TEXT("TEST LOOP")) : (GameState->bPracticeMode ? FString(TEXT("PRACTICE LAB")) : FString(TEXT("LIVE FEED"))));
	const FString PhaseText = GameState ? GameState->GetPhaseText().ToUpper() : FString(TEXT("NO SIGNAL"));
	const FString PresenceText = GameState ? FString::Printf(TEXT("PRESENCE %.0f%%"), FMath::Clamp(GameState->PresenceLevel, 0.0f, 100.0f)) : FString(TEXT("PRESENCE --"));
	const FString ExitText = GameState && GameState->bExitUnlocked ? FString(TEXT("EXIT OPEN")) : FString(TEXT("EXIT LOCKED"));
	const FLinearColor ModeColor = GameState && GameState->bTestMode ? FLinearColor(0.95f, 0.86f, 0.42f, 1.0f) : FLinearColor(0.48f, 0.86f, 0.78f, 1.0f);
	const FLinearColor PresenceColor = GameState && GameState->PresenceLevel >= HudHighMeterWarningThreshold ? FLinearColor(1.0f, 0.28f, 0.18f, 1.0f) : FLinearColor(0.62f, 0.82f, 0.78f, 1.0f);
	const float Gap = 8.0f * GS;
	const float PillY = PanelY + 11.0f * GS;
	const float PillW = (PanelW - 38.0f * GS - Gap * 3.0f) / 4.0f;
	DrawStatusPill(ModeText, PanelX + 15.0f * GS, PillY, PillW, ModeColor, GameState != nullptr);
	DrawStatusPill(PhaseText, PanelX + 15.0f * GS + (PillW + Gap), PillY, PillW, FLinearColor(0.66f, 0.78f, 0.92f, 1.0f), GameState != nullptr);
	DrawStatusPill(PresenceText, PanelX + 15.0f * GS + (PillW + Gap) * 2.0f, PillY, PillW, PresenceColor, GameState != nullptr);
	DrawStatusPill(ExitText, PanelX + 15.0f * GS + (PillW + Gap) * 3.0f, PillY, PillW, GameState && GameState->bExitUnlocked ? FLinearColor(0.36f, 1.0f, 0.68f, 1.0f) : FLinearColor(0.96f, 0.42f, 0.34f, 1.0f), GameState != nullptr);
}

void ABHHUD::DrawSpectatorSupportPanel(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
{
	if (!Canvas || !GEngine || !GameState || !PlayerState || PlayerState->PlayerRole != EBHPlayerRole::Spectator)
	{
		return;
	}

	// Self-contained text panel: scale box + text together by max(widget,text) so it never overflows.
	const float S = FMath::Max(HudWidgetScale, HudTextScale);
	const float SafePad = FMath::Max(18.0f, Canvas->ClipX * 0.014f);
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.42f, 430.0f, 640.0f) * S;
	const float PanelH = 82.0f * S;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY - SafePad - PanelH;
	const FLinearColor Accent(0.54f, 0.66f, 0.96f, 0.90f);
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.010f, 0.013f, 0.018f, 0.82f), Accent);

	DrawHudText(TEXT("SPECTATOR SUPPORT"), PanelX + 20.0f * S, PanelY + 14.0f * S, FLinearColor(0.78f, 0.84f, 1.0f, 0.96f), GEngine->GetSmallFont(), 0.78f * S);
	DrawRightAlignedText(FString::Printf(TEXT("SENT %d"), FMath::Max(0, PlayerState->SpectatorEncouragementCount)), PanelX + PanelW - 20.0f * S, PanelY + 14.0f * S, FLinearColor(0.58f, 0.66f, 0.76f, 0.86f), GEngine->GetSmallFont(), 0.58f * S);

	const float KeyY = PanelY + 39.0f * S;
	const float KeyW = 30.0f * S;
	const float SlotW = (PanelW - 44.0f * S) / 4.0f;
	struct FSupportKey
	{
		const TCHAR* Key;
		const TCHAR* Label;
	};
	static const FSupportKey SupportKeys[] = {
		{TEXT("H"), TEXT("SUPPORT")},
		{TEXT("T"), TEXT("TEACHER")},
		{TEXT("Y"), TEXT("SURVIVOR")},
		{TEXT("U"), TEXT("MONITOR")}
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SupportKeys); ++Index)
	{
		const float SlotX = PanelX + 22.0f * S + SlotW * Index;
		DrawKeyBox(SupportKeys[Index].Key, SlotX, KeyY, KeyW, 22.0f * S, Accent, true);
		DrawWrappedHudText(SupportKeys[Index].Label, SlotX + 36.0f * S, KeyY + 5.0f * S, SlotW - 40.0f * S, MainText(), GEngine->GetSmallFont(), 0.56f * S, 10.0f * S, 1);
	}

	const FString PrefLine = FString::Printf(TEXT("PREF %s / HOST APPROVES NEXT LOBBY"), *SpectatorRolePreferenceLabel(PlayerState->SpectatorRolePreference));
	DrawWrappedHudText(PrefLine, PanelX + 22.0f * S, PanelY + 65.0f * S, PanelW - 44.0f * S, FLinearColor(0.62f, 0.72f, 0.78f, 0.88f), GEngine->GetSmallFont(), 0.50f * S, 10.0f * S, 1);
}

void ABHHUD::DrawLobbyRoster(const ABHGameState* GameState, const ABHPlayerState* LocalPlayerState)
{
	if (!Canvas || !GEngine || !GameState)
	{
		return;
	}

	const UFont* Font = GEngine->GetSmallFont();

	// Collect the roster in PlayerArray order and tally humans vs bots / ready vs waiting.
	TArray<const ABHPlayerState*> Roster;
	int32 HumanCount = 0;
	int32 BotCount = 0;
	int32 ReadyHumans = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* PS = Cast<ABHPlayerState>(RawPS);
		if (!PS)
		{
			continue;
		}
		Roster.Add(PS);
		if (PS->IsABot())
		{
			++BotCount;
		}
		else
		{
			++HumanCount;
			if (PS->bReady)
			{
				++ReadyHumans;
			}
		}
	}

	// Self-contained roster panel: scale box + rows + text together by max(widget,text); the panel height
	// is derived from the scaled row height so it always fits.
	const float S = FMath::Max(HudWidgetScale, HudTextScale);
	const float SafePad = FMath::Max(18.0f, Canvas->ClipX * 0.014f);
	const float RowH = 18.0f * S;
	const float HeaderH = 44.0f * S;
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.20f, 224.0f, 308.0f) * S;
	const float PanelH = HeaderH + RowH * FMath::Max(1, Roster.Num()) + 12.0f * S;
	const float PanelX = SafePad;
	const float PanelY = FMath::Clamp(Canvas->ClipY * 0.16f, 84.0f, FMath::Max(84.0f, Canvas->ClipY - PanelH - SafePad));
	const FLinearColor Accent(0.34f, 0.72f, 0.62f, 0.90f);
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.010f, 0.014f, 0.016f, 0.82f), Accent);

	DrawHudText(TEXT("LOBBY"), PanelX + 16.0f * S, PanelY + 12.0f * S, FLinearColor(0.82f, 0.92f, 0.88f, 0.96f), Font, 0.84f * S);
	DrawRightAlignedText(FString::Printf(TEXT("%d IN"), HumanCount + BotCount), PanelX + PanelW - 16.0f * S, PanelY + 12.0f * S, MutedText(), Font, 0.58f * S);
	const FString Subtitle = FString::Printf(TEXT("%d player%s + %d bot%s  -  %d ready"),
		HumanCount, HumanCount == 1 ? TEXT("") : TEXT("s"),
		BotCount, BotCount == 1 ? TEXT("") : TEXT("s"),
		ReadyHumans);
	DrawHudText(Subtitle, PanelX + 16.0f * S, PanelY + 28.0f * S, FLinearColor(0.60f, 0.70f, 0.70f, 0.90f), Font, 0.56f * S);

	float RowY = PanelY + HeaderH;
	for (const ABHPlayerState* PS : Roster)
	{
		const bool bIsBot = PS->IsABot();
		const bool bIsLocal = (PS == LocalPlayerState);

		FString Name = PS->GetPlayerName();
		if (Name.IsEmpty())
		{
			Name = bIsBot ? TEXT("Bot") : TEXT("Player");
		}
		if (bIsLocal)
		{
			Name += TEXT("  (you)");
		}
		const FLinearColor NameColor = bIsLocal
			? FLinearColor(0.96f, 0.94f, 0.74f, 0.96f)
			: (bIsBot ? FLinearColor(0.62f, 0.70f, 0.78f, 0.92f) : MainText());
		DrawHudText(Name, PanelX + 16.0f * S, RowY, NameColor, Font, 0.66f * S);

		FString Tag;
		FLinearColor TagColor;
		if (bIsBot)
		{
			Tag = TEXT("BOT");
			TagColor = FLinearColor(0.56f, 0.66f, 0.80f, 0.88f);
		}
		else if (PS->bReady)
		{
			Tag = TEXT("READY");
			TagColor = FLinearColor(0.50f, 0.86f, 0.50f, 0.94f);
		}
		else
		{
			Tag = TEXT("WAIT");
			TagColor = FLinearColor(0.92f, 0.62f, 0.30f, 0.92f);
		}
		DrawRightAlignedText(Tag, PanelX + PanelW - 16.0f * S, RowY, TagColor, Font, 0.58f * S);

		RowY += RowH;
	}
}

void ABHHUD::DrawQuestionPanel(const ABHObjectiveStation* Station)
{
	if (!Canvas || !GEngine || !Station || !Station->IsDirectorActive() || Station->IsCompleted() || Station->IsQuestionSolved() || Station->GetQuestionChoiceCount() <= 0)
	{
		return;
	}

	// Mouse interaction state lives on the local pawn (cursor on/off, the in-progress drag). Regions
	// are rebuilt every frame so a click always hit-tests exactly what is on screen this frame.
	const ABHCharacter* LocalChar = Cast<ABHCharacter>(GetOwningPawn());
	const bool bCursor = LocalChar && LocalChar->IsQuestionCursorActive();
	QuestionHitRegions.Reset();

	// First-time question-format coaching (SHARED-D): on the first checkpoint a student faces,
	// explain that the diagram shows the givens (never the answer) and how to answer. Fires once.
	if (!bTaughtQuestionFormat)
	{
		bTaughtQuestionFormat = true;
		if (ABHPlayerController* TeachPC = Cast<ABHPlayerController>(PlayerOwner))
		{
			const bool bHasDiagram = Station->GetQuestionDiagramType() != EBHDiagramType::None;
			TeachPC->ShowLocalStatusMessage(bHasDiagram
				? TEXT("Tip: the diagram shows the given values, never the answer. Press 1-4 to answer, or Tab to use the mouse.")
				: TEXT("Tip: answer with keys 1-4 (calculations: type the number, then Enter), or press Tab to use the mouse."), 5.0f);
		}
	}

	// HUD scale enlarges this centered, readability-critical panel. Because the panel is
	// centered (PanelX derives from PanelW) and every offset/font below is multiplied by S,
	// the layout stays self-consistent and on-screen at any scale; width is re-clamped so it
	// never exceeds the viewport.
	const float S = FMath::Max(HudWidgetScale, HudTextScale);
	const int32 ChoiceCount = FMath::Min(4, Station->GetQuestionChoiceCount());
	const float PanelW = FMath::Min(FMath::Clamp(Canvas->ClipX * 0.60f, 560.0f, 920.0f) * S, Canvas->ClipX * 0.94f);
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRevisionQuestion = BHGS && BHGS->bRevisionMode;
	const EBHQuestionType QType = Station->GetQuestionType();
	// Interaction modes. Matching/ordering become drag-and-drop while the cursor is up (otherwise the
	// classic four choice rows answer them with 1-4); calculation questions add an on-screen keypad.
	const bool bArrangement = Station->HasInteractiveArrangement() && (QType == EBHQuestionType::DragDropMatching || QType == EBHQuestionType::Ordering);
	const bool bDragMode = bCursor && bArrangement;
	const bool bCalc = bRevisionQuestion && QType == EBHQuestionType::Calculation;
	// Draw a diagram whenever the question carries one, in any mode -- so visual tasks
	// also appear at nodes during standard play, not only in the classroom.
	const bool bShowDiagram = Station->GetQuestionDiagramType() != EBHDiagramType::None;
	// Adaptive band height per diagram type (graphs/circuits get more room, spectra less) instead
	// of a one-size 118px; PanelH and ChoiceStartY below derive from DiagramH so layout follows.
	const float DiagramH = (bShowDiagram ? FBHDiagramRenderer::BandHeightFor(Station->GetQuestionDiagramType()) : 0.0f) * S;
	// Answer-area height varies by mode so the panel grows to fit a drag tray or keypad.
	const int32 NumSlots = Station->GetInteractiveSlots().Num();
	float AnswerAreaUnscaled;
	if (bDragMode)
	{
		AnswerAreaUnscaled = 22.0f + NumSlots * 30.0f + 56.0f + 32.0f;
	}
	else if (bCalc)
	{
		AnswerAreaUnscaled = 50.0f + (bCursor ? 152.0f : 0.0f);
	}
	else
	{
		AnswerAreaUnscaled = ChoiceCount * 32.0f;
	}
	const float FeedbackUnscaled = Station->GetQuestionFeedback().IsEmpty() ? 0.0f : 42.0f;
	const float PanelH = (112.0f + AnswerAreaUnscaled + 22.0f + 26.0f + FeedbackUnscaled) * S + DiagramH;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float MaxPanelY = FMath::Max(120.0f, Canvas->ClipY - PanelH - 92.0f);
	const float PanelY = FMath::Min(FMath::Max(Canvas->ClipY * 0.58f, 120.0f), MaxPanelY);
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.016f, 0.018f, 0.017f, 0.88f), FLinearColor(0.95f, 0.56f, 0.18f, 0.95f));

	FString Topic = Station->GetQuestionTopic();
	if (Topic.IsEmpty())
	{
		Topic = TEXT("Class");
	}
	const FString ProgressSuffix = bRevisionQuestion
		? FString::Printf(TEXT(" %d/%d"), FMath::Clamp(Station->GetRevisionQuestionsSolved() + 1, 1, FMath::Max(1, Station->GetRevisionQuestionsRequired())), FMath::Max(1, Station->GetRevisionQuestionsRequired()))
		: TEXT("");
	const bool bReviewQuestion = bRevisionQuestion && Station->IsReviewQuestion();
	const FString ReviewTag = bReviewQuestion ? TEXT("  -  SECOND CHANCE") : TEXT("");
	DrawHudText(FString::Printf(TEXT("%s CHECKPOINT%s%s"), *Topic.ToUpper(), *ProgressSuffix, *ReviewTag),
		PanelX + 22.0f * S, PanelY + 16.0f * S,
		bReviewQuestion ? FLinearColor(0.62f, 0.92f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.72f, 0.36f, 1.0f),
		GEngine->GetSmallFont(), 0.94f * S);
	if (bRevisionQuestion)
	{
		FString CounterText;
		if (Station->GetRevisionCounterType() == EBHRevisionCounterNodeType::PeerReview)
		{
			CounterText = TEXT(" | COUNTER: PEER REVIEW");
		}
		else if (Station->GetRevisionCounterType() == EBHRevisionCounterNodeType::DemonstrationTrap)
		{
			CounterText = TEXT(" | COUNTER: DEMO TRAP");
		}
		const FString Meta = FString::Printf(TEXT("%s | %s | %s%s"),
			*FBHRevisionQuestionBank::QuestionTypeToString(Station->GetQuestionType()),
			*FBHRevisionQuestionBank::DifficultyToString(Station->GetQuestionDifficulty()),
			*Station->GetQuestionSubtopic(),
			*CounterText);
		DrawRightAlignedText(Meta, PanelX + PanelW - 22.0f * S, PanelY + 16.0f * S, FLinearColor(0.78f, 0.86f, 0.94f, 1.0f), GEngine->GetSmallFont(), 0.76f * S);
	}
	// The hold-E task is survivor-only, so show Hall Monitors what their role actually does at this node
	// rather than a "hold E to ..." instruction they can never follow (which read as a contradiction sitting
	// right next to the "press 1-4 to answer" controls).
	const ABHPlayerState* PanelViewerPS = LocalChar ? LocalChar->GetBHPlayerState() : nullptr;
	const bool bMonitorViewer = PanelViewerPS && PanelViewerPS->PlayerRole == EBHPlayerRole::FakeHunter;
	DrawWrappedHudText(bMonitorViewer ? FString(TEXT("You answer to contribute; a survivor holds E to finish this node.")) : Station->GetPhysicalTaskInstruction(), PanelX + 22.0f * S, PanelY + 42.0f * S, PanelW - 44.0f * S, FLinearColor(0.74f, 0.88f, 0.88f, 1.0f), GEngine->GetSmallFont(), 0.72f * S, 13.0f * S, 1);
	DrawWrappedHudText(Station->GetQuestionPrompt(), PanelX + 22.0f * S, PanelY + 62.0f * S, PanelW - 44.0f * S, MainText(), GEngine->GetSmallFont(), 0.92f * S, 17.0f * S, 2);

	float ChoiceStartY = PanelY + 112.0f * S;
	if (bShowDiagram)
	{
		DrawRevisionDiagram(Station, PanelX + 24.0f * S, PanelY + 108.0f * S, PanelW - 48.0f * S, DiagramH - 10.0f * S, S);
		ChoiceStartY = PanelY + 112.0f * S + DiagramH;
		// The collaborative "answer team" line is a classroom-mode concept only.
		if (bRevisionQuestion && !Station->GetRevisionTeamSummary().IsEmpty())
		{
			DrawWrappedHudText(FString::Printf(TEXT("Answer team: %s"), *Station->GetRevisionTeamSummary()), PanelX + 28.0f * S, ChoiceStartY - 17.0f * S, PanelW - 56.0f * S, FLinearColor(0.76f, 0.92f, 0.98f, 1.0f), GEngine->GetSmallFont(), 0.72f * S, 12.0f * S, 1);
		}
	}

	// Cursor position (viewport pixels) for hover highlighting, and helpers shared by every mode to
	// register a clickable region (rebuilt each frame -> always matches the draw) and hit-test hover.
	const UFont* QFont = GEngine->GetSmallFont();
	float CursorX = -1.0f;
	float CursorY = -1.0f;
	if (bCursor && PlayerOwner)
	{
		PlayerOwner->GetMousePosition(CursorX, CursorY);
	}
	auto PushRegion = [&](float RX, float RY, float RW, float RH, EBHQuestionRegionKind Kind, int32 IndexA)
	{
		QuestionHitRegions.Emplace(FBox2D(FVector2D(RX, RY), FVector2D(RX + RW, RY + RH)), Kind, IndexA);
	};
	auto IsHovered = [&](float RX, float RY, float RW, float RH)
	{
		return bCursor && CursorX >= RX && CursorX <= RX + RW && CursorY >= RY && CursorY <= RY + RH;
	};
	auto DrawBtn = [&](float RX, float RY, float RW, float RH, const FString& Label, const FLinearColor& Accent, bool bHover)
	{
		DrawRect(bHover ? FLinearColor(Accent.R, Accent.G, Accent.B, 0.46f) : FLinearColor(0.06f, 0.07f, 0.07f, 0.88f), RX, RY, RW, RH);
		DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.85f), RX, RY, RW, 2.0f * S);
		float TW = 0.0f;
		float TH = 0.0f;
		const float TS = 0.72f * S;
		Canvas->TextSize(QFont, Label, TW, TH, TS, TS);
		DrawHudText(Label, RX + FMath::Max(4.0f * S, (RW - TW) * 0.5f), RY + (RH - TH) * 0.5f, FLinearColor(0.93f, 0.97f, 0.95f, 1.0f), QFont, TS);
	};

	if (bDragMode)
	{
		// --- Interactive matching / ordering: drag the shuffled pieces onto the fixed slots. ---
		const TArray<FString>& Slots = Station->GetInteractiveSlots();
		const TArray<FString>& Pieces = Station->GetInteractivePieces();
		const TArray<int32>& Arr = LocalChar->GetQuestionArrangement();
		const int32 Dragged = LocalChar->GetQuestionDraggedPiece();
		const FLinearColor Teal(0.40f, 0.82f, 0.86f, 1.0f);

		DrawHudText(QType == EBHQuestionType::Ordering ? TEXT("Click a step, then click its slot. Submit when done:") : TEXT("Click a label, then click its match. Submit when done:"),
			PanelX + 28.0f * S, ChoiceStartY, FLinearColor(0.76f, 0.92f, 0.98f, 1.0f), QFont, 0.70f * S);

		const float SlotsY = ChoiceStartY + 20.0f * S;
		const float ZoneX = PanelX + PanelW * 0.40f;
		const float ZoneW = PanelW * 0.54f;
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			const float RowY = SlotsY + Slot * 30.0f * S;
			DrawWrappedHudText(Slots[Slot], PanelX + 30.0f * S, RowY + 5.0f * S, ZoneX - PanelX - 38.0f * S, FLinearColor(0.90f, 0.93f, 0.90f, 1.0f), QFont, 0.70f * S, 12.0f * S, 1);
			const bool bHover = IsHovered(ZoneX, RowY, ZoneW, 26.0f * S);
			DrawRect(bHover ? FLinearColor(0.30f, 0.55f, 0.40f, 0.40f) : FLinearColor(0.05f, 0.07f, 0.07f, 0.80f), ZoneX, RowY, ZoneW, 26.0f * S);
			DrawRect(FLinearColor(0.95f, 0.56f, 0.18f, 0.34f), ZoneX, RowY, 3.0f * S, 26.0f * S);
			const int32 Placed = Arr.IsValidIndex(Slot) ? Arr[Slot] : INDEX_NONE;
			if (Pieces.IsValidIndex(Placed))
			{
				DrawWrappedHudText(Pieces[Placed], ZoneX + 10.0f * S, RowY + 5.0f * S, ZoneW - 16.0f * S, FLinearColor(0.96f, 0.98f, 0.94f, 1.0f), QFont, 0.70f * S, 12.0f * S, 1);
			}
			else
			{
				DrawHudText(Dragged != INDEX_NONE ? TEXT("click to place") : TEXT("empty"), ZoneX + 10.0f * S, RowY + 6.0f * S, FLinearColor(0.55f, 0.62f, 0.62f, 0.85f), QFont, 0.60f * S);
			}
			PushRegion(ZoneX, RowY, ZoneW, 26.0f * S, EBHQuestionRegionKind::Slot, Slot);
		}

		// Tray of unplaced pieces (the dragged one is hidden here and drawn at the cursor instead).
		float TrayY = SlotsY + NumSlots * 30.0f * S + 6.0f * S;
		DrawHudText(TEXT("Pieces:"), PanelX + 30.0f * S, TrayY, FLinearColor(0.66f, 0.74f, 0.78f, 0.9f), QFont, 0.62f * S);
		float ChipX = PanelX + 30.0f * S;
		float ChipY = TrayY + 15.0f * S;
		const float ChipH = 24.0f * S;
		const float ChipMaxRight = PanelX + PanelW - 30.0f * S;
		for (int32 Piece = 0; Piece < Pieces.Num(); ++Piece)
		{
			if (Arr.Contains(Piece) || Piece == Dragged)
			{
				continue;
			}
			float TW = 0.0f;
			float TH = 0.0f;
			Canvas->TextSize(QFont, Pieces[Piece], TW, TH, 0.66f * S, 0.66f * S);
			const float ChipW = FMath::Min(TW + 22.0f * S, PanelW - 64.0f * S);
			if (ChipX + ChipW > ChipMaxRight)
			{
				ChipX = PanelX + 30.0f * S;
				ChipY += ChipH + 6.0f * S;
			}
			DrawBtn(ChipX, ChipY, ChipW, ChipH, Pieces[Piece], Teal, IsHovered(ChipX, ChipY, ChipW, ChipH));
			PushRegion(ChipX, ChipY, ChipW, ChipH, EBHQuestionRegionKind::Piece, Piece);
			ChipX += ChipW + 6.0f * S;
		}

		// Submit.
		const bool bComplete = Arr.Num() == NumSlots && !Arr.Contains(INDEX_NONE);
		const float SubW = 130.0f * S;
		const float SubH = 26.0f * S;
		const float SubX = PanelX + PanelW - SubW - 30.0f * S;
		const float SubY = ChoiceStartY + (22.0f + NumSlots * 30.0f + 56.0f) * S;
		DrawBtn(SubX, SubY, SubW, SubH, bComplete ? TEXT("Submit") : TEXT("Fill all slots"), bComplete ? ActivePalette.Good : FLinearColor(0.5f, 0.5f, 0.5f, 1.0f), IsHovered(SubX, SubY, SubW, SubH));
		PushRegion(SubX, SubY, SubW, SubH, EBHQuestionRegionKind::Submit, INDEX_NONE);

		// The piece being dragged floats under the cursor, drawn last so it sits on top of everything.
		if (Pieces.IsValidIndex(Dragged) && CursorX >= 0.0f)
		{
			float TW = 0.0f;
			float TH = 0.0f;
			Canvas->TextSize(QFont, Pieces[Dragged], TW, TH, 0.66f * S, 0.66f * S);
			const float ChipW = FMath::Min(TW + 22.0f * S, PanelW - 64.0f * S);
			DrawBtn(CursorX - ChipW * 0.5f, CursorY - ChipH * 0.5f, ChipW, ChipH, Pieces[Dragged], ActivePalette.WarnHot, true);
		}
	}
	else if (bCalc)
	{
		// Calculation questions use numeric entry: typed (keyboard) and, while the cursor is up, an
		// on-screen keypad. Both feed the same NumericAnswerEntry buffer + Enter-to-submit path.
		const FString Typed = LocalChar ? LocalChar->GetNumericAnswerEntry() : FString();
		const float EntryY = ChoiceStartY + 4.0f * S;
		DrawRect(FLinearColor(0.05f, 0.06f, 0.06f, 0.86f), PanelX + 26.0f * S, EntryY - 5.0f * S, PanelW - 52.0f * S, 30.0f * S);
		DrawRect(FLinearColor(0.30f, 0.78f, 0.95f, 0.42f), PanelX + 26.0f * S, EntryY - 5.0f * S, 3.0f * S, 30.0f * S);
		DrawKeyBox(TEXT("0-9"), PanelX + 38.0f * S, EntryY - 2.0f * S, 40.0f * S, 22.0f * S, FLinearColor(0.40f, 0.82f, 0.96f, 0.94f), true);
		const FString Shown = Typed.IsEmpty() ? TEXT("_") : (Typed + TEXT("_"));
		DrawHudText(FString::Printf(TEXT("Your answer: %s"), *Shown), PanelX + 92.0f * S, EntryY + 2.0f * S, FLinearColor(0.92f, 0.98f, 1.0f, 1.0f), QFont, 0.92f * S);
		DrawWrappedHudText(TEXT("Type the value (digits, . and -). Backspace edits. Press Enter to submit."), PanelX + 28.0f * S, EntryY + 28.0f * S, PanelW - 56.0f * S, FLinearColor(0.66f, 0.78f, 0.84f, 0.92f), QFont, 0.62f * S, 11.0f * S, 1);

		if (bCursor)
		{
			// Compact keypad: three digit columns x four rows, then a Back/Enter row.
			const float KeyW = 52.0f * S;
			const float KeyH = 26.0f * S;
			const float KeyGap = 6.0f * S;
			const float KpX = PanelX + 30.0f * S;
			const float KpY = EntryY + 44.0f * S;
			const int32 Grid[4][3] = {{7, 8, 9}, {4, 5, 6}, {1, 2, 3}, {-1, 0, -2}};
			const FLinearColor KeyAccent(0.40f, 0.82f, 0.96f, 1.0f);
			for (int32 Row = 0; Row < 4; ++Row)
			{
				for (int32 Col = 0; Col < 3; ++Col)
				{
					const float BX = KpX + Col * (KeyW + KeyGap);
					const float BY = KpY + Row * (KeyH + KeyGap);
					const int32 V = Grid[Row][Col];
					FString Label;
					EBHQuestionRegionKind Kind = EBHQuestionRegionKind::KeypadDigit;
					int32 IndexA = INDEX_NONE;
					if (V >= 0) { Label = FString::FromInt(V); Kind = EBHQuestionRegionKind::KeypadDigit; IndexA = V; }
					else if (V == -1) { Label = TEXT("."); Kind = EBHQuestionRegionKind::KeypadDecimal; }
					else { Label = TEXT("-"); Kind = EBHQuestionRegionKind::KeypadMinus; }
					DrawBtn(BX, BY, KeyW, KeyH, Label, KeyAccent, IsHovered(BX, BY, KeyW, KeyH));
					PushRegion(BX, BY, KeyW, KeyH, Kind, IndexA);
				}
			}
			const float UtilY = KpY + 4 * (KeyH + KeyGap);
			const float BackW = KeyW * 1.5f + KeyGap;
			DrawBtn(KpX, UtilY, BackW, KeyH, TEXT("Back"), FLinearColor(0.92f, 0.62f, 0.38f, 1.0f), IsHovered(KpX, UtilY, BackW, KeyH));
			PushRegion(KpX, UtilY, BackW, KeyH, EBHQuestionRegionKind::KeypadBackspace, INDEX_NONE);
			const float EnterX = KpX + BackW + KeyGap;
			const float EnterW = KeyW * 1.5f + KeyGap;
			DrawBtn(EnterX, UtilY, EnterW, KeyH, TEXT("Enter"), ActivePalette.Good, IsHovered(EnterX, UtilY, EnterW, KeyH));
			PushRegion(EnterX, UtilY, EnterW, KeyH, EBHQuestionRegionKind::KeypadEnter, INDEX_NONE);
		}
	}
	else
	{
		for (int32 Index = 0; Index < ChoiceCount; ++Index)
		{
			const float ChoiceY = ChoiceStartY + Index * 32.0f * S;
			// Cap the answer text scale so long choices stay inside the fixed-height row instead of
			// overflowing / truncating early at high HudScale (the row is 27*S tall, step 32*S).
			const float ChoiceTextScale = FMath::Min(0.82f * S, 0.99f);
			const bool bHover = IsHovered(PanelX + 26.0f * S, ChoiceY - 5.0f * S, PanelW - 52.0f * S, 27.0f * S);
			const FLinearColor RowColor = bHover
				? FLinearColor(0.14f, 0.18f, 0.16f, 0.92f)
				: (Index % 2 == 0 ? FLinearColor(0.045f, 0.052f, 0.050f, 0.80f) : FLinearColor(0.034f, 0.041f, 0.040f, 0.80f));
			DrawRect(RowColor, PanelX + 26.0f * S, ChoiceY - 5.0f * S, PanelW - 52.0f * S, 27.0f * S);
			DrawRect(FLinearColor(0.95f, 0.56f, 0.18f, bHover ? 0.7f : 0.34f), PanelX + 26.0f * S, ChoiceY - 5.0f * S, 3.0f * S, 27.0f * S);
			DrawKeyBox(FString::Printf(TEXT("%d"), Index + 1), PanelX + 38.0f * S, ChoiceY - 2.0f * S, 26.0f * S, 21.0f * S, FLinearColor(0.95f, 0.56f, 0.18f, 0.94f), true);
			DrawWrappedHudText(Station->GetQuestionChoice(Index), PanelX + 76.0f * S, ChoiceY + 1.0f * S, PanelW - 116.0f * S, FLinearColor(0.88f, 0.92f, 0.88f, 1.0f), QFont, ChoiceTextScale, 14.0f * S, 1);
			// The whole row is clickable; acted on only while the cursor is up (see the character).
			PushRegion(PanelX + 26.0f * S, ChoiceY - 5.0f * S, PanelW - 52.0f * S, 27.0f * S, EBHQuestionRegionKind::Choice, Index);
		}

		// Clickable diagram elements (e.g. EM-spectrum bands) resolve to the matching answer choice.
		if (bCursor && bShowDiagram)
		{
			FBHDiagramDrawContext RegionCtx;
			RegionCtx.Scale = S;
			TArray<FBHDiagramClickRegion> DiagramRegions;
			FBHDiagramRenderer::GetClickableRegions(Station->GetQuestionDiagramType(), Station->GetQuestionDiagram(),
				PanelX + 24.0f * S, PanelY + 108.0f * S, PanelW - 48.0f * S, DiagramH - 10.0f * S, RegionCtx, DiagramRegions);
			for (const FBHDiagramClickRegion& DR : DiagramRegions)
			{
				if (!DR.Rect.bIsValid)
				{
					continue;
				}
				// Map the element label to the choice whose text names it (e.g. band "infrared" -> "Infrared").
				int32 MappedChoice = INDEX_NONE;
				for (int32 Index = 0; Index < ChoiceCount; ++Index)
				{
					if (Station->GetQuestionChoice(Index).Contains(DR.Label, ESearchCase::IgnoreCase))
					{
						MappedChoice = Index;
						break;
					}
				}
				if (MappedChoice == INDEX_NONE)
				{
					continue;
				}
				const FVector2D RMin = DR.Rect.Min;
				const FVector2D RSize = DR.Rect.GetSize();
				const bool bHover = IsHovered(RMin.X, RMin.Y, RSize.X, RSize.Y);
				DrawCornerBrackets(RMin.X, RMin.Y, RSize.X, RSize.Y, FLinearColor(0.98f, 0.85f, 0.45f, bHover ? 1.0f : 0.55f), 6.0f * S, 2.0f * S);
				PushRegion(RMin.X, RMin.Y, RSize.X, RSize.Y, EBHQuestionRegionKind::DiagramChoice, MappedChoice);
			}
		}
	}

	// Control hint: how to answer, adapting to the current input mode.
	const FString ControlHint = bCursor
		? (bDragMode ? TEXT("Mouse: click a piece, then click a slot. Click Submit.  Tab = keyboard.")
			: (bCalc ? TEXT("Mouse: tap the keypad, then Enter.  Tab = keyboard.") : TEXT("Mouse: click an answer (or part of the diagram).  Tab = keyboard.")))
		: (bArrangement ? TEXT("Press 1-4 to answer, or Tab to place the pieces with the mouse.")
			: (bCalc ? TEXT("Type the value + Enter, or press Tab for an on-screen keypad.") : TEXT("Press 1-4 to answer, or press Tab to answer with the mouse.")));
	DrawHudText(ControlHint, PanelX + 22.0f * S, ChoiceStartY + AnswerAreaUnscaled * S + 4.0f * S, FLinearColor(0.70f, 0.80f, 0.86f, 0.92f), QFont, 0.62f * S);

	if (!Station->GetQuestionFeedback().IsEmpty())
	{
		// Right/wrong feedback uses the shared Good/Bad tokens so colorblind-safe mode remaps
		// the green/red answer cue -- important in an educational context.
		const FLinearColor FeedbackColor = Station->IsQuestionFeedbackCorrect()
			? ActivePalette.Good
			: ActivePalette.Bad;
		DrawWrappedHudText(Station->GetQuestionFeedback(), PanelX + 22.0f * S, PanelY + PanelH - 38.0f * S, PanelW - 44.0f * S, FeedbackColor, GEngine->GetSmallFont(), 0.80f * S, 14.0f * S, 2);
	}
}

bool ABHHUD::GetQuestionRegionAtCursor(const FVector2D& ScreenPos, FBHQuestionHitRegion& OutRegion) const
{
	// Reverse order so the most-recently-drawn (visually topmost) region wins any overlap.
	for (int32 Index = QuestionHitRegions.Num() - 1; Index >= 0; --Index)
	{
		if (QuestionHitRegions[Index].Contains(ScreenPos))
		{
			OutRegion = QuestionHitRegions[Index];
			return true;
		}
	}
	return false;
}

UTexture2D* ABHHUD::ResolveDiagramTexture(const FString& ObjectPath)
{
	if (ObjectPath.IsEmpty())
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<UTexture2D>* Cached = DiagramTextureCache.Find(ObjectPath))
	{
		if (Cached->IsValid())
		{
			return Cached->Get();
		}
	}
	// A null result can't live in the weak-pointer cache (it would read as "not cached" and reload
	// every frame), so remember genuinely-missing paths separately and short-circuit them.
	if (MissingDiagramTexturePaths.Contains(ObjectPath))
	{
		return nullptr;
	}
	// Cook-safe: load the imported .uasset by object path. Missing/uncooked assets return null and
	// the caller falls back to the procedural diagram.
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (Texture)
	{
		DiagramTextureCache.Add(ObjectPath, Texture);
	}
	else
	{
		MissingDiagramTexturePaths.Add(ObjectPath);
	}
	return Texture;
}

void ABHHUD::DrawRevisionDiagram(const ABHObjectiveStation* Station, float X, float Y, float W, float H, float S)
{
	if (!Canvas || !GEngine || !Station)
	{
		return;
	}

	const FBHDiagramParams& P = Station->GetQuestionDiagram();
	UTexture2D* Image = P.HasImage() ? ResolveDiagramTexture(P.ImageSoftPath) : nullptr;

	// Route the HUD's resolved palette (colorblind / high-contrast aware) and panel scale into
	// the shared renderer so diagrams match the rest of the HUD and stay legible at any HudScale.
	// The actual drawing lives in FBHDiagramRenderer so the train terminal and the PNG-bake
	// commandlet produce the identical picture.
	FBHDiagramDrawContext Ctx;
	Ctx.Scale = S;
	Ctx.Accent = ActivePalette.AccentPrimary;
	Ctx.Warm = ActivePalette.WarnHot;
	Ctx.Good = ActivePalette.Good;
	Ctx.Bad = ActivePalette.Bad;
	Ctx.TextMain = MainText();
	Ctx.TextDim = FLinearColor(0.70f, 0.78f, 0.82f, 0.90f);
	Ctx.Font = GEngine->GetSmallFont();
	Ctx.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Ctx.bEnhanced = FBHDiagramRenderer::IsEnhanced();
	// Hide the editorial concept captions ("gradient = velocity", "Key idea: ...") on recall/
	// identify questions where they would reveal the answer; keep them as hints on calculation and
	// matching/ordering questions (whose answers a concept statement can't give away).
	const EBHQuestionType QType = Station->GetQuestionType();
	Ctx.bShowConceptCaptions = (QType == EBHQuestionType::Calculation || QType == EBHQuestionType::DragDropMatching || QType == EBHQuestionType::Ordering);

	FBHDiagramRenderer::Draw(
		Canvas,
		Station->GetQuestionDiagramType(),
		P,
		Station->GetQuestionSubtopic(),
		Station->GetQuestionFormula(),
		Image,
		X, Y, W, H,
		Ctx);
}

// Dev/QA: overlay a sample diagram so any type/variant can be checked in-game.
//   bh.Diagrams.PreviewType 7   (EBHDiagramType index; -1/0 = off)
//   bh.Diagrams.PreviewVariant 1
static TAutoConsoleVariable<int32> CVarBHDiagramPreviewType(
	TEXT("bh.Diagrams.PreviewType"), -1,
	TEXT("Overlay a sample diagram of this EBHDiagramType index on the HUD (-1/0 = off)."), ECVF_Cheat);
static TAutoConsoleVariable<int32> CVarBHDiagramPreviewVariant(
	TEXT("bh.Diagrams.PreviewVariant"), 0,
	TEXT("Shape variant used by bh.Diagrams.PreviewType."), ECVF_Cheat);

void ABHHUD::DrawDiagramPreview()
{
	if (!Canvas || !GEngine)
	{
		return;
	}
	const int32 TypeIndex = CVarBHDiagramPreviewType.GetValueOnGameThread();
	const UEnum* Enum = StaticEnum<EBHDiagramType>();
	// NumEnums() includes the implicit _MAX sentinel, so the last valid type is NumEnums()-2.
	if (TypeIndex <= 0 || !Enum || TypeIndex >= Enum->NumEnums() - 1)
	{
		return;
	}
	const EBHDiagramType Type = static_cast<EBHDiagramType>(TypeIndex);
	const int32 Variant = CVarBHDiagramPreviewVariant.GetValueOnGameThread();
	FBHDiagramParams P;
	FString Name;
	FBHDiagramRenderer::SampleFor(Type, Variant, P, Name);

	const float S = FMath::Max(HudWidgetScale, HudTextScale);
	const float W = 420.0f * S;
	const float H = FBHDiagramRenderer::BandHeightFor(Type) * S;
	const float X = Canvas->ClipX - W - 24.0f * S;
	const float Y = Canvas->ClipY - H - 64.0f * S;

	FBHDiagramDrawContext Ctx;
	Ctx.Scale = S;
	Ctx.Accent = ActivePalette.AccentPrimary;
	Ctx.Warm = ActivePalette.WarnHot;
	Ctx.Good = ActivePalette.Good;
	Ctx.Bad = ActivePalette.Bad;
	Ctx.TextMain = MainText();
	Ctx.Font = GEngine->GetSmallFont();
	Ctx.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Ctx.bEnhanced = FBHDiagramRenderer::IsEnhanced();

	DrawHudText(FString::Printf(TEXT("DIAGRAM PREVIEW: %s  (type %d, variant %d)"), *Name, TypeIndex, Variant),
		X, Y - 16.0f * S, FLinearColor(0.95f, 0.85f, 0.45f, 1.0f), GEngine->GetSmallFont(), 0.70f * S);
	FBHDiagramRenderer::Draw(Canvas, Type, P, Name, FString(), nullptr, X, Y, W, H, Ctx);
}

void ABHHUD::DrawPhaseBanner(const ABHGameState* GameState, const ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !GameState || !GetWorld())
	{
		return;
	}

	// The guided tutorial shows its own per-phase ShowTutorialCard; the generic "PRACTICE LAB" phase banner is
	// off-message and competes with that card, so suppress it in tutorial mode.
	if (GameState->bTutorialMode)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	float Alpha = FMath::Clamp((PhaseBannerEndTime - Now) / 3.6f, 0.0f, 1.0f);
	if (GameState->RoundPhase == EBHRoundPhase::SurvivorsWin || GameState->RoundPhase == EBHRoundPhase::HunterWin)
	{
		Alpha = FMath::Max(Alpha, 0.86f);
	}

	if (Alpha <= 0.01f)
	{
		return;
	}

	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	FString Title = GameState->GetPhaseText().ToUpper();
	FString Subtitle = GameState->ObjectiveText;
	// Neutral default accent picks up the active map's tint via the palette.
	FLinearColor Accent = ActivePalette.AccentPrimary;

	switch (GameState->RoundPhase)
	{
	case EBHRoundPhase::Prep:
		Title = TEXT("ROLE WARMUP");
		Subtitle = TEXT("Try flashlight, lockers, questions, decoys, scans, captures, and Hall Monitor tools. Hunt start resets everyone.");
		Accent = FLinearColor(0.95f, 0.76f, 0.36f, 1.0f);
		break;
	case EBHRoundPhase::Hunt:
		Title = GameState->bTestMode ? TEXT("TEST ROUND") : (GameState->bPracticeMode ? TEXT("PRACTICE LAB") : (GameState->bRevisionMode ? TEXT("PHYSICS CLASSROOM") : TEXT("HUNT STARTED")));
		Subtitle = GameState->bTestMode ? TEXT("Tester role active. No timer, no minimum players, no forced round end.") : (GameState->bPracticeMode ? TEXT("Round end disabled. Test roles, tasks, and pressure.") : (GameState->bRevisionMode ? TEXT("Solve, correct, contribute, and escape the Physics Teacher.") : TEXT("Finish the objectives and reach the exit.")));
		Accent = FLinearColor(0.92f, 0.18f, 0.12f, 1.0f);
		break;
	case EBHRoundPhase::Intermission:
		Title = FString::Printf(TEXT("SUBWAY %s"), *TrainPhaseLabel(GameState->TrainPhase));
		Subtitle = GameState->TrainAnnouncement.IsEmpty() ? TrainPhaseObjective(GameState->TrainPhase) : GameState->TrainAnnouncement;
		Accent = ActivePalette.AccentSecondary;
		break;
	case EBHRoundPhase::FinalEscape:
		Title = TEXT("FINAL TRAIN");
		Subtitle = GameState->FinalEscapeState == EBHFinalEscapeState::Cutscene
			? TEXT("Evacuation doors unlocking. Teacher release is delayed.")
			: TEXT("Reach any green subway door before departure. Door camping disrupts capture.");
		Accent = FLinearColor(1.0f, 0.42f, 0.20f, 1.0f);
		break;
	case EBHRoundPhase::SurvivorsWin:
		Title = TEXT("SURVIVORS ESCAPED");
		Subtitle = TEXT("Round complete. Returning to lobby shortly.");
		Accent = ActivePalette.Good;
		break;
	case EBHRoundPhase::HunterWin:
		Title = TEXT("TEACHER WINS");
		Subtitle = TEXT("Round complete. Returning to lobby shortly.");
		Accent = ActivePalette.Bad;
		break;
	case EBHRoundPhase::Lobby:
	default:
		Title = TEXT("LOBBY");
		Subtitle = TEXT("Ready up when everyone is connected.");
		break;
	}

	if (Character && Character->IsDetentionMarked() && GameState->RoundPhase == EBHRoundPhase::Hunt)
	{
		Subtitle = TEXT("Detention marked. Move carefully.");
	}
	else if (GameState->RoundPhase == EBHRoundPhase::Hunt && GameState->RoundModifier != EBHRoundModifier::None)
	{
		Subtitle = FString::Printf(TEXT("Modifier: %s. %s"), *GameState->GetRoundModifierText(), *GameState->GetRoundModifierHint());
	}

	// Centered banner: scale width/height/offsets/fonts by the HUD scale preference.
	const float S = FMath::Max(HudWidgetScale, HudTextScale);
	const float PanelW = FMath::Min(FMath::Clamp(Canvas->ClipX * 0.48f, 420.0f, 720.0f) * S, Canvas->ClipX * 0.94f);
	const float PanelH = 86.0f * S;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY * 0.20f - (1.0f - SmoothAlpha) * 18.0f;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, WithAlpha(FLinearColor(0.012f, 0.014f, 0.016f, 0.86f), SmoothAlpha), WithAlpha(Accent, SmoothAlpha));

	float TitleW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(GEngine->GetSmallFont(), Title, TitleW, TextH, 1.34f * S, 1.34f * S);
	DrawHudText(Title, PanelX + (PanelW - TitleW) * 0.5f, PanelY + 17.0f * S, WithAlpha(Accent, SmoothAlpha), GEngine->GetSmallFont(), 1.34f * S);

	DrawWrappedHudText(Subtitle, PanelX + 28.0f * S, PanelY + 52.0f * S, PanelW - 56.0f * S, WithAlpha(FLinearColor(0.82f, 0.90f, 0.88f, 1.0f), SmoothAlpha), GEngine->GetSmallFont(), 0.82f * S, 14.0f * S, 1);
}

void ABHHUD::DrawRoleIntroCard(const ABHPlayerController* BHPC, const ABHGameState* GameState)
{
	if (!Canvas || !GEngine || !BHPC || !GameState)
	{
		return;
	}
	// Only during the live role warmup, and only while the one-shot intro window is active.
	// Prep-gated so it can never appear during a chase.
	if (GameState->RoundPhase != EBHRoundPhase::Prep || !BHPC->HasActiveRoleIntro())
	{
		return;
	}

	const float Alpha = BHPC->GetRoleIntroAlpha();
	if (Alpha <= 0.01f)
	{
		return;
	}
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

	const EBHPlayerRole IntroRole = BHPC->GetRoleIntroRole();
	const FBHRoleIntroCopy Copy = BHGetRoleIntroCopy(IntroRole, BHPC->IsRoleIntroRevisionMode());

	FLinearColor Accent;
	switch (IntroRole)
	{
	case EBHPlayerRole::Hunter:     Accent = ActivePalette.Bad; break;
	case EBHPlayerRole::FakeHunter: Accent = FLinearColor(0.95f, 0.76f, 0.36f, 1.0f); break;
	case EBHPlayerRole::Spectator:  Accent = ActivePalette.TextMuted; break;
	default:                        Accent = ActivePalette.Good; break;
	}

	const UFont* Font = GEngine->GetSmallFont();
	const float S = FMath::Max(HudWidgetScale, HudTextScale);
	const int32 KeyCount = Copy.Keys.Num();
	const float PanelW = FMath::Min(FMath::Clamp(Canvas->ClipX * 0.50f, 460.0f, 760.0f) * S, Canvas->ClipX * 0.94f);
	const float PanelH = (150.0f + KeyCount * 18.0f) * S;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	// Sits just below the centred phase banner so both read at warmup start without overlap.
	const float PanelY = Canvas->ClipY * 0.34f - (1.0f - SmoothAlpha) * 16.0f;

	const bool bHighContrastHud = BHPC->IsHighContrastHudEnabled();
	const FLinearColor Fill = bHighContrastHud ? FLinearColor(0.0f, 0.0f, 0.0f, 0.94f) : FLinearColor(0.014f, 0.016f, 0.020f, 0.90f);
	const FLinearColor BodyText = bHighContrastHud ? FLinearColor(1.0f, 0.99f, 0.92f, 1.0f) : FLinearColor(0.88f, 0.92f, 0.94f, 1.0f);
	DrawPanel(PanelX, PanelY, PanelW, PanelH, WithAlpha(Fill, SmoothAlpha), WithAlpha(Accent, SmoothAlpha));

	float TitleW = 0.0f;
	float TitleH = 0.0f;
	Canvas->TextSize(Font, Copy.Title, TitleW, TitleH, 1.12f * S, 1.12f * S);
	DrawHudText(Copy.Title, PanelX + (PanelW - TitleW) * 0.5f, PanelY + 14.0f * S, WithAlpha(Accent, SmoothAlpha), Font, 1.12f * S);

	DrawWrappedHudText(Copy.Goal, PanelX + 26.0f * S, PanelY + 44.0f * S, PanelW - 52.0f * S, WithAlpha(BodyText, SmoothAlpha), Font, 0.92f * S, 16.0f * S, 2);

	float Cursor = PanelY + 80.0f * S;
	for (const FString& Key : Copy.Keys)
	{
		DrawHudText(FString::Printf(TEXT("- %s"), *Key), PanelX + 30.0f * S, Cursor, WithAlpha(BodyText, SmoothAlpha), Font, 0.80f * S);
		Cursor += 18.0f * S;
	}

	DrawWrappedHudText(Copy.Tip, PanelX + 26.0f * S, Cursor + 6.0f * S, PanelW - 52.0f * S, WithAlpha(Accent, SmoothAlpha), Font, 0.82f * S, 14.0f * S, 2);
}

void ABHHUD::DrawTutorialCard(const ABHPlayerController* BHPC)
{
	if (!Canvas || !GEngine || !BHPC || !BHPC->HasActiveTutorialCard())
	{
		return;
	}
	const float Alpha = BHPC->GetTutorialCardAlpha();
	if (Alpha <= 0.01f)
	{
		return;
	}
	const float Smooth = Alpha * Alpha * (3.0f - 2.0f * Alpha);

	// Full-screen dark wash so the phase hand-off reads as a deliberate loading card, not a freeze or a jump-cut.
	const FLinearColor Wash(0.012f, 0.014f, 0.018f, 0.92f * Smooth);
	DrawPanel(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, Wash, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

	const float S = FMath::Max(HudWidgetScale, HudTextScale);
	const UFont* TitleFont = GEngine->GetLargeFont();
	const UFont* BodyFont = GEngine->GetSmallFont();
	const FLinearColor Accent = WithAlpha(ActivePalette.Good, Smooth);
	const FLinearColor BodyColor = WithAlpha(FLinearColor(0.90f, 0.93f, 0.95f, 1.0f), Smooth);

	const FString& Title = BHPC->GetTutorialCardTitle();
	const float TitleScale = 1.8f * S;
	float TitleW = 0.0f;
	float TitleH = 0.0f;
	Canvas->TextSize(TitleFont, Title, TitleW, TitleH, TitleScale, TitleScale);
	const float TitleY = Canvas->ClipY * 0.40f;
	DrawHudText(Title, (Canvas->ClipX - TitleW) * 0.5f, TitleY, Accent, TitleFont, TitleScale);

	const float BodyW = FMath::Min(Canvas->ClipX * 0.70f, 900.0f * S);
	DrawWrappedHudText(BHPC->GetTutorialCardBody(), (Canvas->ClipX - BodyW) * 0.5f, TitleY + TitleH + 16.0f * S, BodyW, BodyColor, BodyFont, 1.0f * S, 20.0f * S, 3);
}

void ABHHUD::DrawTutorialPrompt(const ABHPlayerController* BHPC)
{
	if (!Canvas || !GEngine || !BHPC || !BHPC->HasActiveTutorialPrompt())
	{
		return;
	}
	const float Alpha = BHPC->GetTutorialPromptAlpha();
	if (Alpha <= 0.01f)
	{
		return;
	}
	const FString& Message = BHPC->GetTutorialPromptMessage();
	const UFont* Font = GEngine->GetSmallFont();
	const float S = FMath::Max(HudWidgetScale, HudTextScale);

	// Bigger, centred caption. The old 0.95 small-font line read too small and was left-aligned inside a wide panel,
	// so the text looked off-centre. Enlarge it, size the panel to the wrapped line count (up to 3 lines), and centre
	// each line both horizontally (bCenterEachLine) and vertically within the panel.
	const float FS = 1.28f * S;
	const float Pad = 34.0f * S;
	const float LineH = 27.0f * S;
	float TextW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(Font, Message, TextW, TextH, FS, FS);
	const float MaxPanelW = Canvas->ClipX * 0.86f;
	const float PanelW = FMath::Clamp(TextW + 2.0f * Pad, 420.0f * S, MaxPanelW);
	const float InnerW = PanelW - 2.0f * Pad;
	// Bias the estimate up (0.9 * InnerW): word-wrap rarely fills a line completely, so dividing by the full width can
	// undercount by one and let a wrapped line spill below the panel. A slightly tall panel is harmless; an overflow isn't.
	const int32 NumLines = FMath::Clamp(FMath::CeilToInt(TextW / FMath::Max(1.0f, InnerW * 0.9f)), 1, 3);
	const float PanelH = NumLines * LineH + 24.0f * S;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	// Top of the screen, well clear of the shared status toast (which sits at ~0.72) and below any phase banner.
	const float PanelY = Canvas->ClipY * 0.11f;

	const bool bHighContrastHud = BHPC->IsHighContrastHudEnabled();
	const FLinearColor Fill = bHighContrastHud ? FLinearColor(0.0f, 0.0f, 0.0f, 0.95f) : FLinearColor(0.020f, 0.030f, 0.050f, 0.90f);
	const FLinearColor Accent = bHighContrastHud ? FLinearColor(0.55f, 0.95f, 1.0f, 1.0f) : FLinearColor(0.32f, 0.74f, 0.95f, 0.96f);
	const FLinearColor TextColor = bHighContrastHud ? FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) : FLinearColor(0.92f, 0.96f, 0.99f, 1.0f);
	DrawPanel(PanelX, PanelY, PanelW, PanelH, WithAlpha(Fill, Alpha), WithAlpha(Accent, Alpha));
	const float TextY = PanelY + (PanelH - NumLines * LineH) * 0.5f;
	DrawWrappedHudText(Message, PanelX + Pad, TextY, InnerW, WithAlpha(TextColor, Alpha), Font, FS, LineH, 3, true);
}

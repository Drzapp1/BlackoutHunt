#include "BHHUD.h"
#include "BHBreakableGlassPane.h"
#include "BHCharacter.h"
#include "BHBreaker.h"
#include "BHExitGate.h"
#include "BHGameState.h"
#include "BHInteractableInterface.h"
#include "BHJumpscareMonster.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "BHSecurityMonitor.h"
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

	FLinearColor MutedText()
	{
		return FLinearColor(0.58f, 0.66f, 0.66f, 0.92f);
	}

	FLinearColor MainText()
	{
		return FLinearColor(0.88f, 0.95f, 0.92f, 1.0f);
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
			return FMath::Clamp(GameState->RevisionContributionTarget, 1, 4);
		}

		const int32 ParticipantCount = CountRevisionParticipants(GameState);
		const int32 StageIndex = GameState ? FMath::Clamp(GameState->TrainStageIndex, 0, 2) : 0;
		const int32 QuestionsPerNode = ParticipantCount >= 10
			? (StageIndex <= 0 ? 2 : 3)
			: (ParticipantCount >= 6 ? 3 : 2);
		return FMath::Clamp(QuestionsPerNode - 1 + StageIndex, 1, 4);
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
			if (PlayerState && PlayerState->IsAliveHunter())
			{
				return TEXT("Warmup: learn routes and practice Q/R. Capture starts in Hunt.");
			}
			if (PlayerState && PlayerState->PlayerRole == EBHPlayerRole::FakeHunter)
			{
				return TEXT("Warmup: try Q real hint, R false marker, and G trap. No capture.");
			}
			return TEXT("Warmup: practice flashlight (F), hiding, and questions. In Hunt, finish objectives then reach the exit.");
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

	if (bShowSurvivorWarnings)
	{
		DrawHorrorOverlay(Character, BHGS);
	}

	const float SafePad = FMath::Max(18.0f, Canvas->ClipX * 0.014f);
	const float ReadoutW = FMath::Clamp(Canvas->ClipX * 0.38f, 280.0f, 560.0f);
	if (BHGS)
	{
		const FString TimerText = BHGS->bTestMode ? FString(TEXT("TEST LOOP")) : (BHGS->bPracticeMode ? FString(TEXT("PRACTICE")) : FString::Printf(TEXT("T-%s"), *FormatClock(BHGS->RemainingTime)));
		const FString ExitText = BHGS->bExitUnlocked ? FString(TEXT("EXIT OPEN")) : FString(TEXT("EXIT LOCKED"));
		const FString ActionLine = BuildHudActionLine(GetWorld(), BHGS, BHPS, Character).ToUpper();
		const FString DetailLine = BuildHudDetailLine(BHGS, BHPS).ToUpper();
		const FString AuxLine = BuildHudAuxLine(BHGS).ToUpper();
		const FLinearColor ExitColor = BHGS->bExitUnlocked ? FLinearColor(0.73f, 0.96f, 0.64f, 0.95f) : FLinearColor(0.96f, 0.24f, 0.16f, 0.95f);
		DrawHudText(FString::Printf(TEXT("%s / %s"), *TimerText, *ExitText), SafePad, SafePad, ExitColor, GEngine->GetSmallFont(), 0.88f);
		DrawWrappedHudText(ActionLine, SafePad, SafePad + 22.0f, ReadoutW, bHighContrastHud ? FLinearColor(0.92f, 0.98f, 0.90f, 1.0f) : FLinearColor(0.84f, 0.80f, 0.70f, 0.90f), GEngine->GetSmallFont(), 0.70f, 13.0f, 2);
		DrawWrappedHudText(DetailLine, SafePad, SafePad + 55.0f, ReadoutW, bHighContrastHud ? FLinearColor(0.78f, 0.92f, 0.88f, 0.96f) : FLinearColor(0.62f, 0.58f, 0.51f, 0.84f), GEngine->GetSmallFont(), 0.60f, 12.0f, 1);
		if (!AuxLine.IsEmpty())
		{
			DrawWrappedHudText(AuxLine, SafePad, SafePad + 73.0f, ReadoutW, bHighContrastHud ? FLinearColor(0.84f, 0.86f, 0.68f, 0.92f) : FLinearColor(0.74f, 0.63f, 0.55f, 0.78f), GEngine->GetSmallFont(), 0.56f, 12.0f, 1);
		}
	}
	else
	{
		DrawHudText(TEXT("NO SIGNAL"), SafePad, SafePad, FLinearColor(0.96f, 0.24f, 0.16f, 0.92f), GEngine->GetSmallFont(), 0.90f);
		DrawHudText(TEXT("HOST OR JOIN"), SafePad, SafePad + 21.0f, FLinearColor(0.62f, 0.58f, 0.51f, 0.78f), GEngine->GetSmallFont(), 0.64f);
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
		DrawRightAlignedText(RoleName.ToUpper(), Canvas->ClipX - SafePad, SafePad, FLinearColor(0.88f, 0.84f, 0.74f, 0.90f), GEngine->GetSmallFont(), 0.82f);
		DrawRightAlignedText(FString::Printf(TEXT("%s / %s / AV%02d"), *LifeName.ToUpper(), *ReadyText, BHPS->AvatarIndex + 1), Canvas->ClipX - SafePad, SafePad + 20.0f, FLinearColor(0.55f, 0.52f, 0.47f, 0.76f), GEngine->GetSmallFont(), 0.58f);
		if ((BHGS && BHGS->bTestMode) || BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			const float ShortcutW = FMath::Clamp(Canvas->ClipX * 0.34f, 310.0f, 540.0f);
			DrawWrappedHudText(TEXT("TEST KEYS  INS RESET  HOME TRAIN  PGUP PHASE  END STATION  PGDN ESCAPE  DEL RECAP"),
				Canvas->ClipX - SafePad - ShortcutW,
				SafePad + 42.0f,
				ShortcutW,
				FLinearColor(0.95f, 0.86f, 0.42f, 0.80f),
				GEngine->GetSmallFont(),
				0.50f,
				11.0f,
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

			DrawRightAlignedText(CaptureReadout, Canvas->ClipX - SafePad, SafePad + 42.0f, CaptureColor, GEngine->GetSmallFont(), 0.66f);
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			const float MonitorW = FMath::Clamp(Canvas->ClipX * 0.30f, 260.0f, 440.0f);
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
				SafePad + 42.0f,
				MonitorW,
				FLinearColor(0.95f, 0.70f, 0.32f, 0.82f),
				GEngine->GetSmallFont(),
				0.50f,
				11.0f,
				2);
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Spectator && BHGS && BHGS->RoundPhase != EBHRoundPhase::Lobby)
		{
			DrawSpectatorSupportPanel(BHGS, BHPS);
		}
	}

	if (Character)
	{
		const float MeterW = FMath::Clamp(Canvas->ClipX * 0.22f, 210.0f, 310.0f);
		const float VitalsY = Canvas->ClipY - SafePad - 132.0f;
		const FString VitalsTitle = Character->IsDetentionMarked()
			? FString::Printf(TEXT("MARKED %.0fs"), Character->GetDetentionMarkRemaining())
			: (Character->IsHiddenInLocker() ? FString(TEXT("CONCEALED")) : FString(TEXT("STATUS")));
		DrawHudText(VitalsTitle.ToUpper(), SafePad, VitalsY - 19.0f, Character->IsDetentionMarked() ? FLinearColor(1.0f, 0.20f, 0.12f, 0.96f) : FLinearColor(0.76f, 0.72f, 0.64f, 0.84f), GEngine->GetSmallFont(), 0.66f);
		DrawProgressBar(TEXT("BATTERY"), Character->GetFlashlightBattery(), SafePad, VitalsY, MeterW, FLinearColor(0.80f, 0.82f, 0.70f, 0.88f));

		const FBHTeacherProximityReadout TeacherProximity = FindTeacherProximity(GetWorld(), Character);
		const FString TeacherText = TeacherProximity.bFound
			? FString::Printf(TEXT("%s %.0fm"), TeacherProximity.bLineOfSight ? TEXT("VISIBLE") : TEXT("NEAR"), TeacherProximity.DistanceCm / 100.0f)
			: FString(TEXT("CLEAR"));
		DrawProgressBar(TEXT("TEACHER"), TeacherProximity.ProximityPercent, SafePad, VitalsY + 32.0f, MeterW, FLinearColor(0.90f, 0.36f, 0.22f, 0.90f), TeacherText);
		DrawRawMeter(TEXT("STAMINA"), Character->GetStaminaPercent(), SafePad, VitalsY + 68.0f, MeterW, FLinearColor(0.75f, 0.83f, 0.54f, 0.88f), false);
		DrawRawMeter(TEXT("FEAR"), Character->GetFear(), SafePad, VitalsY + 86.0f, MeterW, FLinearColor(0.92f, 0.28f, 0.20f, 0.88f), true);
		DrawRawMeter(TEXT("DREAD"), Character->GetDread(), SafePad, VitalsY + 104.0f, MeterW, FLinearColor(0.84f, 0.18f, 0.14f, 0.90f), true);
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
			DrawWrappedHudText(StressHint, SafePad, VitalsY + 122.0f, MeterW, FLinearColor(0.96f, 0.42f, 0.30f, 0.88f), GEngine->GetSmallFont(), 0.48f, 10.0f, 1);
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
				DrawVisibleHunterArrow(Character, LastVisibleHunterLocation, LastVisibleHunterDistanceCm, CueStrength);
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

	if (Character && BHGS && BHGS->RoundPhase != EBHRoundPhase::Lobby && BHPC && (BHPC->IsHudMapVisible() || bPathDetected))
	{
		const float MapW = FMath::Clamp(Canvas->ClipX * 0.26f, 280.0f, 380.0f);
		const float MapX = Canvas->ClipX - SafePad - MapW;
		const float MaxMapY = FMath::Max(SafePad + 44.0f, Canvas->ClipY - SafePad - 270.0f);
		const float MapY = FMath::Clamp(Canvas->ClipY * 0.17f, SafePad + 44.0f, MaxMapY);
		DrawHeatSensor(Character, BHGS, MapX, MapY);
	}

	DrawCCTVRevealMarker(Character, BHPC);
	DrawObjectiveBeats(Character, BHGS);

	const float WarningLevel = bShowSurvivorWarnings ? FMath::Max(Character ? Character->GetDread() : 0.0f, BHGS ? BHGS->PresenceLevel : 0.0f) : 0.0f;
	const float DangerAlpha = FMath::Clamp(WarningLevel / 100.0f, 0.0f, 1.0f);
	DrawCrosshair(DangerAlpha);
	DrawNearbyNameTags(Character);
	DrawInteractionPrompt(Character);

	if (bPathDetected)
	{
		const FString PulseText = TEXT("PATH DETECTED");
		const float PulseScale = FMath::Lerp(1.02f, 1.22f, PathThreatAlpha);
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
			float TextW = 0.0f;
			float TextH = 0.0f;
			Canvas->TextSize(GEngine->GetSmallFont(), Status, TextW, TextH);
			const float ToastW = FMath::Clamp(TextW + 48.0f, 260.0f, Canvas->ClipX * 0.62f);
			const float ToastX = (Canvas->ClipX - ToastW) * 0.5f;
			const bool bLongStatus = TextW > ToastW - 48.0f;
			const float ToastH = bLongStatus ? 66.0f : 48.0f;
			const float ToastY = Canvas->ClipY * 0.72f + (1.0f - StatusAlpha) * 10.0f;
			const FLinearColor ToastFill = bHighContrastHud ? FLinearColor(0.0f, 0.0f, 0.0f, 0.92f) : FLinearColor(0.020f, 0.020f, 0.018f, 0.84f);
			const FLinearColor ToastAccent = bHighContrastHud ? FLinearColor(1.0f, 0.96f, 0.42f, 1.0f) : FLinearColor(0.96f, 0.74f, 0.36f, 0.94f);
			const FLinearColor ToastText = bHighContrastHud ? FLinearColor(1.0f, 0.98f, 0.82f, 1.0f) : FLinearColor(0.96f, 0.87f, 0.62f, 1.0f);
			DrawPanel(ToastX, ToastY, ToastW, ToastH, WithAlpha(ToastFill, StatusAlpha), WithAlpha(ToastAccent, StatusAlpha));
			DrawWrappedHudText(Status, ToastX + 24.0f, ToastY + 15.0f, ToastW - 48.0f, WithAlpha(ToastText, StatusAlpha), GEngine->GetSmallFont(), 0.92f, 17.0f, 2);
		}
	}

	DrawPhaseBanner(BHGS, Character);
}

void ABHHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& FillColor, const FLinearColor& AccentColor)
{
	if (!Canvas || W <= 1.0f || H <= 1.0f)
	{
		return;
	}

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

	float TextW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(GEngine->GetSmallFont(), Key, TextW, TextH, 0.76f, 0.76f);
	DrawHudText(Key, X + (W - TextW) * 0.5f, Y + (H - TextH) * 0.5f - 1.0f, bLit ? FLinearColor(0.88f, 1.0f, 0.96f, 1.0f) : FLinearColor(0.50f, 0.58f, 0.58f, 0.88f), GEngine->GetSmallFont(), 0.76f);
}

void ABHHUD::DrawStatusPill(const FString& Label, float X, float Y, float W, const FLinearColor& AccentColor, bool bLit)
{
	if (!Canvas || !GEngine || Label.IsEmpty())
	{
		return;
	}

	const float H = 21.0f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.34f), X + 2.0f, Y + 2.0f, W, H);
	DrawRect(FLinearColor(0.028f, 0.034f, 0.035f, 0.88f), X, Y, W, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.28f : 0.10f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.90f : 0.34f), X + 8.0f, Y + 7.0f, 7.0f, 7.0f);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.18f : 0.05f), X + 6.0f, Y + 5.0f, 11.0f, 11.0f);
	DrawWrappedHudText(Label, X + 24.0f, Y + 5.0f, W - 29.0f, bLit ? MainText() : FLinearColor(0.58f, 0.64f, 0.64f, 0.86f), GEngine->GetSmallFont(), 0.60f, 10.0f, 1);
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

float ABHHUD::DrawWrappedHudText(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, const UFont* Font, float Scale, float LineHeight, int32 MaxLines) const
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
		DrawHudText(Lines[LineIndex], X, Y + LineIndex * LineHeight, Color, DrawFont, Scale);
	}

	return Lines.Num() * LineHeight;
}

void ABHHUD::DrawProgressBar(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, const FString& ValueText)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	const float BarH = 9.0f;
	const float BarY = Y + 13.0f;
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
	const FLinearColor ReadoutColor = bWarning ? FLinearColor(1.0f, 0.42f, 0.30f, 0.98f) : LabelColor;
	DrawHudText(Label, X, Y - 4.0f, LabelColor, GEngine->GetSmallFont(), 0.66f);
	DrawRightAlignedText(RightText, X + W, Y - 4.0f, ReadoutColor, GEngine->GetSmallFont(), 0.66f);
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
	const float TextScale = 0.58f;
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
	const float TextScale = 0.58f;
	Canvas->TextSize(GEngine->GetSmallFont(), Label, TextW, TextH, TextScale, TextScale);
	const float TextX = FMath::Clamp(ScreenPosition.X - TextW * 0.5f, 10.0f, FMath::Max(10.0f, Canvas->ClipX - TextW - 10.0f));
	const float TextY = FMath::Clamp(MarkerY + BracketH + 7.0f, 46.0f, Canvas->ClipY - TextH - 12.0f);
	DrawHudText(Label, TextX + 1.0f, TextY + 1.0f, ShadowColor, GEngine->GetSmallFont(), TextScale);
	DrawHudText(Label, TextX, TextY, MarkerColor, GEngine->GetSmallFont(), TextScale);
}

void ABHHUD::DrawRawMeter(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, bool bHighIsBad)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	const float LabelW = 58.0f;
	const float BarX = X + LabelW + 8.0f;
	const float BarW = FMath::Max(1.0f, W - LabelW - 8.0f);
	const float BarH = 9.0f;
	const float FillW = BarW * (ClampedValue / 100.0f);
	const bool bWarning = bHighIsBad ? ClampedValue >= HudHighMeterWarningThreshold : ClampedValue <= HudLowMeterWarningThreshold;
	const ABHPlayerController* BHPC = PlayerOwner ? Cast<ABHPlayerController>(PlayerOwner) : nullptr;
	const bool bHighContrast = BHPC && BHPC->IsHighContrastHudEnabled();
	const FLinearColor TextColor = bWarning ? FLinearColor(1.0f, 0.42f, 0.30f, 0.98f) : (bHighContrast ? FLinearColor(0.90f, 0.98f, 0.94f, 1.0f) : FLinearColor(0.62f, 0.70f, 0.68f, 0.88f));
	const FLinearColor EffectiveFill = bWarning ? FLinearColor(1.0f, 0.28f, 0.18f, 0.96f) : FillColor;

	DrawHudText(Label, X, Y - 2.0f, TextColor, GEngine->GetSmallFont(), 0.56f);
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
	if (!LocalPS || !LocalPS->IsAliveSurvivor())
	{
		return;
	}

	const float FearAlpha = Character ? FMath::Clamp(Character->GetFear() / 100.0f, 0.0f, 1.0f) : 0.0f;
	const float DreadAlpha = Character ? FMath::Clamp(Character->GetDread() / 100.0f, 0.0f, 1.0f) : 0.0f;
	const float PresenceAlpha = GameState ? FMath::Clamp(GameState->PresenceLevel / 100.0f, 0.0f, 1.0f) : 0.0f;
	const ABHPlayerController* BHPC = Cast<ABHPlayerController>(PlayerOwner);
	const float HorrorFlashAlpha = BHPC ? BHPC->GetHorrorCueFlashAlpha() : 0.0f;
	const float HorrorBlinkAlpha = BHPC ? BHPC->GetHorrorCueBlinkAlpha() : 0.0f;
	const float OverlayAlpha = FMath::Clamp(FMath::Max(FMath::Max(FearAlpha, DreadAlpha), PresenceAlpha) * 0.34f, 0.0f, 0.34f);
	if (OverlayAlpha <= 0.01f && HorrorFlashAlpha <= 0.01f && HorrorBlinkAlpha <= 0.01f)
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
}

void ABHHUD::DrawHeatSensor(const ABHCharacter* Character, const ABHGameState* GameState, float X, float Y)
{
	if (!Canvas || !GEngine || !Character || !GameState || !GetWorld())
	{
		return;
	}

	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.26f, 280.0f, 380.0f);
	const float PanelH = FMath::Clamp(Canvas->ClipY * 0.38f, 250.0f, 360.0f);
	const float MapX = X + 12.0f;
	const float MapY = Y + 31.0f;
	const float MapW = PanelW - 24.0f;
	const float MapH = PanelH - 47.0f;
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
	DrawHudText(TEXT("MAP"), X + 12.0f, Y + 10.0f, FLinearColor(0.75f, 0.69f, 0.57f, 0.80f), GEngine->GetSmallFont(), 0.64f);
	DrawRightAlignedText(TEXT("65M"), X + PanelW - 13.0f, Y + 10.0f, FLinearColor(0.55f, 0.50f, 0.43f, 0.70f), GEngine->GetSmallFont(), 0.56f);
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
	DrawRect(FLinearColor(0.26f, 0.78f, 0.68f, 0.76f), X + 12.0f, Y + PanelH - 13.0f, 4.0f, 4.0f);
	DrawRect(FLinearColor(0.86f, 0.50f, 0.18f, 0.80f), X + 42.0f, Y + PanelH - 13.0f, 4.0f, 4.0f);
	DrawRect(FLinearColor(0.92f, 0.08f, 0.04f, 0.86f), X + 72.0f, Y + PanelH - 13.0f, 4.0f, 4.0f);
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
		const float Scale = Beat.bPrimary ? 0.56f : 0.48f;
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
		&& (BHPS->IsAliveSurvivor() || (BHGS && BHGS->bRevisionMode && BHPS->PlayerRole == EBHPlayerRole::FakeHunter));
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
				PromptInfo.DisabledReason = FText::FromString(BHPS->PlayerRole == EBHPlayerRole::FakeHunter ? TEXT("CONTRIBUTE IN PHYSICS CLASSROOM") : TEXT("SURVIVOR OBJECTIVE"));
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

	float TextW = 0.0f;
	float TextH = 0.0f;
	const float Scale = bCanInteract ? 0.68f : 0.62f;
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
		const float DetailScale = 0.50f;
		Canvas->TextSize(GEngine->GetSmallFont(), DetailLine, DetailW, DetailH, DetailScale, DetailScale);
		const float DetailX = (Canvas->ClipX - DetailW) * 0.5f;
		const float DetailY = PromptY + TextH + 8.0f;
		const FLinearColor DetailColor = bCanInteract
			? (PromptInfo.bNoisy || PromptInfo.bDangerous
				? (bHighContrastHud ? FLinearColor(1.0f, 0.42f, 0.24f, 1.0f) : FLinearColor(0.96f, 0.28f, 0.14f, 0.84f))
				: (bHighContrastHud ? FLinearColor(0.82f, 0.94f, 0.86f, 0.96f) : FLinearColor(0.56f, 0.62f, 0.58f, 0.74f)))
			: (bHighContrastHud ? FLinearColor(0.92f, 0.74f, 0.52f, 0.96f) : FLinearColor(0.58f, 0.52f, 0.46f, 0.70f));
		DrawHudText(DetailLine, DetailX + 1.0f, DetailY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.46f), GEngine->GetSmallFont(), DetailScale);
		DrawHudText(DetailLine, DetailX, DetailY, DetailColor, GEngine->GetSmallFont(), DetailScale);
	}

	if (const ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target); Station && bCanViewStationQuestion)
	{
		DrawQuestionPanel(Station);
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
		const float Scale = bThreatRole ? 0.98f : 0.90f;
		const FLinearColor TextColor = bTeacherRole
			? FLinearColor(0.96f, 0.18f, 0.12f, Alpha)
			: (bHallMonitorRole
				? FLinearColor(1.0f, 0.56f, 0.18f, Alpha)
				: FLinearColor(0.82f, 0.78f, 0.66f, Alpha));

		float TextW = 0.0f;
		float TextH = 0.0f;
		Canvas->TextSize(GEngine->GetSmallFont(), Label, TextW, TextH, Scale, Scale);
		const float X = ScreenPosition.X - TextW * 0.5f;
		const float Y = ScreenPosition.Y - TextH * 0.5f;
		DrawHudText(Label, X + 1.0f, Y + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, Alpha * 0.70f), GEngine->GetSmallFont(), Scale);
		DrawHudText(Label, X, Y, TextColor, GEngine->GetSmallFont(), Scale);
		DrawLine(X + TextW * 0.18f, Y + TextH + 3.0f, X + TextW * 0.82f, Y + TextH + 3.0f, FLinearColor(TextColor.R, TextColor.G, TextColor.B, Alpha * 0.58f), 2.0f);
	}
}

void ABHHUD::DrawEquipmentStrip(const ABHGameState* GameState)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float SafePad = FMath::Max(22.0f, Canvas->ClipX * 0.018f);
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.44f, 430.0f, 680.0f);
	const float PanelH = 42.0f;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY - SafePad - PanelH;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.008f, 0.011f, 0.012f, 0.58f), FLinearColor(0.45f, 0.70f, 0.68f, 0.64f));

	const FString ModeText = !GameState ? FString(TEXT("OFFLINE")) : (GameState->bTestMode ? FString(TEXT("TEST LOOP")) : (GameState->bPracticeMode ? FString(TEXT("PRACTICE LAB")) : FString(TEXT("LIVE FEED"))));
	const FString PhaseText = GameState ? GameState->GetPhaseText().ToUpper() : FString(TEXT("NO SIGNAL"));
	const FString PresenceText = GameState ? FString::Printf(TEXT("PRESENCE %.0f%%"), FMath::Clamp(GameState->PresenceLevel, 0.0f, 100.0f)) : FString(TEXT("PRESENCE --"));
	const FString ExitText = GameState && GameState->bExitUnlocked ? FString(TEXT("EXIT OPEN")) : FString(TEXT("EXIT LOCKED"));
	const FLinearColor ModeColor = GameState && GameState->bTestMode ? FLinearColor(0.95f, 0.86f, 0.42f, 1.0f) : FLinearColor(0.48f, 0.86f, 0.78f, 1.0f);
	const FLinearColor PresenceColor = GameState && GameState->PresenceLevel >= HudHighMeterWarningThreshold ? FLinearColor(1.0f, 0.28f, 0.18f, 1.0f) : FLinearColor(0.62f, 0.82f, 0.78f, 1.0f);
	const float Gap = 8.0f;
	const float PillY = PanelY + 11.0f;
	const float PillW = (PanelW - 38.0f - Gap * 3.0f) / 4.0f;
	DrawStatusPill(ModeText, PanelX + 15.0f, PillY, PillW, ModeColor, GameState != nullptr);
	DrawStatusPill(PhaseText, PanelX + 15.0f + (PillW + Gap), PillY, PillW, FLinearColor(0.66f, 0.78f, 0.92f, 1.0f), GameState != nullptr);
	DrawStatusPill(PresenceText, PanelX + 15.0f + (PillW + Gap) * 2.0f, PillY, PillW, PresenceColor, GameState != nullptr);
	DrawStatusPill(ExitText, PanelX + 15.0f + (PillW + Gap) * 3.0f, PillY, PillW, GameState && GameState->bExitUnlocked ? FLinearColor(0.36f, 1.0f, 0.68f, 1.0f) : FLinearColor(0.96f, 0.42f, 0.34f, 1.0f), GameState != nullptr);
}

void ABHHUD::DrawSpectatorSupportPanel(const ABHGameState* GameState, const ABHPlayerState* PlayerState)
{
	if (!Canvas || !GEngine || !GameState || !PlayerState || PlayerState->PlayerRole != EBHPlayerRole::Spectator)
	{
		return;
	}

	const float SafePad = FMath::Max(18.0f, Canvas->ClipX * 0.014f);
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.42f, 430.0f, 640.0f);
	const float PanelH = 82.0f;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY - SafePad - PanelH;
	const FLinearColor Accent(0.54f, 0.66f, 0.96f, 0.90f);
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.010f, 0.013f, 0.018f, 0.82f), Accent);

	DrawHudText(TEXT("SPECTATOR SUPPORT"), PanelX + 20.0f, PanelY + 14.0f, FLinearColor(0.78f, 0.84f, 1.0f, 0.96f), GEngine->GetSmallFont(), 0.78f);
	DrawRightAlignedText(FString::Printf(TEXT("SENT %d"), FMath::Max(0, PlayerState->SpectatorEncouragementCount)), PanelX + PanelW - 20.0f, PanelY + 14.0f, FLinearColor(0.58f, 0.66f, 0.76f, 0.86f), GEngine->GetSmallFont(), 0.58f);

	const float KeyY = PanelY + 39.0f;
	const float KeyW = 30.0f;
	const float SlotW = (PanelW - 44.0f) / 4.0f;
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
		const float SlotX = PanelX + 22.0f + SlotW * Index;
		DrawKeyBox(SupportKeys[Index].Key, SlotX, KeyY, KeyW, 22.0f, Accent, true);
		DrawWrappedHudText(SupportKeys[Index].Label, SlotX + 36.0f, KeyY + 5.0f, SlotW - 40.0f, MainText(), GEngine->GetSmallFont(), 0.56f, 10.0f, 1);
	}

	const FString PrefLine = FString::Printf(TEXT("PREF %s / HOST APPROVES NEXT LOBBY"), *SpectatorRolePreferenceLabel(PlayerState->SpectatorRolePreference));
	DrawWrappedHudText(PrefLine, PanelX + 22.0f, PanelY + 65.0f, PanelW - 44.0f, FLinearColor(0.62f, 0.72f, 0.78f, 0.88f), GEngine->GetSmallFont(), 0.50f, 10.0f, 1);
}

void ABHHUD::DrawQuestionPanel(const ABHObjectiveStation* Station)
{
	if (!Canvas || !GEngine || !Station || !Station->IsDirectorActive() || Station->IsCompleted() || Station->IsQuestionSolved() || Station->GetQuestionChoiceCount() <= 0)
	{
		return;
	}

	const int32 ChoiceCount = FMath::Min(4, Station->GetQuestionChoiceCount());
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.60f, 560.0f, 920.0f);
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRevisionQuestion = BHGS && BHGS->bRevisionMode;
	// Draw a diagram whenever the question carries one, in any mode -- so visual tasks
	// also appear at nodes during standard play, not only in the classroom.
	const bool bShowDiagram = Station->GetQuestionDiagramType() != EBHDiagramType::None;
	const float DiagramH = bShowDiagram ? 118.0f : 0.0f;
	const float PanelH = 194.0f + DiagramH + ChoiceCount * 32.0f + (Station->GetQuestionFeedback().IsEmpty() ? 0.0f : 42.0f);
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
		PanelX + 22.0f, PanelY + 16.0f,
		bReviewQuestion ? FLinearColor(0.62f, 0.92f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.72f, 0.36f, 1.0f),
		GEngine->GetSmallFont(), 0.94f);
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
		DrawRightAlignedText(Meta, PanelX + PanelW - 22.0f, PanelY + 16.0f, FLinearColor(0.78f, 0.86f, 0.94f, 1.0f), GEngine->GetSmallFont(), 0.76f);
	}
	DrawWrappedHudText(Station->GetPhysicalTaskInstruction(), PanelX + 22.0f, PanelY + 42.0f, PanelW - 44.0f, FLinearColor(0.74f, 0.88f, 0.88f, 1.0f), GEngine->GetSmallFont(), 0.72f, 13.0f, 1);
	DrawWrappedHudText(Station->GetQuestionPrompt(), PanelX + 22.0f, PanelY + 62.0f, PanelW - 44.0f, MainText(), GEngine->GetSmallFont(), 0.92f, 17.0f, 2);

	float ChoiceStartY = PanelY + 112.0f;
	if (bShowDiagram)
	{
		DrawRevisionDiagram(Station, PanelX + 24.0f, PanelY + 108.0f, PanelW - 48.0f, DiagramH - 10.0f);
		ChoiceStartY = PanelY + 112.0f + DiagramH;
		// The collaborative "answer team" line is a classroom-mode concept only.
		if (bRevisionQuestion && !Station->GetRevisionTeamSummary().IsEmpty())
		{
			DrawWrappedHudText(FString::Printf(TEXT("Answer team: %s"), *Station->GetRevisionTeamSummary()), PanelX + 28.0f, ChoiceStartY - 17.0f, PanelW - 56.0f, FLinearColor(0.76f, 0.92f, 0.98f, 1.0f), GEngine->GetSmallFont(), 0.72f, 12.0f, 1);
		}
	}

	// Calculation questions use typed numeric entry instead of multiple choice, so the
	// student must actually compute the value rather than recognise it among options.
	if (bRevisionQuestion && Station->GetQuestionType() == EBHQuestionType::Calculation)
	{
		const ABHCharacter* LocalChar = Cast<ABHCharacter>(GetOwningPawn());
		const FString Typed = LocalChar ? LocalChar->GetNumericAnswerEntry() : FString();
		const float EntryY = ChoiceStartY + 4.0f;
		DrawRect(FLinearColor(0.05f, 0.06f, 0.06f, 0.86f), PanelX + 26.0f, EntryY - 5.0f, PanelW - 52.0f, 30.0f);
		DrawRect(FLinearColor(0.30f, 0.78f, 0.95f, 0.42f), PanelX + 26.0f, EntryY - 5.0f, 3.0f, 30.0f);
		DrawKeyBox(TEXT("0-9"), PanelX + 38.0f, EntryY - 2.0f, 40.0f, 22.0f, FLinearColor(0.40f, 0.82f, 0.96f, 0.94f), true);
		const FString Shown = Typed.IsEmpty() ? TEXT("_") : (Typed + TEXT("_"));
		DrawHudText(FString::Printf(TEXT("Your answer: %s"), *Shown), PanelX + 92.0f, EntryY + 2.0f, FLinearColor(0.92f, 0.98f, 1.0f, 1.0f), GEngine->GetSmallFont(), 0.92f);
		DrawWrappedHudText(TEXT("Type the value (digits, . and -). Backspace edits. Press Enter to submit."), PanelX + 28.0f, EntryY + 28.0f, PanelW - 56.0f, FLinearColor(0.66f, 0.78f, 0.84f, 0.92f), GEngine->GetSmallFont(), 0.62f, 11.0f, 1);
	}
	else
	{
		for (int32 Index = 0; Index < ChoiceCount; ++Index)
		{
			const float ChoiceY = ChoiceStartY + Index * 32.0f;
			const FLinearColor RowColor = Index % 2 == 0 ? FLinearColor(0.045f, 0.052f, 0.050f, 0.80f) : FLinearColor(0.034f, 0.041f, 0.040f, 0.80f);
			DrawRect(RowColor, PanelX + 26.0f, ChoiceY - 5.0f, PanelW - 52.0f, 27.0f);
			DrawRect(FLinearColor(0.95f, 0.56f, 0.18f, 0.34f), PanelX + 26.0f, ChoiceY - 5.0f, 3.0f, 27.0f);
			DrawKeyBox(FString::Printf(TEXT("%d"), Index + 1), PanelX + 38.0f, ChoiceY - 2.0f, 26.0f, 21.0f, FLinearColor(0.95f, 0.56f, 0.18f, 0.94f), true);
			DrawWrappedHudText(Station->GetQuestionChoice(Index), PanelX + 76.0f, ChoiceY + 1.0f, PanelW - 116.0f, FLinearColor(0.88f, 0.92f, 0.88f, 1.0f), GEngine->GetSmallFont(), 0.82f, 14.0f, 1);
		}
	}

	if (!Station->GetQuestionFeedback().IsEmpty())
	{
		const FLinearColor FeedbackColor = Station->IsQuestionFeedbackCorrect()
			? FLinearColor(0.42f, 1.0f, 0.62f, 1.0f)
			: FLinearColor(1.0f, 0.42f, 0.34f, 1.0f);
		DrawWrappedHudText(Station->GetQuestionFeedback(), PanelX + 22.0f, PanelY + PanelH - 38.0f, PanelW - 44.0f, FeedbackColor, GEngine->GetSmallFont(), 0.80f, 14.0f, 2);
	}
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
	// Cook-safe: load the imported .uasset by object path. Missing/uncooked assets return
	// null and the caller falls back to the procedural diagram. Cache the result either
	// way so a missing asset is not re-searched every frame.
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	DiagramTextureCache.Add(ObjectPath, Texture);
	return Texture;
}

void ABHHUD::DrawRevisionDiagram(const ABHObjectiveStation* Station, float X, float Y, float W, float H)
{
	if (!Canvas || !GEngine || !Station)
	{
		return;
	}

	const FBHDiagramParams& P = Station->GetQuestionDiagram();

	// Optional illustrated image diagram: if the question references one and it loads,
	// draw it letterboxed into the panel and skip the procedural schematic.
	if (P.HasImage())
	{
		if (UTexture2D* Tex = ResolveDiagramTexture(P.ImageSoftPath))
		{
			DrawRect(FLinearColor(0.02f, 0.025f, 0.028f, 0.92f), X, Y, W, H);
			const float TexW = FMath::Max(1.0f, static_cast<float>(Tex->GetSizeX()));
			const float TexH = FMath::Max(1.0f, static_cast<float>(Tex->GetSizeY()));
			const float Scale = FMath::Min((W - 12.0f) / TexW, (H - 12.0f) / TexH);
			const float DrawW = TexW * Scale;
			const float DrawH = TexH * Scale;
			const float DrawX = X + (W - DrawW) * 0.5f;
			const float DrawY = Y + (H - DrawH) * 0.5f;
			DrawTextureSimple(Tex, DrawX, DrawY, Scale, false);

			const FString ImageSubtopic = Station->GetQuestionSubtopic();
			if (!ImageSubtopic.IsEmpty())
			{
				DrawHudText(ImageSubtopic.ToUpper(), X + 10.0f, Y + 4.0f, FLinearColor(0.66f, 0.82f, 0.94f, 0.92f), GEngine->GetSmallFont(), 0.62f);
			}
			if (!Station->GetQuestionFormula().IsEmpty())
			{
				DrawRightAlignedText(FString::Printf(TEXT("Key idea: %s"), *Station->GetQuestionFormula()), X + W - 10.0f, Y + H - 18.0f, FLinearColor(0.95f, 0.84f, 0.45f, 1.0f), GEngine->GetSmallFont(), 0.66f);
			}
			return;
		}
		// Fall through to the procedural diagram when the texture is unavailable.
	}

	DrawRect(FLinearColor(0.025f, 0.032f, 0.036f, 0.88f), X, Y, W, H);
	DrawRect(FLinearColor(0.18f, 0.28f, 0.30f, 0.82f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(0.18f, 0.28f, 0.30f, 0.58f), X, Y + H - 1.0f, W, 1.0f);
	for (float GridX = X + 42.0f; GridX < X + W - 12.0f; GridX += 48.0f)
	{
		DrawRect(FLinearColor(0.42f, 0.58f, 0.58f, 0.055f), GridX, Y + 6.0f, 1.0f, H - 12.0f);
	}
	for (float GridY = Y + 30.0f; GridY < Y + H - 8.0f; GridY += 28.0f)
	{
		DrawRect(FLinearColor(0.42f, 0.58f, 0.58f, 0.055f), X + 8.0f, GridY, W - 16.0f, 1.0f);
	}
	DrawCornerBrackets(X + 6.0f, Y + 6.0f, W - 12.0f, H - 12.0f, FLinearColor(0.48f, 0.92f, 0.86f, 0.28f), 10.0f, 1.0f);

	const FLinearColor LineColor(0.48f, 0.92f, 0.86f, 1.0f);
	const FLinearColor WarmColor(0.95f, 0.62f, 0.28f, 1.0f);
	const float MidY = Y + H * 0.52f;
	const float Left = X + 26.0f;
	const float Right = X + W - 26.0f;
	const float Top = Y + 16.0f;
	const float Bottom = Y + H - 18.0f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	switch (Station->GetQuestionDiagramType())
	{
	case EBHDiagramType::MotionGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom - 8.0f, X + W * 0.42f, Y + H * 0.38f, LineColor, 3.0f);
		DrawLine(X + W * 0.42f, Y + H * 0.38f, Right, Y + H * 0.26f, LineColor, 3.0f);
		DrawHudText(TEXT("gradient = velocity"), Left + 8.0f, Top + 4.0f, WarmColor, GEngine->GetSmallFont(), 0.72f);
		break;
	case EBHDiagramType::VelocityGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawRect(FLinearColor(0.30f, 0.60f, 0.78f, 0.24f), Left + 10.0f, MidY, W * 0.38f, Bottom - MidY);
		DrawLine(Left + 10.0f, MidY, X + W * 0.52f, MidY, LineColor, 3.0f);
		DrawLine(X + W * 0.52f, MidY, Right, Top + 8.0f, LineColor, 3.0f);
		DrawHudText(TEXT("area = distance"), Left + 18.0f, Bottom - 34.0f, WarmColor, GEngine->GetSmallFont(), 0.72f);
		// Data-driven: axis captions and any value note for this question.
		if (!P.XAxis.IsEmpty()) { DrawRightAlignedText(P.XAxis, Right - 6.0f, Bottom - 14.0f, FLinearColor(0.70f, 0.78f, 0.82f, 0.90f), GEngine->GetSmallFont(), 0.60f); }
		if (!P.YAxis.IsEmpty()) { DrawHudText(P.YAxis, Left + 4.0f, Top - 4.0f, FLinearColor(0.70f, 0.78f, 0.82f, 0.90f), GEngine->GetSmallFont(), 0.60f); }
		if (P.HasValues() && !P.LabelA.IsEmpty()) { DrawHudText(P.LabelA, Left + 18.0f, Top + 6.0f, MainText(), GEngine->GetSmallFont(), 0.60f); }
		break;
	case EBHDiagramType::ForceArrows:
		DrawLine(X + W * 0.50f, MidY, X + W * 0.26f, MidY, WarmColor, 4.0f);
		DrawLine(X + W * 0.50f, MidY, X + W * 0.74f, MidY, LineColor, 4.0f);
		DrawLine(X + W * 0.74f, MidY, X + W * 0.70f, MidY - 10.0f, LineColor, 3.0f);
		DrawLine(X + W * 0.74f, MidY, X + W * 0.70f, MidY + 10.0f, LineColor, 3.0f);
		DrawHudText(TEXT("resultant force"), X + W * 0.39f, Top + 8.0f, MainText(), GEngine->GetSmallFont(), 0.78f);
		// Data-driven: label the two force magnitudes shown by the arrows.
		if (P.HasValues())
		{
			if (!P.LabelA.IsEmpty()) { DrawHudText(P.LabelA, X + W * 0.16f, MidY - 18.0f, WarmColor, GEngine->GetSmallFont(), 0.66f); }
			if (!P.LabelB.IsEmpty()) { DrawHudText(P.LabelB, X + W * 0.66f, MidY - 18.0f, LineColor, GEngine->GetSmallFont(), 0.66f); }
		}
		break;
	case EBHDiagramType::SpringGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Right - 32.0f, Top + 18.0f, LineColor, 3.0f);
		DrawHudText(TEXT("gradient = k"), Left + 18.0f, Top + 6.0f, WarmColor, GEngine->GetSmallFont(), 0.76f);
		break;
	case EBHDiagramType::MomentBeam:
		DrawRect(FLinearColor(0.55f, 0.50f, 0.38f, 1.0f), Left, MidY, Right - Left, 8.0f);
		DrawLine(X + W * 0.48f, MidY + 8.0f, X + W * 0.48f - 14.0f, Bottom, WarmColor, 3.0f);
		DrawLine(X + W * 0.48f, MidY + 8.0f, X + W * 0.48f + 14.0f, Bottom, WarmColor, 3.0f);
		DrawLine(X + W * 0.78f, MidY - 30.0f, X + W * 0.78f, MidY, LineColor, 4.0f);
		DrawHudText(TEXT("moment = Fd"), Left + 14.0f, Top + 8.0f, MainText(), GEngine->GetSmallFont(), 0.78f);
		// Data-driven: label the distances/forces either side of the pivot.
		if (P.HasValues())
		{
			if (!P.LabelA.IsEmpty()) { DrawHudText(P.LabelA, Left + 14.0f, MidY - 18.0f, WarmColor, GEngine->GetSmallFont(), 0.64f); }
			if (!P.LabelC.IsEmpty()) { DrawRightAlignedText(P.LabelC, Right - 6.0f, MidY - 18.0f, LineColor, GEngine->GetSmallFont(), 0.64f); }
			if (!P.LabelB.IsEmpty()) { DrawHudText(P.LabelB, Left + 14.0f, Bottom - 12.0f, MainText(), GEngine->GetSmallFont(), 0.60f); }
			if (!P.LabelD.IsEmpty()) { DrawRightAlignedText(P.LabelD, Right - 6.0f, Bottom - 12.0f, MainText(), GEngine->GetSmallFont(), 0.60f); }
		}
		break;
	case EBHDiagramType::Circuit:
		DrawLine(Left, Top + 16.0f, Right, Top + 16.0f, LineColor, 2.0f);
		DrawLine(Right, Top + 16.0f, Right, Bottom - 10.0f, LineColor, 2.0f);
		DrawLine(Right, Bottom - 10.0f, Left, Bottom - 10.0f, LineColor, 2.0f);
		DrawLine(Left, Bottom - 10.0f, Left, Top + 16.0f, LineColor, 2.0f);
		DrawRect(WarmColor, Left + W * 0.36f, Top + 8.0f, 34.0f, 16.0f);
		DrawHudText(TEXT("A series | V parallel"), Left + 18.0f, MidY - 8.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		// Data-driven: label the resistor(s) and supply with their actual values.
		if (P.HasValues())
		{
			if (!P.LabelA.IsEmpty()) { DrawHudText(P.LabelA, Left + W * 0.30f, Top + 26.0f, WarmColor, GEngine->GetSmallFont(), 0.62f); }
			if (!P.LabelC.IsEmpty()) { DrawRightAlignedText(P.LabelC, Right - 8.0f, Top + 26.0f, WarmColor, GEngine->GetSmallFont(), 0.62f); }
			if (!P.LabelB.IsEmpty()) { DrawHudText(P.LabelB, Left + 18.0f, Bottom - 8.0f, LineColor, GEngine->GetSmallFont(), 0.62f); }
		}
		break;
	case EBHDiagramType::IVGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Right - 24.0f, Top + 12.0f, LineColor, 3.0f);
		// Data-driven: mark the (V, I) reading using ValueC/ValueD as the axis maxima.
		if (P.HasValues() && P.ValueC > 0.0f && P.ValueD > 0.0f)
		{
			const float PtX = Left + (Right - Left) * FMath::Clamp(P.ValueA / P.ValueC, 0.0f, 1.0f);
			const float PtY = Bottom - (Bottom - Top) * FMath::Clamp(P.ValueB / P.ValueD, 0.0f, 1.0f);
			DrawRect(WarmColor, PtX - 3.0f, PtY - 3.0f, 6.0f, 6.0f);
		}
		{
			const FString IVLabel = !P.LabelA.IsEmpty() ? P.LabelA : FString(TEXT("straight: ohmic"));
			DrawHudText(IVLabel, Left + 16.0f, Top + 8.0f, WarmColor, GEngine->GetSmallFont(), 0.74f);
		}
		if (!P.XAxis.IsEmpty())
		{
			DrawRightAlignedText(P.XAxis, Right - 6.0f, Bottom - 14.0f, FLinearColor(0.70f, 0.78f, 0.82f, 0.90f), GEngine->GetSmallFont(), 0.60f);
		}
		if (!P.YAxis.IsEmpty())
		{
			DrawHudText(P.YAxis, Left + 4.0f, Top - 4.0f, FLinearColor(0.70f, 0.78f, 0.82f, 0.90f), GEngine->GetSmallFont(), 0.60f);
		}
		break;
	case EBHDiagramType::StaticCharge:
		DrawHudText(TEXT("+ + +"), Left + 24.0f, MidY - 8.0f, WarmColor, GEngine->GetSmallFont(), 1.0f);
		DrawHudText(TEXT("- - -"), Right - 92.0f, MidY - 8.0f, LineColor, GEngine->GetSmallFont(), 1.0f);
		DrawLine(X + W * 0.42f, MidY, X + W * 0.58f, MidY, MainText(), 3.0f);
		DrawHudText(TEXT("opposites attract"), X + W * 0.38f, Top + 8.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		break;
	case EBHDiagramType::Wave:
	{
		const int32 Segments = 36;
		// Data-driven: amplitude fraction (ValueA) and cycle count (ValueB) shape the wave.
		// Defaults reproduce the original 2-cycle schematic.
		const float AmpFrac = P.ValueA > 0.0f ? FMath::Clamp(P.ValueA, 0.05f, 0.45f) : 0.22f;
		const float Cycles = P.ValueB > 0.0f ? FMath::Clamp(P.ValueB, 1.0f, 6.0f) : 2.0f;
		FVector2D Prev(Left, MidY);
		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float T = static_cast<float>(Index) / Segments;
			const float PX = FMath::Lerp(Left, Right, T);
			const float PY = MidY + FMath::Sin(T * PI * 2.0f * Cycles + Now * 2.0f) * H * AmpFrac;
			DrawLine(Prev.X, Prev.Y, PX, PY, LineColor, 2.5f);
			Prev = FVector2D(PX, PY);
		}
		const FString WaveLabel = P.HasValues()
			? FString::Printf(TEXT("%s | %s"), *P.LabelA, *P.LabelB)
			: FString(TEXT("amplitude | wavelength"));
		DrawHudText(WaveLabel, Left + 14.0f, Top + 6.0f, WarmColor, GEngine->GetSmallFont(), 0.74f);
		break;
	}
	case EBHDiagramType::EMSpectrum:
	{
		const TCHAR* Labels[] = {TEXT("R"), TEXT("M"), TEXT("IR"), TEXT("VIS"), TEXT("UV"), TEXT("X"), TEXT("G")};
		const float SegmentW = (Right - Left) / 7.0f;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float SX = Left + SegmentW * Index;
			DrawRect(Index % 2 == 0 ? FLinearColor(0.22f, 0.32f, 0.42f, 1.0f) : FLinearColor(0.34f, 0.26f, 0.42f, 1.0f), SX, MidY - 16.0f, SegmentW - 3.0f, 32.0f);
			DrawHudText(Labels[Index], SX + 8.0f, MidY - 5.0f, MainText(), GEngine->GetSmallFont(), 0.72f);
		}
		DrawHudText(TEXT("long wavelength -> high frequency"), Left + 10.0f, Top + 4.0f, WarmColor, GEngine->GetSmallFont(), 0.68f);
		break;
	}
	case EBHDiagramType::RayDiagram:
	{
		const float CX = X + W * 0.50f;
		DrawLine(CX, Top, CX, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		// Surface line through the strike point.
		DrawLine(Left, MidY, Right, MidY, FLinearColor(0.45f, 0.50f, 0.52f, 0.8f), 1.0f);
		// Data-driven: incident/reflected rays drawn at the question's angle from the normal.
		const float AngleDeg = P.AngleOrShape > 0.0f ? FMath::Clamp(P.AngleOrShape, 5.0f, 80.0f) : 35.0f;
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const float RayLen = (MidY - Top) * 0.92f;
		const float Dx = FMath::Sin(Rad) * RayLen;
		const float Dy = FMath::Cos(Rad) * RayLen;
		DrawLine(CX - Dx, MidY - Dy, CX, MidY, WarmColor, 3.0f);
		DrawLine(CX, MidY, CX + Dx, MidY - Dy, LineColor, 3.0f);
		const FString RayLabel = !P.LabelA.IsEmpty() ? P.LabelA : FString(TEXT("normal | i = r"));
		DrawHudText(RayLabel, Left + 8.0f, Top + 6.0f, MainText(), GEngine->GetSmallFont(), 0.72f);
		break;
	}
	case EBHDiagramType::Sankey:
		DrawRect(LineColor, Left, MidY - 12.0f, W * 0.42f, 24.0f);
		DrawRect(FLinearColor(0.52f, 0.90f, 0.54f, 1.0f), X + W * 0.50f, MidY - 10.0f, W * 0.28f, 20.0f);
		DrawRect(WarmColor, X + W * 0.50f, MidY + 18.0f, W * 0.20f, 14.0f);
		DrawHudText(TEXT("input -> useful + wasted"), Left + 12.0f, Top + 6.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		// Data-driven: label the input / useful / wasted arrows with their values.
		if (P.HasValues())
		{
			if (!P.LabelA.IsEmpty()) { DrawHudText(P.LabelA, Left + 6.0f, MidY - 26.0f, MainText(), GEngine->GetSmallFont(), 0.60f); }
			if (!P.LabelB.IsEmpty()) { DrawHudText(P.LabelB, X + W * 0.50f, MidY - 24.0f, FLinearColor(0.52f, 0.90f, 0.54f, 1.0f), GEngine->GetSmallFont(), 0.58f); }
			if (!P.LabelC.IsEmpty()) { DrawHudText(P.LabelC, X + W * 0.50f, MidY + 34.0f, WarmColor, GEngine->GetSmallFont(), 0.58f); }
		}
		break;
	case EBHDiagramType::EnergyChain:
	default:
		DrawRect(FLinearColor(0.26f, 0.42f, 0.34f, 1.0f), Left, MidY - 14.0f, 90.0f, 28.0f);
		DrawLine(Left + 95.0f, MidY, Left + 150.0f, MidY, LineColor, 3.0f);
		DrawRect(FLinearColor(0.35f, 0.34f, 0.52f, 1.0f), Left + 156.0f, MidY - 14.0f, 96.0f, 28.0f);
		DrawLine(Left + 258.0f, MidY, Left + 312.0f, MidY, LineColor, 3.0f);
		DrawRect(FLinearColor(0.46f, 0.31f, 0.28f, 1.0f), Left + 318.0f, MidY - 14.0f, 96.0f, 28.0f);
		DrawHudText(TEXT("store -> pathway -> store"), Left + 12.0f, Top + 6.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		// Data-driven: label the three boxes of this question's energy chain.
		if (P.HasValues())
		{
			if (!P.LabelA.IsEmpty()) { DrawHudText(P.LabelA, Left + 6.0f, MidY + 18.0f, MainText(), GEngine->GetSmallFont(), 0.56f); }
			if (!P.LabelB.IsEmpty()) { DrawHudText(P.LabelB, Left + 160.0f, MidY + 18.0f, MainText(), GEngine->GetSmallFont(), 0.56f); }
			if (!P.LabelC.IsEmpty()) { DrawHudText(P.LabelC, Left + 322.0f, MidY + 18.0f, MainText(), GEngine->GetSmallFont(), 0.56f); }
		}
		break;
	}

	// Caption the diagram with this question's specific subtopic so it reads as the
	// concept under test, not a generic schematic. (Answer-safe: never reveals the value.)
	const FString Subtopic = Station->GetQuestionSubtopic();
	if (!Subtopic.IsEmpty())
	{
		DrawHudText(Subtopic.ToUpper(), X + 10.0f, Y + 4.0f, FLinearColor(0.66f, 0.82f, 0.94f, 0.92f), GEngine->GetSmallFont(), 0.62f);
	}

	if (!Station->GetQuestionFormula().IsEmpty())
	{
		DrawRightAlignedText(FString::Printf(TEXT("Key idea: %s"), *Station->GetQuestionFormula()), X + W - 10.0f, Y + H - 18.0f, FLinearColor(0.95f, 0.84f, 0.45f, 1.0f), GEngine->GetSmallFont(), 0.66f);
	}
}

void ABHHUD::DrawPhaseBanner(const ABHGameState* GameState, const ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !GameState || !GetWorld())
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
	FLinearColor Accent(0.40f, 0.90f, 0.82f, 1.0f);

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
		Accent = FLinearColor(0.30f, 0.92f, 0.82f, 1.0f);
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
		Accent = FLinearColor(0.42f, 1.0f, 0.62f, 1.0f);
		break;
	case EBHRoundPhase::HunterWin:
		Title = TEXT("TEACHER WINS");
		Subtitle = TEXT("Round complete. Returning to lobby shortly.");
		Accent = FLinearColor(1.0f, 0.24f, 0.16f, 1.0f);
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

	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.48f, 420.0f, 720.0f);
	const float PanelH = 86.0f;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY * 0.20f - (1.0f - SmoothAlpha) * 18.0f;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, WithAlpha(FLinearColor(0.012f, 0.014f, 0.016f, 0.86f), SmoothAlpha), WithAlpha(Accent, SmoothAlpha));

	float TitleW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(GEngine->GetSmallFont(), Title, TitleW, TextH, 1.34f, 1.34f);
	DrawHudText(Title, PanelX + (PanelW - TitleW) * 0.5f, PanelY + 17.0f, WithAlpha(Accent, SmoothAlpha), GEngine->GetSmallFont(), 1.34f);

	DrawWrappedHudText(Subtitle, PanelX + 28.0f, PanelY + 52.0f, PanelW - 56.0f, WithAlpha(FLinearColor(0.82f, 0.90f, 0.88f, 1.0f), SmoothAlpha), GEngine->GetSmallFont(), 0.82f, 14.0f, 1);
}

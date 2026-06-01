#include "BHGameState.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FString BHPluralSuffix(int32 Count)
	{
		return Count == 1 ? FString() : FString(TEXT("s"));
	}

	FString BHGameStateTrainPhaseLabel(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("Arrival");
		case EBHTrainPhase::Recap:
			return TEXT("Recap");
		case EBHTrainPhase::BonusQuestion:
			return TEXT("Bonus");
		case EBHTrainPhase::Shop:
			return TEXT("Shop");
		case EBHTrainPhase::StationStop:
			return TEXT("Station stop");
		case EBHTrainPhase::Departing:
			return TEXT("Departing");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("Train");
		}
	}

	FString BHGameStateFinalEscapeLabel(EBHFinalEscapeState State)
	{
		switch (State)
		{
		case EBHFinalEscapeState::Locked:
			return TEXT("Locked");
		case EBHFinalEscapeState::Cutscene:
			return TEXT("Unlocking");
		case EBHFinalEscapeState::EscapeActive:
			return TEXT("Board now");
		case EBHFinalEscapeState::Departed:
			return TEXT("Departed");
		case EBHFinalEscapeState::Failed:
			return TEXT("Failed");
		case EBHFinalEscapeState::Inactive:
		default:
			return TEXT("Inactive");
		}
	}

	FString BHGameStateClock(float Seconds)
	{
		const int32 ClampedSeconds = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), ClampedSeconds / 60, ClampedSeconds % 60);
	}

	FString BHGameStateTrainAction(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("Next: board now; recap starts when doors close.");
		case EBHTrainPhase::Recap:
			return TEXT("Next: read recap boards; snacks, drinks, and minigames are optional while you wait.");
		case EBHTrainPhase::BonusQuestion:
			return TEXT("Next: answer the bonus terminal with 1-4, or play minigames for small role points.");
		case EBHTrainPhase::Shop:
			return TEXT("Next: buy upgrades at role-eligible terminals; food, drinks, and minigames stay optional.");
		case EBHTrainPhase::StationStop:
			return TEXT("Next: board before departure; outside players are pulled aboard.");
		case EBHTrainPhase::Departing:
			return TEXT("Next: hold position while the next route loads.");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("Next: wait for train route data.");
		}
	}

	FString BHGameStateTrainDoorStatus(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
		case EBHTrainPhase::StationStop:
			return TEXT("doors open");
		case EBHTrainPhase::Departing:
			return TEXT("doors closed for departure");
		case EBHTrainPhase::Recap:
		case EBHTrainPhase::BonusQuestion:
		case EBHTrainPhase::Shop:
			return TEXT("doors closed while moving");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("doors pending");
		}
	}

	FString BHGameStateProgressCount(const TCHAR* Label, int32 Completed, int32 Required)
	{
		if (Required <= 0)
		{
			return FString::Printf(TEXT("%s clear"), Label);
		}

		const int32 ClampedRequired = FMath::Max(1, Required);
		const int32 ClampedCompleted = FMath::Clamp(Completed, 0, ClampedRequired);
		return FString::Printf(TEXT("%s %d/%d"), Label, ClampedCompleted, ClampedRequired);
	}
}

ABHGameState::ABHGameState()
{
	RoundPhase = EBHRoundPhase::Lobby;
	RemainingTime = 0;
	BreakersCompleted = 0;
	BreakersRequired = 3;
	SideObjectivesCompleted = 0;
	SideObjectivesRequired = 0;
	bExitUnlocked = false;
	RoundSeed = 0;
	ObjectiveText = TEXT("Reach the lobby and ready up.");
	NextLevelName = TEXT("Facility");
	ActiveLevelName = TEXT("Facility");
	TargetHunterCount = 1;
	ObjectiveIntensity = 1;
	bInfectionMode = false;
	bPartyPace = false;
	RoundModifier = EBHRoundModifier::None;
	NextFogPreset = EBHFogPreset::Heavy;
	ActiveFogPreset = EBHFogPreset::Heavy;
	bFogPresetOverride = false;
	PresenceLevel = 0.0f;
	PresenceText = TEXT("The building is listening.");
	PresencePulse = 0;
	ObjectiveBeats.Reset();
	bPracticeMode = false;
	bTutorialMode = false;
	bTestMode = false;
	bBotMode = false;
	TargetBotCount = 0;
	BotDifficulty = EBHBotDifficulty::Normal;
	RevisionMode = EBHRevisionMode::None;
	bRevisionMode = false;
	RevisionTopicMask = 0x0F;
	RevisionDifficultyMix = EBHRevisionDifficultyMix::Adaptive;
	RevisionClassThreshold = 70.0f;
	RevisionIndividualThreshold = 50.0f;
	RevisionRoundDuration = 600;
	RevisionScareIntensity = 3;
	RevisionContributionTarget = 1;
	RevisionClassMasteryAverage = 0.0f;
	RevisionWeakTopic = EBHPhysicsTopic::ForcesAndMotion;
	RevisionReviewTimeRemaining = 0;
	RevisionReviewText = TEXT("");
	TrainPhase = EBHTrainPhase::Inactive;
	TrainStageIndex = 0;
	TrainPhaseEndServerTime = 0.0f;
	TrainDestinationName = TEXT("");
	TrainAnnouncement = TEXT("");
	TrainRecapOverview = TEXT("");
	TrainRecapTopics = TEXT("");
	TrainRecapMissedQuestions = TEXT("");
	TrainRecapTips = TEXT("");
	FinalEscapeState = EBHFinalEscapeState::Inactive;
	FinalEscapeCutsceneEndServerTime = 0.0f;
	FinalEscapeEndServerTime = 0.0f;
	HunterReleaseServerTime = 0.0f;
	bCaptureDisabled = false;
	bPlayerInputFrozen = false;
	bHunterInputFrozen = false;
}

void ABHGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHGameState, RoundPhase);
	DOREPLIFETIME(ABHGameState, RemainingTime);
	DOREPLIFETIME(ABHGameState, BreakersCompleted);
	DOREPLIFETIME(ABHGameState, BreakersRequired);
	DOREPLIFETIME(ABHGameState, SideObjectivesCompleted);
	DOREPLIFETIME(ABHGameState, SideObjectivesRequired);
	DOREPLIFETIME(ABHGameState, bExitUnlocked);
	DOREPLIFETIME(ABHGameState, RoundSeed);
	DOREPLIFETIME(ABHGameState, ObjectiveText);
	DOREPLIFETIME(ABHGameState, NextLevelName);
	DOREPLIFETIME(ABHGameState, ActiveLevelName);
	DOREPLIFETIME(ABHGameState, TargetHunterCount);
	DOREPLIFETIME(ABHGameState, ObjectiveIntensity);
	DOREPLIFETIME(ABHGameState, bInfectionMode);
	DOREPLIFETIME(ABHGameState, bPartyPace);
	DOREPLIFETIME(ABHGameState, RoundModifier);
	DOREPLIFETIME(ABHGameState, NextFogPreset);
	DOREPLIFETIME(ABHGameState, ActiveFogPreset);
	DOREPLIFETIME(ABHGameState, bFogPresetOverride);
	DOREPLIFETIME(ABHGameState, PresenceLevel);
	DOREPLIFETIME(ABHGameState, PresenceText);
	DOREPLIFETIME(ABHGameState, PresencePulse);
	DOREPLIFETIME(ABHGameState, ObjectiveBeats);
	DOREPLIFETIME(ABHGameState, bPracticeMode);
	DOREPLIFETIME(ABHGameState, bTutorialMode);
	DOREPLIFETIME(ABHGameState, bTestMode);
	DOREPLIFETIME(ABHGameState, bBotMode);
	DOREPLIFETIME(ABHGameState, TargetBotCount);
	DOREPLIFETIME(ABHGameState, BotDifficulty);
	DOREPLIFETIME(ABHGameState, RevisionMode);
	DOREPLIFETIME(ABHGameState, bRevisionMode);
	DOREPLIFETIME(ABHGameState, RevisionTopicMask);
	DOREPLIFETIME(ABHGameState, RevisionDifficultyMix);
	DOREPLIFETIME(ABHGameState, RevisionClassThreshold);
	DOREPLIFETIME(ABHGameState, RevisionIndividualThreshold);
	DOREPLIFETIME(ABHGameState, RevisionRoundDuration);
	DOREPLIFETIME(ABHGameState, RevisionScareIntensity);
	DOREPLIFETIME(ABHGameState, RevisionContributionTarget);
	DOREPLIFETIME(ABHGameState, RevisionClassMasteryAverage);
	DOREPLIFETIME(ABHGameState, RevisionWeakTopic);
	DOREPLIFETIME(ABHGameState, RevisionReviewTimeRemaining);
	DOREPLIFETIME(ABHGameState, RevisionReviewText);
	DOREPLIFETIME(ABHGameState, TrainPhase);
	DOREPLIFETIME(ABHGameState, TrainStageIndex);
	DOREPLIFETIME(ABHGameState, TrainPhaseEndServerTime);
	DOREPLIFETIME(ABHGameState, TrainDestinationName);
	DOREPLIFETIME(ABHGameState, TrainAnnouncement);
	DOREPLIFETIME(ABHGameState, TrainRecapOverview);
	DOREPLIFETIME(ABHGameState, TrainRecapTopics);
	DOREPLIFETIME(ABHGameState, TrainRecapMissedQuestions);
	DOREPLIFETIME(ABHGameState, TrainRecapTips);
	DOREPLIFETIME(ABHGameState, FinalEscapeState);
	DOREPLIFETIME(ABHGameState, FinalEscapeCutsceneEndServerTime);
	DOREPLIFETIME(ABHGameState, FinalEscapeEndServerTime);
	DOREPLIFETIME(ABHGameState, HunterReleaseServerTime);
	DOREPLIFETIME(ABHGameState, bCaptureDisabled);
	DOREPLIFETIME(ABHGameState, bPlayerInputFrozen);
	DOREPLIFETIME(ABHGameState, bHunterInputFrozen);
}

FString ABHGameState::GetPhaseText() const
{
	if (bTestMode && RoundPhase == EBHRoundPhase::Hunt)
	{
		return TEXT("Test Round");
	}

	if (bTestMode && RoundPhase == EBHRoundPhase::Lobby)
	{
		return TEXT("Test Lobby");
	}

	if (bPracticeMode && RoundPhase == EBHRoundPhase::Hunt)
	{
		return TEXT("Practice Lab");
	}

	if (bRevisionMode && RoundPhase == EBHRoundPhase::Hunt)
	{
		return TEXT("Physics Classroom");
	}

	if (bRevisionMode && RoundPhase == EBHRoundPhase::Lobby)
	{
		return TEXT("Classroom Lobby");
	}

	if (bBotMode && RoundPhase == EBHRoundPhase::Lobby)
	{
		return TEXT("Bot Lobby");
	}

	switch (RoundPhase)
	{
	case EBHRoundPhase::Lobby:
		return TEXT("Lobby");
	case EBHRoundPhase::Prep:
		return TEXT("Role Warmup");
	case EBHRoundPhase::Hunt:
		return TEXT("Hunt");
	case EBHRoundPhase::Intermission:
		return TEXT("Train Intermission");
	case EBHRoundPhase::FinalEscape:
		return TEXT("Final Escape");
	case EBHRoundPhase::SurvivorsWin:
		return TEXT("Survivors Win");
	case EBHRoundPhase::HunterWin:
		return TEXT("Teacher Wins");
	default:
		return TEXT("Unknown");
	}
}

FString ABHGameState::GetRoundModifierText() const
{
	return GetRoundModifierTextFor(RoundModifier);
}

FString ABHGameState::GetRoundModifierHint() const
{
	return GetRoundModifierHintFor(RoundModifier);
}

FString ABHGameState::GetPublicObjectiveActionText() const
{
	switch (RoundPhase)
	{
	case EBHRoundPhase::Lobby:
		return TEXT("Next: ready up, then wait for the host start.");
	case EBHRoundPhase::Prep:
		return TEXT("Next: warm up roles and routes before the hunt starts.");
	case EBHRoundPhase::Intermission:
		return BHGameStateTrainAction(TrainPhase);
	case EBHRoundPhase::FinalEscape:
		if (FinalEscapeState == EBHFinalEscapeState::Cutscene)
		{
			return TEXT("Next: hold position while evacuation doors unlock; Teacher release is delayed.");
		}
		if (FinalEscapeState == EBHFinalEscapeState::Departed || FinalEscapeState == EBHFinalEscapeState::Failed)
		{
			return TEXT("Next: wait for the round result.");
		}
		return TEXT("Next: board any open evacuation train door before departure.");
	case EBHRoundPhase::SurvivorsWin:
		return TEXT("Next: survivors escaped; returning to lobby.");
	case EBHRoundPhase::HunterWin:
		return TEXT("Next: Teacher wins; returning to lobby.");
	case EBHRoundPhase::Hunt:
	default:
		break;
	}

	if (bExitUnlocked)
	{
		return TEXT("Next: reach the active exit gate.");
	}

	const int32 RemainingBreakers = FMath::Max(0, BreakersRequired - BreakersCompleted);
	const int32 RemainingStations = FMath::Max(0, SideObjectivesRequired - SideObjectivesCompleted);
	if (bRevisionMode)
	{
		if (RevisionReviewTimeRemaining > 0)
		{
			return TEXT("Next: review the weak topic and correct missed answers.");
		}
		if (RemainingStations > 0)
		{
			return FString::Printf(TEXT("Next: answer station questions; %d class node%s left."),
				RemainingStations,
				*BHPluralSuffix(RemainingStations));
		}
		if (RevisionClassMasteryAverage < RevisionClassThreshold)
		{
			return FString::Printf(TEXT("Next: raise class mastery to %.0f%%."), RevisionClassThreshold);
		}
		return FString::Printf(TEXT("Next: help everyone reach %.0f%% mastery and %d contribution%s."),
			RevisionIndividualThreshold,
			FMath::Max(1, RevisionContributionTarget),
			*BHPluralSuffix(FMath::Max(1, RevisionContributionTarget)));
	}

	if (RemainingBreakers > 0 && RemainingStations > 0)
	{
		return FString::Printf(TEXT("Next: repair %d breaker%s and finish %d station task%s."),
			RemainingBreakers,
			*BHPluralSuffix(RemainingBreakers),
			RemainingStations,
			*BHPluralSuffix(RemainingStations));
	}
	if (RemainingBreakers > 0)
	{
		return FString::Printf(TEXT("Next: repair %d breaker%s to restore exit power."),
			RemainingBreakers,
			*BHPluralSuffix(RemainingBreakers));
	}
	if (RemainingStations > 0)
	{
		return FString::Printf(TEXT("Next: complete %d station task%s to unlock the exit."),
			RemainingStations,
			*BHPluralSuffix(RemainingStations));
	}
	return TEXT("Next: all objectives are complete; find the active exit gate.");
}

FString ABHGameState::GetObjectiveProgressText() const
{
	if (RoundPhase == EBHRoundPhase::Intermission)
	{
		const float ServerNow = GetServerWorldTimeSeconds();
		const FString Destination = TrainDestinationName.IsEmpty() ? FString(TEXT("next stop")) : TrainDestinationName;
		return FString::Printf(TEXT("Train %s | %s | Destination %s | %s"),
			*BHGameStateTrainPhaseLabel(TrainPhase),
			*BHGameStateClock(TrainPhaseEndServerTime - ServerNow),
			*Destination,
			*BHGameStateTrainDoorStatus(TrainPhase));
	}

	if (RoundPhase == EBHRoundPhase::FinalEscape)
	{
		const float ServerNow = GetServerWorldTimeSeconds();
		if (FinalEscapeState == EBHFinalEscapeState::Cutscene)
		{
			return FString::Printf(TEXT("Final train | Unlocking | Control %s | Teacher release %s | Departs %s"),
				*BHGameStateClock(FinalEscapeCutsceneEndServerTime - ServerNow),
				*BHGameStateClock(HunterReleaseServerTime - ServerNow),
				*BHGameStateClock(FinalEscapeEndServerTime - ServerNow));
		}

		const FString ReleaseText = HunterReleaseServerTime > ServerNow
			? FString::Printf(TEXT("Teacher release %s"), *BHGameStateClock(HunterReleaseServerTime - ServerNow))
			: FString(TEXT("Teacher released"));
		return FString::Printf(TEXT("Final train | %s | Departs %s | %s"),
			*BHGameStateFinalEscapeLabel(FinalEscapeState),
			*BHGameStateClock(FinalEscapeEndServerTime - ServerNow),
			*ReleaseText);
	}

	const FString ExitText = bExitUnlocked ? FString(TEXT("Exit open")) : FString(TEXT("Exit locked"));
	if (bRevisionMode)
	{
		return FString::Printf(TEXT("Class %.0f%%/%.0f%% | %s | Contribution target %d | %s"),
			RevisionClassMasteryAverage,
			RevisionClassThreshold,
			*BHGameStateProgressCount(TEXT("Nodes"), SideObjectivesCompleted, SideObjectivesRequired),
			FMath::Max(1, RevisionContributionTarget),
			*ExitText);
	}

	return FString::Printf(TEXT("%s | %s | Presence %.0f%% | %s"),
		*BHGameStateProgressCount(TEXT("Power"), BreakersCompleted, BreakersRequired),
		*BHGameStateProgressCount(TEXT("Side tasks"), SideObjectivesCompleted, SideObjectivesRequired),
		FMath::Clamp(PresenceLevel, 0.0f, 100.0f),
		*ExitText);
}

FString ABHGameState::GetRoundModifierTextFor(EBHRoundModifier Modifier)
{
	switch (Modifier)
	{
	case EBHRoundModifier::LightsOut:
		return TEXT("Lights Out");
	case EBHRoundModifier::LoudFooting:
		return TEXT("Loud Footing");
	case EBHRoundModifier::JammedDoors:
		return TEXT("Jammed Doors");
	case EBHRoundModifier::DeadCCTV:
		return TEXT("Dead CCTV");
	case EBHRoundModifier::PanicSurge:
		return TEXT("Panic Surge");
	case EBHRoundModifier::None:
	default:
		return TEXT("None");
	}
}

FString ABHGameState::GetRoundModifierHintFor(EBHRoundModifier Modifier)
{
	switch (Modifier)
	{
	case EBHRoundModifier::LightsOut:
		return TEXT("More circuits start dark; use lit routes and breaker rooms.");
	case EBHRoundModifier::LoudFooting:
		return TEXT("Footsteps carry farther; slow movement is safer.");
	case EBHRoundModifier::JammedDoors:
		return TEXT("More doors start shut; plan alternate chase loops.");
	case EBHRoundModifier::DeadCCTV:
		return TEXT("Several camera feeds are offline, leaving blind spots.");
	case EBHRoundModifier::PanicSurge:
		return TEXT("Presence rises faster and scare pressure cycles sooner.");
	case EBHRoundModifier::None:
	default:
		return TEXT("Standard map rules.");
	}
}

void ABHGameState::SetRoundPhase(EBHRoundPhase NewPhase)
{
	RoundPhase = NewPhase;
}

void ABHGameState::SetRemainingTime(int32 NewTime)
{
	RemainingTime = FMath::Max(0, NewTime);
}

void ABHGameState::SetBreakerCounts(int32 Completed, int32 Required)
{
	BreakersCompleted = FMath::Max(0, Completed);
	BreakersRequired = FMath::Max(0, Required);
}

void ABHGameState::SetSideObjectiveCounts(int32 Completed, int32 Required)
{
	SideObjectivesCompleted = FMath::Max(0, Completed);
	SideObjectivesRequired = FMath::Max(0, Required);
}

void ABHGameState::SetExitUnlocked(bool bUnlocked)
{
	bExitUnlocked = bUnlocked;
}

void ABHGameState::SetDirectorState(int32 NewRoundSeed, const FString& NewObjectiveText, const FString& NewNextLevelName)
{
	RoundSeed = NewRoundSeed;
	ObjectiveText = NewObjectiveText;
	NextLevelName = NewNextLevelName;
}

void ABHGameState::SetRoundOptions(int32 NewTargetHunterCount, int32 NewObjectiveIntensity, bool bNewInfectionMode, bool bNewPartyPace, EBHRoundModifier NewRoundModifier)
{
	TargetHunterCount = FMath::Clamp(NewTargetHunterCount, 1, 3);
	ObjectiveIntensity = FMath::Clamp(NewObjectiveIntensity, 0, 3);
	bInfectionMode = bNewInfectionMode;
	bPartyPace = bNewPartyPace;
	RoundModifier = NewRoundModifier;
}

void ABHGameState::SetFogOptions(EBHFogPreset NewNextFogPreset, bool bNewFogPresetOverride)
{
	NextFogPreset = NewNextFogPreset;
	bFogPresetOverride = bNewFogPresetOverride;
}

void ABHGameState::SetActiveFogPreset(EBHFogPreset NewActiveFogPreset)
{
	ActiveFogPreset = NewActiveFogPreset;
}

void ABHGameState::SetActiveLevelName(const FString& NewActiveLevelName)
{
	ActiveLevelName = NewActiveLevelName;
}

void ABHGameState::SetPresenceState(float NewPresenceLevel, const FString& NewPresenceText, int32 NewPresencePulse)
{
	PresenceLevel = FMath::Clamp(NewPresenceLevel, 0.0f, 100.0f);
	PresenceText = NewPresenceText;
	PresencePulse = FMath::Max(0, NewPresencePulse);
}

void ABHGameState::SetObjectiveBeats(const TArray<FBHObjectiveBeat>& NewObjectiveBeats)
{
	ObjectiveBeats = NewObjectiveBeats;
}

void ABHGameState::SetPracticeMode(bool bNewPracticeMode)
{
	bPracticeMode = bNewPracticeMode;
}

void ABHGameState::SetTutorialMode(bool bNewTutorialMode)
{
	bTutorialMode = bNewTutorialMode;
}

void ABHGameState::SetTestMode(bool bNewTestMode)
{
	bTestMode = bNewTestMode;
}

void ABHGameState::SetBotOptions(bool bNewBotMode, int32 NewTargetBotCount, EBHBotDifficulty NewBotDifficulty)
{
	bBotMode = bNewBotMode;
	TargetBotCount = FMath::Clamp(NewTargetBotCount, 0, 11);
	BotDifficulty = NewBotDifficulty;
}

void ABHGameState::SetRevisionOptions(EBHRevisionMode NewRevisionMode, int32 NewTopicMask, EBHRevisionDifficultyMix NewDifficultyMix, float NewClassThreshold, float NewIndividualThreshold, int32 NewRoundDuration, int32 NewScareIntensity)
{
	RevisionMode = NewRevisionMode;
	bRevisionMode = RevisionMode != EBHRevisionMode::None;
	RevisionTopicMask = NewTopicMask <= 0 ? 0x0F : (NewTopicMask & 0x0F);
	RevisionDifficultyMix = NewDifficultyMix;
	RevisionClassThreshold = FMath::Clamp(NewClassThreshold, 0.0f, 100.0f);
	RevisionIndividualThreshold = FMath::Clamp(NewIndividualThreshold, 0.0f, 100.0f);
	RevisionRoundDuration = FMath::Clamp(NewRoundDuration, 60, 3600);
	RevisionScareIntensity = FMath::Clamp(NewScareIntensity, 0, 3);
}

void ABHGameState::SetRevisionContributionTarget(int32 NewContributionTarget)
{
	RevisionContributionTarget = FMath::Clamp(NewContributionTarget, 1, 4);
}

void ABHGameState::SetRevisionSummary(float NewClassMasteryAverage, EBHPhysicsTopic NewWeakTopic, int32 NewReviewTimeRemaining, const FString& NewReviewText)
{
	RevisionClassMasteryAverage = FMath::Clamp(NewClassMasteryAverage, 0.0f, 100.0f);
	RevisionWeakTopic = NewWeakTopic;
	RevisionReviewTimeRemaining = FMath::Max(0, NewReviewTimeRemaining);
	RevisionReviewText = NewReviewText;
}

void ABHGameState::SetTrainState(EBHTrainPhase NewTrainPhase, int32 NewStageIndex, float NewPhaseEndServerTime, const FString& NewDestinationName, const FString& NewAnnouncement)
{
	TrainPhase = NewTrainPhase;
	TrainStageIndex = FMath::Max(0, NewStageIndex);
	TrainPhaseEndServerTime = FMath::Max(0.0f, NewPhaseEndServerTime);
	TrainDestinationName = NewDestinationName;
	TrainAnnouncement = NewAnnouncement;
}

void ABHGameState::SetTrainRecap(const FString& NewOverview, const FString& NewTopics, const FString& NewMissedQuestions, const FString& NewTips)
{
	TrainRecapOverview = NewOverview;
	TrainRecapTopics = NewTopics;
	TrainRecapMissedQuestions = NewMissedQuestions;
	TrainRecapTips = NewTips;
}

void ABHGameState::SetFinalEscapeState(EBHFinalEscapeState NewFinalEscapeState, float NewCutsceneEndServerTime, float NewEscapeEndServerTime, float NewHunterReleaseServerTime)
{
	FinalEscapeState = NewFinalEscapeState;
	FinalEscapeCutsceneEndServerTime = FMath::Max(0.0f, NewCutsceneEndServerTime);
	FinalEscapeEndServerTime = FMath::Max(0.0f, NewEscapeEndServerTime);
	HunterReleaseServerTime = FMath::Max(0.0f, NewHunterReleaseServerTime);
}

void ABHGameState::SetIntermissionLocks(bool bNewCaptureDisabled, bool bNewPlayerInputFrozen, bool bNewHunterInputFrozen)
{
	bCaptureDisabled = bNewCaptureDisabled;
	bPlayerInputFrozen = bNewPlayerInputFrozen;
	bHunterInputFrozen = bNewHunterInputFrozen;
}

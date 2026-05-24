#include "BHGameState.h"
#include "Net/UnrealNetwork.h"

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
	bPracticeMode = false;
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
	RevisionScareIntensity = 2;
	RevisionClassMasteryAverage = 0.0f;
	RevisionWeakTopic = EBHPhysicsTopic::ForcesAndMotion;
	RevisionReviewTimeRemaining = 0;
	RevisionReviewText = TEXT("");
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
	DOREPLIFETIME(ABHGameState, bPracticeMode);
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
	DOREPLIFETIME(ABHGameState, RevisionClassMasteryAverage);
	DOREPLIFETIME(ABHGameState, RevisionWeakTopic);
	DOREPLIFETIME(ABHGameState, RevisionReviewTimeRemaining);
	DOREPLIFETIME(ABHGameState, RevisionReviewText);
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
		return TEXT("Survivor Prep");
	case EBHRoundPhase::Hunt:
		return TEXT("Hunt");
	case EBHRoundPhase::SurvivorsWin:
		return TEXT("Survivors Win");
	case EBHRoundPhase::HunterWin:
		return TEXT("Teacher Wins");
	default:
		return TEXT("Unknown");
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

void ABHGameState::SetPracticeMode(bool bNewPracticeMode)
{
	bPracticeMode = bNewPracticeMode;
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

void ABHGameState::SetRevisionSummary(float NewClassMasteryAverage, EBHPhysicsTopic NewWeakTopic, int32 NewReviewTimeRemaining, const FString& NewReviewText)
{
	RevisionClassMasteryAverage = FMath::Clamp(NewClassMasteryAverage, 0.0f, 100.0f);
	RevisionWeakTopic = NewWeakTopic;
	RevisionReviewTimeRemaining = FMath::Max(0, NewReviewTimeRemaining);
	RevisionReviewText = NewReviewText;
}

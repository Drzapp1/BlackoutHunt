#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BHTypes.h"
#include "BHGameState.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABHGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	FString GetPhaseText() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	FString GetRoundModifierText() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	FString GetRoundModifierHint() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	FString GetPublicObjectiveActionText() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	FString GetObjectiveProgressText() const;

	static FString GetRoundModifierTextFor(EBHRoundModifier Modifier);
	static FString GetRoundModifierHintFor(EBHRoundModifier Modifier);

	void SetRoundPhase(EBHRoundPhase NewPhase);
	void SetRemainingTime(int32 NewTime);
	void SetBreakerCounts(int32 Completed, int32 Required);
	void SetSideObjectiveCounts(int32 Completed, int32 Required);
	void SetExitUnlocked(bool bUnlocked);
	void SetDirectorState(int32 NewRoundSeed, const FString& NewObjectiveText, const FString& NewNextLevelName);
	void SetRoundOptions(int32 NewTargetHunterCount, int32 NewObjectiveIntensity, bool bNewInfectionMode, bool bNewPartyPace, EBHRoundModifier NewRoundModifier);
	void SetFogOptions(EBHFogPreset NewNextFogPreset, bool bNewFogPresetOverride);
	void SetActiveFogPreset(EBHFogPreset NewActiveFogPreset);
	void SetActiveLevelName(const FString& NewActiveLevelName);
	void SetPresenceState(float NewPresenceLevel, const FString& NewPresenceText, int32 NewPresencePulse);
	void SetObjectiveBeats(const TArray<FBHObjectiveBeat>& NewObjectiveBeats);
	void SetPracticeMode(bool bNewPracticeMode);
	void SetTestMode(bool bNewTestMode);
	void SetBotOptions(bool bNewBotMode, int32 NewTargetBotCount, EBHBotDifficulty NewBotDifficulty);
	void SetRevisionOptions(EBHRevisionMode NewRevisionMode, int32 NewTopicMask, EBHRevisionDifficultyMix NewDifficultyMix, float NewClassThreshold, float NewIndividualThreshold, int32 NewRoundDuration, int32 NewScareIntensity);
	void SetRevisionContributionTarget(int32 NewContributionTarget);
	void SetRevisionSummary(float NewClassMasteryAverage, EBHPhysicsTopic NewWeakTopic, int32 NewReviewTimeRemaining, const FString& NewReviewText);
	void SetTrainState(EBHTrainPhase NewTrainPhase, int32 NewStageIndex, float NewPhaseEndServerTime, const FString& NewDestinationName, const FString& NewAnnouncement);
	void SetTrainRecap(const FString& NewOverview, const FString& NewTopics, const FString& NewMissedQuestions, const FString& NewTips);
	void SetFinalEscapeState(EBHFinalEscapeState NewFinalEscapeState, float NewCutsceneEndServerTime, float NewEscapeEndServerTime, float NewHunterReleaseServerTime);
	void SetIntermissionLocks(bool bNewCaptureDisabled, bool bNewPlayerInputFrozen, bool bNewHunterInputFrozen);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHRoundPhase RoundPhase;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 RemainingTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 BreakersCompleted;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 BreakersRequired;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 SideObjectivesCompleted;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 SideObjectivesRequired;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bExitUnlocked;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 RoundSeed;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	FString ObjectiveText;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	FString NextLevelName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	FString ActiveLevelName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 TargetHunterCount;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 ObjectiveIntensity;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bInfectionMode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bPartyPace;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHRoundModifier RoundModifier;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHFogPreset NextFogPreset;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHFogPreset ActiveFogPreset;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bFogPresetOverride;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Horror")
	float PresenceLevel;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Horror")
	FString PresenceText;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Horror")
	int32 PresencePulse;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Navigation")
	TArray<FBHObjectiveBeat> ObjectiveBeats;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Practice")
	bool bPracticeMode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Test")
	bool bTestMode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Bots")
	bool bBotMode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Bots")
	int32 TargetBotCount;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Bots")
	EBHBotDifficulty BotDifficulty;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHRevisionMode RevisionMode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	bool bRevisionMode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 RevisionTopicMask;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHRevisionDifficultyMix RevisionDifficultyMix;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float RevisionClassThreshold;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float RevisionIndividualThreshold;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 RevisionRoundDuration;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 RevisionScareIntensity;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 RevisionContributionTarget;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float RevisionClassMasteryAverage;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHPhysicsTopic RevisionWeakTopic;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 RevisionReviewTimeRemaining;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString RevisionReviewText;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	EBHTrainPhase TrainPhase;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	int32 TrainStageIndex;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	float TrainPhaseEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	FString TrainDestinationName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	FString TrainAnnouncement;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	FString TrainRecapOverview;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	FString TrainRecapTopics;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	FString TrainRecapMissedQuestions;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	FString TrainRecapTips;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Final Escape")
	EBHFinalEscapeState FinalEscapeState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Final Escape")
	float FinalEscapeCutsceneEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Final Escape")
	float FinalEscapeEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Final Escape")
	float HunterReleaseServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Final Escape")
	bool bCaptureDisabled;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Final Escape")
	bool bPlayerInputFrozen;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Final Escape")
	bool bHunterInputFrozen;
};

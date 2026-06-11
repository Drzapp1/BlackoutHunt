// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHBotPolicySubsystem.generated.h"

// Pure bot-behavior rules shared by ABHBotController and the game mode's bot roster services. Kept
// free of UWorld/actor state so the decision tables are headlessly provable (BHBotBehaviorTests.cpp);
// the callers gather the live facts and these functions answer "what would a fair bot do".
namespace BHBotBehavior
{
// One roster slot as the trim logic sees it (order matches ABHGameMode::BotControllers).
struct FBHBotRemovalCandidate
{
	bool bAlive = false;
	bool bHunter = false;
};

// Picks which bot a roster trim should delete, newest-first inside safety tiers:
// benched/captured/escaped bots go first, then alive non-hunters, and an alive hunter only when
// removing it leaves at least one alive hunter in a live round (otherwise deleting the bot Teacher
// instantly resolves SurvivorsWin). Returns INDEX_NONE when every candidate is the protected lone
// hunter — the caller defers the trim to a later roster refresh instead of ending the round.
BLACKOUTHUNT_API int32 SelectBotRemovalIndex(const TArray<FBHBotRemovalCandidate>& Candidates, bool bRoundLive, int32 AliveHunterCount);

// Seconds a bot should avoid a question node after the station refused its submission, escalating
// with consecutive refusals. The first refusal costs nothing (a correction hold or a mid-flight
// question swap deserves one local retry); repeats mean the node will not take this bot's answers
// (spent once-per-node attempt), so it must spread to another node instead of camping this one.
BLACKOUTHUNT_API float AnswerRejectionCooldownSeconds(int32 ConsecutiveRejections);

// Honest-vision range multiplier for a bot's sight check. Mirrors what blinds humans: a student bot
// standing inside the Teacher's blackout — or looking INTO it from outside (darkness hides both ways
// for everyone but the Teacher who cast it) — has its sight cut hard, and a bot Teacher hunting in
// heavy/extreme fog can only spot a lights-off target at reduced range — a target with its flashlight
// ON gives itself away at full range, exactly the trade humans play.
// Scale arguments come from the bh.Bot*SightScale cvars so live tuning stays possible.
BLACKOUTHUNT_API float HonestSightRangeScale(bool bViewerIsHunter, bool bViewerInTeacherBlackout, bool bTargetInTeacherBlackout, EBHFogPreset FogPreset, bool bTargetFlashlightOn, float BlackoutScale, float HeavyFogScale, float ExtremeFogScale);

// Threat contributed to a location by one RECORDED hunter sighting: linear distance falloff from the
// sighting's location (not the hunter's live position), faded by sighting age and zero once the
// sighting leaves memory. This is the no-wallhack risk model — unseen hunters contribute nothing.
BLACKOUTHUNT_API float SightingThreatPressure(float SightingAgeSeconds, float MemorySeconds, float Distance2D, float PressureRange);

// Prop hunt: a disguised prop must never advertise itself, so the survivor brain's decoy/trap drops
// (an audible beacon at the bot's own hiding spot) are suppressed for prop-side bots in that mode.
BLACKOUTHUNT_API bool ShouldSuppressBotDecoyDrop(bool bPropHuntMode, EBHPlayerRole BotRole);
}

UCLASS()
class BLACKOUTHUNT_API UBHBotPolicySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	FBHBotPolicyResult ScoreCandidates(const FBHBotPolicyFeatures& Features, TArray<FBHBotDecisionCandidate>& Candidates);
	bool IsPolicyFileLoaded() const { return bPolicyFileLoaded; }
	bool IsBudgetDisabled() const { return bDisabledForBudget; }
	FString GetPolicyStatus() const;

private:
	float ScoreSingleCandidate(const FBHBotPolicyFeatures& Features, const FBHBotDecisionCandidate& Candidate) const;
	float GetIntentWeight(EBHBotIntent Intent) const;
	float GetPersonalityIntentWeight(EBHBotPersonality Personality, EBHBotIntent Intent) const;
	void LoadPolicyFile();

	TMap<EBHBotIntent, float> IntentWeights;
	TMap<FName, float> PersonalityIntentWeights;
	bool bPolicyFileLoaded = false;
	bool bDisabledForBudget = false;
	double TotalScoreSeconds = 0.0;
	int32 ScoreCalls = 0;
};

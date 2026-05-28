#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BHTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "BHBotController.generated.h"

class ABHBreaker;
class ABHCharacter;
class ABHExitGate;
class ABHLocker;
class ABHObjectiveStation;
class UAITask_MoveTo;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UStateTree;
class UStateTreeAIComponent;
struct FAIStimulus;

UCLASS()
class BLACKOUTHUNT_API ABHBotController : public AAIController
{
	GENERATED_BODY()

public:
	ABHBotController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	FString GetBotDebugLine() const;
	FString GetBotArchetypeLabel() const;
	bool RunStateTreeIntent(EBHBotIntent Intent, AActor* Target, const FVector& Location, float AcceptanceRadius);
	bool HasRecentStateTreeStimulus(EBHBotStimulusType Type, float MaxAgeSeconds, FVector& OutLocation) const;
	void ReportStateTreeDebugState(const FString& InStateName);

private:
	ABHCharacter* GetBHCharacter() const;
	class ABHPlayerState* GetBHPlayerState() const;
	class ABHGameState* GetBHGameState() const;
	class ABHGameMode* GetBHGameMode() const;

	void Think();
	void ThinkWithCandidates(ABHCharacter* BotCharacter, class ABHPlayerState* BotPS, class ABHGameState* BHGS, const TCHAR* EmptyPatrolLabel);
	void BuildDecisionCandidates(ABHCharacter* BotCharacter, class ABHPlayerState* BotPS, class ABHGameState* BHGS, TArray<FBHBotDecisionCandidate>& OutCandidates);
	void BuildSurvivorDecisionCandidates(ABHCharacter* BotCharacter, class ABHGameState* BHGS, TArray<FBHBotDecisionCandidate>& OutCandidates);
	void BuildTeacherDecisionCandidates(ABHCharacter* BotCharacter, bool bCanCapture, TArray<FBHBotDecisionCandidate>& OutCandidates);
	FBHBotPolicyFeatures BuildPolicyFeatures(ABHCharacter* BotCharacter, class ABHPlayerState* BotPS, class ABHGameState* BHGS, const TArray<FBHBotDecisionCandidate>& Candidates) const;
	FBHBotPolicyResult ScoreDecisionCandidate(const FBHBotPolicyFeatures& Features, TArray<FBHBotDecisionCandidate>& Candidates) const;
	void CommitDecision(ABHCharacter* BotCharacter, class ABHPlayerState* BotPS, class ABHGameState* BHGS, const FBHBotDecisionCandidate& Decision);
	void AddCandidate(TArray<FBHBotDecisionCandidate>& Candidates, EBHBotIntent Intent, AActor* Target, const FVector& Location, float BaseScore, float Risk, float Urgency, const FString& DebugLabel) const;
	void RecordStimulus(EBHBotStimulusType Type, AActor* TargetActor, const FVector& Location, const FString& Reason, float Strength = 1.0f);
	EBHBotPersonality ChoosePersonality(class ABHPlayerState* BotPS) const;
	float ApplyDifficultyNoise(float Score) const;

	bool MoveTowardActor(AActor* Target, float AcceptanceRadius);
	bool MoveTowardLocation(const FVector& Location, float AcceptanceRadius);
	FVector GetStablePatrolPoint(const ABHCharacter* BotCharacter, float HoldSeconds = 5.0f, float MinDistance = 700.0f);
	void ClearInteraction();
	bool HoldInteraction(AActor* Target);
	bool IsCloseTo(const AActor* Target, float Distance) const;
	bool HasUsableNavLocation(const FVector& Location, AActor* LogTarget);
	bool ProjectUsableNavLocation(const FVector& Location, FVector& OutLocation, AActor* LogTarget);
	void NoteMovementProgress();
	bool HandleStuck(AActor* GoalActor);
	void LogUnreachableOnce(AActor* Target, const FString& Reason);
	void StartStateTreeIfAvailable();
	bool ShouldUseStateTreeBrain() const;
	bool ShouldFallbackFromStateTree(FString& OutReason) const;
	void ActivateStateTreePolicyFallback(const FString& Reason);

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	ABHCharacter* FindVisibleSurvivor(float Range, bool bIncludeHidden) const;
	ABHCharacter* FindVisibleTeacherThreat(float Range) const;
	bool CanSeeCharacter(const ABHCharacter* Other, float Range, bool bIncludeHidden) const;
	ABHObjectiveStation* FindActiveStation(bool bNeedUnsolvedQuestion) const;
	ABHBreaker* FindActiveBreaker() const;
	ABHExitGate* FindActiveExit() const;
	ABHLocker* FindNearestLocker(bool bRequireEmpty, bool bRequireOccupied, float Range) const;
	ABHLocker* FindSuspiciousLocker(const FVector& NearLocation, float Range, bool bAllowEmptySuspicion) const;
	AActor* FindSurvivorObjective() const;
	AActor* FindHunterPatrolObjective() const;
	float GetObjectivePressure(const class ABHGameState* BHGS) const;
	float GetThreatPressureForLocation(const FVector& Location) const;
	int32 CountNearbyAllies(const FVector& Location, EBHPlayerRole AllyRole, float Range) const;
	bool ShouldStayHidden(const ABHCharacter* BotCharacter, const ABHCharacter* Threat, const class ABHGameState* BHGS) const;
	FVector BuildAmbushLocation(const FVector& LastSeenLocation, const FVector& ObjectiveLocation) const;

	void HandleSurvivorThreat(ABHCharacter* BotCharacter, ABHCharacter* Threat);
	void HandleSurvivorObjective(ABHCharacter* BotCharacter, AActor* Target, class ABHGameState* BHGS);
	void HandleStationAnswer(ABHCharacter* BotCharacter, ABHObjectiveStation* Station);
	float GetCorrectAnswerChance() const;
	int32 ChooseAnswerIndex(ABHObjectiveStation* Station) const;

	float ThinkInterval;
	float SightRange;
	float HearingMemorySeconds;
	float StuckSeconds;
	float NextThinkTime;
	float LastAnswerAttemptTime;
	float LastTrapAttemptTime;
	float LastPowerAttemptTime;
	float LastProgressTime;
	float LastDecisionTime;
	float LastBaitAttemptTime;
	float LastHideExitTime;
	float LastSeenHunterStimulusTime;
	float LastSeenSurvivorStimulusTime;
	int32 DecisionsMade;
	int32 IntentSwitches;
	FVector LastProgressLocation;
	FVector LastKnownSurvivorLocation;
	float LastKnownSurvivorTime;
	float LastDecisionScore;
	int32 LastDecisionCandidateCount;
	FBHBotMemory LocalMemory;
	EBHBotPersonality Personality;
	bool bPersonalityChosen;
	bool bLastDecisionUsedPolicyModel;
	TWeakObjectPtr<AActor> CurrentInteractTarget;
	TWeakObjectPtr<AActor> LastUnreachableTarget;
	TWeakObjectPtr<AActor> CurrentClaimTarget;
	TWeakObjectPtr<AActor> LastStateTreeIntentTarget;
	EBHBotIntent CurrentIntent;
	EBHBotIntent LastStateTreeIntent;
	FString LastDecisionDebugLabel;
	FVector LastStateTreeIntentLocation;
	float LastStateTreeIntentTime;
	FVector CurrentPatrolDestination;
	float CurrentPatrolDestinationExpireTime;
	bool bHasPatrolDestination;
	bool bUseStateTreeAI;
	bool bStateTreeBrainRunning;
	bool bStateTreePolicyFallbackActivated;

	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UAIPerceptionComponent> BotPerceptionComponent;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightSenseConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingSenseConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UStateTree> HunterStateTreeAsset;

	UPROPERTY(Transient)
	TObjectPtr<UStateTree> ActiveStateTreeAsset;
};

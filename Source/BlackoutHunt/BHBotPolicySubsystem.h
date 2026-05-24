#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHBotPolicySubsystem.generated.h"

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
	void LoadPolicyFile();

	TMap<EBHBotIntent, float> IntentWeights;
	bool bPolicyFileLoaded = false;
	bool bDisabledForBudget = false;
	double TotalScoreSeconds = 0.0;
	int32 ScoreCalls = 0;
};

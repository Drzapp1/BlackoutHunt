#include "BHBotPolicySubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString BHBotPolicyDirectory()
{
	return TEXT("D:/MainGame/Models/BlackoutHuntBotPolicy");
}

FString BHBotPolicyWeightsPath()
{
	return FPaths::Combine(BHBotPolicyDirectory(), TEXT("bot_policy_weights.ini"));
}

bool ParseIntentName(const FString& Name, EBHBotIntent& OutIntent)
{
	if (Name.Equals(TEXT("Patrol"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::Patrol; return true; }
	if (Name.Equals(TEXT("AnswerStation"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::AnswerStation; return true; }
	if (Name.Equals(TEXT("WorkStation"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::WorkStation; return true; }
	if (Name.Equals(TEXT("RepairBreaker"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::RepairBreaker; return true; }
	if (Name.Equals(TEXT("Escape"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::Escape; return true; }
	if (Name.Equals(TEXT("Hide"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::Hide; return true; }
	if (Name.Equals(TEXT("Flee"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::Flee; return true; }
	if (Name.Equals(TEXT("Bait"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::Bait; return true; }
	if (Name.Equals(TEXT("Chase"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::Chase; return true; }
	if (Name.Equals(TEXT("InvestigateNoise"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::InvestigateNoise; return true; }
	if (Name.Equals(TEXT("InvestigateLastSeen"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::InvestigateLastSeen; return true; }
	if (Name.Equals(TEXT("SearchLocker"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::SearchLocker; return true; }
	if (Name.Equals(TEXT("AmbushObjective"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::AmbushObjective; return true; }
	if (Name.Equals(TEXT("UseScan"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::UseScan; return true; }
	if (Name.Equals(TEXT("UsePower"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::UsePower; return true; }
	if (Name.Equals(TEXT("DropTrap"), ESearchCase::IgnoreCase)) { OutIntent = EBHBotIntent::DropTrap; return true; }
	return false;
}
}

void UBHBotPolicySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPolicyFile();
}

void UBHBotPolicySubsystem::LoadPolicyFile()
{
	IntentWeights.Reset();
	bPolicyFileLoaded = false;

	const FString PolicyPath = BHBotPolicyWeightsPath();
	if (!FPaths::FileExists(PolicyPath))
	{
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot policy: no local policy file at %s; using C++ utility scorer."), *PolicyPath);
		return;
	}

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *PolicyPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt bot policy: failed to read %s; using C++ utility scorer."), *PolicyPath);
		return;
	}

	for (FString Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		FString Key;
		FString Value;
		if (!Line.Split(TEXT("="), &Key, &Value))
		{
			continue;
		}

		Key.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();
		EBHBotIntent Intent = EBHBotIntent::None;
		if (ParseIntentName(Key, Intent))
		{
			IntentWeights.Add(Intent, FCString::Atof(*Value));
		}
	}

	bPolicyFileLoaded = IntentWeights.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot policy: %s %d local intent weights from %s."),
		bPolicyFileLoaded ? TEXT("loaded") : TEXT("ignored"),
		IntentWeights.Num(),
		*PolicyPath);
}

FBHBotPolicyResult UBHBotPolicySubsystem::ScoreCandidates(const FBHBotPolicyFeatures& Features, TArray<FBHBotDecisionCandidate>& Candidates)
{
	FBHBotPolicyResult Result;
	if (Candidates.IsEmpty())
	{
		return Result;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	float BestScore = -TNumericLimits<float>::Max();
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const float Score = ScoreSingleCandidate(Features, Candidates[Index]);
		Candidates[Index].BaseScore = Score;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = Index;
		}
	}

	TotalScoreSeconds += FPlatformTime::Seconds() - StartSeconds;
	++ScoreCalls;
	const double AverageMs = ScoreCalls > 0 ? (TotalScoreSeconds / static_cast<double>(ScoreCalls)) * 1000.0 : 0.0;
	if (AverageMs > 4.0 && ScoreCalls > 16)
	{
		bDisabledForBudget = true;
		bPolicyFileLoaded = false;
		IntentWeights.Reset();
		UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt bot policy: disabled local policy after %.2fms average scoring budget."), AverageMs);
	}

	Result.ChosenIndex = BestIndex;
	Result.Score = BestScore;
	Result.bUsedModel = bPolicyFileLoaded && !bDisabledForBudget;
	Result.DebugLabel = Candidates.IsValidIndex(BestIndex) ? Candidates[BestIndex].DebugLabel : FString();
	return Result;
}

float UBHBotPolicySubsystem::ScoreSingleCandidate(const FBHBotPolicyFeatures& Features, const FBHBotDecisionCandidate& Candidate) const
{
	float Score = Candidate.BaseScore
		+ Candidate.Urgency * 0.35f
		- Candidate.Risk * 0.45f
		- FMath::Clamp(Candidate.Distance / 12000.0f, 0.0f, 1.0f) * 0.28f
		- static_cast<float>(Features.TargetClaimCount) * 0.38f
		+ GetIntentWeight(Candidate.Intent);

	switch (Features.Personality)
	{
	case EBHBotPersonality::Cautious:
		Score += (Candidate.Intent == EBHBotIntent::Hide || Candidate.Intent == EBHBotIntent::Flee) ? 0.34f : 0.0f;
		Score -= Candidate.Risk * 0.20f;
		break;
	case EBHBotPersonality::Objective:
		Score += (Candidate.Intent == EBHBotIntent::AnswerStation || Candidate.Intent == EBHBotIntent::WorkStation || Candidate.Intent == EBHBotIntent::RepairBreaker || Candidate.Intent == EBHBotIntent::Escape) ? 0.30f : 0.0f;
		break;
	case EBHBotPersonality::Bold:
		Score += (Candidate.Intent == EBHBotIntent::Bait || Candidate.Intent == EBHBotIntent::RepairBreaker || Candidate.Intent == EBHBotIntent::Chase) ? 0.24f : 0.0f;
		Score += Features.ThreatPressure * 0.08f;
		break;
	case EBHBotPersonality::Trickster:
		Score += (Candidate.Intent == EBHBotIntent::Bait || Candidate.Intent == EBHBotIntent::DropTrap || Candidate.Intent == EBHBotIntent::AmbushObjective) ? 0.40f : 0.0f;
		break;
	case EBHBotPersonality::Panicked:
		Score += (Candidate.Intent == EBHBotIntent::Flee || Candidate.Intent == EBHBotIntent::Hide || Candidate.Intent == EBHBotIntent::UseScan) ? 0.44f : 0.0f;
		Score -= (Candidate.Intent == EBHBotIntent::AnswerStation || Candidate.Intent == EBHBotIntent::WorkStation) ? 0.22f : 0.0f;
		break;
	case EBHBotPersonality::Aggressive:
		Score += (Candidate.Intent == EBHBotIntent::Chase || Candidate.Intent == EBHBotIntent::UsePower || Candidate.Intent == EBHBotIntent::SearchLocker) ? 0.42f : 0.0f;
		break;
	case EBHBotPersonality::Suspicious:
		Score += (Candidate.Intent == EBHBotIntent::InvestigateNoise || Candidate.Intent == EBHBotIntent::InvestigateLastSeen || Candidate.Intent == EBHBotIntent::SearchLocker) ? 0.36f : 0.0f;
		break;
	case EBHBotPersonality::Ambusher:
		Score += (Candidate.Intent == EBHBotIntent::AmbushObjective || Candidate.Intent == EBHBotIntent::InvestigateLastSeen) ? 0.44f : 0.0f;
		break;
	default:
		break;
	}

	switch (Features.Difficulty)
	{
	case EBHBotDifficulty::Easy:
		Score += FMath::FRandRange(-0.28f, 0.18f);
		break;
	case EBHBotDifficulty::Hard:
		Score += Candidate.Urgency * 0.16f - Candidate.Risk * 0.10f;
		break;
	case EBHBotDifficulty::Normal:
	default:
		Score += FMath::FRandRange(-0.08f, 0.08f);
		break;
	}

	return Score;
}

float UBHBotPolicySubsystem::GetIntentWeight(EBHBotIntent Intent) const
{
	if (const float* Weight = IntentWeights.Find(Intent))
	{
		return *Weight;
	}
	return 0.0f;
}

FString UBHBotPolicySubsystem::GetPolicyStatus() const
{
	const double AverageMs = ScoreCalls > 0 ? (TotalScoreSeconds / static_cast<double>(ScoreCalls)) * 1000.0 : 0.0;
	return FString::Printf(TEXT("policy=%s weights=%d avg=%.3fms%s path=%s"),
		bPolicyFileLoaded ? TEXT("local-file") : TEXT("cpp-fallback"),
		IntentWeights.Num(),
		AverageMs,
		bDisabledForBudget ? TEXT(" disabled-budget") : TEXT(""),
		*BHBotPolicyWeightsPath());
}

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHBotPolicySubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString BHBotPolicyDirectory()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Models"), TEXT("BlackoutHuntBotPolicy"));
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

bool ParsePersonalityName(const FString& Name, EBHBotPersonality& OutPersonality)
{
	if (Name.Equals(TEXT("Cautious"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Cautious; return true; }
	if (Name.Equals(TEXT("Objective"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Objective; return true; }
	if (Name.Equals(TEXT("Bold"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Bold; return true; }
	if (Name.Equals(TEXT("Trickster"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Trickster; return true; }
	if (Name.Equals(TEXT("Panicked"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Panicked; return true; }
	if (Name.Equals(TEXT("Aggressive"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Aggressive; return true; }
	if (Name.Equals(TEXT("Suspicious"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Suspicious; return true; }
	if (Name.Equals(TEXT("Ambusher"), ESearchCase::IgnoreCase)) { OutPersonality = EBHBotPersonality::Ambusher; return true; }
	return false;
}

FName MakePersonalityIntentKey(EBHBotPersonality Personality, EBHBotIntent Intent)
{
	return FName(*FString::Printf(TEXT("%d.%d"), static_cast<int32>(Personality), static_cast<int32>(Intent)));
}
}

namespace BHBotBehavior
{
int32 SelectBotRemovalIndex(const TArray<FBHBotRemovalCandidate>& Candidates, bool bRoundLive, int32 AliveHunterCount)
{
	int32 BestIndex = INDEX_NONE;
	int32 BestTier = TNumericLimits<int32>::Max();
	// Newest-first inside a tier: iterate from the end so the most recently added bot wins ties,
	// preserving the old pop-the-newest behaviour wherever the safety tiers agree.
	for (int32 Index = Candidates.Num() - 1; Index >= 0; --Index)
	{
		const FBHBotRemovalCandidate& Candidate = Candidates[Index];
		int32 Tier = 0;
		if (Candidate.bAlive)
		{
			if (Candidate.bHunter)
			{
				// An alive hunter bot may only be trimmed when at least one other alive hunter (human
				// or bot) remains in a live round; deleting the lone hunter resolves the round on the
				// next win-condition tick, which a roster clamp must never do.
				if (bRoundLive && AliveHunterCount <= 1)
				{
					continue;
				}
				Tier = 2;
			}
			else
			{
				Tier = 1;
			}
		}

		if (Tier < BestTier)
		{
			BestTier = Tier;
			BestIndex = Index;
			if (BestTier == 0)
			{
				break;
			}
		}
	}
	return BestIndex;
}

float AnswerRejectionCooldownSeconds(int32 ConsecutiveRejections)
{
	if (ConsecutiveRejections < 2)
	{
		return 0.0f;
	}
	// Two consecutive refusals: the station is actively holding this bot off (correction hold) — step
	// aside long enough for the hold to lapse and teammates to use the node. Four or more: this bot's
	// answers are structurally rejected here (spent once-per-node attempt), so stay away much longer.
	return ConsecutiveRejections >= 4 ? 45.0f : 16.0f;
}

float HonestSightRangeScale(bool bViewerIsHunter, bool bViewerInTeacherBlackout, bool bTargetInTeacherBlackout, EBHFogPreset FogPreset, bool bTargetFlashlightOn, float BlackoutScale, float HeavyFogScale, float ExtremeFogScale)
{
	float Scale = 1.0f;
	// The blackout blinds students (survivors/monitors) in BOTH directions — standing inside it, or
	// peering into it from outside (a human can't see into the dark either). Only the Teacher who cast
	// it keeps full sight, the same asymmetry the power has for humans (it weakens STUDENT flashlights).
	if (!bViewerIsHunter && (bViewerInTeacherBlackout || bTargetInTeacherBlackout))
	{
		Scale *= FMath::Clamp(BlackoutScale, 0.05f, 1.0f);
	}
	// Fog only hides a target that is not lit up: a flashlight beam carries through fog and gives the
	// holder away at full range, preserving the lights-on-vs-stealth trade humans play against the
	// Teacher. Light fog leaves sight untouched.
	if (bViewerIsHunter && !bTargetFlashlightOn)
	{
		if (FogPreset == EBHFogPreset::Heavy)
		{
			Scale *= FMath::Clamp(HeavyFogScale, 0.05f, 1.0f);
		}
		else if (FogPreset == EBHFogPreset::Extreme)
		{
			Scale *= FMath::Clamp(ExtremeFogScale, 0.05f, 1.0f);
		}
	}
	return Scale;
}

float SightingThreatPressure(float SightingAgeSeconds, float MemorySeconds, float Distance2D, float PressureRange)
{
	if (MemorySeconds <= 0.0f || PressureRange <= 0.0f)
	{
		return 0.0f;
	}

	const float Age = FMath::Max(0.0f, SightingAgeSeconds);
	if (Age > MemorySeconds || Distance2D > PressureRange)
	{
		return 0.0f;
	}

	// Fade up to 60% with age rather than to zero: a sighting on the edge of memory still marks the
	// area as warmer than a never-seen corridor, but no longer dominates the risk score.
	const float AgeFade = 1.0f - FMath::Clamp(Age / MemorySeconds, 0.0f, 1.0f) * 0.6f;
	return (1.0f - FMath::Clamp(Distance2D / PressureRange, 0.0f, 1.0f)) * AgeFade;
}

bool ShouldSuppressBotDecoyDrop(bool bPropHuntMode, EBHPlayerRole BotRole)
{
	return bPropHuntMode && BotRole == EBHPlayerRole::Survivor;
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
	PersonalityIntentWeights.Reset();
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

		FString PersonalityName;
		FString IntentName;
		if (Key.Split(TEXT("."), &PersonalityName, &IntentName))
		{
			PersonalityName.TrimStartAndEndInline();
			IntentName.TrimStartAndEndInline();
			EBHBotPersonality Personality = EBHBotPersonality::Objective;
			EBHBotIntent Intent = EBHBotIntent::None;
			if (ParsePersonalityName(PersonalityName, Personality) && ParseIntentName(IntentName, Intent))
			{
				PersonalityIntentWeights.Add(MakePersonalityIntentKey(Personality, Intent), FCString::Atof(*Value));
			}
			continue;
		}

		EBHBotIntent Intent = EBHBotIntent::None;
		if (ParseIntentName(Key, Intent))
		{
			IntentWeights.Add(Intent, FCString::Atof(*Value));
		}
	}

	bPolicyFileLoaded = IntentWeights.Num() > 0 || PersonalityIntentWeights.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot policy: %s %d local intent weights and %d personality weights from %s."),
		bPolicyFileLoaded ? TEXT("loaded") : TEXT("ignored"),
		IntentWeights.Num(),
		PersonalityIntentWeights.Num(),
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
	if (bPolicyFileLoaded && AverageMs > 4.0 && ScoreCalls > 16)
	{
		bDisabledForBudget = true;
		bPolicyFileLoaded = false;
		IntentWeights.Reset();
		PersonalityIntentWeights.Reset();
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
		- static_cast<float>(Candidate.TargetClaimCount) * 0.38f
		+ GetIntentWeight(Candidate.Intent)
		+ GetPersonalityIntentWeight(Features.Personality, Candidate.Intent);

	switch (Features.Personality)
	{
	case EBHBotPersonality::Cautious:
		Score += (Candidate.Intent == EBHBotIntent::Hide || Candidate.Intent == EBHBotIntent::Flee) ? 0.34f : 0.0f;
		Score -= Candidate.Risk * 0.20f + Features.ThreatPressure * 0.08f;
		break;
	case EBHBotPersonality::Objective:
		Score += (Candidate.Intent == EBHBotIntent::AnswerStation || Candidate.Intent == EBHBotIntent::WorkStation || Candidate.Intent == EBHBotIntent::RepairBreaker || Candidate.Intent == EBHBotIntent::Escape) ? 0.30f + Features.ObjectivePressure * 0.18f : 0.0f;
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

	if (Features.Role == EBHPlayerRole::FakeHunter)
	{
		switch (Features.Personality)
		{
		case EBHBotPersonality::Trickster:
			Score += (Candidate.Intent == EBHBotIntent::UsePower || Candidate.Intent == EBHBotIntent::DropTrap) ? 0.24f : 0.0f;
			break;
		case EBHBotPersonality::Suspicious:
			Score += (Candidate.Intent == EBHBotIntent::Patrol || Candidate.Intent == EBHBotIntent::InvestigateNoise || Candidate.Intent == EBHBotIntent::UseScan) ? 0.18f : 0.0f;
			break;
		case EBHBotPersonality::Ambusher:
			Score += (Candidate.Intent == EBHBotIntent::AmbushObjective || Candidate.Intent == EBHBotIntent::DropTrap) ? 0.22f : 0.0f;
			break;
		default:
			break;
		}
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

float UBHBotPolicySubsystem::GetPersonalityIntentWeight(EBHBotPersonality Personality, EBHBotIntent Intent) const
{
	if (const float* Weight = PersonalityIntentWeights.Find(MakePersonalityIntentKey(Personality, Intent)))
	{
		return *Weight;
	}
	return 0.0f;
}

FString UBHBotPolicySubsystem::GetPolicyStatus() const
{
	const double AverageMs = ScoreCalls > 0 ? (TotalScoreSeconds / static_cast<double>(ScoreCalls)) * 1000.0 : 0.0;
	return FString::Printf(TEXT("policy=%s weights=%d personalityWeights=%d avg=%.3fms%s path=%s"),
		bPolicyFileLoaded ? TEXT("local-file") : TEXT("cpp-fallback"),
		IntentWeights.Num(),
		PersonalityIntentWeights.Num(),
		AverageMs,
		bDisabledForBudget ? TEXT(" disabled-budget") : TEXT(""),
		*BHBotPolicyWeightsPath());
}

#include "BHPowerupLibrary.h"

const TArray<FBHPowerupDefinition>& FBHPowerupLibrary::GetDefaultPowerups()
{
	static TArray<FBHPowerupDefinition> Powerups;
	if (!Powerups.IsEmpty())
	{
		return Powerups;
	}

	auto AddPowerup = [](EBHPowerupType Type, const TCHAR* Name, const TCHAR* Description, int32 Cost, int32 MaxCharges, float Duration, float Cooldown)
	{
		FBHPowerupDefinition Definition;
		Definition.Type = Type;
		Definition.Name = Name;
		Definition.Description = Description;
		Definition.Cost = Cost;
		Definition.MaxCharges = MaxCharges;
		Definition.DurationSeconds = Duration;
		Definition.CooldownSeconds = Cooldown;
		Powerups.Add(Definition);
	};

	AddPowerup(EBHPowerupType::StaminaBoost, TEXT("Stamina Boost"), TEXT("Recover stamina faster for the next chase window."), 28, 2, 95.0f, 18.0f);
	AddPowerup(EBHPowerupType::SprintBurst, TEXT("Sprint Burst"), TEXT("Short speed burst. Cooldown prevents permanent outrunning."), 36, 2, 4.0f, 28.0f);
	AddPowerup(EBHPowerupType::FlashlightBoost, TEXT("Light Boost"), TEXT("Refills battery and strengthens the beam for a limited time."), 24, 2, 70.0f, 20.0f);
	AddPowerup(EBHPowerupType::QuestionHint, TEXT("Question Hint"), TEXT("Shows a hint for the active node or train question."), 18, 3, 20.0f, 8.0f);
	AddPowerup(EBHPowerupType::DecoySound, TEXT("Decoy Sound"), TEXT("Drops a fake sound ping to pull the Teacher off-route."), 32, 2, 0.0f, 14.0f);
	AddPowerup(EBHPowerupType::DoorRush, TEXT("Door Rush"), TEXT("Final escape speed assist near open subway doors."), 38, 1, 22.0f, 30.0f);
	AddPowerup(EBHPowerupType::TeacherScanFocus, TEXT("Teacher Scan Focus"), TEXT("Passive upgrade. Heartbeat scan cools down faster and reaches farther."), 40, 2, 0.0f, 0.0f);
	AddPowerup(EBHPowerupType::TeacherBlackoutSurge, TEXT("Teacher Blackout Surge"), TEXT("Passive upgrade. Blackout hits more lights and cools down faster."), 50, 2, 0.0f, 0.0f);
	AddPowerup(EBHPowerupType::TeacherPatrolIntel, TEXT("Teacher Patrol Intel"), TEXT("Passive upgrade. Scans include rough recent-noise direction."), 35, 2, 0.0f, 0.0f);
	return Powerups;
}

bool FBHPowerupLibrary::GetDefinition(EBHPowerupType Type, FBHPowerupDefinition& OutDefinition)
{
	for (const FBHPowerupDefinition& Definition : GetDefaultPowerups())
	{
		if (Definition.Type == Type)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	return false;
}

bool FBHPowerupLibrary::IsTeacherPowerup(EBHPowerupType Type)
{
	return Type == EBHPowerupType::TeacherScanFocus
		|| Type == EBHPowerupType::TeacherBlackoutSurge
		|| Type == EBHPowerupType::TeacherPatrolIntel;
}

FString FBHPowerupLibrary::PowerupTypeToString(EBHPowerupType Type)
{
	FBHPowerupDefinition Definition;
	return GetDefinition(Type, Definition) ? Definition.Name : TEXT("Powerup");
}

int32 FBHPowerupLibrary::QuestionPointValue(EBHQuestionDifficulty Difficulty, bool bBonusQuestion)
{
	int32 Points = 8;
	switch (Difficulty)
	{
	case EBHQuestionDifficulty::Hard:
		Points = 18;
		break;
	case EBHQuestionDifficulty::Medium:
		Points = 12;
		break;
	case EBHQuestionDifficulty::Easy:
	default:
		Points = 8;
		break;
	}

	return bBonusQuestion ? FMath::CeilToInt(Points * 1.25f) : Points;
}

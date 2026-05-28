#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

class BLACKOUTHUNT_API FBHPowerupLibrary
{
public:
	static const TArray<FBHPowerupDefinition>& GetDefaultPowerups();
	static bool GetDefinition(EBHPowerupType Type, FBHPowerupDefinition& OutDefinition);
	static bool IsTeacherPowerup(EBHPowerupType Type);
	static FString PowerupTypeToString(EBHPowerupType Type);
	static int32 QuestionPointValue(EBHQuestionDifficulty Difficulty, bool bBonusQuestion);
};

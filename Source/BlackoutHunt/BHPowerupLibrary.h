// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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

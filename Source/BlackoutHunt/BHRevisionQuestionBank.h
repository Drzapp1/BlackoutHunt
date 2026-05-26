#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

class BLACKOUTHUNT_API FBHRevisionQuestionBank
{
public:
	static const TArray<FBHRevisionQuestion>& GetQuestions();
	static bool Validate(FString& OutSummary);
	static bool FindQuestion(const FString& Id, FBHRevisionQuestion& OutQuestion);
	static bool SelectQuestion(EBHPhysicsTopic Topic, EBHRevisionDifficultyMix DifficultyMix, int32 Seed, const TArray<EBHPhysicsTopic>& WeakTopics, FBHRevisionQuestion& OutQuestion);
	static bool SelectQuestionByDifficulty(EBHPhysicsTopic Topic, EBHQuestionDifficulty Difficulty, int32 Seed, FBHRevisionQuestion& OutQuestion);
	static EBHPhysicsTopic TopicForStationType(EBHObjectiveStationType StationType);
	static FString TopicToString(EBHPhysicsTopic Topic);
	static FString QuestionTypeToString(EBHQuestionType Type);
	static FString DifficultyToString(EBHQuestionDifficulty Difficulty);
	static int32 TopicMaskBit(EBHPhysicsTopic Topic);
};

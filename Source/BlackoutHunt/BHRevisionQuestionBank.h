// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

class BLACKOUTHUNT_API FBHRevisionQuestionBank
{
public:
	// Active question bank. Prefers an editable JSON override file when present and structurally
	// valid (see GetOverrideFilePath); otherwise returns the built-in compiled bank.
	static const TArray<FBHRevisionQuestion>& GetQuestions();
	// The built-in (compiled C++) bank, ignoring any JSON override. Used as the fallback and as
	// the source for ExportQuestionBankToOverrideFile / the round-trip fidelity test.
	static const TArray<FBHRevisionQuestion>& GetBuiltInQuestions();
	// Structural validation of the active bank (well-formed questions, unique ids, every topic
	// represented). Accepts teacher content of any size.
	static bool Validate(FString& OutSummary);
	// Strict distribution guard for the built-in bank only (exact counts). For tests.
	static bool ValidateBuiltInDistribution(FString& OutSummary);

	// Editable JSON question content (see Docs/REVISION_QUALITY_PLAN.md).
	static FString GetOverrideFilePath();
	static FString SerializeQuestionsToJson(const TArray<FBHRevisionQuestion>& Questions);
	static bool ParseQuestionsFromJson(const FString& JsonText, TArray<FBHRevisionQuestion>& OutQuestions, FString& OutError);
	// Writes the built-in bank to the override file so teachers have a full editable starting set.
	static bool ExportQuestionBankToOverrideFile(FString& OutMessage);

	static bool FindQuestion(const FString& Id, FBHRevisionQuestion& OutQuestion);
	// Selection is constrained to the host-configured difficulty band [MinDifficulty, MaxDifficulty] and,
	// when AllowedIds is non-null and non-empty, to that exact question set (case-insensitive id match).
	// Defaults (Easy..Hard, no id set) reproduce the historical behaviour, so existing callers are unchanged.
	static bool SelectQuestion(EBHPhysicsTopic Topic, EBHRevisionDifficultyMix DifficultyMix, int32 Seed, const TArray<EBHPhysicsTopic>& WeakTopics, FBHRevisionQuestion& OutQuestion,
		EBHQuestionDifficulty MinDifficulty = EBHQuestionDifficulty::Easy, EBHQuestionDifficulty MaxDifficulty = EBHQuestionDifficulty::Hard, const TArray<FString>* AllowedIds = nullptr);
	static bool SelectQuestionByDifficulty(EBHPhysicsTopic Topic, EBHQuestionDifficulty Difficulty, int32 Seed, FBHRevisionQuestion& OutQuestion,
		EBHQuestionDifficulty MinDifficulty = EBHQuestionDifficulty::Easy, EBHQuestionDifficulty MaxDifficulty = EBHQuestionDifficulty::Hard, const TArray<FString>* AllowedIds = nullptr);
	// Pick a "proper" (non-multiple-choice) drag question -- DragDropMatching or Ordering -- for a
	// topic, preferring PreferredDifficulty but falling back to any difficulty. Used to mix proper
	// questions into both revision and standard Hunt nodes. Returns false if the topic has none.
	static bool SelectDragQuestion(EBHPhysicsTopic Topic, EBHQuestionDifficulty PreferredDifficulty, int32 Seed, FBHRevisionQuestion& OutQuestion,
		EBHQuestionDifficulty MinDifficulty = EBHQuestionDifficulty::Easy, EBHQuestionDifficulty MaxDifficulty = EBHQuestionDifficulty::Hard, const TArray<FString>* AllowedIds = nullptr);
	static EBHPhysicsTopic TopicForStationType(EBHObjectiveStationType StationType);

	// Host "exact question set" support: named id lists saved under Saved/ClassroomPresets/QuestionSets/<id>.json.
	static FString GetQuestionSetFilePath(const FString& SetId);
	static bool LoadQuestionSetIds(const FString& SetId, TArray<FString>& OutQuestionIds, FString& OutMessage);
	static bool SaveQuestionSet(const FString& SetId, const FString& DisplayName, const TArray<FString>& QuestionIds, FString& OutMessage);
	static void GetAvailableQuestionSetIds(TArray<FString>& OutSetIds);
	// Drop the cached active bank so the next GetQuestions() reloads the JSON override (host live re-edit).
	static void InvalidateQuestionCache();

	// Parse a matching/ordering choice string into the player-facing slots (fixed drop targets /
	// positions) and the canonical pieces (the correct content for each slot, in slot order). Drives
	// both the interactive drag task and server-side grading of an arrangement. Returns false unless
	// it parses into >= 2 slots with a matching number of pieces.
	//   Ordering ("a -> b -> c"):           slots = ["1.","2.","3."],  pieces = ["a","b","c"]
	//   Matching ("Key1: v1; Key2 -> v2"):  slots = ["Key1","Key2"],   pieces = ["v1","v2"]
	// Matching tolerates either ": " or " -> " inside a pair and "; " between pairs.
	static bool ParseArrangementChoice(const FString& ChoiceText, EBHQuestionType Type, TArray<FString>& OutSlots, TArray<FString>& OutPieces);
	static FString TopicToString(EBHPhysicsTopic Topic);
	static FString QuestionTypeToString(EBHQuestionType Type);
	static FString DifficultyToString(EBHQuestionDifficulty Difficulty);
	static int32 TopicMaskBit(EBHPhysicsTopic Topic);
};

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

class UBHGameSettings;

struct FBHLessonPreset
{
	FString Id;
	FString DisplayName;
	int32 TopicMask = 0x0F;
	EBHRevisionDifficultyMix DifficultyMix = EBHRevisionDifficultyMix::Adaptive;
	float ClassThreshold = 70.0f;
	float IndividualThreshold = 50.0f;
	int32 RoundSeconds = 300;
	int32 ScareIntensity = 2;
	FString MapName = TEXT("Facility");
	int32 BotCount = 0;
	EBHBotDifficulty BotDifficulty = EBHBotDifficulty::Normal;
	bool bReducedJumpscares = false;
	bool bReducedFlash = false;
	bool bReducedCameraShake = false;
	bool bCaptions = true;
	bool bHighContrastHud = false;
	bool bBuiltin = false;
};

struct FBHManualQuestionSet
{
	FBHLessonPreset Preset;
	TArray<FBHRevisionQuestion> Questions;
	int32 RequestedQuestionCount = 12;
	int32 Seed = 1;
	FDateTime GeneratedAtUtc;
};

class FBHLessonPresetStore
{
public:
	static FString GetStoragePath();
	static FString GetQuestionSetDirectory();
	static FString GetSelectedPresetId();
	static void SetSelectedPresetId(const FString& PresetId);

	static FBHLessonPreset MakeDefaultPreset(const UBHGameSettings* Settings);
	static TArray<FBHLessonPreset> GetBuiltinPresets(const UBHGameSettings* Settings);
	static bool LoadCustomPresets(TArray<FBHLessonPreset>& OutPresets, FString& OutMessage);
	static bool LoadAllPresets(TArray<FBHLessonPreset>& OutPresets, FString& OutMessage);
	static bool TryFindPreset(const FString& PresetId, FBHLessonPreset& OutPreset, FString& OutMessage);
	static bool SaveCustomPreset(const FBHLessonPreset& Preset, FString& OutMessage);
	static bool BuildManualQuestionSet(const FBHLessonPreset& Preset, int32 QuestionCount, int32 Seed, FBHManualQuestionSet& OutQuestionSet, FString& OutMessage);
	static FString FormatManualQuestionSetMarkdown(const FBHManualQuestionSet& QuestionSet);
	static bool SaveManualQuestionSet(const FBHLessonPreset& Preset, int32 QuestionCount, FString& OutPath, FString& OutMessage);

	static FBHLessonPreset ValidatePreset(const FBHLessonPreset& Preset, bool* bOutAdjusted = nullptr);
	static int32 ClampManualQuestionSetCount(int32 QuestionCount);
	static FString SanitizeDisplayName(const FString& DisplayName);
	static FString MakeCustomPresetId(const FString& DisplayName);
	static FString NormalizeMapName(FString MapName);
	static FString DifficultyMixToString(EBHRevisionDifficultyMix DifficultyMix);
	static EBHRevisionDifficultyMix ParseDifficultyMix(const FString& DifficultyMix, EBHRevisionDifficultyMix DefaultMix);
	static FString BotDifficultyToString(EBHBotDifficulty Difficulty);
	static EBHBotDifficulty ParseBotDifficulty(const FString& Difficulty, EBHBotDifficulty DefaultDifficulty);
	static FString TopicMaskToText(int32 TopicMask);
	static FString DescribePreset(const FBHLessonPreset& Preset);
	static FString BuildRevisionLaunchOptions(const FBHLessonPreset& Preset, bool bLiveClassroom);
};

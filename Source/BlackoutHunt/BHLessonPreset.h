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

	// --- Host lobby customization (Pillar 5 extension) ---
	// Difficulty range: live selection is clamped to [MinDifficulty, MaxDifficulty]; the first
	// question per student (and the adaptive starting tier) uses StartingDifficulty.
	EBHQuestionDifficulty StartingDifficulty = EBHQuestionDifficulty::Easy;
	EBHQuestionDifficulty MinDifficulty = EBHQuestionDifficulty::Easy;
	EBHQuestionDifficulty MaxDifficulty = EBHQuestionDifficulty::Hard;
	// Exact question set: when non-empty, the round draws only from the saved set with this id
	// (Saved/ClassroomPresets/QuestionSets/<id>.json). Empty = use the whole active bank.
	FString QuestionSetId;
	// Map routing / maps-used: ordered stage maps (variable length, repeats allowed). Empty = the
	// default Facility -> Substation -> Foggrounds sequence. This array is both the pool and the order.
	TArray<FString> MapRoute;
	// Procedural layout knobs (only applied when the round runs the runtime generator -- see
	// bForceProcedural). 0 means "use the generator default".
	int32 LayoutSeed = 0;       // 0 = generator's fixed default seed; non-zero = host-chosen seed
	int32 BreakerCount = 0;     // 0 = default; else clamped to 3..12 placed breakers
	int32 LayoutDensity = 0;    // 0 = default (100); else 50..160 percent (partitions + objective count)
	bool bForceProcedural = false; // run the procedural generator even when authored maps are enabled
	// Named procedural layout preset (host-friendly alternative to a raw seed). When set to a known name
	// (see FBHLessonPresetStore::GetLayoutPresetNames), ValidatePreset EXPANDS it into the seed/density/
	// breaker/force-procedural fields above, so the round needs no extra plumbing. Empty = use those raw
	// fields directly. To author a fully custom layout instead, bake an authored .umap variant (see docs).
	FString LayoutPresetName;

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
	static FString QuestionDifficultyToString(EBHQuestionDifficulty Difficulty);
	static EBHQuestionDifficulty ParseQuestionDifficulty(const FString& Difficulty, EBHQuestionDifficulty DefaultDifficulty);
	static const TArray<FString>& GetDefaultMapRoute();
	static TArray<FString> GetAvailableMapNames();
	static FString MapRouteToString(const TArray<FString>& MapRoute);
	static TArray<FString> ParseMapRoute(const FString& MapRouteCsv);
	// Host-friendly named procedural layout presets (an alternative to a raw seed). GetLayoutPresetNames
	// returns the curated list; ApplyNamedLayoutPreset expands a name into the seed/density/breaker/force
	// fields and returns false (leaving them unchanged) when the name is empty or unknown.
	static TArray<FString> GetLayoutPresetNames();
	static bool ApplyNamedLayoutPreset(const FString& PresetName, int32& OutSeed, int32& OutDensity, int32& OutBreakers, bool& bOutForceProcedural);
	static FString BotDifficultyToString(EBHBotDifficulty Difficulty);
	static EBHBotDifficulty ParseBotDifficulty(const FString& Difficulty, EBHBotDifficulty DefaultDifficulty);
	static FString TopicMaskToText(int32 TopicMask);
	static FString DescribePreset(const FBHLessonPreset& Preset);
	static FString BuildRevisionLaunchOptions(const FBHLessonPreset& Preset, bool bLiveClassroom);
};

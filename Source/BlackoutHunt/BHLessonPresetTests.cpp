// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHLessonPreset.h"
#include "BHGameSettings.h"
#include "Containers/Set.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
struct FBHLessonPresetStorageTestGuard
{
	FBHLessonPresetStorageTestGuard()
		: StoragePath(FBHLessonPresetStore::GetStoragePath())
		, BackupPath(StoragePath + TEXT(".automation-backup-") + FGuid::NewGuid().ToString(EGuidFormats::Digits))
		, OriginalSelectedPresetId(FBHLessonPresetStore::GetSelectedPresetId())
	{
		const FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		bStoragePathIsSafe = !StoragePath.IsEmpty() && StoragePath.StartsWith(SavedDir);
		if (!bStoragePathIsSafe)
		{
			return;
		}

		bHadStorageFile = IFileManager::Get().FileExists(*StoragePath);
		if (bHadStorageFile)
		{
			bBackupCreated = IFileManager::Get().Copy(*BackupPath, *StoragePath, true, true) == COPY_OK;
			if (!bBackupCreated)
			{
				bStoragePathIsSafe = false;
				return;
			}
		}
		IFileManager::Get().Delete(*StoragePath, false, true, true);
	}

	~FBHLessonPresetStorageTestGuard()
	{
		if (bStoragePathIsSafe)
		{
			IFileManager::Get().Delete(*StoragePath, false, true, true);
			if (bHadStorageFile && bBackupCreated && IFileManager::Get().FileExists(*BackupPath))
			{
				IFileManager::Get().Copy(*StoragePath, *BackupPath, true, true);
			}
			IFileManager::Get().Delete(*BackupPath, false, true, true);
		}
		FBHLessonPresetStore::SetSelectedPresetId(OriginalSelectedPresetId);
	}

	FString StoragePath;
	FString BackupPath;
	FString OriginalSelectedPresetId;
	bool bStoragePathIsSafe = false;
	bool bHadStorageFile = false;
	bool bBackupCreated = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHLessonPresetValidationTest,
	"BlackoutHunt.Classroom.LessonPresetValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHLessonPresetValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<FBHLessonPreset> Builtins = FBHLessonPresetStore::GetBuiltinPresets(nullptr);
	TestTrue(TEXT("Built-in classroom presets are available without local saved data."), Builtins.Num() >= 4);

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const FBHLessonPreset DefaultPreset = FBHLessonPresetStore::MakeDefaultPreset(Settings);
	TestEqual(TEXT("Default lesson preset uses classroom revision round seconds."),
		DefaultPreset.RoundSeconds,
		FMath::Clamp(Settings ? Settings->RevisionRoundSeconds : 600, 60, 3600));
	TestTrue(TEXT("Blank or punctuation-only preset names are rejected before save."),
		FBHLessonPresetStore::SanitizeDisplayName(TEXT("  !!! <>  ")).IsEmpty());

	bool bFoundElectricity = false;
	bool bFoundLowScare = false;
	for (const FBHLessonPreset& Preset : Builtins)
	{
		bFoundElectricity |= Preset.DisplayName.Equals(TEXT("Electricity easy"), ESearchCase::CaseSensitive);
		bFoundLowScare |= Preset.DisplayName.Equals(TEXT("Low scare"), ESearchCase::CaseSensitive);
	}
	TestTrue(TEXT("Electricity easy preset exists."), bFoundElectricity);
	TestTrue(TEXT("Low scare preset exists."), bFoundLowScare);

	FBHLessonPreset StalePreset;
	StalePreset.DisplayName = TEXT("  Unsafe !!! Hard <> Name  ");
	StalePreset.TopicMask = 0;
	StalePreset.DifficultyMix = static_cast<EBHRevisionDifficultyMix>(255);
	StalePreset.ClassThreshold = 140.0f;
	StalePreset.IndividualThreshold = -20.0f;
	StalePreset.RoundSeconds = 12;
	StalePreset.ScareIntensity = 9;
	StalePreset.MapName = TEXT("UnknownMap");
	StalePreset.BotCount = 99;
	StalePreset.BotDifficulty = static_cast<EBHBotDifficulty>(255);

	bool bAdjusted = false;
	const FBHLessonPreset CleanPreset = FBHLessonPresetStore::ValidatePreset(StalePreset, &bAdjusted);
	TestTrue(TEXT("Stale preset values report adjustment."), bAdjusted);
	TestEqual(TEXT("Empty topic masks fall back to all topics."), CleanPreset.TopicMask, 0x0F);
	TestEqual(TEXT("Invalid difficulty mix falls back to adaptive."), CleanPreset.DifficultyMix, EBHRevisionDifficultyMix::Adaptive);
	TestEqual(TEXT("Class threshold clamps to 100."), CleanPreset.ClassThreshold, 100.0f);
	TestEqual(TEXT("Individual threshold clamps to 0."), CleanPreset.IndividualThreshold, 0.0f);
	TestEqual(TEXT("Round seconds clamp to one minute minimum."), CleanPreset.RoundSeconds, 60);
	TestEqual(TEXT("Scare intensity clamps to supported range."), CleanPreset.ScareIntensity, 3);
	TestEqual(TEXT("Unknown maps fall back to Facility."), CleanPreset.MapName, FString(TEXT("Facility")));
	TestEqual(TEXT("Bot count clamps to current classroom cap."), CleanPreset.BotCount, 11);
	TestEqual(TEXT("Invalid bot difficulty falls back to normal."), CleanPreset.BotDifficulty, EBHBotDifficulty::Normal);

	FBHLessonPreset UnsupportedTopicPreset = CleanPreset;
	UnsupportedTopicPreset.TopicMask = 0x10;
	const FBHLessonPreset CleanUnsupportedTopicPreset = FBHLessonPresetStore::ValidatePreset(UnsupportedTopicPreset);
	TestEqual(TEXT("Unsupported-only topic masks fall back to all topics."), CleanUnsupportedTopicPreset.TopicMask, 0x0F);

	const FString LaunchOptions = FBHLessonPresetStore::BuildRevisionLaunchOptions(CleanPreset, true);
	TestTrue(TEXT("Launch options keep revision mode enabled."), LaunchOptions.Contains(TEXT("BHRevisionMode=1")));
	TestTrue(TEXT("Launch options carry clamped topic mask."), LaunchOptions.Contains(TEXT("BHRevisionTopics=15")));
	TestTrue(TEXT("Launch options carry live classroom flag."), LaunchOptions.Contains(TEXT("BHLiveClassroom=1")));
	TestTrue(TEXT("Launch options carry bot fill when requested."), LaunchOptions.Contains(TEXT("BHBotCount=11")));

	FBHLessonPreset NoBotPreset = CleanPreset;
	NoBotPreset.BotCount = 0;
	const FString NoBotLaunchOptions = FBHLessonPresetStore::BuildRevisionLaunchOptions(NoBotPreset, true);
	TestFalse(TEXT("Launch options omit bot mode when no bots are requested."), NoBotLaunchOptions.Contains(TEXT("BHBotMode=1")));

	FBHLessonPreset ManualQuestionPreset = CleanPreset;
	ManualQuestionPreset.DisplayName = TEXT("Manual Waves Hard");
	ManualQuestionPreset.TopicMask = 0x04;
	ManualQuestionPreset.DifficultyMix = EBHRevisionDifficultyMix::Hard;

	FBHManualQuestionSet ManualQuestionSet;
	FString ManualQuestionMessage;
	TestTrue(TEXT("Manual question sets build from lesson preset topic and difficulty."),
		FBHLessonPresetStore::BuildManualQuestionSet(ManualQuestionPreset, 12, 1337, ManualQuestionSet, ManualQuestionMessage));
	TestEqual(TEXT("Manual question set clamps and returns the requested default worksheet size."),
		ManualQuestionSet.Questions.Num(),
		12);
	TestEqual(TEXT("Zero manual question count uses the teacher-friendly default."),
		FBHLessonPresetStore::ClampManualQuestionSetCount(0),
		12);
	TestEqual(TEXT("Large manual question count clamps to a printable cap."),
		FBHLessonPresetStore::ClampManualQuestionSetCount(100),
		32);

	TSet<FString> ManualQuestionIds;
	bool bManualQuestionsAreUnique = true;
	bool bManualQuestionsStayOnTopic = true;
	bool bManualQuestionSetHasHardQuestion = false;
	for (const FBHRevisionQuestion& Question : ManualQuestionSet.Questions)
	{
		bManualQuestionsAreUnique &= !ManualQuestionIds.Contains(Question.Id);
		ManualQuestionIds.Add(Question.Id);
		bManualQuestionsStayOnTopic &= Question.Topic == EBHPhysicsTopic::Waves;
		bManualQuestionSetHasHardQuestion |= Question.Difficulty == EBHQuestionDifficulty::Hard;
	}
	TestTrue(TEXT("Manual question set avoids duplicate question ids."), bManualQuestionsAreUnique);
	TestTrue(TEXT("Manual question set respects preset topic mask."), bManualQuestionsStayOnTopic);
	TestTrue(TEXT("Manual hard preset includes hard revision questions."), bManualQuestionSetHasHardQuestion);

	const FString ManualQuestionMarkdown = FBHLessonPresetStore::FormatManualQuestionSetMarkdown(ManualQuestionSet);
	TestTrue(TEXT("Manual question set markdown has a printable title."), ManualQuestionMarkdown.Contains(TEXT("# Blackout Hunt Manual Question Set")));
	TestTrue(TEXT("Manual question set markdown names the preset."), ManualQuestionMarkdown.Contains(TEXT("Manual Waves Hard")));
	TestTrue(TEXT("Manual question set markdown includes the answer key."), ManualQuestionMarkdown.Contains(TEXT("## Answer Key")));
	TestTrue(TEXT("Manual question set markdown records the question count."), ManualQuestionMarkdown.Contains(TEXT("Questions: 12")));

	FString SavedManualQuestionSetPath;
	FString SavedManualQuestionSetMessage;
	TestTrue(TEXT("Manual question set saves to a teacher-local markdown file."),
		FBHLessonPresetStore::SaveManualQuestionSet(ManualQuestionPreset, 12, SavedManualQuestionSetPath, SavedManualQuestionSetMessage));
	TestTrue(TEXT("Saved manual question set path stays under Saved."),
		SavedManualQuestionSetPath.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir())));
	TestTrue(TEXT("Saved manual question set file exists."), IFileManager::Get().FileExists(*SavedManualQuestionSetPath));
	if (IFileManager::Get().FileExists(*SavedManualQuestionSetPath))
	{
		FString SavedManualQuestionMarkdown;
		TestTrue(TEXT("Saved manual question set markdown can be read."),
			FFileHelper::LoadFileToString(SavedManualQuestionMarkdown, *SavedManualQuestionSetPath));
		TestTrue(TEXT("Saved manual question set markdown includes answer key."),
			SavedManualQuestionMarkdown.Contains(TEXT("## Answer Key")));
		IFileManager::Get().Delete(*SavedManualQuestionSetPath, false, true, true);
	}

	FBHLessonPresetStorageTestGuard StorageGuard;
	TestTrue(TEXT("Lesson preset storage test path stays under Saved."), StorageGuard.bStoragePathIsSafe);
	if (StorageGuard.bStoragePathIsSafe)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(StorageGuard.StoragePath), true);

		FFileHelper::SaveStringToFile(TEXT("{not valid json"), *StorageGuard.StoragePath);
		TArray<FBHLessonPreset> BadFilePresets;
		FString BadFileMessage;
		TestFalse(TEXT("Invalid lesson preset JSON fails gracefully."), FBHLessonPresetStore::LoadCustomPresets(BadFilePresets, BadFileMessage));
		TestEqual(TEXT("Invalid lesson preset JSON yields no custom presets."), BadFilePresets.Num(), 0);
		TestTrue(TEXT("Invalid lesson preset JSON reports a teacher-readable message."), BadFileMessage.Contains(TEXT("not valid JSON")));
		IFileManager::Get().Delete(*StorageGuard.StoragePath, false, true, true);

		FBHLessonPreset SavedPreset;
		SavedPreset.DisplayName = TEXT("  Waves Hard Revision  ");
		SavedPreset.TopicMask = 0x04;
		SavedPreset.DifficultyMix = EBHRevisionDifficultyMix::Hard;
		SavedPreset.ClassThreshold = 76.0f;
		SavedPreset.IndividualThreshold = 61.0f;
		SavedPreset.RoundSeconds = 540;
		SavedPreset.ScareIntensity = 1;
		SavedPreset.MapName = TEXT("Fog");
		SavedPreset.BotCount = 3;
		SavedPreset.BotDifficulty = EBHBotDifficulty::Hard;
		SavedPreset.bReducedJumpscares = true;
		SavedPreset.bReducedFlash = true;
		SavedPreset.bReducedCameraShake = false;
		SavedPreset.bCaptions = true;
		SavedPreset.bHighContrastHud = true;

		FString SaveMessage;
		TestTrue(TEXT("Named custom lesson preset saves."), FBHLessonPresetStore::SaveCustomPreset(SavedPreset, SaveMessage));
		TestTrue(TEXT("Save message names the saved preset."), SaveMessage.Contains(TEXT("Waves Hard Revision")));
		TestEqual(TEXT("Saved preset becomes selected."), FBHLessonPresetStore::GetSelectedPresetId(), FString(TEXT("custom.waves_hard_revision")));

		TArray<FBHLessonPreset> ReloadedCustomPresets;
		FString ReloadMessage;
		TestTrue(TEXT("Saved custom lesson presets reload."), FBHLessonPresetStore::LoadCustomPresets(ReloadedCustomPresets, ReloadMessage));
		TestEqual(TEXT("Only the isolated test preset exists in test storage."), ReloadedCustomPresets.Num(), 1);
		if (ReloadedCustomPresets.Num() == 1)
		{
			const FBHLessonPreset ReloadedPreset = ReloadedCustomPresets[0];
			TestEqual(TEXT("Reloaded preset id is stable."), ReloadedPreset.Id, FString(TEXT("custom.waves_hard_revision")));
			TestEqual(TEXT("Reloaded preset name is sanitized."), ReloadedPreset.DisplayName, FString(TEXT("Waves Hard Revision")));
			TestEqual(TEXT("Reloaded preset topic mask persists."), ReloadedPreset.TopicMask, 0x04);
			TestEqual(TEXT("Reloaded preset difficulty persists."), ReloadedPreset.DifficultyMix, EBHRevisionDifficultyMix::Hard);
			TestEqual(TEXT("Reloaded preset class target persists."), ReloadedPreset.ClassThreshold, 76.0f);
			TestEqual(TEXT("Reloaded preset individual target persists."), ReloadedPreset.IndividualThreshold, 61.0f);
			TestEqual(TEXT("Reloaded preset duration persists."), ReloadedPreset.RoundSeconds, 540);
			TestEqual(TEXT("Reloaded preset scare intensity persists."), ReloadedPreset.ScareIntensity, 1);
			TestEqual(TEXT("Reloaded preset map normalizes alias."), ReloadedPreset.MapName, FString(TEXT("Foggrounds")));
			TestEqual(TEXT("Reloaded preset bot count persists."), ReloadedPreset.BotCount, 3);
			TestEqual(TEXT("Reloaded preset bot difficulty persists."), ReloadedPreset.BotDifficulty, EBHBotDifficulty::Hard);
			TestTrue(TEXT("Reloaded preset reduced jumpscares persists."), ReloadedPreset.bReducedJumpscares);
			TestTrue(TEXT("Reloaded preset reduced flash persists."), ReloadedPreset.bReducedFlash);
			TestFalse(TEXT("Reloaded preset camera shake preference persists."), ReloadedPreset.bReducedCameraShake);
			TestTrue(TEXT("Reloaded preset captions persists."), ReloadedPreset.bCaptions);
			TestTrue(TEXT("Reloaded preset high contrast persists."), ReloadedPreset.bHighContrastHud);
		}

		FBHLessonPreset FoundPreset;
		FString FindMessage;
		TestTrue(TEXT("Saved preset can be found by display name."), FBHLessonPresetStore::TryFindPreset(TEXT("Waves Hard Revision"), FoundPreset, FindMessage));
		TestEqual(TEXT("Display-name lookup returns the saved preset id."), FoundPreset.Id, FString(TEXT("custom.waves_hard_revision")));
	}

	return true;
}

#endif

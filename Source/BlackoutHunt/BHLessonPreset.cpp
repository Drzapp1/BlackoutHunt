#include "BHLessonPreset.h"
#include "BHGameSettings.h"
#include "BHRevisionQuestionBank.h"
#include "Containers/Set.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr TCHAR BHLessonPresetConfigSection[] = TEXT("BlackoutHunt.LessonPresets");
constexpr TCHAR BHLessonPresetSelectedKey[] = TEXT("SelectedPresetId");
constexpr TCHAR BHLessonPresetDefaultId[] = TEXT("builtin.mixed_exam_prep");

FString BHLessonPresetJsonString(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, const FString& DefaultValue = FString())
{
	if (!JsonObject.IsValid())
	{
		return DefaultValue;
	}

	FString Value;
	return JsonObject->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

int32 BHLessonPresetJsonInt(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, const int32 DefaultValue)
{
	if (!JsonObject.IsValid())
	{
		return DefaultValue;
	}

	double Value = 0.0;
	return JsonObject->TryGetNumberField(FieldName, Value) ? FMath::RoundToInt(Value) : DefaultValue;
}

float BHLessonPresetJsonFloat(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, const float DefaultValue)
{
	if (!JsonObject.IsValid())
	{
		return DefaultValue;
	}

	double Value = 0.0;
	return JsonObject->TryGetNumberField(FieldName, Value) ? static_cast<float>(Value) : DefaultValue;
}

bool BHLessonPresetJsonBool(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, const bool bDefaultValue)
{
	if (!JsonObject.IsValid())
	{
		return bDefaultValue;
	}

	bool bValue = bDefaultValue;
	return JsonObject->TryGetBoolField(FieldName, bValue) ? bValue : bDefaultValue;
}

FString BHLessonPresetJsonToString(const TSharedRef<FJsonObject>& JsonObject)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

bool BHLessonPresetStringToJson(const FString& Input, TSharedPtr<FJsonObject>& OutJsonObject)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Input);
	return FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid();
}

FString BHLessonPresetBuiltinId(const FString& Token)
{
	return FString::Printf(TEXT("builtin.%s"), *Token);
}

int32 BHLessonPresetDefaultRoundSeconds(const UBHGameSettings* Settings = GetDefault<UBHGameSettings>())
{
	return FMath::Clamp(Settings ? Settings->RevisionRoundSeconds : 600, 60, 3600);
}

FBHLessonPreset BHLessonPresetMake(
	const FString& Id,
	const FString& DisplayName,
	int32 TopicMask,
	EBHRevisionDifficultyMix DifficultyMix,
	float ClassThreshold,
	float IndividualThreshold,
	int32 RoundSeconds,
	int32 ScareIntensity,
	const FString& MapName,
	int32 BotCount,
	EBHBotDifficulty BotDifficulty,
	bool bReducedJumpscares,
	bool bReducedFlash,
	bool bReducedCameraShake)
{
	FBHLessonPreset Preset;
	Preset.Id = Id;
	Preset.DisplayName = DisplayName;
	Preset.TopicMask = TopicMask;
	Preset.DifficultyMix = DifficultyMix;
	Preset.ClassThreshold = ClassThreshold;
	Preset.IndividualThreshold = IndividualThreshold;
	Preset.RoundSeconds = RoundSeconds;
	Preset.ScareIntensity = ScareIntensity;
	Preset.MapName = MapName;
	Preset.BotCount = BotCount;
	Preset.BotDifficulty = BotDifficulty;
	Preset.bReducedJumpscares = bReducedJumpscares;
	Preset.bReducedFlash = bReducedFlash;
	Preset.bReducedCameraShake = bReducedCameraShake;
	Preset.bCaptions = true;
	Preset.bHighContrastHud = false;
	Preset.bBuiltin = Id.StartsWith(TEXT("builtin."));
	return FBHLessonPresetStore::ValidatePreset(Preset);
}

TSharedRef<FJsonObject> BHLessonPresetToJson(const FBHLessonPreset& Preset)
{
	const FBHLessonPreset CleanPreset = FBHLessonPresetStore::ValidatePreset(Preset);
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("id"), CleanPreset.Id);
	JsonObject->SetStringField(TEXT("name"), CleanPreset.DisplayName);
	JsonObject->SetNumberField(TEXT("topic_mask"), CleanPreset.TopicMask);
	JsonObject->SetStringField(TEXT("difficulty_mix"), FBHLessonPresetStore::DifficultyMixToString(CleanPreset.DifficultyMix));
	JsonObject->SetNumberField(TEXT("class_threshold"), CleanPreset.ClassThreshold);
	JsonObject->SetNumberField(TEXT("individual_threshold"), CleanPreset.IndividualThreshold);
	JsonObject->SetNumberField(TEXT("round_seconds"), CleanPreset.RoundSeconds);
	JsonObject->SetNumberField(TEXT("scare_intensity"), CleanPreset.ScareIntensity);
	JsonObject->SetStringField(TEXT("map"), CleanPreset.MapName);
	JsonObject->SetNumberField(TEXT("bot_count"), CleanPreset.BotCount);
	JsonObject->SetStringField(TEXT("bot_difficulty"), FBHLessonPresetStore::BotDifficultyToString(CleanPreset.BotDifficulty));
	JsonObject->SetBoolField(TEXT("reduced_jumpscares"), CleanPreset.bReducedJumpscares);
	JsonObject->SetBoolField(TEXT("reduced_flash"), CleanPreset.bReducedFlash);
	JsonObject->SetBoolField(TEXT("reduced_camera_shake"), CleanPreset.bReducedCameraShake);
	JsonObject->SetBoolField(TEXT("captions"), CleanPreset.bCaptions);
	JsonObject->SetBoolField(TEXT("high_contrast_hud"), CleanPreset.bHighContrastHud);
	return JsonObject;
}

bool BHLessonPresetFromJson(const TSharedPtr<FJsonObject>& JsonObject, FBHLessonPreset& OutPreset)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	FBHLessonPreset Preset;
	Preset.DisplayName = BHLessonPresetJsonString(JsonObject, TEXT("name"));
	Preset.Id = BHLessonPresetJsonString(JsonObject, TEXT("id"), FBHLessonPresetStore::MakeCustomPresetId(Preset.DisplayName));
	if (Preset.DisplayName.IsEmpty())
	{
		return false;
	}

	Preset.TopicMask = BHLessonPresetJsonInt(JsonObject, TEXT("topic_mask"), 0x0F);
	Preset.DifficultyMix = FBHLessonPresetStore::ParseDifficultyMix(
		BHLessonPresetJsonString(JsonObject, TEXT("difficulty_mix")),
		EBHRevisionDifficultyMix::Adaptive);
	Preset.ClassThreshold = BHLessonPresetJsonFloat(JsonObject, TEXT("class_threshold"), 70.0f);
	Preset.IndividualThreshold = BHLessonPresetJsonFloat(JsonObject, TEXT("individual_threshold"), 50.0f);
	Preset.RoundSeconds = BHLessonPresetJsonInt(JsonObject, TEXT("round_seconds"), BHLessonPresetDefaultRoundSeconds());
	Preset.ScareIntensity = BHLessonPresetJsonInt(JsonObject, TEXT("scare_intensity"), 2);
	Preset.MapName = BHLessonPresetJsonString(JsonObject, TEXT("map"), TEXT("Facility"));
	Preset.BotCount = BHLessonPresetJsonInt(JsonObject, TEXT("bot_count"), 0);
	Preset.BotDifficulty = FBHLessonPresetStore::ParseBotDifficulty(
		BHLessonPresetJsonString(JsonObject, TEXT("bot_difficulty")),
		EBHBotDifficulty::Normal);
	Preset.bReducedJumpscares = BHLessonPresetJsonBool(JsonObject, TEXT("reduced_jumpscares"), false);
	Preset.bReducedFlash = BHLessonPresetJsonBool(JsonObject, TEXT("reduced_flash"), false);
	Preset.bReducedCameraShake = BHLessonPresetJsonBool(JsonObject, TEXT("reduced_camera_shake"), false);
	Preset.bCaptions = BHLessonPresetJsonBool(JsonObject, TEXT("captions"), true);
	Preset.bHighContrastHud = BHLessonPresetJsonBool(JsonObject, TEXT("high_contrast_hud"), false);
	Preset.bBuiltin = false;

	OutPreset = FBHLessonPresetStore::ValidatePreset(Preset);
	OutPreset.bBuiltin = false;
	return true;
}

TArray<EBHPhysicsTopic> BHLessonPresetTopicsForMask(int32 TopicMask)
{
	TopicMask &= 0x0F;
	if (TopicMask <= 0)
	{
		TopicMask = 0x0F;
	}

	TArray<EBHPhysicsTopic> Topics;
	if ((TopicMask & FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::ForcesAndMotion)) != 0)
	{
		Topics.Add(EBHPhysicsTopic::ForcesAndMotion);
	}
	if ((TopicMask & FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Electricity)) != 0)
	{
		Topics.Add(EBHPhysicsTopic::Electricity);
	}
	if ((TopicMask & FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Waves)) != 0)
	{
		Topics.Add(EBHPhysicsTopic::Waves);
	}
	if ((TopicMask & FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Energy)) != 0)
	{
		Topics.Add(EBHPhysicsTopic::Energy);
	}
	return Topics;
}

void BHLessonPresetAddUniqueDifficulty(TArray<EBHQuestionDifficulty>& Difficulties, EBHQuestionDifficulty Difficulty)
{
	if (!Difficulties.Contains(Difficulty))
	{
		Difficulties.Add(Difficulty);
	}
}

TArray<EBHQuestionDifficulty> BHLessonPresetDifficultyOrder(EBHRevisionDifficultyMix DifficultyMix, int32 QuestionIndex)
{
	TArray<EBHQuestionDifficulty> Difficulties;
	switch (DifficultyMix)
	{
	case EBHRevisionDifficultyMix::Easy:
		BHLessonPresetAddUniqueDifficulty(Difficulties, QuestionIndex % 4 == 3 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Easy);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Easy);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Medium);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Hard);
		break;
	case EBHRevisionDifficultyMix::Hard:
		BHLessonPresetAddUniqueDifficulty(Difficulties, QuestionIndex % 4 == 3 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Hard);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Medium);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Easy);
		break;
	case EBHRevisionDifficultyMix::Adaptive:
		switch (QuestionIndex % 4)
		{
		case 0:
		case 2:
			BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Medium);
			break;
		case 1:
			BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Hard);
			break;
		default:
			BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Easy);
			break;
		}
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Medium);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Hard);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Easy);
		break;
	case EBHRevisionDifficultyMix::Balanced:
	default:
		switch (QuestionIndex % 4)
		{
		case 0:
			BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Easy);
			break;
		case 1:
		case 2:
			BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Medium);
			break;
		default:
			BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Hard);
			break;
		}
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Medium);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Easy);
		BHLessonPresetAddUniqueDifficulty(Difficulties, EBHQuestionDifficulty::Hard);
		break;
	}
	return Difficulties;
}

FString BHLessonPresetInlineText(FString Text)
{
	Text.ReplaceInline(TEXT("\r"), TEXT(" "));
	Text.ReplaceInline(TEXT("\n"), TEXT(" "));
	while (Text.Contains(TEXT("  ")))
	{
		Text.ReplaceInline(TEXT("  "), TEXT(" "));
	}
	return Text.TrimStartAndEnd();
}

FString BHLessonPresetChoiceLabel(int32 ChoiceIndex)
{
	FString Label;
	Label.AppendChar(static_cast<TCHAR>('A' + FMath::Clamp(ChoiceIndex, 0, 25)));
	return Label;
}

FString BHLessonPresetCorrectAnswerText(const FBHRevisionQuestion& Question)
{
	const int32 CorrectChoiceIndex = Question.Answer.CorrectChoiceIndex;
	if (Question.Answer.Choices.IsValidIndex(CorrectChoiceIndex))
	{
		return FString::Printf(TEXT("%s. %s"),
			*BHLessonPresetChoiceLabel(CorrectChoiceIndex),
			*BHLessonPresetInlineText(Question.Answer.Choices[CorrectChoiceIndex]));
	}
	if (!Question.Answer.Formula.IsEmpty())
	{
		return BHLessonPresetInlineText(Question.Answer.Formula);
	}
	if (Question.Answer.NumericTolerance > 0.0f)
	{
		return FString::Printf(TEXT("%s +/- %s"),
			*FString::SanitizeFloat(Question.Answer.NumericAnswer),
			*FString::SanitizeFloat(Question.Answer.NumericTolerance));
	}
	return FString::SanitizeFloat(Question.Answer.NumericAnswer);
}

bool BHLessonPresetAddQuestionIfUnique(const FBHRevisionQuestion& Question, TSet<FString>& UsedQuestionIds, FBHManualQuestionSet& OutQuestionSet)
{
	if (Question.Id.IsEmpty() || UsedQuestionIds.Contains(Question.Id))
	{
		return false;
	}

	UsedQuestionIds.Add(Question.Id);
	OutQuestionSet.Questions.Add(Question);
	return true;
}
}

FString FBHLessonPresetStore::GetStoragePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ClassroomPresets") / TEXT("LessonPresets.json"));
}

FString FBHLessonPresetStore::GetQuestionSetDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ClassroomPresets") / TEXT("QuestionSets"));
}

FString FBHLessonPresetStore::GetSelectedPresetId()
{
	FString PresetId = BHLessonPresetDefaultId;
	if (GConfig)
	{
		GConfig->GetString(BHLessonPresetConfigSection, BHLessonPresetSelectedKey, PresetId, GGameUserSettingsIni);
	}
	return PresetId.IsEmpty() ? FString(BHLessonPresetDefaultId) : PresetId;
}

void FBHLessonPresetStore::SetSelectedPresetId(const FString& PresetId)
{
	if (!GConfig || PresetId.IsEmpty())
	{
		return;
	}

	GConfig->SetString(BHLessonPresetConfigSection, BHLessonPresetSelectedKey, *PresetId, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FBHLessonPreset FBHLessonPresetStore::MakeDefaultPreset(const UBHGameSettings* Settings)
{
	const int32 RoundSeconds = BHLessonPresetDefaultRoundSeconds(Settings);
	const int32 ScareIntensity = FMath::Clamp(Settings ? Settings->RevisionScareIntensity : 2, 0, 3);

	FBHLessonPreset Preset;
	Preset.Id = BHLessonPresetDefaultId;
	Preset.DisplayName = TEXT("Mixed exam prep");
	Preset.TopicMask = 0x0F;
	Preset.DifficultyMix = EBHRevisionDifficultyMix::Adaptive;
	Preset.ClassThreshold = Settings ? Settings->RevisionClassThreshold : 70.0f;
	Preset.IndividualThreshold = Settings ? Settings->RevisionIndividualThreshold : 50.0f;
	Preset.RoundSeconds = RoundSeconds;
	Preset.ScareIntensity = ScareIntensity;
	Preset.MapName = TEXT("Facility");
	Preset.BotCount = 0;
	Preset.BotDifficulty = Settings ? Settings->DefaultBotDifficulty : EBHBotDifficulty::Normal;
	Preset.bReducedJumpscares = false;
	Preset.bReducedFlash = false;
	Preset.bReducedCameraShake = false;
	Preset.bCaptions = true;
	Preset.bHighContrastHud = false;
	Preset.bBuiltin = true;
	return ValidatePreset(Preset);
}

TArray<FBHLessonPreset> FBHLessonPresetStore::GetBuiltinPresets(const UBHGameSettings* Settings)
{
	const int32 DefaultRoundSeconds = BHLessonPresetDefaultRoundSeconds(Settings);
	const int32 LongerRoundSeconds = FMath::Clamp(FMath::Max(DefaultRoundSeconds, 420), 60, 3600);

	TArray<FBHLessonPreset> Presets;
	Presets.Add(BHLessonPresetMake(
		BHLessonPresetBuiltinId(TEXT("electricity_easy")),
		TEXT("Electricity easy"),
		0x02,
		EBHRevisionDifficultyMix::Easy,
		60.0f,
		40.0f,
		LongerRoundSeconds,
		1,
		TEXT("Facility"),
		0,
		EBHBotDifficulty::Easy,
		true,
		true,
		true));
	Presets.Add(MakeDefaultPreset(Settings));
	Presets.Add(BHLessonPresetMake(
		BHLessonPresetBuiltinId(TEXT("low_scare")),
		TEXT("Low scare"),
		0x0F,
		EBHRevisionDifficultyMix::Balanced,
		70.0f,
		50.0f,
		LongerRoundSeconds,
		0,
		TEXT("Facility"),
		0,
		EBHBotDifficulty::Normal,
		true,
		true,
		true));
	Presets.Add(BHLessonPresetMake(
		BHLessonPresetBuiltinId(TEXT("hard_mode")),
		TEXT("Hard mode"),
		0x0F,
		EBHRevisionDifficultyMix::Hard,
		80.0f,
		60.0f,
		DefaultRoundSeconds,
		3,
		TEXT("Substation"),
		5,
		EBHBotDifficulty::Hard,
		false,
		false,
		false));
	return Presets;
}

bool FBHLessonPresetStore::LoadCustomPresets(TArray<FBHLessonPreset>& OutPresets, FString& OutMessage)
{
	OutPresets.Reset();
	OutMessage.Reset();

	const FString Path = GetStoragePath();
	if (!FPaths::FileExists(Path))
	{
		return true;
	}

	FString Input;
	if (!FFileHelper::LoadFileToString(Input, *Path))
	{
		OutMessage = FString::Printf(TEXT("Could not read lesson presets from %s."), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	if (!BHLessonPresetStringToJson(Input, Root))
	{
		OutMessage = TEXT("Lesson preset file is not valid JSON; built-in presets are still available.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* PresetValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("presets"), PresetValues) || !PresetValues)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& PresetValue : *PresetValues)
	{
		FBHLessonPreset Preset;
		if (PresetValue.IsValid() && BHLessonPresetFromJson(PresetValue->AsObject(), Preset))
		{
			if (!Preset.Id.StartsWith(TEXT("custom.")))
			{
				Preset.Id = MakeCustomPresetId(Preset.DisplayName);
			}
			Preset.bBuiltin = false;
			OutPresets.Add(Preset);
		}
	}

	return true;
}

bool FBHLessonPresetStore::LoadAllPresets(TArray<FBHLessonPreset>& OutPresets, FString& OutMessage)
{
	OutPresets = GetBuiltinPresets(GetDefault<UBHGameSettings>());

	TArray<FBHLessonPreset> CustomPresets;
	const bool bLoadedCustom = LoadCustomPresets(CustomPresets, OutMessage);
	for (const FBHLessonPreset& CustomPreset : CustomPresets)
	{
		OutPresets.Add(CustomPreset);
	}

	return bLoadedCustom;
}

bool FBHLessonPresetStore::TryFindPreset(const FString& PresetId, FBHLessonPreset& OutPreset, FString& OutMessage)
{
	TArray<FBHLessonPreset> Presets;
	LoadAllPresets(Presets, OutMessage);

	const FString WantedId = PresetId.IsEmpty() ? GetSelectedPresetId() : PresetId;
	for (const FBHLessonPreset& Preset : Presets)
	{
		if (Preset.Id.Equals(WantedId, ESearchCase::IgnoreCase)
			|| Preset.DisplayName.Equals(WantedId, ESearchCase::IgnoreCase))
		{
			OutPreset = ValidatePreset(Preset);
			return true;
		}
	}

	for (const FBHLessonPreset& Preset : Presets)
	{
		if (Preset.Id.Equals(BHLessonPresetDefaultId, ESearchCase::IgnoreCase))
		{
			OutPreset = ValidatePreset(Preset);
			OutMessage = FString::Printf(TEXT("Lesson preset '%s' was not found; using Mixed exam prep."), *WantedId);
			return false;
		}
	}

	OutPreset = MakeDefaultPreset(GetDefault<UBHGameSettings>());
	OutMessage = TEXT("Lesson presets were unavailable; using built-in defaults.");
	return false;
}

bool FBHLessonPresetStore::SaveCustomPreset(const FBHLessonPreset& Preset, FString& OutMessage)
{
	const FString CleanDisplayName = SanitizeDisplayName(Preset.DisplayName);
	if (CleanDisplayName.IsEmpty())
	{
		OutMessage = TEXT("Enter a preset name before saving.");
		return false;
	}

	FBHLessonPreset CleanPreset = Preset;
	CleanPreset.DisplayName = CleanDisplayName;
	CleanPreset.Id = MakeCustomPresetId(CleanDisplayName);
	CleanPreset.bBuiltin = false;
	CleanPreset = ValidatePreset(CleanPreset);
	CleanPreset.DisplayName = CleanDisplayName;
	CleanPreset.Id = MakeCustomPresetId(CleanDisplayName);
	CleanPreset.bBuiltin = false;

	TArray<FBHLessonPreset> CustomPresets;
	FString LoadMessage;
	const bool bLoadedExistingPresets = LoadCustomPresets(CustomPresets, LoadMessage);
	FString BackupMessage;
	const FString Path = GetStoragePath();
	if (!bLoadedExistingPresets && FPaths::FileExists(Path))
	{
		const FString BackupPath = FString::Printf(TEXT("%s.invalid-%s"),
			*Path,
			*FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		if (IFileManager::Get().Copy(*BackupPath, *Path, true, true) == COPY_OK)
		{
			BackupMessage = FString::Printf(TEXT(" Previous unreadable file was backed up to %s."), *BackupPath);
		}
	}

	CustomPresets.RemoveAll([&CleanPreset](const FBHLessonPreset& ExistingPreset)
	{
		return ExistingPreset.Id.Equals(CleanPreset.Id, ESearchCase::IgnoreCase)
			|| ExistingPreset.DisplayName.Equals(CleanPreset.DisplayName, ESearchCase::IgnoreCase);
	});
	CustomPresets.Add(CleanPreset);

	CustomPresets.Sort([](const FBHLessonPreset& A, const FBHLessonPreset& B)
	{
		return A.DisplayName < B.DisplayName;
	});

	TArray<TSharedPtr<FJsonValue>> PresetValues;
	for (const FBHLessonPreset& CustomPreset : CustomPresets)
	{
		PresetValues.Add(MakeShared<FJsonValueObject>(BHLessonPresetToJson(CustomPreset)));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetArrayField(TEXT("presets"), PresetValues);

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(BHLessonPresetJsonToString(Root), *Path))
	{
		OutMessage = FString::Printf(TEXT("Could not save lesson preset to %s."), *Path);
		return false;
	}

	SetSelectedPresetId(CleanPreset.Id);
	OutMessage = FString::Printf(TEXT("Saved lesson preset '%s'.%s"), *CleanPreset.DisplayName, *BackupMessage);
	return true;
}

int32 FBHLessonPresetStore::ClampManualQuestionSetCount(int32 QuestionCount)
{
	return QuestionCount <= 0 ? 12 : FMath::Clamp(QuestionCount, 4, 32);
}

bool FBHLessonPresetStore::BuildManualQuestionSet(const FBHLessonPreset& Preset, int32 QuestionCount, int32 Seed, FBHManualQuestionSet& OutQuestionSet, FString& OutMessage)
{
	OutQuestionSet = FBHManualQuestionSet();
	OutMessage.Reset();

	const FBHLessonPreset CleanPreset = ValidatePreset(Preset);
	const int32 ClampedQuestionCount = ClampManualQuestionSetCount(QuestionCount);
	const int32 EffectiveSeed = FMath::Max(1, FMath::Abs(Seed));
	TArray<EBHPhysicsTopic> Topics = BHLessonPresetTopicsForMask(CleanPreset.TopicMask);
	if (Topics.IsEmpty())
	{
		Topics = BHLessonPresetTopicsForMask(0x0F);
	}

	OutQuestionSet.Preset = CleanPreset;
	OutQuestionSet.RequestedQuestionCount = ClampedQuestionCount;
	OutQuestionSet.Seed = EffectiveSeed;
	OutQuestionSet.GeneratedAtUtc = FDateTime::UtcNow();

	TSet<FString> UsedQuestionIds;
	for (int32 QuestionIndex = 0; QuestionIndex < ClampedQuestionCount; ++QuestionIndex)
	{
		bool bAddedQuestion = false;
		// Unsigned arithmetic keeps the seed mixing well-defined (EffectiveSeed can be near INT_MAX)
		// and guarantees a non-negative TopicStart, avoiding a negative array index below.
		const int32 TopicStart = Topics.Num() > 0
			? static_cast<int32>((static_cast<uint32>(QuestionIndex) + static_cast<uint32>(EffectiveSeed)) % static_cast<uint32>(Topics.Num()))
			: 0;

		// DifficultyOrder depends only on the difficulty mix and the question index, not on
		// the topic, so compute it once per question instead of inside the TopicOffset loop.
		const TArray<EBHQuestionDifficulty> DifficultyOrder = BHLessonPresetDifficultyOrder(CleanPreset.DifficultyMix, QuestionIndex);

		for (int32 TopicOffset = 0; TopicOffset < Topics.Num() && !bAddedQuestion; ++TopicOffset)
		{
			const EBHPhysicsTopic Topic = Topics[(TopicStart + TopicOffset) % Topics.Num()];

			for (int32 DifficultyIndex = 0; DifficultyIndex < DifficultyOrder.Num() && !bAddedQuestion; ++DifficultyIndex)
			{
				const EBHQuestionDifficulty Difficulty = DifficultyOrder[DifficultyIndex];
				for (int32 Salt = 0; Salt < 32 && !bAddedQuestion; ++Salt)
				{
					FBHRevisionQuestion Candidate;
					const int32 QuestionSeed = static_cast<int32>(static_cast<uint32>(EffectiveSeed)
						+ static_cast<uint32>(QuestionIndex) * 1009u
						+ static_cast<uint32>(TopicOffset) * 97u
						+ static_cast<uint32>(DifficultyIndex) * 31u
						+ static_cast<uint32>(Salt));
					if (FBHRevisionQuestionBank::SelectQuestionByDifficulty(Topic, Difficulty, QuestionSeed, Candidate))
					{
						bAddedQuestion = BHLessonPresetAddQuestionIfUnique(Candidate, UsedQuestionIds, OutQuestionSet);
					}
				}
			}
		}

		if (!bAddedQuestion)
		{
			const TArray<FBHRevisionQuestion>& Bank = FBHRevisionQuestionBank::GetQuestions();
			for (const FBHRevisionQuestion& Question : Bank)
			{
				if ((CleanPreset.TopicMask & FBHRevisionQuestionBank::TopicMaskBit(Question.Topic)) != 0
					&& BHLessonPresetAddQuestionIfUnique(Question, UsedQuestionIds, OutQuestionSet))
				{
					bAddedQuestion = true;
					break;
				}
			}
		}

		if (!bAddedQuestion)
		{
			break;
		}
	}

	if (OutQuestionSet.Questions.Num() <= 0)
	{
		OutMessage = TEXT("Could not generate a manual question set because the revision question bank was empty.");
		return false;
	}
	if (OutQuestionSet.Questions.Num() < ClampedQuestionCount)
	{
		OutMessage = FString::Printf(TEXT("Generated %d unique questions for '%s'; the bank did not have enough unused matches for %d."),
			OutQuestionSet.Questions.Num(),
			*CleanPreset.DisplayName,
			ClampedQuestionCount);
		return true;
	}

	OutMessage = FString::Printf(TEXT("Generated %d manual questions for '%s'."), OutQuestionSet.Questions.Num(), *CleanPreset.DisplayName);
	return true;
}

FString FBHLessonPresetStore::FormatManualQuestionSetMarkdown(const FBHManualQuestionSet& QuestionSet)
{
	const FBHLessonPreset CleanPreset = ValidatePreset(QuestionSet.Preset);
	FString Output;
	Output += TEXT("# Blackout Hunt Manual Question Set\n\n");
	Output += FString::Printf(TEXT("Preset: %s\n"), *CleanPreset.DisplayName);
	Output += FString::Printf(TEXT("Focus: %s\n"), *TopicMaskToText(CleanPreset.TopicMask));
	Output += FString::Printf(TEXT("Difficulty mix: %s\n"), *DifficultyMixToString(CleanPreset.DifficultyMix));
	Output += FString::Printf(TEXT("Map: %s\n"), *CleanPreset.MapName);
	Output += FString::Printf(TEXT("Questions: %d\n"), QuestionSet.Questions.Num());
	Output += FString::Printf(TEXT("Seed: %d\n"), QuestionSet.Seed);
	Output += FString::Printf(TEXT("Generated UTC: %s\n\n"), *QuestionSet.GeneratedAtUtc.ToString(TEXT("%Y-%m-%d %H:%M:%S")));

	Output += TEXT("## Questions\n\n");
	for (int32 Index = 0; Index < QuestionSet.Questions.Num(); ++Index)
	{
		const FBHRevisionQuestion& Question = QuestionSet.Questions[Index];
		Output += FString::Printf(TEXT("%d. [%s | %s | %s] %s\n"),
			Index + 1,
			*BHLessonPresetInlineText(Question.TopicName.IsEmpty() ? FBHRevisionQuestionBank::TopicToString(Question.Topic) : Question.TopicName),
			*FBHRevisionQuestionBank::DifficultyToString(Question.Difficulty),
			*FBHRevisionQuestionBank::QuestionTypeToString(Question.Type),
			*BHLessonPresetInlineText(Question.Prompt));

		for (int32 ChoiceIndex = 0; ChoiceIndex < Question.Answer.Choices.Num(); ++ChoiceIndex)
		{
			Output += FString::Printf(TEXT("   %s. %s\n"),
				*BHLessonPresetChoiceLabel(ChoiceIndex),
				*BHLessonPresetInlineText(Question.Answer.Choices[ChoiceIndex]));
		}
		if (!Question.Hint.IsEmpty())
		{
			Output += FString::Printf(TEXT("   Hint: %s\n"), *BHLessonPresetInlineText(Question.Hint));
		}
		if (!Question.Answer.Formula.IsEmpty())
		{
			Output += FString::Printf(TEXT("   Formula: %s\n"), *BHLessonPresetInlineText(Question.Answer.Formula));
		}
		Output += TEXT("\n");
	}

	Output += TEXT("## Answer Key\n\n");
	for (int32 Index = 0; Index < QuestionSet.Questions.Num(); ++Index)
	{
		const FBHRevisionQuestion& Question = QuestionSet.Questions[Index];
		Output += FString::Printf(TEXT("%d. %s\n"), Index + 1, *BHLessonPresetCorrectAnswerText(Question));
		if (!Question.Explanation.IsEmpty())
		{
			Output += FString::Printf(TEXT("   %s\n"), *BHLessonPresetInlineText(Question.Explanation));
		}
	}

	return Output;
}

bool FBHLessonPresetStore::SaveManualQuestionSet(const FBHLessonPreset& Preset, int32 QuestionCount, FString& OutPath, FString& OutMessage)
{
	OutPath.Reset();

	FBHManualQuestionSet QuestionSet;
	const int32 Seed = FMath::Max(1, static_cast<int32>(FDateTime::UtcNow().GetTicks() & 0x7fffffff));
	if (!BuildManualQuestionSet(Preset, QuestionCount, Seed, QuestionSet, OutMessage))
	{
		return false;
	}

	const FString Directory = GetQuestionSetDirectory();
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutMessage = FString::Printf(TEXT("Could not create question set folder %s."), *Directory);
		return false;
	}

	const FString Token = MakeCustomPresetId(QuestionSet.Preset.DisplayName).RightChop(7);
	const FString SafeToken = Token.IsEmpty() ? FString(TEXT("question_set")) : Token;
	const FString FileName = FString::Printf(TEXT("%s-%s.md"),
		*SafeToken,
		*FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	OutPath = Directory / FileName;

	const FString Markdown = FormatManualQuestionSetMarkdown(QuestionSet);
	if (!FFileHelper::SaveStringToFile(Markdown, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutMessage = FString::Printf(TEXT("Could not save manual question set to %s."), *OutPath);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Generated %d-question manual set from '%s': %s"),
		QuestionSet.Questions.Num(),
		*QuestionSet.Preset.DisplayName,
		*OutPath);
	return true;
}

FBHLessonPreset FBHLessonPresetStore::ValidatePreset(const FBHLessonPreset& Preset, bool* bOutAdjusted)
{
	bool bAdjusted = false;
	FBHLessonPreset CleanPreset = Preset;

	const FString CleanDisplayName = SanitizeDisplayName(CleanPreset.DisplayName);
	if (!CleanDisplayName.Equals(CleanPreset.DisplayName, ESearchCase::CaseSensitive))
	{
		bAdjusted = true;
		CleanPreset.DisplayName = CleanDisplayName;
	}
	if (CleanPreset.DisplayName.IsEmpty())
	{
		CleanPreset.DisplayName = TEXT("Classroom preset");
		bAdjusted = true;
	}

	const int32 MaskedTopicMask = CleanPreset.TopicMask & 0x0F;
	const int32 CleanTopicMask = MaskedTopicMask <= 0 ? 0x0F : MaskedTopicMask;
	if (CleanTopicMask != CleanPreset.TopicMask)
	{
		bAdjusted = true;
		CleanPreset.TopicMask = CleanTopicMask;
	}

	const uint8 DifficultyValue = static_cast<uint8>(CleanPreset.DifficultyMix);
	if (DifficultyValue > static_cast<uint8>(EBHRevisionDifficultyMix::Adaptive))
	{
		CleanPreset.DifficultyMix = EBHRevisionDifficultyMix::Adaptive;
		bAdjusted = true;
	}

	const float CleanClassThreshold = FMath::Clamp(CleanPreset.ClassThreshold, 0.0f, 100.0f);
	const float CleanIndividualThreshold = FMath::Clamp(CleanPreset.IndividualThreshold, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(CleanClassThreshold, CleanPreset.ClassThreshold)
		|| !FMath::IsNearlyEqual(CleanIndividualThreshold, CleanPreset.IndividualThreshold))
	{
		bAdjusted = true;
		CleanPreset.ClassThreshold = CleanClassThreshold;
		CleanPreset.IndividualThreshold = CleanIndividualThreshold;
	}

	const int32 CleanRoundSeconds = FMath::Clamp(CleanPreset.RoundSeconds, 60, 3600);
	if (CleanRoundSeconds != CleanPreset.RoundSeconds)
	{
		bAdjusted = true;
		CleanPreset.RoundSeconds = CleanRoundSeconds;
	}

	const int32 CleanScareIntensity = FMath::Clamp(CleanPreset.ScareIntensity, 0, 3);
	if (CleanScareIntensity != CleanPreset.ScareIntensity)
	{
		bAdjusted = true;
		CleanPreset.ScareIntensity = CleanScareIntensity;
	}

	const FString CleanMapName = NormalizeMapName(CleanPreset.MapName);
	if (!CleanMapName.Equals(CleanPreset.MapName, ESearchCase::CaseSensitive))
	{
		bAdjusted = true;
		CleanPreset.MapName = CleanMapName;
	}

	const int32 CleanBotCount = FMath::Clamp(CleanPreset.BotCount, 0, 11);
	if (CleanBotCount != CleanPreset.BotCount)
	{
		bAdjusted = true;
		CleanPreset.BotCount = CleanBotCount;
	}

	const uint8 BotDifficultyValue = static_cast<uint8>(CleanPreset.BotDifficulty);
	if (BotDifficultyValue > static_cast<uint8>(EBHBotDifficulty::Hard))
	{
		CleanPreset.BotDifficulty = EBHBotDifficulty::Normal;
		bAdjusted = true;
	}

	if (CleanPreset.Id.IsEmpty())
	{
		CleanPreset.Id = CleanPreset.bBuiltin
			? BHLessonPresetBuiltinId(MakeCustomPresetId(CleanPreset.DisplayName).RightChop(7))
			: MakeCustomPresetId(CleanPreset.DisplayName);
		bAdjusted = true;
	}

	if (bOutAdjusted)
	{
		*bOutAdjusted = bAdjusted;
	}
	return CleanPreset;
}

FString FBHLessonPresetStore::SanitizeDisplayName(const FString& DisplayName)
{
	FString Trimmed = DisplayName.TrimStartAndEnd();
	FString Sanitized;
	bool bLastWasSpace = false;
	for (const TCHAR Character : Trimmed)
	{
		if (FChar::IsAlnum(Character) || Character == TEXT('-') || Character == TEXT('_'))
		{
			Sanitized.AppendChar(Character);
			bLastWasSpace = false;
		}
		else if (FChar::IsWhitespace(Character) && !bLastWasSpace && !Sanitized.IsEmpty())
		{
			Sanitized.AppendChar(TEXT(' '));
			bLastWasSpace = true;
		}

		if (Sanitized.Len() >= 36)
		{
			break;
		}
	}
	Sanitized.TrimStartAndEndInline();
	return Sanitized;
}

FString FBHLessonPresetStore::MakeCustomPresetId(const FString& DisplayName)
{
	const FString CleanDisplayName = SanitizeDisplayName(DisplayName);
	FString Token;
	bool bLastWasSeparator = false;
	for (const TCHAR Character : CleanDisplayName)
	{
		if (FChar::IsAlnum(Character))
		{
			Token.AppendChar(FChar::ToLower(Character));
			bLastWasSeparator = false;
		}
		else if (!bLastWasSeparator && !Token.IsEmpty())
		{
			Token.AppendChar(TEXT('_'));
			bLastWasSeparator = true;
		}
	}
	Token.TrimEndInline();
	while (Token.EndsWith(TEXT("_")))
	{
		Token.LeftChopInline(1);
	}
	if (Token.IsEmpty())
	{
		Token = TEXT("classroom_preset");
	}
	return FString::Printf(TEXT("custom.%s"), *Token.Left(40));
}

FString FBHLessonPresetStore::NormalizeMapName(FString MapName)
{
	MapName.TrimStartAndEndInline();
	if (MapName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || MapName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
	{
		return TEXT("Foggrounds");
	}
	return MapName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? TEXT("Substation") : TEXT("Facility");
}

FString FBHLessonPresetStore::DifficultyMixToString(EBHRevisionDifficultyMix DifficultyMix)
{
	switch (DifficultyMix)
	{
	case EBHRevisionDifficultyMix::Easy:
		return TEXT("Easy");
	case EBHRevisionDifficultyMix::Hard:
		return TEXT("Hard");
	case EBHRevisionDifficultyMix::Adaptive:
		return TEXT("Adaptive");
	case EBHRevisionDifficultyMix::Balanced:
	default:
		return TEXT("Balanced");
	}
}

EBHRevisionDifficultyMix FBHLessonPresetStore::ParseDifficultyMix(const FString& DifficultyMix, EBHRevisionDifficultyMix DefaultMix)
{
	if (DifficultyMix.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Easy;
	}
	if (DifficultyMix.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Hard;
	}
	if (DifficultyMix.Equals(TEXT("Adaptive"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Adaptive;
	}
	if (DifficultyMix.Equals(TEXT("Balanced"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Balanced;
	}
	return DefaultMix;
}

FString FBHLessonPresetStore::BotDifficultyToString(EBHBotDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EBHBotDifficulty::Easy:
		return TEXT("Easy");
	case EBHBotDifficulty::Hard:
		return TEXT("Hard");
	case EBHBotDifficulty::Normal:
	default:
		return TEXT("Normal");
	}
}

EBHBotDifficulty FBHLessonPresetStore::ParseBotDifficulty(const FString& Difficulty, EBHBotDifficulty DefaultDifficulty)
{
	if (Difficulty.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Easy;
	}
	if (Difficulty.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Hard;
	}
	if (Difficulty.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Normal;
	}
	return DefaultDifficulty;
}

FString FBHLessonPresetStore::TopicMaskToText(int32 TopicMask)
{
	TopicMask = TopicMask <= 0 ? 0x0F : (TopicMask & 0x0F);
	if (TopicMask == 0x0F)
	{
		return TEXT("All");
	}

	TArray<FString> Parts;
	if ((TopicMask & 0x01) != 0)
	{
		Parts.Add(TEXT("Forces"));
	}
	if ((TopicMask & 0x02) != 0)
	{
		Parts.Add(TEXT("Electricity"));
	}
	if ((TopicMask & 0x04) != 0)
	{
		Parts.Add(TEXT("Waves"));
	}
	if ((TopicMask & 0x08) != 0)
	{
		Parts.Add(TEXT("Energy"));
	}
	return Parts.IsEmpty() ? FString(TEXT("All")) : FString::Join(Parts, TEXT(", "));
}

FString FBHLessonPresetStore::DescribePreset(const FBHLessonPreset& Preset)
{
	const FBHLessonPreset CleanPreset = ValidatePreset(Preset);
	return FString::Printf(TEXT("%s | Focus %s | %s | Targets %.0f/%.0f | %d sec | Scare %d | %s | Bots %d %s"),
		*CleanPreset.DisplayName,
		*TopicMaskToText(CleanPreset.TopicMask),
		*DifficultyMixToString(CleanPreset.DifficultyMix),
		CleanPreset.ClassThreshold,
		CleanPreset.IndividualThreshold,
		CleanPreset.RoundSeconds,
		CleanPreset.ScareIntensity,
		*CleanPreset.MapName,
		CleanPreset.BotCount,
		*BotDifficultyToString(CleanPreset.BotDifficulty));
}

FString FBHLessonPresetStore::BuildRevisionLaunchOptions(const FBHLessonPreset& Preset, bool bLiveClassroom)
{
	const FBHLessonPreset CleanPreset = ValidatePreset(Preset);
	FString Options = FString::Printf(
		TEXT("?BHStageIndex=0?BHRevisionMode=1?BHRevisionTopics=%d?BHRevisionDifficultyMix=%s?BHRevisionClassThreshold=%.0f?BHRevisionIndividualThreshold=%.0f?BHHuntSeconds=%d?BHScareIntensity=%d"),
		CleanPreset.TopicMask,
		*DifficultyMixToString(CleanPreset.DifficultyMix),
		CleanPreset.ClassThreshold,
		CleanPreset.IndividualThreshold,
		CleanPreset.RoundSeconds,
		CleanPreset.ScareIntensity);

	if (CleanPreset.BotCount > 0)
	{
		Options += FString::Printf(TEXT("?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s"),
			CleanPreset.BotCount,
			*BotDifficultyToString(CleanPreset.BotDifficulty));
	}
	if (bLiveClassroom)
	{
		Options += TEXT("?BHLiveClassroom=1");
	}
	return Options;
}

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHPlayerController.h"
#include "BHAccountSubsystem.h"
#include "BHAutomationSupport.h"
#include "BHFeedbackSubsystem.h"
#include "BHCharacter.h"
#include "BHCosmeticUnlocks.h"
#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHJumpscareMonster.h"
#include "BHJumpscareVariantLibrary.h"
#include "BHLessonPreset.h"
#include "BHNetworkSupport.h"
#include "BHPlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/InputSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SBHClassroomBoard.h"
#include "SBHMainMenu.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#include <initializer_list>

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformApplicationMisc.h"
#endif

namespace
{
constexpr TCHAR BHAudioConfigSection[] = TEXT("BlackoutHunt.Audio");
constexpr TCHAR BHComfortConfigSection[] = TEXT("BlackoutHunt.Comfort");
constexpr TCHAR BHGraphicsConfigSection[] = TEXT("BlackoutHunt.Graphics");
constexpr TCHAR BHAtmosphereProfileConfigSection[] = TEXT("BlackoutHunt.FoggroundsAtmosphereProfile");
constexpr TCHAR BHAmbientMusicAssetPath[] = TEXT("/Game/BlackoutHunt/Audio/SW_EerieLobbyLoop.SW_EerieLobbyLoop");
constexpr TCHAR BHMenuClickAssetPath[] = TEXT("/Game/BlackoutHunt/Audio/SW_MenuClick.SW_MenuClick");
constexpr float BHGraphicsGiB = 1024.0f * 1024.0f * 1024.0f;
constexpr int32 BHAdaptiveMinRenderScale = 50;
constexpr int32 BHAdaptiveMaxStep = 4;
constexpr float BHAdaptiveEvaluationIntervalSeconds = 1.5f;
constexpr float BHAdaptiveUnderTargetRatio = 0.96f;
constexpr float BHAdaptiveSevereUnderTargetRatio = 0.84f;
constexpr float BHAdaptiveStableTargetRatio = 0.99f;
constexpr float BHAdaptiveClearlyOverTargetRatio = 1.04f;
constexpr int32 BHAdaptiveUnderTargetSamplesBeforeAdjust = 2;
constexpr int32 BHAdaptiveStableSamplesBeforeRecover = 8;
constexpr int32 BHAdaptiveOverTargetSamplesBeforeRecover = 2;

const FSlateBrush* BHUiWhiteBrush()
{
	return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
}

FSlateFontInfo BHUiFont(const int32 Size, const FName Typeface = FName(TEXT("Regular")))
{
	return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
}

FLinearColor BHLoadingAccentForTitle(const FString& DisplayTitle)
{
	if (DisplayTitle.Contains(TEXT("JOIN")))
	{
		return FLinearColor(0.28f, 0.70f, 1.0f, 1.0f);
	}

	if (DisplayTitle.Contains(TEXT("PRACTICE")) || DisplayTitle.Contains(TEXT("TEST")))
	{
		return FLinearColor(0.42f, 0.95f, 0.58f, 1.0f);
	}

	if (DisplayTitle.Contains(TEXT("BOT")))
	{
		return FLinearColor(0.78f, 0.58f, 1.0f, 1.0f);
	}

	if (DisplayTitle.Contains(TEXT("MAIN MENU")))
	{
		return FLinearColor(1.0f, 0.56f, 0.30f, 1.0f);
	}

	return FLinearColor(0.22f, 0.82f, 0.74f, 1.0f);
}

FString BHLoadingStatusForTitle(const FString& DisplayTitle)
{
	if (DisplayTitle.Contains(TEXT("JOIN")))
	{
		return TEXT("NETWORK HANDSHAKE");
	}

	if (DisplayTitle.Contains(TEXT("PRACTICE")) || DisplayTitle.Contains(TEXT("TEST")))
	{
		return TEXT("SANDBOX ROUTE");
	}

	if (DisplayTitle.Contains(TEXT("CLASSROOM")))
	{
		return TEXT("CLASSROOM SESSION");
	}

	if (DisplayTitle.Contains(TEXT("BOT")))
	{
		return TEXT("OFFLINE ROUTE");
	}

	if (DisplayTitle.Contains(TEXT("MAIN MENU")))
	{
		return TEXT("RETURNING");
	}

	return TEXT("LOADING ROUTE");
}

struct FBHConsoleVariableSetting
{
	const TCHAR* Name;
	const TCHAR* Value;
};

struct FBHGraphicsHardwareProfile
{
	FString GpuBrand;
	float SystemMemoryGB = 0.0f;
	float DedicatedVideoMemoryGB = 0.0f;
	int32 PhysicalCores = 0;
	int32 LogicalCores = 0;
	bool bLikelyIntegratedGpu = false;
	bool bLikelySoftwareGpu = false;
	bool bPreferSafeResolution = false;
	int32 RecommendedPreset = 1;
	int32 RecommendedFpsGoal = 60;
	int32 RecommendedRenderScale = 82;
};

float BHClampVolume(float Volume)
{
	return FMath::Clamp(Volume, 0.0f, 1.0f);
}

int32 BHClampGraphicsQuality(int32 Quality)
{
	return FMath::Clamp(Quality, 0, 3);
}

int32 BHClampRenderScale(int32 Percent)
{
	return FMath::Clamp(Percent, BHAdaptiveMinRenderScale, 100);
}

int32 BHClampFpsGoal(int32 FpsGoal)
{
	return FMath::Clamp(FpsGoal, 30, 240);
}

float BHFpsGoalToFrameTimeBudgetMs(int32 FpsGoal)
{
	return 1000.0f / static_cast<float>(BHClampFpsGoal(FpsGoal));
}

int32 BHResolveGraphicsFrameRateLimit(bool bAdaptiveGraphicsEnabled, int32 AdaptiveFpsGoal, int32 FrameRateLimit)
{
	if (bAdaptiveGraphicsEnabled)
	{
		return BHClampFpsGoal(AdaptiveFpsGoal);
	}

	return FrameRateLimit <= 0 ? 0 : FMath::Clamp(FrameRateLimit, 30, 360);
}

const TCHAR* BHGraphicsPresetLabel(int32 Quality)
{
	static const TCHAR* Labels[] = { TEXT("Low 4GB"), TEXT("Medium"), TEXT("High 16GB"), TEXT("Ultra") };
	return Labels[BHClampGraphicsQuality(Quality)];
}

float BHGraphicsPresetFoliageDistanceScale(int32 Quality)
{
	switch (BHClampGraphicsQuality(Quality))
	{
	case 0:
		return 0.65f;
	case 1:
		return 0.85f;
	default:
		return 1.0f;
	}
}

float BHGraphicsPresetStaticMeshDistanceScale(int32 Quality)
{
	switch (BHClampGraphicsQuality(Quality))
	{
	case 0:
		return 1.45f;
	case 1:
		return 1.15f;
	default:
		return 1.0f;
	}
}

bool BHStringContainsAny(const FString& Value, std::initializer_list<const TCHAR*> Needles)
{
	for (const TCHAR* Needle : Needles)
	{
		if (Value.Contains(Needle, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FBHGraphicsHardwareProfile BHScanGraphicsHardwareProfile()
{
	FBHGraphicsHardwareProfile Profile;
	Profile.GpuBrand = FPlatformMisc::GetPrimaryGPUBrand().TrimStartAndEnd();
	Profile.PhysicalCores = FMath::Max(1, FPlatformMisc::NumberOfCores());
	Profile.LogicalCores = FMath::Max(Profile.PhysicalCores, FPlatformMisc::NumberOfCoresIncludingHyperthreads());

	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	if (MemoryStats.TotalPhysical > 0)
	{
		Profile.SystemMemoryGB = static_cast<float>(MemoryStats.TotalPhysical) / BHGraphicsGiB;
	}
	else
	{
		Profile.SystemMemoryGB = static_cast<float>(FPlatformMemory::GetConstants().TotalPhysicalGB);
	}

#if PLATFORM_WINDOWS
	const FWindowsPlatformApplicationMisc::FGPUInfo GpuInfo = FWindowsPlatformApplicationMisc::GetBestGPUInfo();
	if (GpuInfo.DedicatedVideoMemory > 0)
	{
		Profile.DedicatedVideoMemoryGB = static_cast<float>(GpuInfo.DedicatedVideoMemory) / BHGraphicsGiB;
	}
#endif

	const FString GpuBrand = Profile.GpuBrand;
	Profile.bLikelySoftwareGpu = GpuBrand.IsEmpty()
		|| BHStringContainsAny(GpuBrand, {
			TEXT("VirtualBox"),
			TEXT("VMware"),
			TEXT("Parallels"),
			TEXT("Microsoft Basic"),
			TEXT("Microsoft Remote"),
			TEXT("WARP"),
			TEXT("SwiftShader"),
			TEXT("llvmpipe"),
			TEXT("Remote"),
			TEXT("Generic"),
			TEXT("Unknown"),
			TEXT("Software")
		});
	Profile.bLikelyIntegratedGpu = BHStringContainsAny(GpuBrand, {
			TEXT("Intel"),
			TEXT("UHD"),
			TEXT("Iris"),
			TEXT("Vega"),
			TEXT("Radeon Graphics"),
			TEXT("Integrated")
		})
		&& !BHStringContainsAny(GpuBrand, { TEXT("NVIDIA"), TEXT("GeForce"), TEXT("RTX"), TEXT("GTX"), TEXT("Arc") });

	const bool bVeryLowSystemMemory = Profile.SystemMemoryGB > 0.0f && Profile.SystemMemoryGB <= 6.25f;
	const bool bLowSystemMemory = Profile.SystemMemoryGB > 0.0f && Profile.SystemMemoryGB <= 10.25f;
	const bool bUnknownVideoMemory = Profile.DedicatedVideoMemoryGB <= 0.0f;
	const bool bKnownLowVideoMemory = Profile.DedicatedVideoMemoryGB > 0.0f && Profile.DedicatedVideoMemoryGB <= 4.25f;
	const bool bModerateVideoMemory = Profile.DedicatedVideoMemoryGB > 0.0f && Profile.DedicatedVideoMemoryGB <= 6.25f;
	const bool bStrongVideoMemory = Profile.DedicatedVideoMemoryGB >= 7.5f;
	const bool bVeryStrongVideoMemory = Profile.DedicatedVideoMemoryGB >= 10.5f;
	// A machine with plenty of system RAM (>=16 GB) and a real multi-core CPU is not a "Low 4GB" box even
	// if its GPU brand looks integrated - route it to at least Medium (which keeps Lumen OFF) instead of
	// the floor. Discrete GPUs still reach High/Ultra below; software GPUs and low-RAM/low-VRAM boxes are
	// unaffected and still drop to Low.
	const bool bAbundantSystemMemory = Profile.SystemMemoryGB >= 15.5f && Profile.PhysicalCores >= 6;
	const bool bIntegratedOrUnknownGpuNeedsLow = (Profile.bLikelyIntegratedGpu || (Profile.GpuBrand.IsEmpty() && bUnknownVideoMemory))
		&& !bAbundantSystemMemory;

	if (Profile.bLikelySoftwareGpu || bIntegratedOrUnknownGpuNeedsLow || bVeryLowSystemMemory || bKnownLowVideoMemory)
	{
		Profile.RecommendedPreset = 0;
		Profile.RecommendedFpsGoal = Profile.bLikelySoftwareGpu ? 30 : 45;
		Profile.RecommendedRenderScale = Profile.bLikelySoftwareGpu ? 60 : 67;
		Profile.bPreferSafeResolution = true;
	}
	else if (Profile.bLikelyIntegratedGpu || bModerateVideoMemory || bLowSystemMemory || Profile.PhysicalCores <= 4)
	{
		Profile.RecommendedPreset = 1;
		Profile.RecommendedFpsGoal = 60;
		Profile.RecommendedRenderScale = 82;
	}
	else if (bVeryStrongVideoMemory && Profile.SystemMemoryGB >= 24.0f && Profile.PhysicalCores >= 8)
	{
		Profile.RecommendedPreset = 3;
		Profile.RecommendedFpsGoal = 120;
		Profile.RecommendedRenderScale = 100;
	}
	else if (bStrongVideoMemory || Profile.SystemMemoryGB >= 15.5f)
	{
		Profile.RecommendedPreset = 2;
		Profile.RecommendedFpsGoal = 90;
		Profile.RecommendedRenderScale = 100;
	}
	else
	{
		Profile.RecommendedPreset = 1;
		Profile.RecommendedFpsGoal = 60;
		Profile.RecommendedRenderScale = 82;
	}

	return Profile;
}

bool BHPlayerControllerSoftObjectPathExists(const FSoftObjectPath& ObjectPath)
{
	if (ObjectPath.IsNull())
	{
		return false;
	}

	static TMap<FSoftObjectPath, bool> OptionalAssetPathCache;
	if (const bool* bCachedExists = OptionalAssetPathCache.Find(ObjectPath))
	{
		return *bCachedExists;
	}

	const FString PackageName = ObjectPath.GetLongPackageName();
	const bool bExists = !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
	OptionalAssetPathCache.Add(ObjectPath, bExists);
	return bExists;
}

void BHLogOptionalPlayerAssetFallbackOnce(const FSoftObjectPath& ObjectPath, const TCHAR* Context, const TCHAR* Reason)
{
	if (ObjectPath.IsNull())
	{
		return;
	}

	static TSet<FSoftObjectPath> FallbackAssetPaths;
	if (FallbackAssetPaths.Contains(ObjectPath))
	{
		return;
	}

	FallbackAssetPaths.Add(ObjectPath);
	UE_LOG(LogTemp, Verbose, TEXT("BlackoutHunt %s %s, using fallback: %s"), Context, Reason, *ObjectPath.ToString());
}

USoundBase* BHLoadGameplaySound(const TSoftObjectPtr<USoundBase>& SoundAsset, const TCHAR* Context)
{
	if (SoundAsset.IsNull())
	{
		return nullptr;
	}

	static TMap<FSoftObjectPath, TWeakObjectPtr<USoundBase>> LoadedSounds;
	static TSet<FSoftObjectPath> MissingSoundPaths;

	const FSoftObjectPath ObjectPath = SoundAsset.ToSoftObjectPath();
	if (TWeakObjectPtr<USoundBase>* CachedSound = LoadedSounds.Find(ObjectPath))
	{
		if (CachedSound->IsValid())
		{
			return CachedSound->Get();
		}
	}

	if (MissingSoundPaths.Contains(ObjectPath))
	{
		return nullptr;
	}

	if (!BHPlayerControllerSoftObjectPathExists(ObjectPath))
	{
		MissingSoundPaths.Add(ObjectPath);
		BHLogOptionalPlayerAssetFallbackOnce(ObjectPath, Context, TEXT("missing"));
		return nullptr;
	}

	USoundBase* Sound = SoundAsset.LoadSynchronous();
	if (!Sound)
	{
		MissingSoundPaths.Add(ObjectPath);
		BHLogOptionalPlayerAssetFallbackOnce(ObjectPath, Context, TEXT("failed to load"));
		return nullptr;
	}

	LoadedSounds.Add(ObjectPath, Sound);
	return Sound;
}

bool BHFindJumpscareVariantForCue(FName VariantId, FBHJumpscareVariant& OutVariant)
{
	return FindResolvedJumpscareVariantById(VariantId, OutVariant);
}

int32 BHCountConnectedPlayerControllers(UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get())
		{
			++Count;
		}
	}
	return Count;
}

float BHLoadAudioPreference(const TCHAR* Key, float DefaultValue)
{
	float Value = DefaultValue;
	if (GConfig)
	{
		GConfig->GetFloat(BHAudioConfigSection, Key, Value, GGameUserSettingsIni);
	}
	return BHClampVolume(Value);
}

bool BHLoadComfortPreference(const TCHAR* Key, bool bDefaultValue)
{
	bool bValue = bDefaultValue;
	if (GConfig)
	{
		GConfig->GetBool(BHComfortConfigSection, Key, bValue, GGameUserSettingsIni);
	}
	return bValue;
}

float BHLoadComfortScalarPreference(const TCHAR* Key, float DefaultValue)
{
	float Value = DefaultValue;
	if (GConfig)
	{
		GConfig->GetFloat(BHComfortConfigSection, Key, Value, GGameUserSettingsIni);
	}
	return Value;
}

FString BHBotDifficultyToString(EBHBotDifficulty Difficulty)
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

FString BHRevisionDifficultyMixToString(EBHRevisionDifficultyMix DifficultyMix)
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

FString BHSanitizeDisplayName(FString DisplayName)
{
	DisplayName.TrimStartAndEndInline();

	FString Sanitized;
	bool bLastWasSpace = false;
	for (const TCHAR Character : DisplayName)
	{
		if (FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-') || Character == TEXT('.'))
		{
			Sanitized.AppendChar(Character);
			bLastWasSpace = false;
		}
		else if (FChar::IsWhitespace(Character) && !bLastWasSpace && !Sanitized.IsEmpty())
		{
			Sanitized.AppendChar(TEXT(' '));
			bLastWasSpace = true;
		}

		if (Sanitized.Len() >= 24)
		{
			break;
		}
	}

	Sanitized.TrimStartAndEndInline();
	return Sanitized;
}

bool BHIsUsefulDisplayName(const FString& DisplayName)
{
	const FString CleanDisplayName = BHSanitizeDisplayName(DisplayName);
	return CleanDisplayName.Len() >= 2
		&& !CleanDisplayName.Equals(TEXT("Guest"), ESearchCase::IgnoreCase)
		&& !CleanDisplayName.Equals(TEXT("Player"), ESearchCase::IgnoreCase);
}

EBHBotDifficulty BHParseBotDifficulty(const FString& Difficulty)
{
	if (Difficulty.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Easy;
	}
	if (Difficulty.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Hard;
	}
	return EBHBotDifficulty::Normal;
}

FString BHNormalizeRuntimeLevelName(FString LevelName)
{
	LevelName.TrimStartAndEndInline();
	if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || LevelName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
	{
		return TEXT("Foggrounds");
	}
	if (LevelName.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase))
	{
		return TEXT("Tutorial");
	}
	return LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? TEXT("Substation") : TEXT("Facility");
}

FString BHFogPresetToString(EBHFogPreset Preset)
{
	switch (Preset)
	{
	case EBHFogPreset::Light:
		return TEXT("Light");
	case EBHFogPreset::Extreme:
		return TEXT("Extreme");
	case EBHFogPreset::Heavy:
	default:
		return TEXT("Heavy");
	}
}

EBHFogPreset BHParseFogPreset(const FString& Preset)
{
	if (Preset.Equals(TEXT("Light"), ESearchCase::IgnoreCase) || Preset.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Light;
	}
	if (Preset.Equals(TEXT("Extreme"), ESearchCase::IgnoreCase) || Preset.Equals(TEXT("Max"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Extreme;
	}
	return EBHFogPreset::Heavy;
}

FString BHMakeListenOptions(const FString& LevelName, const FString& ExtraOptions = FString())
{
	FString Options = FString::Printf(TEXT("listen?BHLevel=%s?BHFogPreset=Heavy"), *BHNormalizeRuntimeLevelName(LevelName));
	if (!ExtraOptions.IsEmpty())
	{
		Options += ExtraOptions.StartsWith(TEXT("?")) ? ExtraOptions : FString::Printf(TEXT("?%s"), *ExtraOptions);
	}
	// Pin a freshly hosted game (live classroom, Host*, bot, test) to stage 0 unless the caller already
	// specified a stage. Without an explicit BHStageIndex the GameMode falls back to the GameInstance's
	// persisted stage index (only reset at a train run's final recap), so abandoning a run mid-way and
	// re-hosting would otherwise inherit a stale stage (wrong durations/targets/recap framing).
	if (!Options.Contains(TEXT("BHStageIndex=")))
	{
		Options += TEXT("?BHStageIndex=0");
	}
	// Raise the engine session cap (AGameSession::MaxPlayers, default 16) to the configured class size
	// via the ?MaxPlayers= URL option AGameSession::InitOptions reads. Without this the engine rejects
	// students past 16 at login even though ABHGameMode's own cap is the configured value.
	UBHGameSettings::AppendMaxPlayersOption(Options);
	return Options;
}

bool BHHasMultihomeOverride()
{
	TCHAR Home[256] = {};
	return FParse::Value(FCommandLine::Get(), TEXT("MULTIHOME="), Home, UE_ARRAY_COUNT(Home));
}

void BHApplyClassroomLoopbackBinding(const UBHGameSettings* Settings)
{
	if (!Settings || !Settings->bClassroomLoopbackOnlyHost || BHHasMultihomeOverride())
	{
		return;
	}

	FCommandLine::Append(TEXT(" -MULTIHOME=127.0.0.1"));
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt classroom host binding set to 127.0.0.1 for tunnel-only classroom networking."));
}

bool BHIsUrlOptionEnabled(const UWorld* World, const TCHAR* OptionName)
{
	if (!World || !OptionName)
	{
		return false;
	}

	FString Value = World->URL.GetOption(OptionName, TEXT(""));
	Value.TrimStartAndEndInline();
	return Value.Equals(TEXT("1"))
		|| Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("on"), ESearchCase::IgnoreCase);
}

FString BHBoolSettingText(const bool bEnabled)
{
	return bEnabled ? TEXT("Enabled") : TEXT("Disabled");
}

FString BHReadConfigValue(const TCHAR* Section, const TCHAR* Key, const FString& IniPath)
{
	FString Value;
	if (GConfig && GConfig->GetString(Section, Key, Value, IniPath))
	{
		Value.TrimStartAndEndInline();
		return Value.IsEmpty() ? FString(TEXT("<blank>")) : Value;
	}

	return TEXT("<unset>");
}

FString BHPathStateText(const FString& Path, const bool bDirectory)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(Path);
	const bool bExists = bDirectory
		? IFileManager::Get().DirectoryExists(*FullPath)
		: IFileManager::Get().FileExists(*FullPath);
	return FString::Printf(TEXT("%s (%s)"), *FullPath, bExists ? TEXT("found") : TEXT("missing"));
}

FString BHQuoteProcessArg(FString Arg)
{
	Arg.ReplaceInline(TEXT("\""), TEXT("\\\""));
	return FString::Printf(TEXT("\"%s\""), *Arg);
}

FString BHFindLatestPreflightReport(const FString& SupportDir)
{
	TArray<FString> Reports;
	IFileManager::Get().FindFilesRecursive(Reports, *SupportDir, TEXT("PREFLIGHT.md"), true, false);
	FString BestReport;
	FDateTime BestTime = FDateTime::MinValue();
	for (const FString& Report : Reports)
	{
		const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*Report);
		if (BestReport.IsEmpty() || Timestamp > BestTime)
		{
			BestReport = Report;
			BestTime = Timestamp;
		}
	}

	return BestReport;
}

void BHAppendPreflightLine(TArray<FString>& Lines, const FString& Label, const FString& Value)
{
	Lines.Add(FString::Printf(TEXT("%s: %s"), *Label, *Value));
}

FString BHJoinWarnings(const TArray<FString>& Warnings)
{
	if (Warnings.IsEmpty())
	{
		return TEXT("None");
	}

	TArray<FString> Numbered;
	Numbered.Reserve(Warnings.Num());
	for (int32 Index = 0; Index < Warnings.Num(); ++Index)
	{
		Numbered.Add(FString::Printf(TEXT("%d. %s"), Index + 1, *Warnings[Index]));
	}
	return FString::Join(Numbered, TEXT("\n"));
}

FLinearColor BHAvatarPaletteColor(int32 Index)
{
	static const FLinearColor Palette[] = {
		FLinearColor(0.22f, 0.58f, 0.74f, 1.0f),
		FLinearColor(0.78f, 0.46f, 0.18f, 1.0f),
		FLinearColor(0.46f, 0.72f, 0.28f, 1.0f),
		FLinearColor(0.70f, 0.30f, 0.42f, 1.0f),
		FLinearColor(0.52f, 0.44f, 0.86f, 1.0f),
		FLinearColor(0.84f, 0.75f, 0.24f, 1.0f),
		FLinearColor(0.28f, 0.68f, 0.62f, 1.0f),
		FLinearColor(0.76f, 0.76f, 0.80f, 1.0f),
		// Hidden prestige tints (indices 8..13) -- must match the ShirtColor entries in BHCosmeticUnlocks.cpp
		// and the identical palettes in BHGameMode/BHCharacter.
		FLinearColor(0.86f, 0.90f, 0.82f, 1.0f), // Chalk
		FLinearColor(0.95f, 0.22f, 0.62f, 1.0f), // Arcade
		FLinearColor(0.18f, 0.92f, 0.45f, 1.0f), // Exit Sign
		FLinearColor(0.42f, 0.86f, 1.00f, 1.0f), // Afterimage
		FLinearColor(0.64f, 0.44f, 0.22f, 1.0f), // Veteran
		FLinearColor(0.40f, 0.12f, 0.20f, 1.0f)  // Faculty
	};

	return Palette[FMath::Abs(Index) % UE_ARRAY_COUNT(Palette)];
}

FLinearColor BHSanitizeAvatarColor(FLinearColor Color)
{
	Color.R = FMath::Clamp(Color.R, 0.02f, 1.0f);
	Color.G = FMath::Clamp(Color.G, 0.02f, 1.0f);
	Color.B = FMath::Clamp(Color.B, 0.02f, 1.0f);
	Color.A = 1.0f;
	return Color;
}

int32 BHNearestAvatarColorIndex(const FLinearColor& Color)
{
	int32 BestIndex = 0;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index <= BHCosmeticMaxIndex(EBHCosmeticCategory::ShirtColor); ++Index)
	{
		const FLinearColor Candidate = BHAvatarPaletteColor(Index);
		const float Distance =
			FMath::Square(Color.R - Candidate.R)
			+ FMath::Square(Color.G - Candidate.G)
			+ FMath::Square(Color.B - Candidate.B);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestIndex = Index;
		}
	}
	return BestIndex;
}

void BHApplyLocalAvatarStyle(ABHPlayerController* Controller)
{
	if (ABHCharacter* ControlledCharacter = Controller ? Cast<ABHCharacter>(Controller->GetPawn()) : nullptr)
	{
		ControlledCharacter->ApplyAvatarStyle();
	}
}

template <int32 Count>
void BHApplyConsoleVariables(ABHPlayerController* Controller, const FBHConsoleVariableSetting (&Settings)[Count])
{
	if (!Controller)
	{
		return;
	}

	for (const FBHConsoleVariableSetting& Setting : Settings)
	{
		Controller->ConsoleCommand(FString::Printf(TEXT("%s %s"), Setting.Name, Setting.Value));
	}
}

void BHApplyHorrorLookConsoleVariables(ABHPlayerController* Controller)
{
	static const FBHConsoleVariableSetting HorrorLookSettings[] = {
		{ TEXT("r.DefaultFeature.AutoExposure"), TEXT("1") },
		{ TEXT("r.DefaultFeature.AmbientOcclusion"), TEXT("1") },
		{ TEXT("r.EyeAdaptationQuality"), TEXT("2") },
		{ TEXT("r.LocalExposure"), TEXT("1") },
		{ TEXT("r.Tonemapper.Quality"), TEXT("5") },
		{ TEXT("r.MotionBlurQuality"), TEXT("0") }
	};
	BHApplyConsoleVariables(Controller, HorrorLookSettings);
}

void BHApplyScalabilityGroups(ABHPlayerController* Controller, int32 Quality)
{
	if (!Controller)
	{
		return;
	}

	const TCHAR* Groups[] = {
		TEXT("sg.ViewDistanceQuality"),
		TEXT("sg.AntiAliasingQuality"),
		TEXT("sg.ShadowQuality"),
		TEXT("sg.GlobalIlluminationQuality"),
		TEXT("sg.ReflectionQuality"),
		TEXT("sg.PostProcessQuality"),
		TEXT("sg.TextureQuality"),
		TEXT("sg.EffectsQuality"),
		TEXT("sg.FoliageQuality"),
		TEXT("sg.ShadingQuality")
	};

	for (const TCHAR* Group : Groups)
	{
		Controller->ConsoleCommand(FString::Printf(TEXT("%s %d"), Group, Quality));
	}
}

bool BHLooksLikeLowerBodyName(const FString& RawName)
{
	FString Name = RawName.ToLower();
	return Name.Contains(TEXT("leg"))
		|| Name.Contains(TEXT("thigh"))
		|| Name.Contains(TEXT("calf"))
		|| Name.Contains(TEXT("shin"))
		|| Name.Contains(TEXT("knee"))
		|| Name.Contains(TEXT("foot"))
		|| Name.Contains(TEXT("toe"));
}

void BHApplyUpperBodyCloseVisualMask(AActor* VisualActor)
{
	if (!VisualActor)
	{
		return;
	}

	TArray<USkeletalMeshComponent*> SkeletalComponents;
	VisualActor->GetComponents<USkeletalMeshComponent>(SkeletalComponents);
	for (USkeletalMeshComponent* SkeletalComponent : SkeletalComponents)
	{
		if (!SkeletalComponent)
		{
			continue;
		}

		const int32 BoneCount = SkeletalComponent->GetNumBones();
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			const FName BoneName = SkeletalComponent->GetBoneName(BoneIndex);
			if (!BoneName.IsNone() && BHLooksLikeLowerBodyName(BoneName.ToString()))
			{
				SkeletalComponent->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
			}
		}
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	VisualActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && BHLooksLikeLowerBodyName(PrimitiveComponent->GetName()))
		{
			PrimitiveComponent->SetHiddenInGame(true);
			PrimitiveComponent->SetVisibility(false, true);
		}
	}
}

void BHFitCloseVisualActorToCamera(AActor* VisualActor, float MaxHeight = 165.0f)
{
	if (!VisualActor || MaxHeight <= 0.0f)
	{
		return;
	}

	const FBox Bounds = VisualActor->GetComponentsBoundingBox(true);
	const float Height = Bounds.IsValid ? Bounds.GetSize().Z : 0.0f;
	if (Height > MaxHeight)
	{
		VisualActor->SetActorScale3D(VisualActor->GetActorScale3D() * (MaxHeight / Height));
	}
}

struct FBHAtmosphereSliderSpec
{
	const TCHAR* ParameterName;
	const TCHAR* Label;
	float MinValue;
	float MaxValue;
	int32 Decimals;
};

static const FBHAtmosphereSliderSpec BHFogConsoleSliders[] = {
	{TEXT("Fog.Density"), TEXT("Density"), 0.0f, 2.0f, 3},
	{TEXT("Fog.HeightFalloff"), TEXT("Height Falloff"), 0.001f, 0.250f, 3},
	{TEXT("Fog.MaxOpacity"), TEXT("Max Opacity"), 0.0f, 1.0f, 3},
	{TEXT("Fog.StartDistance"), TEXT("Start Distance"), 0.0f, 5000.0f, 0},
	{TEXT("Fog.EndDistance"), TEXT("End Distance"), 0.0f, 12000.0f, 0},
	{TEXT("Fog.Inscatter.R"), TEXT("Inscatter R"), 0.0f, 1.0f, 3},
	{TEXT("Fog.Inscatter.G"), TEXT("Inscatter G"), 0.0f, 1.0f, 3},
	{TEXT("Fog.Inscatter.B"), TEXT("Inscatter B"), 0.0f, 1.0f, 3},
	{TEXT("Fog.DirectionalExponent"), TEXT("Directional Exponent"), 2.0f, 64.0f, 1},
	{TEXT("Fog.DirectionalStart"), TEXT("Directional Start"), 0.0f, 5000.0f, 0},
	{TEXT("Fog.Directional.R"), TEXT("Directional R"), 0.0f, 1.5f, 3},
	{TEXT("Fog.Directional.G"), TEXT("Directional G"), 0.0f, 1.5f, 3},
	{TEXT("Fog.Directional.B"), TEXT("Directional B"), 0.0f, 1.5f, 3}
};

static const FBHAtmosphereSliderSpec BHVolumetricConsoleSliders[] = {
	{TEXT("Volumetric.Extinction"), TEXT("Extinction"), 0.1f, 6.0f, 2},
	{TEXT("Volumetric.Distance"), TEXT("View Distance"), 500.0f, 10000.0f, 0},
	{TEXT("Volumetric.Start"), TEXT("Start Distance"), 0.0f, 5000.0f, 0},
	{TEXT("Volumetric.NearFade"), TEXT("Near Fade"), 0.0f, 1000.0f, 0},
	{TEXT("Volumetric.Scattering"), TEXT("Scattering"), -0.9f, 0.9f, 3},
	{TEXT("Volumetric.Albedo.R"), TEXT("Albedo R"), 0.0f, 1.0f, 3},
	{TEXT("Volumetric.Albedo.G"), TEXT("Albedo G"), 0.0f, 1.0f, 3},
	{TEXT("Volumetric.Albedo.B"), TEXT("Albedo B"), 0.0f, 1.0f, 3},
	{TEXT("Volumetric.Emissive.R"), TEXT("Emissive R"), 0.0f, 0.5f, 3},
	{TEXT("Volumetric.Emissive.G"), TEXT("Emissive G"), 0.0f, 0.5f, 3},
	{TEXT("Volumetric.Emissive.B"), TEXT("Emissive B"), 0.0f, 0.5f, 3}
};

static const FBHAtmosphereSliderSpec BHLocalFogConsoleSliders[] = {
	{TEXT("LocalFog.Radial"), TEXT("Radial Density"), 0.0f, 8.0f, 2},
	{TEXT("LocalFog.Height"), TEXT("Height Density"), 0.0f, 8.0f, 2},
	{TEXT("LocalFog.Falloff"), TEXT("Height Falloff"), 1.0f, 6000.0f, 0},
	{TEXT("LocalFog.Offset"), TEXT("Height Offset"), -2.0f, 2.0f, 2},
	{TEXT("LocalFog.Phase"), TEXT("Phase G"), 0.0f, 0.999f, 3},
	{TEXT("LocalFog.Albedo.R"), TEXT("Albedo R"), 0.0f, 1.0f, 3},
	{TEXT("LocalFog.Albedo.G"), TEXT("Albedo G"), 0.0f, 1.0f, 3},
	{TEXT("LocalFog.Albedo.B"), TEXT("Albedo B"), 0.0f, 1.0f, 3},
	{TEXT("LocalFog.Emissive.R"), TEXT("Emissive R"), 0.0f, 0.5f, 3},
	{TEXT("LocalFog.Emissive.G"), TEXT("Emissive G"), 0.0f, 0.5f, 3},
	{TEXT("LocalFog.Emissive.B"), TEXT("Emissive B"), 0.0f, 0.5f, 3}
};

static const FBHAtmosphereSliderSpec BHSkyConsoleSliders[] = {
	{TEXT("Sky.Luminance.R"), TEXT("Sky R"), 0.0f, 2.0f, 3},
	{TEXT("Sky.Luminance.G"), TEXT("Sky G"), 0.0f, 2.0f, 3},
	{TEXT("Sky.Luminance.B"), TEXT("Sky B"), 0.0f, 2.0f, 3},
	{TEXT("Sky.Aerial.R"), TEXT("Aerial R"), 0.0f, 2.0f, 3},
	{TEXT("Sky.Aerial.G"), TEXT("Aerial G"), 0.0f, 2.0f, 3},
	{TEXT("Sky.Aerial.B"), TEXT("Aerial B"), 0.0f, 2.0f, 3},
	{TEXT("Sky.MultiScattering"), TEXT("Multi Scattering"), 0.0f, 2.0f, 3},
	{TEXT("Sky.HeightFogContribution"), TEXT("Height Fog Contribution"), 0.0f, 1.0f, 3},
	{TEXT("Sky.AerialDistance"), TEXT("Aerial Distance"), 0.0f, 3.0f, 3},
	{TEXT("Sky.Ground.R"), TEXT("Ground R"), 0.0f, 1.0f, 3},
	{TEXT("Sky.Ground.G"), TEXT("Ground G"), 0.0f, 1.0f, 3},
	{TEXT("Sky.Ground.B"), TEXT("Ground B"), 0.0f, 1.0f, 3},
	{TEXT("Sky.RayleighScale"), TEXT("Rayleigh Scale"), 0.0f, 2.0f, 3},
	{TEXT("Sky.MieScale"), TEXT("Mie Scale"), 0.0f, 5.0f, 3},
	{TEXT("Sky.MieAbsorptionScale"), TEXT("Mie Absorption"), 0.0f, 5.0f, 3},
	{TEXT("Sky.MieAnisotropy"), TEXT("Mie Anisotropy"), 0.0f, 0.999f, 3},
	{TEXT("Sky.AbsorptionScale"), TEXT("Absorption Scale"), 0.0f, 0.5f, 3}
};

static const FBHAtmosphereSliderSpec BHMoonConsoleSliders[] = {
	{TEXT("Moon.Intensity"), TEXT("Intensity"), 0.0f, 5.0f, 3},
	{TEXT("Moon.Indirect"), TEXT("Indirect"), 0.0f, 5.0f, 3},
	{TEXT("Moon.Volumetric"), TEXT("Volumetric"), 0.0f, 2.0f, 3},
	{TEXT("Moon.Pitch"), TEXT("Pitch"), -90.0f, 90.0f, 1},
	{TEXT("Moon.Yaw"), TEXT("Yaw"), -180.0f, 180.0f, 1},
	{TEXT("Moon.Color.R"), TEXT("Color R"), 0.0f, 1.5f, 3},
	{TEXT("Moon.Color.G"), TEXT("Color G"), 0.0f, 1.5f, 3},
	{TEXT("Moon.Color.B"), TEXT("Color B"), 0.0f, 1.5f, 3},
	{TEXT("Moon.Disk.R"), TEXT("Disk R"), 0.0f, 2.0f, 3},
	{TEXT("Moon.Disk.G"), TEXT("Disk G"), 0.0f, 2.0f, 3},
	{TEXT("Moon.Disk.B"), TEXT("Disk B"), 0.0f, 2.0f, 3}
};

static const FBHAtmosphereSliderSpec BHSkyLightConsoleSliders[] = {
	{TEXT("SkyLight.Intensity"), TEXT("Intensity"), 0.0f, 3.0f, 3},
	{TEXT("SkyLight.Volumetric"), TEXT("Volumetric"), 0.0f, 2.0f, 3},
	{TEXT("SkyLight.Color.R"), TEXT("Color R"), 0.0f, 1.5f, 3},
	{TEXT("SkyLight.Color.G"), TEXT("Color G"), 0.0f, 1.5f, 3},
	{TEXT("SkyLight.Color.B"), TEXT("Color B"), 0.0f, 1.5f, 3},
	{TEXT("SkyLight.Lower.R"), TEXT("Lower R"), 0.0f, 1.0f, 3},
	{TEXT("SkyLight.Lower.G"), TEXT("Lower G"), 0.0f, 1.0f, 3},
	{TEXT("SkyLight.Lower.B"), TEXT("Lower B"), 0.0f, 1.0f, 3}
};

static const FBHAtmosphereSliderSpec BHPostConsoleSliders[] = {
	{TEXT("Post.Exposure"), TEXT("Exposure"), 0.03f, 2.0f, 3},
	{TEXT("Post.ExposureBias"), TEXT("Exposure Bias"), -3.0f, 3.0f, 3},
	{TEXT("Post.Vignette"), TEXT("Vignette"), 0.0f, 1.0f, 3},
	{TEXT("Post.FilmGrain"), TEXT("Film Grain"), 0.0f, 1.0f, 3},
	{TEXT("Post.Saturation.R"), TEXT("Saturation R"), 0.0f, 2.0f, 3},
	{TEXT("Post.Saturation.G"), TEXT("Saturation G"), 0.0f, 2.0f, 3},
	{TEXT("Post.Saturation.B"), TEXT("Saturation B"), 0.0f, 2.0f, 3},
	{TEXT("Post.Contrast.R"), TEXT("Contrast R"), 0.0f, 2.0f, 3},
	{TEXT("Post.Contrast.G"), TEXT("Contrast G"), 0.0f, 2.0f, 3},
	{TEXT("Post.Contrast.B"), TEXT("Contrast B"), 0.0f, 2.0f, 3},
	{TEXT("Post.SceneTint.R"), TEXT("Scene Tint R"), 0.0f, 2.0f, 3},
	{TEXT("Post.SceneTint.G"), TEXT("Scene Tint G"), 0.0f, 2.0f, 3},
	{TEXT("Post.SceneTint.B"), TEXT("Scene Tint B"), 0.0f, 2.0f, 3},
	{TEXT("Post.IndirectIntensity"), TEXT("Indirect Intensity"), 0.0f, 2.0f, 3},
	{TEXT("Post.Indirect.R"), TEXT("Indirect R"), 0.0f, 2.0f, 3},
	{TEXT("Post.Indirect.G"), TEXT("Indirect G"), 0.0f, 2.0f, 3},
	{TEXT("Post.Indirect.B"), TEXT("Indirect B"), 0.0f, 2.0f, 3}
};

static const FBHAtmosphereSliderSpec BHFlashlightConsoleSliders[] = {
	{TEXT("Flashlight.IntensityScale"), TEXT("Intensity Scale"), 0.0f, 3.0f, 3},
	{TEXT("Flashlight.RadiusScale"), TEXT("Radius Scale"), 0.0f, 3.0f, 3},
	{TEXT("Flashlight.VolumetricScale"), TEXT("Fog Scatter Scale"), 0.0f, 3.0f, 3},
	{TEXT("Flashlight.BeamLengthScale"), TEXT("Beam Length Scale"), 0.0f, 3.0f, 3},
	{TEXT("Flashlight.BeamOpacityScale"), TEXT("Beam Opacity Scale"), 0.0f, 3.0f, 3},
	{TEXT("Flashlight.BeamBrightnessScale"), TEXT("Beam Brightness Scale"), 0.0f, 3.0f, 3},
	{TEXT("Flashlight.ConeScale"), TEXT("Cone Scale"), 0.25f, 2.0f, 3},
	{TEXT("Flashlight.RayVisibilityScale"), TEXT("Fog Ray Visibility"), 0.0f, 3.0f, 3},
	{TEXT("Flashlight.RayWidthScale"), TEXT("Fog Ray Width"), 0.25f, 3.0f, 3},
	{TEXT("Flashlight.RayStartOffset"), TEXT("Fog Ray Start"), 0.0f, 220.0f, 0}
};

template <typename FuncType>
void BHForEachAtmosphereSliderSpec(FuncType&& Func)
{
	for (const FBHAtmosphereSliderSpec& Spec : BHFogConsoleSliders) { Func(Spec); }
	for (const FBHAtmosphereSliderSpec& Spec : BHVolumetricConsoleSliders) { Func(Spec); }
	for (const FBHAtmosphereSliderSpec& Spec : BHLocalFogConsoleSliders) { Func(Spec); }
	for (const FBHAtmosphereSliderSpec& Spec : BHSkyConsoleSliders) { Func(Spec); }
	for (const FBHAtmosphereSliderSpec& Spec : BHMoonConsoleSliders) { Func(Spec); }
	for (const FBHAtmosphereSliderSpec& Spec : BHSkyLightConsoleSliders) { Func(Spec); }
	for (const FBHAtmosphereSliderSpec& Spec : BHPostConsoleSliders) { Func(Spec); }
	for (const FBHAtmosphereSliderSpec& Spec : BHFlashlightConsoleSliders) { Func(Spec); }
}

FString BHFormatAtmosphereValue(float Value, int32 Decimals)
{
	switch (FMath::Clamp(Decimals, 0, 3))
	{
	case 0:
		return FString::Printf(TEXT("%.0f"), Value);
	case 1:
		return FString::Printf(TEXT("%.1f"), Value);
	case 2:
		return FString::Printf(TEXT("%.2f"), Value);
	default:
		return FString::Printf(TEXT("%.3f"), Value);
	}
}

FLinearColor BHSetColorComponent(FLinearColor Color, const FString& Channel, float Value)
{
	if (Channel.Equals(TEXT("R"), ESearchCase::IgnoreCase))
	{
		Color.R = Value;
	}
	else if (Channel.Equals(TEXT("G"), ESearchCase::IgnoreCase))
	{
		Color.G = Value;
	}
	else if (Channel.Equals(TEXT("B"), ESearchCase::IgnoreCase))
	{
		Color.B = Value;
	}
	Color.A = 1.0f;
	return Color;
}

float BHGetColorComponent(const FLinearColor& Color, const FString& Channel)
{
	if (Channel.Equals(TEXT("R"), ESearchCase::IgnoreCase))
	{
		return Color.R;
	}
	if (Channel.Equals(TEXT("G"), ESearchCase::IgnoreCase))
	{
		return Color.G;
	}
	if (Channel.Equals(TEXT("B"), ESearchCase::IgnoreCase))
	{
		return Color.B;
	}
	return Color.A;
}

FVector4 BHSetVectorComponent(FVector4 Vector, const FString& Channel, float Value)
{
	if (Channel.Equals(TEXT("R"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("X"), ESearchCase::IgnoreCase))
	{
		Vector.X = Value;
	}
	else if (Channel.Equals(TEXT("G"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Y"), ESearchCase::IgnoreCase))
	{
		Vector.Y = Value;
	}
	else if (Channel.Equals(TEXT("B"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Z"), ESearchCase::IgnoreCase))
	{
		Vector.Z = Value;
	}
	else
	{
		Vector.W = Value;
	}
	return Vector;
}

float BHGetVectorComponent(const FVector4& Vector, const FString& Channel)
{
	if (Channel.Equals(TEXT("R"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("X"), ESearchCase::IgnoreCase))
	{
		return Vector.X;
	}
	if (Channel.Equals(TEXT("G"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Y"), ESearchCase::IgnoreCase))
	{
		return Vector.Y;
	}
	if (Channel.Equals(TEXT("B"), ESearchCase::IgnoreCase) || Channel.Equals(TEXT("Z"), ESearchCase::IgnoreCase))
	{
		return Vector.Z;
	}
	return Vector.W;
}

class SBHAtmosphereConsole : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBHAtmosphereConsole) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABHPlayerController>, PlayerController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PlayerController = InArgs._PlayerController;

		TSharedRef<SVerticalBox> SliderList = SNew(SVerticalBox);
		AddSection(SliderList, TEXT("Height Fog"), BHFogConsoleSliders);
		AddSection(SliderList, TEXT("Volumetric Fog"), BHVolumetricConsoleSliders);
		AddSection(SliderList, TEXT("Local Fog Volumes"), BHLocalFogConsoleSliders);
		AddSection(SliderList, TEXT("Sky Atmosphere"), BHSkyConsoleSliders);
		AddSection(SliderList, TEXT("Moon Light"), BHMoonConsoleSliders);
		AddSection(SliderList, TEXT("Sky Light"), BHSkyLightConsoleSliders);
		AddSection(SliderList, TEXT("Post Process"), BHPostConsoleSliders);
		AddSection(SliderList, TEXT("Flashlight / Beam"), BHFlashlightConsoleSliders);

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(FMargin(16.0f))
			[
				SNew(SBox)
				.WidthOverride(520.0f)
				.MaxDesiredHeight(760.0f)
				[
					SNew(SBorder)
					.BorderImage(BHUiWhiteBrush())
					.BorderBackgroundColor(FLinearColor(0.015f, 0.020f, 0.026f, 0.94f))
					.Padding(FMargin(14.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Atmosphere Console")))
								.Font(BHUiFont(16, FName(TEXT("Bold"))))
								.ColorAndOpacity(FLinearColor(0.90f, 0.96f, 1.0f, 1.0f))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
							[
								MakeButton(TEXT("Save"), [this]()
								{
									if (ABHPlayerController* PC = PlayerController.Get())
									{
										PC->SaveAtmosphereConsoleProfileForMenu();
									}
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
							[
								MakeButton(TEXT("Load"), [this]()
								{
									if (ABHPlayerController* PC = PlayerController.Get())
									{
										PC->LoadAtmosphereConsoleProfileForMenu();
									}
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
							[
								MakeButton(TEXT("Playable"), [this]()
								{
									if (ABHPlayerController* PC = PlayerController.Get())
									{
										PC->ApplyPlayableAtmosphereConsoleForMenu();
									}
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
							[
								MakeButton(TEXT("Reset"), [this]()
								{
									if (ABHPlayerController* PC = PlayerController.Get())
									{
										PC->ResetAtmosphereConsoleForMenu();
									}
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
							[
								MakeButton(TEXT("Close"), [this]()
								{
									if (ABHPlayerController* PC = PlayerController.Get())
									{
										PC->HideAtmosphereConsole();
									}
								})
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 8.0f, 0.0f, 10.0f))
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								if (const ABHPlayerController* PC = PlayerController.Get())
								{
									return FText::FromString(PC->GetAtmosphereConsoleSummaryForMenu());
								}
								return FText::FromString(TEXT("No local controller."));
							})
							.Font(BHUiFont(10))
							.ColorAndOpacity(FLinearColor(0.68f, 0.76f, 0.82f, 1.0f))
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SliderList
							]
						]
					]
				]
			]
		];
	}

private:
	template <int32 Count>
	void AddSection(TSharedRef<SVerticalBox> Root, const TCHAR* Title, const FBHAtmosphereSliderSpec (&Specs)[Count])
	{
		Root->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 8.0f, 0.0f, 4.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Title))
			.Font(BHUiFont(12, FName(TEXT("Bold"))))
			.ColorAndOpacity(FLinearColor(0.56f, 0.82f, 0.96f, 1.0f))
		];

		for (const FBHAtmosphereSliderSpec& Spec : Specs)
		{
			Root->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
			[
				MakeSliderRow(Spec)
			];
		}

		Root->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
		[
			SNew(SSeparator)
			.Thickness(1.0f)
			.ColorAndOpacity(FLinearColor(0.16f, 0.22f, 0.28f, 1.0f))
		];
	}

	TSharedRef<SWidget> MakeButton(const TCHAR* Label, TFunction<void()> Action)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(10.0f, 4.0f))
			.ButtonColorAndOpacity(FLinearColor(0.08f, 0.12f, 0.16f, 1.0f))
			.OnClicked_Lambda([Action]()
			{
				Action();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(BHUiFont(10, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.88f, 0.94f, 1.0f, 1.0f))
			];
	}

	TSharedRef<SWidget> MakeSliderRow(const FBHAtmosphereSliderSpec& Spec)
	{
		const FName ParameterName(Spec.ParameterName);
		const float MinValue = Spec.MinValue;
		const float MaxValue = Spec.MaxValue;
		const int32 Decimals = Spec.Decimals;

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.42f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Spec.Label))
				.Font(BHUiFont(10))
				.ColorAndOpacity(FLinearColor(0.82f, 0.88f, 0.92f, 1.0f))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.44f)
			.Padding(FMargin(8.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.Value_Lambda([this, ParameterName, MinValue, MaxValue]()
				{
					const ABHPlayerController* PC = PlayerController.Get();
					if (!PC || MaxValue <= MinValue)
					{
						return 0.0f;
					}
					return FMath::Clamp((PC->GetAtmosphereConsoleValue(ParameterName) - MinValue) / (MaxValue - MinValue), 0.0f, 1.0f);
				})
				.OnValueChanged_Lambda([this, ParameterName, MinValue, MaxValue](float NormalizedValue)
				{
					if (ABHPlayerController* PC = PlayerController.Get())
					{
						PC->SetAtmosphereConsoleValue(ParameterName, FMath::Lerp(MinValue, MaxValue, FMath::Clamp(NormalizedValue, 0.0f, 1.0f)));
					}
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.14f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Right)
				.Text_Lambda([this, ParameterName, Decimals]()
				{
					const ABHPlayerController* PC = PlayerController.Get();
					return FText::FromString(BHFormatAtmosphereValue(PC ? PC->GetAtmosphereConsoleValue(ParameterName) : 0.0f, Decimals));
				})
				.Font(BHUiFont(9, FName(TEXT("Bold"))))
				.ColorAndOpacity(FLinearColor(0.70f, 0.94f, 0.92f, 1.0f))
			];
	}

	TWeakObjectPtr<ABHPlayerController> PlayerController;
};
}

ABHPlayerController::ABHPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		RemoveMainMenuWidget();
		HideTravelLoadingScreen();
		EnsureAudioPreferencesLoaded();
		EnsureComfortPreferencesLoaded();
		EnsureGraphicsPreferencesLoaded();
		ApplyStartupGraphicsSettings();
		MaybeShowWeakDeviceNotice();
		ApplyVirtualBoxSafeModeIfNeeded();
		BindGameWindowCloseOverride();
		PushLocalCosmeticsToServer(false);
		ShowLocalStatusMessage(TEXT("Escape opens menu. Enter readies up. F flashlight, E interact, 1-4 answer."), 5.0f);
		UWorld* World = GetWorld();
		const bool bRemovedByHost = BHIsUrlOptionEnabled(World, TEXT("BHRemovedByHost="));
		const bool bLiveClassroomHost = BHIsUrlOptionEnabled(World, TEXT("BHLiveClassroom=")) && GetNetMode() == NM_ListenServer;
		if (GetNetMode() == NM_Standalone || bRemovedByHost || bLiveClassroomHost)
		{
			ShowMainMenu();
			if (bLiveClassroomHost)
			{
				FString BoardMessage;
				OpenClassroomBoardForMenu(BoardMessage);
				ShowLocalStatusMessage(TEXT("Live Classroom hosted. Share the JOIN address, assign roles, and kick blockers from the roster."), 8.0f);
				GetWorldTimerManager().SetTimer(ClassroomPreflightTimerHandle, this, &ABHPlayerController::RunClassroomNetworkPreflight, 4.0f, false);
				GetWorldTimerManager().SetTimer(ClassroomFallbackTimerHandle, this, &ABHPlayerController::RunClassroomFallbackCheck, 40.0f, false);
			}
			else if (bRemovedByHost)
			{
				ShowLocalStatusMessage(TEXT("You were removed from the classroom lobby by the host. You can rejoin if invited."), 8.0f);
			}
			else if (UBHGameInstance* MenuBHGI = GetGameInstance<UBHGameInstance>())
			{
				// A student whose join failed is bounced here as Standalone; surface why (timeout,
				// refused, version mismatch, full) instead of dropping them at a silent menu.
				const FString FailureMessage = MenuBHGI->ConsumePendingNetworkFailureMessage();
				if (!FailureMessage.IsEmpty())
				{
					ShowLocalStatusMessage(FailureMessage, 9.0f);
				}
			}
		}
		else
		{
			ApplyGameplayInputMode();
		}
		UpdateAmbientMusic();
		GetWorldTimerManager().SetTimer(DisplayNameSyncTimerHandle, this, &ABHPlayerController::PushLocalProfileToServer, 1.0f, false);
		if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
		{
			if (BHGI->IsAutomationEnabled())
			{
				BHGI->LogAutomationMarkerOnce(TEXT("MENU_READY"));
			}
		}
		ScheduleAutomation();
		GetWorldTimerManager().SetTimer(AtmosphereProfileLoadTimerHandle, this, &ABHPlayerController::ApplySavedAtmosphereConsoleProfileFromTimer, 1.0f, false);
	}
}

void ABHPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAmbientMusic();
	HandleRoundPhaseUiState();
	TickAdaptiveGraphics(DeltaSeconds);
	TickHorrorCueEffects(DeltaSeconds);
	TickAutomation();
}

void ABHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutomationStartupTimerHandle);
		World->GetTimerManager().ClearTimer(AutomationQuitTimerHandle);
		World->GetTimerManager().ClearTimer(ClassroomPreflightTimerHandle);
		World->GetTimerManager().ClearTimer(ClassroomFallbackTimerHandle);
		World->GetTimerManager().ClearTimer(AtmosphereProfileLoadTimerHandle);
		World->GetTimerManager().ClearTimer(DisplayNameSyncTimerHandle);
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	HideClassroomBoard();
	RemoveMainMenuWidget();
	RemoveAtmosphereConsoleWidget();
	HideTravelLoadingScreen();
	AtmosphereConsoleDefaultValues.Reset();
	bAtmosphereConsoleDefaultsCaptured = false;

	if (AmbientMusicComponent)
	{
		AmbientMusicComponent->Stop();
		AmbientMusicComponent->DestroyComponent();
		AmbientMusicComponent = nullptr;
		bAmbientMusicStarted = false;
	}

	Super::EndPlay(EndPlayReason);
}

void ABHPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	if (IsLocalController())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AtmosphereProfileLoadTimerHandle);
		}
		RemoveMainMenuWidget();
		RemoveAtmosphereConsoleWidget();
		HideClassroomBoard();
		AtmosphereConsoleDefaultValues.Reset();
		bAtmosphereConsoleDefaultsCaptured = false;
	}

	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ABHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindAction(TEXT("Menu"), IE_Pressed, this, &ABHPlayerController::ToggleMainMenu);
		InputComponent->BindAction(TEXT("Map"), IE_Pressed, this, &ABHPlayerController::ToggleHudMap);
		InputComponent->BindAction(TEXT("CycleCrosshair"), IE_Pressed, this, &ABHPlayerController::CycleCrosshairStyle);
		InputComponent->BindAction(TEXT("ForceStartRound"), IE_Pressed, this, &ABHPlayerController::ForceStartRound);
		InputComponent->BindAction(TEXT("ClassroomBoard"), IE_Pressed, this, &ABHPlayerController::ToggleClassroomBoard);
		InputComponent->BindAction(TEXT("SpectatorEncourage"), IE_Pressed, this, &ABHPlayerController::SpectatorEncourage);
		InputComponent->BindAction(TEXT("SpectatorQueueTeacher"), IE_Pressed, this, &ABHPlayerController::SpectatorQueueTeacher);
		InputComponent->BindAction(TEXT("SpectatorQueueSurvivor"), IE_Pressed, this, &ABHPlayerController::SpectatorQueueSurvivor);
		InputComponent->BindAction(TEXT("SpectatorQueueMonitor"), IE_Pressed, this, &ABHPlayerController::SpectatorQueueMonitor);
		InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ABHPlayerController::TesterGrantTrainResources);
		InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ABHPlayerController::TesterOpenTrainIntermission);
		InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ABHPlayerController::TesterAdvanceTrainPhase);
		InputComponent->BindKey(EKeys::F11, IE_Pressed, this, &ABHPlayerController::ToggleAtmosphereConsole);
		InputComponent->BindKey(EKeys::Pause, IE_Pressed, this, &ABHPlayerController::ToggleAtmosphereConsole);
		InputComponent->BindKey(EKeys::Backslash, IE_Pressed, this, &ABHPlayerController::ToggleAtmosphereConsole);
		InputComponent->BindKey(EKeys::F12, IE_Pressed, this, &ABHPlayerController::TesterForceFinalRecap);
		InputComponent->BindKey(EKeys::Insert, IE_Pressed, this, &ABHPlayerController::TesterGrantTrainResources);
		InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &ABHPlayerController::TesterOpenTrainIntermission);
		InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &ABHPlayerController::TesterAdvanceTrainPhase);
		InputComponent->BindKey(EKeys::End, IE_Pressed, this, &ABHPlayerController::TesterLoadFinalStation);
		InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &ABHPlayerController::TesterTriggerFinalEscape);
		InputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &ABHPlayerController::TesterForceFinalRecap);
		InputComponent->BindKey(EKeys::NumPadFive, IE_Pressed, this, &ABHPlayerController::TesterGrantTrainResources);
		InputComponent->BindKey(EKeys::NumPadSix, IE_Pressed, this, &ABHPlayerController::TesterOpenTrainIntermission);
		InputComponent->BindKey(EKeys::NumPadSeven, IE_Pressed, this, &ABHPlayerController::TesterAdvanceTrainPhase);
		InputComponent->BindKey(EKeys::NumPadEight, IE_Pressed, this, &ABHPlayerController::TesterLoadFinalStation);
		InputComponent->BindKey(EKeys::NumPadNine, IE_Pressed, this, &ABHPlayerController::TesterTriggerFinalEscape);
		InputComponent->BindKey(EKeys::NumPadZero, IE_Pressed, this, &ABHPlayerController::TesterForceFinalRecap);
	}
}

void ABHPlayerController::HostGame()
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("LOADING FACILITY"), TEXT("Opening local lobby."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(TEXT("Facility"))), true, BHMakeListenOptions(TEXT("Facility")));
}

void ABHPlayerController::HostSubstationGame()
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("LOADING SUBSTATION"), TEXT("Opening local lobby."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(TEXT("Substation"))), true, BHMakeListenOptions(TEXT("Substation")));
}

void ABHPlayerController::HostFoggroundsGame()
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("LOADING FOGGROUNDS"), TEXT("Opening local lobby."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(TEXT("Foggrounds"))), true, BHMakeListenOptions(TEXT("Foggrounds")));
}

void ABHPlayerController::HostPracticeGame()
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("PRACTICE LAB"), TEXT("Preparing safe test route."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(TEXT("Facility"))), true, TEXT("listen?BHLevel=Facility?BHPractice=1"));
}

void ABHPlayerController::HostTutorialGame()
{
	// The plain entry point (console + default menu button) is the full course: start at Survivor and chain
	// Survivor -> Teacher -> Monitor.
	HostTutorialPhase(EBHTutorialPhase::Survivor, true);
}

void ABHPlayerController::HostTutorialPhase(EBHTutorialPhase StartPhase, bool bChain)
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("TUTORIAL"), TEXT("Loading the guided practice tutorial."));
	// Resolves to the baked Tutorial.umap (BHResolveLevelMapPackage special-cases Tutorial); ?BHTutorial=1
	// rides on the practice sandbox and the map-baked ABHTutorialDirector drives the selected lesson.
	const TCHAR* PhaseName = TEXT("Survivor");
	switch (StartPhase)
	{
	case EBHTutorialPhase::Teacher: PhaseName = TEXT("Teacher"); break;
	case EBHTutorialPhase::Monitor: PhaseName = TEXT("Monitor"); break;
	case EBHTutorialPhase::Survivor:
	default:                        PhaseName = TEXT("Survivor"); break;
	}
	// Pin BHStageIndex=0 so IsFinalStage() is deterministically false in the tutorial regardless of the player's
	// persisted campaign stage (otherwise a player who last reached stage 2 would make the tutorial read as the
	// final stage). The chaining travels in AdvanceTutorialPhase carry the same pin.
	const FString Options = FString::Printf(TEXT("listen?BHLevel=Tutorial?BHTutorial=1?BHTutorialPhase=%s?BHTutorialChain=%d?BHStageIndex=0"),
		PhaseName, bChain ? 1 : 0);
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(TEXT("Tutorial"))), true, Options);
}

void ABHPlayerController::HostTestRound()
{
	FString Message;
	HostTestRoundForMenu(TEXT("Facility"), Message);
}

void ABHPlayerController::HostSubstationTestRound()
{
	FString Message;
	HostTestRoundForMenu(TEXT("Substation"), Message);
}

void ABHPlayerController::HostFoggroundsTestRound()
{
	FString Message;
	HostTestRoundForMenu(TEXT("Foggrounds"), Message);
}

void ABHPlayerController::HostBotGame()
{
	FString Message;
	HostBotGameForMenu(TEXT("Facility"), Message);
}

void ABHPlayerController::HostBotSubstationGame()
{
	FString Message;
	HostBotGameForMenu(TEXT("Substation"), Message);
}

void ABHPlayerController::HostBotFoggroundsGame()
{
	FString Message;
	HostBotGameForMenu(TEXT("Foggrounds"), Message);
}

void ABHPlayerController::HostPhysicsClassroom()
{
	FString Message;
	HostPhysicsClassroomForMenu(Message);
}

void ABHPlayerController::JoinGame(const FString& Address)
{
	const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(Address);
	if (NormalizedAddress.IsEmpty())
	{
		ShowLocalStatusMessage(TEXT("Usage: JoinGame <host-ip-or-domain>:7777"));
		return;
	}

	// Echo this client's server-issued reconnect token (Bug-9 secure reconnect) so a mid-round rejoin is
	// matched by the secret token, not the spoofable display name. THIS is the path the menu JOIN button
	// uses (UBHGameInstance::JoinGame is only an Exec console command and does the same append), so the
	// token must be added here too or a dropped student rejoins tokenless and loses their role/points.
	FString TravelAddress = NormalizedAddress;
	if (const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		const FString& ReconnectToken = BHGI->GetClientReconnectToken();
		if (!ReconnectToken.IsEmpty())
		{
			TravelAddress += FString::Printf(TEXT("?BHReconnect=%s"), *ReconnectToken);
		}
	}

	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("JOINING GAME"), FString::Printf(TEXT("Connecting to %s."), *NormalizedAddress));
	ClientTravel(TravelAddress, TRAVEL_Absolute);
}

void ABHPlayerController::HostOnlineGame()
{
	FString Message;
	HostOnlineGameForMenu(TEXT("Facility"), Message);
}

void ABHPlayerController::HostOnlineSubstationGame()
{
	FString Message;
	HostOnlineGameForMenu(TEXT("Substation"), Message);
}

void ABHPlayerController::HostOnlineFoggroundsGame()
{
	FString Message;
	HostOnlineGameForMenu(TEXT("Foggrounds"), Message);
}

void ABHPlayerController::FindOnlineGames()
{
	FString Message;
	FindOnlineGamesForMenu(Message);
}

void ABHPlayerController::JoinOnlineGame(int32 SessionIndex)
{
	FString Message;
	JoinOnlineGameForMenu(SessionIndex, Message);
}

void ABHPlayerController::DestroyOnlineSession()
{
	FString Message;
	DestroyOnlineSessionForMenu(Message);
}

void ABHPlayerController::AccountGuest()
{
	FString Message;
	ContinueAsGuestForMenu(Message);
}

void ABHPlayerController::LoginGoogle()
{
	FString Message;
	BeginAccountLoginForMenu(TEXT("google"), Message);
}

void ABHPlayerController::LoginMicrosoft()
{
	FString Message;
	BeginAccountLoginForMenu(TEXT("microsoft"), Message);
}

void ABHPlayerController::AccountPollLogin()
{
	FString Message;
	PollAccountLoginForMenu(Message);
}

void ABHPlayerController::AccountSync()
{
	FString Message;
	SyncAccountForMenu(Message);
}

void ABHPlayerController::AccountSignOut()
{
	FString Message;
	SignOutAccountForMenu(Message);
}

void ABHPlayerController::AccountCreateLocal(const FString& Username, const FString& Password)
{
	FString Message;
	CreateOrUpdateLocalCredentialForMenu(Username, Password, Message);
}

void ABHPlayerController::AccountLoginLocal(const FString& Username, const FString& Password)
{
	FString Message;
	LoginLocalCredentialForMenu(Username, Password, Message);
}

void ABHPlayerController::AccountForgetLocal()
{
	FString Message;
	ForgetLocalCredentialForMenu(Message);
}

void ABHPlayerController::AccountResetLocalClassroomData()
{
	FString Message;
	ResetLocalClassroomDataForMenu(Message);
}

void ABHPlayerController::CreateGameHotspot()
{
	FString Message;
	CreateGameHotspotForMenu(Message);
}

void ABHPlayerController::StopGameHotspot()
{
	FString Message;
	StopGameHotspotForMenu(Message);
}

void ABHPlayerController::StartInternetTunnel(int32 LocalPort)
{
	FString Message;
	StartInternetTunnelForMenu(Message, LocalPort);
}

void ABHPlayerController::StopInternetTunnel()
{
	FString Message;
	StopInternetTunnelForMenu(Message);
}

void ABHPlayerController::OpenInternetTunnelSetup(int32 LocalPort)
{
	FString Message;
	OpenInternetTunnelSetupForMenu(Message, LocalPort);
}

void ABHPlayerController::ForceStartRound()
{
	ServerForceStartRound();
}

void ABHPlayerController::ToggleClassroomBoard()
{
	if (ClassroomBoardWindow.IsValid())
	{
		HideClassroomBoard();
		return;
	}

	ShowClassroomBoard();
}

void ABHPlayerController::ShowClassroomBoard()
{
	FString Message;
	const bool bOpened = TryOpenClassroomBoardWindow(Message);
	ShowLocalStatusMessage(Message, bOpened ? 3.0f : 4.0f);
}

void ABHPlayerController::HideClassroomBoard()
{
	if (!ClassroomBoardWindow.IsValid())
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RequestDestroyWindow(ClassroomBoardWindow.ToSharedRef());
	}
	ClassroomBoardWindow.Reset();
}

void ABHPlayerController::ToggleHudMap()
{
	SetHudMapVisible(!bHudMapVisible);
}

void ABHPlayerController::SetHudMapVisible(bool bVisible)
{
	bHudMapVisible = bVisible;
	ShowLocalStatusMessage(bHudMapVisible ? TEXT("Map raised.") : TEXT("Map lowered."), 1.4f);
}

void ABHPlayerController::CycleCrosshairStyle()
{
	SetCrosshairStyle(CrosshairStyle + 1);
}

void ABHPlayerController::SetCrosshairStyle(int32 StyleIndex)
{
	static const TCHAR* StyleNames[] = {
		TEXT("scratched"),
		TEXT("pin"),
		TEXT("split"),
		TEXT("bare")
	};

	CrosshairStyle = FMath::Clamp(StyleIndex, 0, static_cast<int32>(UE_ARRAY_COUNT(StyleNames)) - 1);
	if (StyleIndex >= static_cast<int32>(UE_ARRAY_COUNT(StyleNames)))
	{
		CrosshairStyle = 0;
	}

	ShowLocalStatusMessage(FString::Printf(TEXT("Reticle: %s."), StyleNames[CrosshairStyle]), 1.4f);
}

void ABHPlayerController::SetNextLevel(const FString& LevelName)
{
	ServerSetNextLevel(LevelName);
}

void ABHPlayerController::SetAvatar(int32 AvatarIndex)
{
	FString Message;
	SetAvatarForMenu(AvatarIndex - 1, Message);
}

void ABHPlayerController::SetAvatarColor(int32 ColorIndex)
{
	FString Message;
	SetAvatarColorForMenu(ColorIndex - 1, Message);
}

void ABHPlayerController::SetAvatarHeadwear(int32 HeadwearIndex)
{
	FString Message;
	SetAvatarHeadwearForMenu(HeadwearIndex - 1, Message);
}

void ABHPlayerController::SetAvatarGear(int32 GearIndex)
{
	FString Message;
	SetAvatarGearForMenu(GearIndex - 1, Message);
}

void ABHPlayerController::VoteMap(const FString& LevelName)
{
	ServerSetMapVote(LevelName);
}

void ABHPlayerController::VoteFogPreset(const FString& Preset)
{
	ServerSetFogPresetVote(BHParseFogPreset(Preset));
}

void ABHPlayerController::SetFogPreset(const FString& Preset)
{
	ServerSetFogPresetOverride(BHParseFogPreset(Preset));
}

void ABHPlayerController::UseFogPresetVotes()
{
	ServerClearFogPresetOverride();
}

void ABHPlayerController::SetHunterCount(int32 HunterCount)
{
	ServerSetTargetHunterCount(HunterCount);
}

void ABHPlayerController::SetObjectiveIntensity(int32 Intensity)
{
	ServerSetObjectiveIntensity(Intensity);
}

void ABHPlayerController::SetBotCount(int32 BotCount)
{
	ServerSetBotCount(BotCount);
}

void ABHPlayerController::SetBotDifficulty(const FString& Difficulty)
{
	ServerSetBotDifficulty(BHParseBotDifficulty(Difficulty));
}

void ABHPlayerController::BotStatus()
{
	ServerBotStatus();
}

void ABHPlayerController::BotDumpMemory()
{
	ServerBotDumpMemory();
}

void ABHPlayerController::BotNavCheck()
{
	ServerBotNavCheck();
}

void ABHPlayerController::BotForceHunt()
{
	ServerBotForceHunt();
}

void ABHPlayerController::BotSoak(const FString& LevelName, int32 Seconds, int32 BotCount)
{
	ServerBotSoak(LevelName, Seconds, BotCount);
}

void ABHPlayerController::SetPhysicsTopics(const FString& Topics)
{
	ServerSetPhysicsTopics(Topics);
}

void ABHPlayerController::SetRevisionDifficultyMix(const FString& DifficultyMix)
{
	ServerSetRevisionDifficultyMix(DifficultyMix);
}

void ABHPlayerController::SetRevisionThresholds(float ClassPercent, float IndividualPercent)
{
	ServerSetRevisionThresholds(ClassPercent, IndividualPercent);
}

void ABHPlayerController::SetScareIntensity(int32 Intensity)
{
	ServerSetScareIntensity(Intensity);
}

void ABHPlayerController::ForceReview()
{
	ServerForceReview();
}

void ABHPlayerController::TesterShortcuts()
{
	ShowLocalStatusMessage(TEXT("Tester shortcuts: press Escape, open Round, then use Test Commands for jumpscares, atmosphere probes, train, and final escape tools."), 10.0f);
}

void ABHPlayerController::TesterGrantTrainResources()
{
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting points and powerups."), 1.5f);
	ServerTesterGrantTrainResources();
}

void ABHPlayerController::TesterOpenTrainIntermission()
{
	HideMainMenu();
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting train intermission."), 1.5f);
	ServerTesterOpenTrainIntermission();
}

void ABHPlayerController::TesterAdvanceTrainPhase()
{
	HideMainMenu();
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting next train phase."), 1.5f);
	ServerTesterAdvanceTrainPhase();
}

void ABHPlayerController::TesterLoadFinalStation()
{
	HideMainMenu();
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting Foggrounds final station."), 1.5f);
	ServerTesterLoadFinalStation();
}

void ABHPlayerController::TesterTriggerFinalEscape()
{
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting final escape unlock."), 1.5f);
	ServerTesterTriggerFinalEscape();
}

void ABHPlayerController::TesterForceFinalRecap()
{
	HideMainMenu();
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting final train recap."), 1.5f);
	ServerTesterForceFinalRecap();
}

void ABHPlayerController::RevisionStatus()
{
	ServerRevisionStatus();
}

void ABHPlayerController::ExportRevisionReport()
{
	ServerExportRevisionReport();
}

void ABHPlayerController::ExportPlaytestTelemetry()
{
	ServerExportPlaytestTelemetry();
}

void ABHPlayerController::SpectatorEncourage()
{
	ServerRequestSpectatorEncouragement();
}

void ABHPlayerController::SpectatorQueueTeacher()
{
	ServerSetSpectatorRolePreference(EBHPlayerRole::Hunter);
}

void ABHPlayerController::SpectatorQueueSurvivor()
{
	ServerSetSpectatorRolePreference(EBHPlayerRole::Survivor);
}

void ABHPlayerController::SpectatorQueueMonitor()
{
	ServerSetSpectatorRolePreference(EBHPlayerRole::FakeHunter);
}

void ABHPlayerController::SpectatorClearRolePreference()
{
	ServerSetSpectatorRolePreference(EBHPlayerRole::Unassigned);
}

void ABHPlayerController::ToggleInfectionMode()
{
	ServerToggleInfectionMode();
}

void ABHPlayerController::TogglePaceMode()
{
	ServerTogglePaceMode();
}

void ABHPlayerController::RefreshPracticeRound()
{
	ServerRefreshPracticeRound();
}

void ABHPlayerController::TriggerPracticeJumpscare()
{
	ServerTriggerPracticeJumpscare();
}

void ABHPlayerController::JumpscareTest(const FString& VariantToken)
{
	ServerJumpscareTest(VariantToken);
}

void ABHPlayerController::AtmosphereTest(const FString& Command)
{
	ServerAtmosphereTest(Command);
}

void ABHPlayerController::ToggleAtmosphereConsole()
{
	if (AtmosphereConsoleWidget.IsValid())
	{
		HideAtmosphereConsole();
		return;
	}

	ShowAtmosphereConsole();
}

void ABHPlayerController::ShowAtmosphereConsole()
{
	FString Message;
	if (!RequireLocalHostAdmin(Message, TEXT("open the atmosphere console")))
	{
		return;
	}

	if (!GEngine || !GEngine->GameViewport)
	{
		ShowLocalStatusMessage(TEXT("Atmosphere console needs an active game viewport."), 3.0f);
		return;
	}

	if (AtmosphereConsoleWidget.IsValid())
	{
		ShowLocalStatusMessage(TEXT("Atmosphere console is already open."), 1.6f);
		return;
	}

	HideMainMenu();
	HideTravelLoadingScreen();
	CaptureAtmosphereConsoleDefaults();

	SAssignNew(AtmosphereConsoleWidget, SBHAtmosphereConsole)
		.PlayerController(TWeakObjectPtr<ABHPlayerController>(this));

	GEngine->GameViewport->AddViewportWidgetContent(AtmosphereConsoleWidget.ToSharedRef(), 250);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(AtmosphereConsoleWidget);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	ShowLocalStatusMessage(TEXT("Atmosphere console opened."), 1.6f);
}

void ABHPlayerController::HideAtmosphereConsole()
{
	RemoveAtmosphereConsoleWidget();
	if (!MainMenuWidget.IsValid() && !TravelLoadingScreenWidget.IsValid())
	{
		ApplyGameplayInputMode();
	}
}

void ABHPlayerController::ShowMainMenu()
{
	if (!IsLocalController() || MainMenuWidget.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	HideTravelLoadingScreen();
	RemoveAtmosphereConsoleWidget();

	SAssignNew(MainMenuWidget, SBHMainMenu)
		.PlayerController(this);

	GEngine->GameViewport->AddViewportWidgetContent(MainMenuWidget.ToSharedRef(), 100);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(MainMenuWidget);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	UpdateAmbientMusic();
}

void ABHPlayerController::RemoveMainMenuWidget()
{
	if (MainMenuWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MainMenuWidget.ToSharedRef());
	}

	MainMenuWidget.Reset();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	}
}

void ABHPlayerController::RemoveAtmosphereConsoleWidget()
{
	if (AtmosphereConsoleWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(AtmosphereConsoleWidget.ToSharedRef());
	}

	AtmosphereConsoleWidget.Reset();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	}
}

void ABHPlayerController::HideMainMenu()
{
	RemoveMainMenuWidget();
	ApplyGameplayInputMode();
	UpdateAmbientMusic();
}

void ABHPlayerController::ToggleMainMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	if (MainMenuWidget.IsValid())
	{
		HideMainMenu();
	}
	else
	{
		ShowMainMenu();
	}
}

void ABHPlayerController::ReturnToMainMenu()
{
	HideMainMenu();
	// Tear down any online session (host OR client) before leaving, so the lingering session does not
	// block a later re-host ("session already exists") or block this client from joining another game.
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->LeaveOnlineSessionIfActive();
	}
	ShowTravelLoadingScreen(TEXT("MAIN MENU"), TEXT("Returning to the entry screen."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true);
}

void ABHPlayerController::QuitGame()
{
	RequestCleanQuit(TEXT("menu"));
}

void ABHPlayerController::ShowTravelLoadingScreen(const FString& Title, const FString& Detail)
{
	if (!IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	HideTravelLoadingScreen();

	const FString DisplayTitle = Title.IsEmpty() ? FString(TEXT("LOADING")) : Title.ToUpper();
	const FString DisplayDetail = Detail.IsEmpty() ? FString(TEXT("Preparing session.")) : Detail;
	const FString DisplayStatus = BHLoadingStatusForTitle(DisplayTitle);
	const FLinearColor LoadingAccent = BHLoadingAccentForTitle(DisplayTitle);

	SAssignNew(TravelLoadingScreenWidget, SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(BHUiWhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.004f, 0.006f, 0.008f, 0.98f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.HeightOverride(6.0f)
			[
				SNew(SBorder)
				.BorderImage(BHUiWhiteBrush())
				.BorderBackgroundColor(FLinearColor(LoadingAccent.R, LoadingAccent.G, LoadingAccent.B, 0.78f))
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBox)
			.HeightOverride(3.0f)
			[
				SNew(SBorder)
				.BorderImage(BHUiWhiteBrush())
				.BorderBackgroundColor(FLinearColor(0.95f, 0.42f, 0.22f, 0.60f))
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(760.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(STextBlock)
					.Font(BHUiFont(10, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(LoadingAccent.R, LoadingAccent.G, LoadingAccent.B, 0.90f))
					.Justification(ETextJustify::Center)
					.Text(FText::FromString(DisplayStatus))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(BHUiFont(30, FName(TEXT("Bold"))))
					.ColorAndOpacity(FLinearColor(0.86f, 1.0f, 0.95f, 1.0f))
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
					.WrapTextAt(720.0f)
					.ShadowOffset(FVector2D(2.0f, 2.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
					.Text(FText::FromString(DisplayTitle))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 8.0f, 0.0f, 18.0f)
				[
					SNew(STextBlock)
					.Font(BHUiFont(12))
					.ColorAndOpacity(FLinearColor(0.62f, 0.74f, 0.74f, 1.0f))
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
					.WrapTextAt(720.0f)
					.Text(FText::FromString(DisplayDetail))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SThrobber)
					.NumPieces(4)
					.Animate(SThrobber::All)
				]
			]
		];

	GEngine->GameViewport->AddViewportWidgetContent(TravelLoadingScreenWidget.ToSharedRef(), 1000);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = false;
	UpdateAmbientMusic();
}

void ABHPlayerController::HideTravelLoadingScreen()
{
	if (TravelLoadingScreenWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(TravelLoadingScreenWidget.ToSharedRef());
	}

	TravelLoadingScreenWidget.Reset();
}

bool ABHPlayerController::HostOnlineGameForMenu(const FString& LevelName, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("host online sessions")))
	{
		return false;
	}

	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to host an online session.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryHostOnlineGame(BHNormalizeRuntimeLevelName(LevelName), OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 8.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::HostPracticeGameForMenu(FString& OutMessage)
{
	OutMessage = TEXT("Starting Practice Lab.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HostPracticeGame();
	return true;
}

bool ABHPlayerController::HostTutorialGameForMenu(FString& OutMessage)
{
	OutMessage = TEXT("Starting the tutorial.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HostTutorialGame();
	return true;
}

bool ABHPlayerController::HostTutorialPhaseForMenu(EBHTutorialPhase StartPhase, bool bChain, FString& OutMessage)
{
	const TCHAR* Label = TEXT("Survivor");
	switch (StartPhase)
	{
	case EBHTutorialPhase::Teacher: Label = TEXT("Teacher"); break;
	case EBHTutorialPhase::Monitor: Label = TEXT("Hall Monitor"); break;
	case EBHTutorialPhase::Survivor:
	default:                        Label = TEXT("Survivor"); break;
	}
	OutMessage = bChain ? FString(TEXT("Starting the full tutorial course."))
		: FString::Printf(TEXT("Starting the %s tutorial."), Label);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HostTutorialPhase(StartPhase, bChain);
	return true;
}

bool ABHPlayerController::HostTestRoundForMenu(const FString& LevelName, FString& OutMessage)
{
	const FString NormalizedLevel = BHNormalizeRuntimeLevelName(LevelName);
	const FString Options = BHMakeListenOptions(NormalizedLevel, TEXT("?BHTestMode=1"));
	OutMessage = FString::Printf(TEXT("Starting %s Test Round."), *NormalizedLevel);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HideMainMenu();
	ShowTravelLoadingScreen(FString::Printf(TEXT("TEST %s"), *NormalizedLevel.ToUpper()), TEXT("Loading mechanics sandbox."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(NormalizedLevel)), true, Options);
	return true;
}

bool ABHPlayerController::HostPhysicsClassroomForMenu(FString& OutMessage)
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	FBHLessonPreset Preset;
	FString PresetMessage;
	FBHLessonPresetStore::TryFindPreset(FBHLessonPresetStore::GetSelectedPresetId(), Preset, PresetMessage);
	Preset = FBHLessonPresetStore::ValidatePreset(Preset);
	OutMessage = FString::Printf(TEXT("Starting IGCSE Physics Classroom with %s."), *Preset.DisplayName);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	BHApplyClassroomLoopbackBinding(Settings);
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("PHYSICS CLASSROOM"), TEXT("Preparing revision escape route."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(Preset.MapName)), true, BHMakeListenOptions(Preset.MapName, FBHLessonPresetStore::BuildRevisionLaunchOptions(Preset, false)));
	return true;
}

bool ABHPlayerController::HostLiveClassroomForMenu(FString& OutMessage)
{
	return HostLiveClassroomForMenu(TEXT("Facility"), OutMessage);
}

bool ABHPlayerController::HostLiveClassroomForMenu(const FString& LevelName, FString& OutMessage)
{
	const FString NormalizedLevel = BHNormalizeRuntimeLevelName(LevelName);
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	FBHLessonPreset Preset;
	FString PresetMessage;
	FBHLessonPresetStore::TryFindPreset(FBHLessonPresetStore::GetSelectedPresetId(), Preset, PresetMessage);
	Preset = FBHLessonPresetStore::ValidatePreset(Preset);
	FString JoinAddress;
	if (UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr)
	{
		const FString ConfiguredClassroomAddress = BHGI->GetConfiguredClassroomJoinAddress(7777);
		if (BHGI->IsAutomationEnabled())
		{
			BHGI->LogAutomationMarkerOnce(TEXT("LIVE_CLASSROOM_TUNNEL_SKIPPED"));
		}
		else
		{
			FString TunnelMessage;
			BHGI->TryStartInternetTunnel(TunnelMessage, 7777);
		}
		if (!ConfiguredClassroomAddress.IsEmpty())
		{
			BHGI->SetPublicJoinAddress(ConfiguredClassroomAddress);
		}
		JoinAddress = BHGI->GetPreferredClassroomJoinAddress(7777);
	}
	Preset.MapName = NormalizedLevel;
	const FString Options = BHMakeListenOptions(NormalizedLevel, FBHLessonPresetStore::BuildRevisionLaunchOptions(Preset, true));
	OutMessage = JoinAddress.IsEmpty()
		? FString::Printf(TEXT("Starting Live Classroom on %s with %s."), *NormalizedLevel, *Preset.DisplayName)
		: FString::Printf(TEXT("Starting Live Classroom on %s with %s. Students join %s."), *NormalizedLevel, *Preset.DisplayName, *JoinAddress);
	ShowLocalStatusMessage(OutMessage, 4.0f);
	BHApplyClassroomLoopbackBinding(Settings);
	HideMainMenu();
	ShowTravelLoadingScreen(FString::Printf(TEXT("LIVE %s"), *NormalizedLevel.ToUpper()), TEXT("Opening classroom host."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(NormalizedLevel)), true, Options);
	return true;
}

bool ABHPlayerController::HostBotGameForMenu(const FString& LevelName, FString& OutMessage)
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const int32 BotCount = FMath::Clamp(Settings ? Settings->DefaultBotCount : 5, 0, 11);
	const EBHBotDifficulty Difficulty = Settings ? Settings->DefaultBotDifficulty : EBHBotDifficulty::Normal;
	const FString NormalizedLevel = BHNormalizeRuntimeLevelName(LevelName);
	const FString Options = BHMakeListenOptions(NormalizedLevel, FString::Printf(TEXT("?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s?BHHumanRole=Survivor"),
		BotCount,
		*BHBotDifficultyToString(Difficulty)));

	OutMessage = FString::Printf(TEXT("Starting Bot %s with %d %s bots. Friends can still join."), *NormalizedLevel, BotCount, *BHBotDifficultyToString(Difficulty));
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HideMainMenu();
	ShowTravelLoadingScreen(FString::Printf(TEXT("BOT %s"), *NormalizedLevel.ToUpper()), TEXT("Opening a bot lobby others can join."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(NormalizedLevel)), true, Options);
	return true;
}

bool ABHPlayerController::FindOnlineGamesForMenu(FString& OutMessage)
{
	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to find online sessions.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryFindOnlineGames(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 8.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::JoinOnlineGameForMenu(int32 SessionIndex, FString& OutMessage)
{
	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to join an online session.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryJoinOnlineGame(SessionIndex, OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 8.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::DestroyOnlineSessionForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("destroy online sessions")))
	{
		return false;
	}

	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to clean up the online session.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryDestroyOnlineSession(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::ContinueAsGuestForMenu(FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->ContinueAsGuest(OutMessage);
	if (bSuccess)
	{
		PushLocalProfileToServer();
	}
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::BeginAccountLoginForMenu(const FString& Provider, FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->BeginProviderLogin(Provider, OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 8.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::PollAccountLoginForMenu(FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->PollProviderLogin(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::SyncAccountForMenu(FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->SyncProgress(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::SignOutAccountForMenu(FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->SignOut(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::CreateOrUpdateLocalCredentialForMenu(const FString& Username, const FString& Password, FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->CreateOrUpdateLocalCredential(Username, Password, OutMessage);
	if (bSuccess)
	{
		PushLocalProfileToServer();
	}
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::LoginLocalCredentialForMenu(const FString& Username, const FString& Password, FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->LoginLocalCredential(Username, Password, OutMessage);
	if (bSuccess)
	{
		PushLocalProfileToServer();
	}
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::ForgetLocalCredentialForMenu(FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->ForgetLocalCredential(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::ResetLocalClassroomDataForMenu(FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->ResetLocalClassroomData(OutMessage);
	if (bSuccess)
	{
		PushLocalProfileToServer();
	}
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::SubmitFeedbackForMenu(EBHFeedbackKind Kind, const FString& Message, int32 Rating, const FString& Contact, bool bIncludeDiagnostics, FString& OutMessage)
{
	UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr;
	if (!Feedback)
	{
		OutMessage = TEXT("No feedback subsystem was available.");
		return false;
	}

	const bool bSubmitted = Feedback->SubmitFeedback(Kind, Message, Rating, Contact, bIncludeDiagnostics, OutMessage);
	ShowLocalStatusMessage(OutMessage, 4.0f);
	return bSubmitted;
}

bool ABHPlayerController::SubmitRoundSurveyForMenu(int32 OverallRating, const FString& Difficulty, bool bWouldRecommend, const FString& Comment, FString& OutMessage)
{
	UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr;
	if (!Feedback)
	{
		OutMessage = TEXT("No feedback subsystem was available.");
		return false;
	}

	const bool bSubmitted = Feedback->SubmitRoundSurvey(OverallRating, Difficulty, bWouldRecommend, Comment, OutMessage);
	ShowLocalStatusMessage(OutMessage, 4.0f);
	return bSubmitted;
}

bool ABHPlayerController::IsFeedbackBackendConfiguredForMenu() const
{
	const UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr;
	return Feedback ? Feedback->IsBackendConfigured() : false;
}

bool ABHPlayerController::IsEndOfRoundSurveyPendingForMenu() const
{
	const UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr;
	return Feedback ? Feedback->IsEndOfRoundSurveyPending() : false;
}

bool ABHPlayerController::ShouldIncludeFeedbackDiagnosticsByDefaultForMenu() const
{
	const UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr;
	return Feedback ? Feedback->ShouldIncludeDiagnosticsByDefault() : true;
}

FString ABHPlayerController::GetFeedbackPrivacySummaryForMenu() const
{
	const UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr;
	return Feedback ? Feedback->GetPrivacySummary() : FString();
}

void ABHPlayerController::ClearEndOfRoundSurveyPendingForMenu()
{
	if (UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr)
	{
		Feedback->ClearEndOfRoundSurveyPending();
	}
}

bool ABHPlayerController::OpenClassroomBoardForMenu(FString& OutMessage)
{
	const bool bOpened = TryOpenClassroomBoardWindow(OutMessage);
	ShowLocalStatusMessage(OutMessage, bOpened ? 3.0f : 4.0f);
	return bOpened;
}

bool ABHPlayerController::OpenClassroomSupportFolderForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("open the classroom support folder")))
	{
		return false;
	}

	const FString SupportDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Builds"), TEXT("Support")));
	IFileManager::Get().MakeDirectory(*SupportDir, true);
	FPlatformProcess::ExploreFolder(*SupportDir);
	OutMessage = TEXT("Support folder opened. Run Tools\\New-ClassroomSupportBundle.ps1 from the project root to create a fresh PREFLIGHT report.");
	ShowLocalStatusMessage(OutMessage, 6.0f);
	return true;
}

bool ABHPlayerController::OpenClassroomLogFolderForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("open the classroom log folder")))
	{
		return false;
	}

	const FString LogDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs")));
	IFileManager::Get().MakeDirectory(*LogDir, true);
	FPlatformProcess::ExploreFolder(*LogDir);
	OutMessage = TEXT("Runtime log folder opened. Review logs before sharing outside the project team.");
	ShowLocalStatusMessage(OutMessage, 6.0f);
	return true;
}

bool ABHPlayerController::OpenClassroomPackageFolderForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("open the classroom package folder")))
	{
		return false;
	}

	const FString ProjectPackageDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Builds"), TEXT("Windows")));
	if (IFileManager::Get().DirectoryExists(*ProjectPackageDir))
	{
		FPlatformProcess::ExploreFolder(*ProjectPackageDir);
		OutMessage = TEXT("Windows classroom package folder opened.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return true;
	}

	const FString RuntimeBinaryDir = FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir());
	if (IFileManager::Get().DirectoryExists(*RuntimeBinaryDir))
	{
		FPlatformProcess::ExploreFolder(*RuntimeBinaryDir);
		OutMessage = TEXT("Builds\\Windows was not found; opened the current executable folder instead.");
		ShowLocalStatusMessage(OutMessage, 6.0f);
		return true;
	}

	OutMessage = FString::Printf(TEXT("Classroom package folder was not found at %s."), *ProjectPackageDir);
	ShowLocalStatusMessage(OutMessage, 6.0f);
	return false;
}

bool ABHPlayerController::OpenClassroomDeploymentGuideForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("open the classroom deployment guide")))
	{
		return false;
	}

	const FString GuidePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs"), TEXT("CLASSROOM_DEPLOYMENT.md")));
	if (!IFileManager::Get().FileExists(*GuidePath))
	{
		OutMessage = FString::Printf(TEXT("Classroom deployment guide was not found at %s."), *GuidePath);
		ShowLocalStatusMessage(OutMessage, 6.0f);
		return false;
	}

	FPlatformProcess::LaunchFileInDefaultExternalApplication(*GuidePath);
	OutMessage = TEXT("Classroom deployment guide opened.");
	ShowLocalStatusMessage(OutMessage, 5.0f);
	return true;
}

bool ABHPlayerController::CreateClassroomSupportBundleForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("create a classroom support bundle")))
	{
		return false;
	}

	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString SupportDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Builds"), TEXT("Support")));
	const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Tools"), TEXT("New-ClassroomSupportBundle.ps1")));
	IFileManager::Get().MakeDirectory(*SupportDir, true);

	if (!IFileManager::Get().FileExists(*ScriptPath))
	{
		OutMessage = FString::Printf(TEXT("Support bundle tool was not found at %s."), *ScriptPath);
		ShowLocalStatusMessage(OutMessage, 6.0f);
		return false;
	}

	const FString Params = FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -File %s"), *BHQuoteProcessArg(ScriptPath));
	const TCHAR* PrimaryShell =
#if PLATFORM_WINDOWS
		TEXT("powershell.exe");
#else
		TEXT("pwsh");
#endif
	FProcHandle Process = FPlatformProcess::CreateProc(PrimaryShell, *Params, true, true, true, nullptr, 0, *ProjectDir, nullptr);

#if PLATFORM_WINDOWS
	if (!Process.IsValid())
	{
		Process = FPlatformProcess::CreateProc(TEXT("pwsh.exe"), *Params, true, true, true, nullptr, 0, *ProjectDir, nullptr);
	}
#endif

	if (!Process.IsValid())
	{
		OutMessage = TEXT("Could not start PowerShell for the classroom support bundle tool.");
		ShowLocalStatusMessage(OutMessage, 6.0f);
		return false;
	}

	FPlatformProcess::CloseProc(Process);
	FPlatformProcess::ExploreFolder(*SupportDir);
	OutMessage = TEXT("Classroom support bundle started. Watch Builds\\Support for the new PREFLIGHT.md and zip.");
	ShowLocalStatusMessage(OutMessage, 8.0f);
	return true;
}

bool ABHPlayerController::RefreshClassroomPreflightForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("refresh classroom preflight")))
	{
		return false;
	}

	LastClassroomPreflightSummaryTime = -1000.0;
	const FBHClassroomPreflightSummary Summary = GetClassroomPreflightSummaryForMenu();
	OutMessage = Summary.ReadyStatus.IsEmpty() ? TEXT("Classroom preflight refreshed.") : Summary.ReadyStatus;
	ShowLocalStatusMessage(OutMessage, Summary.bReadyForClassroom ? 5.0f : 8.0f);
	return Summary.bReadyForClassroom;
}

bool ABHPlayerController::CopyClassroomJoinCodeForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("copy the classroom join code")))
	{
		return false;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	const FBHInternetTunnelResult TunnelStatus = FBHNetworkSupport::GetInternetTunnelStatus(7777);
	const FString ConfiguredEndpoint = BHGI ? BHGI->GetConfiguredClassroomJoinAddress(7777) : FString();
	const FString PublicJoinAddress = BHGI ? BHGI->GetPublicJoinAddress() : FString();
	FString JoinAddress = PublicJoinAddress.IsEmpty() ? ConfiguredEndpoint : PublicJoinAddress;
	if (JoinAddress.IsEmpty() && !TunnelStatus.TunnelAddress.IsEmpty())
	{
		JoinAddress = TunnelStatus.TunnelAddress;
	}
	if (JoinAddress.IsEmpty() && Settings && !Settings->bClassroomLoopbackOnlyHost)
	{
		JoinAddress = BHGI ? BHGI->GetPreferredClassroomJoinAddress(7777) : FBHNetworkSupport::ResolveLocalJoinAddress(7777);
	}

	const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(JoinAddress);
	const FString InviteCode = FBHNetworkSupport::MakeJoinInviteCode(NormalizedAddress);
	if (NormalizedAddress.IsEmpty() || InviteCode.IsEmpty())
	{
		OutMessage = TEXT("No classroom endpoint is ready to copy. Start/refresh the tunnel or configure a classroom endpoint first.");
		ShowLocalStatusMessage(OutMessage, 7.0f);
		return false;
	}

	FPlatformApplicationMisc::ClipboardCopy(*InviteCode);
	OutMessage = FString::Printf(TEXT("Copied classroom join code for %s."), *NormalizedAddress);
	ShowLocalStatusMessage(OutMessage, 5.0f);
	return true;
}

FBHClassroomPreflightSummary ABHPlayerController::GetClassroomPreflightSummaryForMenu() const
{
	const double Now = FPlatformTime::Seconds();
	if (!CachedClassroomPreflightSummary.DetailText.IsEmpty() && (Now - LastClassroomPreflightSummaryTime) < 1.0)
	{
		return CachedClassroomPreflightSummary;
	}

	FBHClassroomPreflightSummary Summary;
	Summary.bHostOnlyAvailable = CanUseClassroomHostControlsForMenu();
	if (!Summary.bHostOnlyAvailable)
	{
		Summary.ReadyStatus = TEXT("Host-only preflight");
		Summary.DetailText = TEXT("Classroom setup details are only shown on the host machine.");
		CachedClassroomPreflightSummary = Summary;
		LastClassroomPreflightSummaryTime = Now;
		return Summary;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const UWorld* World = GetWorld();
	const UBHGameInstance* BHGI = World ? World->GetGameInstance<UBHGameInstance>() : nullptr;
	const FBHInternetTunnelResult TunnelStatus = FBHNetworkSupport::GetInternetTunnelStatus(7777);
	const FString ConfiguredEndpoint = BHGI ? BHGI->GetConfiguredClassroomJoinAddress(7777) : FString();
	const FString PublicJoinAddress = BHGI ? BHGI->GetPublicJoinAddress() : FString();
	const FString PreferredJoinAddress = BHGI ? BHGI->GetPreferredClassroomJoinAddress(7777) : FBHNetworkSupport::ResolveLocalJoinAddress(7777);
	const bool bClassroomMode = Settings && Settings->bClassroomMode;
	const bool bLoopbackOnly = Settings && Settings->bClassroomLoopbackOnlyHost;
	const bool bTunnelAllowed = Settings && Settings->bAllowTunnelHelper;
	const bool bHotspotAllowed = Settings && Settings->bAllowHotspotHelper;
	const bool bHasUsableEndpoint = !ConfiguredEndpoint.IsEmpty() || !TunnelStatus.TunnelAddress.IsEmpty();
	const bool bNetworkLooksReady = bLoopbackOnly ? (TunnelStatus.bTunnelReady || !ConfiguredEndpoint.IsEmpty()) : !PreferredJoinAddress.IsEmpty();
	const bool bTunnelConfirmed = !bLoopbackOnly || TunnelStatus.bTunnelReady;
	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString RuntimeLogDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs")));
	const FString SupportDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Builds"), TEXT("Support")));
	const FString WindowsPackageRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Builds"), TEXT("Windows")));
	const FString RuntimeBinaryDir = FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir());
	const FString DeploymentGuidePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Docs"), TEXT("CLASSROOM_DEPLOYMENT.md")));
	const FString SupportBundleScriptPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Tools"), TEXT("New-ClassroomSupportBundle.ps1")));
	const bool bPackageRootFound = IFileManager::Get().DirectoryExists(*WindowsPackageRoot);
	const bool bRuntimeBinaryDirFound = IFileManager::Get().DirectoryExists(*RuntimeBinaryDir);
	const bool bDeploymentGuideFound = IFileManager::Get().FileExists(*DeploymentGuidePath);
	const bool bSupportBundleToolFound = IFileManager::Get().FileExists(*SupportBundleScriptPath);

	TArray<FString> Lines;
	TArray<FString> Warnings;
	FString NextAction = TEXT("Host or restart Live Classroom, then copy the join code for students.");

	if (!Settings || !World)
	{
		Warnings.Add(TEXT("Runtime settings are not fully available yet."));
		NextAction = TEXT("Open the menu again after the world finishes loading.");
	}
	else if (!bClassroomMode)
	{
		Warnings.Add(TEXT("Classroom mode is disabled in game settings."));
		NextAction = TEXT("Enable the classroom profile before live class hosting.");
	}
	else if (bLoopbackOnly && !bHasUsableEndpoint)
	{
		Warnings.Add(TEXT("Loopback-only hosting needs a configured Playit endpoint or a detected tunnel allocation."));
		NextAction = TEXT("Use Start Playit, then open the deployment guide if the classroom endpoint is still missing.");
	}
	else if (bLoopbackOnly && !bTunnelConfirmed)
	{
		Warnings.Add(TEXT("The local Playit helper is not reporting a live allocation yet. Press Start Playit before students join."));
		NextAction = TEXT("Press Start Playit and wait for the helper to report a live allocation; no Windows administrator prompt is expected.");
	}
	if (bLoopbackOnly && !bTunnelAllowed && !TunnelStatus.bTunnelReady)
	{
		Warnings.Add(TEXT("Tunnel helper is disabled, so IT or the teacher must run the classroom tunnel outside the game."));
	}
	if (!bPackageRootFound && !bRuntimeBinaryDirFound)
	{
		Warnings.Add(TEXT("No package or runtime executable folder could be resolved."));
	}
	if (bGraphicsLikelySoftwareGpu)
	{
		Warnings.Add(TEXT("Graphics adapter looks like software or VM rendering. Auto graphics uses Low 4GB plus 1280x720 windowed on first launch; validate on real classroom hardware."));
	}
	else if (bAutoHardwareGraphicsEnabled && bGraphicsPreferSafeResolution)
	{
		Warnings.Add(TEXT("Low-spec graphics detected. Auto graphics uses Low 4GB plus 1280x720 windowed on first launch unless this machine already saved a resolution."));
	}

	Summary.bReadyForClassroom = bClassroomMode && bNetworkLooksReady && bTunnelConfirmed;
	if (!Settings || !World)
	{
		Summary.ReadyStatus = TEXT("Unknown");
	}
	else if (!bClassroomMode)
	{
		Summary.ReadyStatus = TEXT("Needs setup");
	}
	else if (bLoopbackOnly && !bHasUsableEndpoint)
	{
		Summary.ReadyStatus = TEXT("Missing endpoint");
	}
	else if (bLoopbackOnly && !bTunnelConfirmed)
	{
		Summary.ReadyStatus = TEXT("Needs tunnel");
	}
	else if (!bPackageRootFound && !bRuntimeBinaryDirFound)
	{
		Summary.ReadyStatus = TEXT("Package path unknown");
	}
	else if (bGraphicsLikelySoftwareGpu)
	{
		Summary.ReadyStatus = TEXT("Ready - validate graphics");
	}
	else if (GraphicsPresetQuality <= 0 && bGraphicsAutoSafeResolutionApplied)
	{
		Summary.ReadyStatus = TEXT("Ready - low graphics 720p");
	}
	else if (GraphicsPresetQuality <= 0)
	{
		Summary.ReadyStatus = TEXT("Ready - low graphics profile");
	}
	else
	{
		Summary.ReadyStatus = TEXT("Ready");
	}

	const FString ProjectName = BHReadConfigValue(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectName"), GGameIni);
	const FString ProjectVersion = BHReadConfigValue(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), GGameIni);
	const FString DefaultRHI = BHReadConfigValue(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("DefaultGraphicsRHI"), GEngineIni);
	const FString OnlineSubsystem = BHGI ? BHGI->GetOnlineSubsystemName() : TEXT("Unavailable");
	const FString LatestPreflight = BHFindLatestPreflightReport(SupportDir);
	const FString NetModeText = World
		? (World->GetNetMode() == NM_ListenServer ? TEXT("Listen server") : (World->GetNetMode() == NM_Standalone ? TEXT("Standalone") : TEXT("Client")))
		: TEXT("No world");
	const AGameStateBase* BaseGameState = World ? World->GetGameState() : nullptr;
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const int32 ConnectedPlayers = BaseGameState ? BaseGameState->PlayerArray.Num() : 0;
	const FString ActiveMapName = World ? FPackageName::GetShortName(World->GetMapName()) : TEXT("<unknown>");
	const FString ClassroomModeText = BHGS && BHGS->bRevisionMode
		? TEXT("Live Classroom active")
		: (bClassroomMode ? TEXT("Classroom profile enabled") : TEXT("Classroom profile disabled"));
	const FString LoopbackText = bLoopbackOnly
		? (BHHasMultihomeOverride() ? TEXT("Enabled, but -MULTIHOME override is present") : TEXT("Enabled, host binds 127.0.0.1"))
		: TEXT("Disabled, host may bind a LAN interface");
	const FString DirectLanText = bLoopbackOnly
		? TEXT("IT-managed only from Host LAN; Live Classroom avoids inbound firewall prompts")
		: TEXT("IT-managed: confirm UDP 7777/firewall before students join");
	const FString AdminAccessText = bLoopbackOnly
		? TEXT("Not required for Live Classroom defaults; Playit/loopback avoids Windows Firewall consent")
		: TEXT("May be required by direct LAN/firewall policy; prefer Playit/loopback on locked-down PCs");
	const FString HotspotText = bHotspotAllowed
		? TEXT("Enabled, but likely requires school IT or administrator networking permission")
		: TEXT("Disabled for classroom default; use Playit/loopback instead");
	const FString TeacherSafeActionsText = bSupportBundleToolFound
		? TEXT("Refresh, Copy Join Code, Start Playit, Open Logs, Open Package, Support Bundle")
		: TEXT("Refresh, Copy Join Code, Start Playit, Open Logs, Open Package; support-bundle tool is missing");
	const FString ItManagedActionsText = TEXT("Direct LAN firewall changes and hotspot setup; do not require these from a standard teacher account");
	FString JoinCodeAddress = PublicJoinAddress.IsEmpty() ? ConfiguredEndpoint : PublicJoinAddress;
	if (JoinCodeAddress.IsEmpty() && !TunnelStatus.TunnelAddress.IsEmpty())
	{
		JoinCodeAddress = TunnelStatus.TunnelAddress;
	}
	if (JoinCodeAddress.IsEmpty() && !bLoopbackOnly)
	{
		JoinCodeAddress = PreferredJoinAddress;
	}
	const FString JoinCodeText = !JoinCodeAddress.IsEmpty()
		? FString::Printf(TEXT("Copy action available for %s"), *JoinCodeAddress)
		: TEXT("<not ready for loopback classroom>");
	const FString PackageText = bPackageRootFound
		? FString::Printf(TEXT("%s (found)"), *WindowsPackageRoot)
		: FString::Printf(TEXT("%s (not found; use runtime executable folder below)"), *WindowsPackageRoot);
	FString GraphicsStatus = TEXT("Ready");
	if (bGraphicsLikelySoftwareGpu)
	{
		GraphicsStatus = TEXT("Needs hardware validation");
	}
	else if (GraphicsPresetQuality <= 0 && bGraphicsAutoSafeResolutionApplied)
	{
		GraphicsStatus = TEXT("Low graphics profile, 720p windowed");
	}
	else if (GraphicsPresetQuality <= 0)
	{
		GraphicsStatus = TEXT("Low graphics profile");
	}
	else if (bGraphicsLikelyIntegratedGpu)
	{
		GraphicsStatus = TEXT("Integrated GPU - validate FPS");
	}

	BHAppendPreflightLine(Lines, TEXT("Ready"), Summary.ReadyStatus);
	BHAppendPreflightLine(Lines, TEXT("Next action"), NextAction);
	BHAppendPreflightLine(Lines, TEXT("Version / beta target"), FString::Printf(TEXT("%s %s"), *ProjectName, *ProjectVersion));
	BHAppendPreflightLine(Lines, TEXT("Map / classroom mode"), FString::Printf(TEXT("%s, %s"), *ActiveMapName, *ClassroomModeText));
	BHAppendPreflightLine(Lines, TEXT("Host mode"), FString::Printf(TEXT("%s, %d connected player(s)"), *NetModeText, ConnectedPlayers));
	BHAppendPreflightLine(Lines, TEXT("Configured Playit endpoint"), ConfiguredEndpoint.IsEmpty() ? TEXT("<unset>") : ConfiguredEndpoint);
	BHAppendPreflightLine(Lines, TEXT("Preferred join address"), PreferredJoinAddress.IsEmpty() ? TEXT("<unavailable>") : PreferredJoinAddress);
	BHAppendPreflightLine(Lines, TEXT("Classroom join code"), JoinCodeText);
	BHAppendPreflightLine(Lines, TEXT("Loopback mode"), LoopbackText);
	BHAppendPreflightLine(Lines, TEXT("Direct LAN availability"), DirectLanText);
	BHAppendPreflightLine(Lines, TEXT("Admin access"), AdminAccessText);
	BHAppendPreflightLine(Lines, TEXT("Teacher-safe actions"), TeacherSafeActionsText);
	BHAppendPreflightLine(Lines, TEXT("IT-managed actions"), ItManagedActionsText);
	BHAppendPreflightLine(Lines, TEXT("Tunnel helper permission"), BHBoolSettingText(bTunnelAllowed));
	BHAppendPreflightLine(Lines, TEXT("Tunnel status"), TunnelStatus.Message.IsEmpty() ? TEXT("No tunnel status available.") : TunnelStatus.Message);
	BHAppendPreflightLine(Lines, TEXT("Hotspot helper"), HotspotText);
	BHAppendPreflightLine(Lines, TEXT("Online subsystem"), OnlineSubsystem);
	BHAppendPreflightLine(Lines, TEXT("Default RHI"), DefaultRHI);
	BHAppendPreflightLine(Lines, TEXT("Graphics"), GetGraphicsSummaryForMenu());
	BHAppendPreflightLine(Lines, TEXT("Graphics status"), GraphicsStatus);
	BHAppendPreflightLine(Lines, TEXT("Runtime logs"), BHPathStateText(RuntimeLogDir, true));
	BHAppendPreflightLine(Lines, TEXT("Playit log"), TunnelStatus.LogPath.IsEmpty() ? TEXT("<not available on this platform>") : BHPathStateText(TunnelStatus.LogPath, false));
	BHAppendPreflightLine(Lines, TEXT("Support bundle folder"), BHPathStateText(SupportDir, true));
	BHAppendPreflightLine(Lines, TEXT("Support bundle tool"), FString::Printf(TEXT("%s (%s)"), *SupportBundleScriptPath, bSupportBundleToolFound ? TEXT("found") : TEXT("missing in this build")));
	BHAppendPreflightLine(Lines, TEXT("Windows package root"), PackageText);
	BHAppendPreflightLine(Lines, TEXT("Runtime executable folder"), FString::Printf(TEXT("%s (%s)"), *RuntimeBinaryDir, bRuntimeBinaryDirFound ? TEXT("found") : TEXT("missing")));
	BHAppendPreflightLine(Lines, TEXT("Latest PREFLIGHT.md"), LatestPreflight.IsEmpty() ? TEXT("<none yet>") : LatestPreflight);
	BHAppendPreflightLine(Lines, TEXT("Deployment guide"), FString::Printf(TEXT("%s (%s)"), *DeploymentGuidePath, bDeploymentGuideFound ? TEXT("found") : TEXT("missing")));
	BHAppendPreflightLine(Lines, TEXT("Warnings"), BHJoinWarnings(Warnings));
	BHAppendPreflightLine(Lines, TEXT("Privacy"), TEXT("Only paths and safe setup state are shown here; saved account data and secrets are not displayed."));

	Summary.DetailText = FString::Join(Lines, TEXT("\n"));
	CachedClassroomPreflightSummary = Summary;
	LastClassroomPreflightSummaryTime = Now;
	return Summary;
}

bool ABHPlayerController::CanUseClassroomHostControlsForMenu() const
{
	return IsLocalHostAdminContext();
}

bool ABHPlayerController::CreateGameHotspotForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("create the game hotspot")))
	{
		return false;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings || !Settings->bAllowHotspotHelper)
	{
		OutMessage = TEXT("Game hotspot helper is disabled in classroom settings.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to create a game hotspot.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryCreateGameHotspot(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 10.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::StopGameHotspotForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("stop the game hotspot")))
	{
		return false;
	}

	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to stop the game hotspot.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryStopGameHotspot(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::StartInternetTunnelForMenu(FString& OutMessage, int32 LocalPort)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("start the internet tunnel helper")))
	{
		return false;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings || !Settings->bAllowTunnelHelper)
	{
		OutMessage = TEXT("Internet tunnel helper is disabled in classroom settings.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to start the internet tunnel helper.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryStartInternetTunnel(OutMessage, LocalPort);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 10.0f : 8.0f);
	return bSuccess;
}

bool ABHPlayerController::StopInternetTunnelForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("stop the internet tunnel helper")))
	{
		return false;
	}

	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to stop the internet tunnel helper.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryStopInternetTunnel(OutMessage);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::OpenInternetTunnelSetupForMenu(FString& OutMessage, int32 LocalPort)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("open internet tunnel setup")))
	{
		return false;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings || !Settings->bAllowTunnelHelper)
	{
		OutMessage = TEXT("Internet tunnel helper is disabled in classroom settings.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr;
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available to open internet tunnel setup.");
		ShowLocalStatusMessage(OutMessage, 5.0f);
		return false;
	}

	const bool bSuccess = BHGI->TryOpenInternetTunnelSetup(OutMessage, LocalPort);
	ShowLocalStatusMessage(OutMessage, bSuccess ? 8.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::SetNextLevelForMenu(const FString& LevelName, FString& OutMessage)
{
	ServerSetNextLevel(LevelName);
	OutMessage = FString::Printf(TEXT("Next level request sent: %s."), *LevelName);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::VoteMapForMenu(const FString& LevelName, FString& OutMessage)
{
	ServerSetMapVote(LevelName);
	OutMessage = FString::Printf(TEXT("Map vote sent: %s."), *LevelName);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::VoteFogPresetForMenu(EBHFogPreset FogPreset, FString& OutMessage)
{
	ServerSetFogPresetVote(FogPreset);
	OutMessage = FString::Printf(TEXT("Fog vote sent: %s."), *BHFogPresetToString(FogPreset));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetFogPresetOverrideForMenu(EBHFogPreset FogPreset, FString& OutMessage)
{
	ServerSetFogPresetOverride(FogPreset);
	OutMessage = FString::Printf(TEXT("Fog override request sent: %s."), *BHFogPresetToString(FogPreset));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::UseFogPresetVotesForMenu(FString& OutMessage)
{
	ServerClearFogPresetOverride();
	OutMessage = TEXT("Fog preset will follow lobby votes.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetHunterCountForMenu(int32 HunterCount, FString& OutMessage)
{
	ServerSetTargetHunterCount(HunterCount);
	OutMessage = FString::Printf(TEXT("Teacher count request sent: %d."), HunterCount);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetObjectiveIntensityForMenu(int32 Intensity, FString& OutMessage)
{
	ServerSetObjectiveIntensity(Intensity);
	OutMessage = FString::Printf(TEXT("Objective intensity request sent: %d."), Intensity);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetBotCountForMenu(int32 BotCount, FString& OutMessage)
{
	const int32 ClampedCount = FMath::Clamp(BotCount, 0, 11);
	ServerSetBotCount(ClampedCount);
	if (UBHGameSettings* Settings = GetMutableDefault<UBHGameSettings>())
	{
		// Remember the host's last choice so the next "Bot <Map>" launch defaults to it.
		Settings->DefaultBotCount = ClampedCount;
		Settings->SaveConfig();
	}
	OutMessage = FString::Printf(TEXT("Bot count request sent: %d."), ClampedCount);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetBotDifficultyForMenu(EBHBotDifficulty Difficulty, FString& OutMessage)
{
	ServerSetBotDifficulty(Difficulty);
	if (UBHGameSettings* Settings = GetMutableDefault<UBHGameSettings>())
	{
		// Persist alongside DefaultBotCount so a fresh bot host reuses the last difficulty too.
		Settings->DefaultBotDifficulty = Difficulty;
		Settings->SaveConfig();
	}
	OutMessage = FString::Printf(TEXT("Bot difficulty request sent: %s."), *BHBotDifficultyToString(Difficulty));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::FillBotsForMenu(FString& OutMessage)
{
	ServerFillBots();
	OutMessage = TEXT("Fill request sent: topping the lobby up with bots.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ToggleInfectionModeForMenu(FString& OutMessage)
{
	ServerToggleInfectionMode();
	OutMessage = TEXT("Infection toggle sent.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::TogglePaceModeForMenu(FString& OutMessage)
{
	ServerTogglePaceMode();
	OutMessage = TEXT("Pace toggle sent.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetPracticeRoleForMenu(EBHPlayerRole NewRole, FString& OutMessage)
{
	ServerSetPracticeRole(NewRole);
	OutMessage = TEXT("Practice role request sent.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetPracticeModifierForMenu(EBHRoundModifier NewModifier, FString& OutMessage)
{
	ServerSetPracticeModifier(NewModifier);
	OutMessage = TEXT("Practice modifier request sent.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::RefreshPracticeRoundForMenu(FString& OutMessage)
{
	ServerRefreshPracticeRound();
	OutMessage = TEXT("Practice refresh request sent.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::TriggerPracticeJumpscareForMenu(FString& OutMessage)
{
	HideMainMenu();
	ServerTriggerPracticeJumpscare();
	OutMessage = TEXT("");
	return true;
}

bool ABHPlayerController::TriggerJumpscareVariantForMenu(const FString& VariantToken, FString& OutMessage)
{
	const FString Token = VariantToken.TrimStartAndEnd();
	const FString Normalized = Token.ToLower();
	if (Normalized != TEXT("list") && Normalized != TEXT("help") && Normalized != TEXT("?"))
	{
		HideMainMenu();
	}

	ServerJumpscareTest(Token);
	if (Token.Equals(TEXT("all"), ESearchCase::IgnoreCase))
	{
		OutMessage = TEXT("Queued every jumpscare variant.");
	}
	else if (Normalized.StartsWith(TEXT("super")) || Normalized == TEXT("chain"))
	{
		OutMessage = TEXT("Super jumpscare chain sent.");
	}
	else
	{
		OutMessage = FString::Printf(TEXT("Jumpscare test sent: %s."), Token.IsEmpty() ? TEXT("list") : *Token);
	}
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::RunAtmosphereTestForMenu(const FString& Command, FString& OutMessage)
{
	const FString Normalized = Command.TrimStartAndEnd().ToLower();
	if (Normalized != TEXT("bots") && Normalized != TEXT("statetree"))
	{
		HideMainMenu();
	}

	ServerAtmosphereTest(Command);
	OutMessage = FString::Printf(TEXT("Atmosphere test sent: %s."), Command.IsEmpty() ? TEXT("help") : *Command);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

float ABHPlayerController::GetAtmosphereConsoleValue(FName ParameterName) const
{
	const float DefaultValue = AtmosphereConsoleDefaultValues.Contains(ParameterName)
		? AtmosphereConsoleDefaultValues[ParameterName]
		: 0.0f;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return DefaultValue;
	}

	const FString Parameter = ParameterName.ToString();
	const auto SuffixAfter = [&Parameter](const TCHAR* Prefix) -> FString
	{
		const FString PrefixString(Prefix);
		return Parameter.StartsWith(PrefixString) ? Parameter.RightChop(PrefixString.Len()) : FString();
	};

	if (Parameter.StartsWith(TEXT("Fog.")) || Parameter.StartsWith(TEXT("Volumetric.")))
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			const UExponentialHeightFogComponent* Fog = It->GetComponent();
			if (!Fog)
			{
				continue;
			}

			if (Parameter == TEXT("Fog.Density")) { return Fog->FogDensity; }
			if (Parameter == TEXT("Fog.HeightFalloff")) { return Fog->FogHeightFalloff; }
			if (Parameter == TEXT("Fog.MaxOpacity")) { return Fog->FogMaxOpacity; }
			if (Parameter == TEXT("Fog.StartDistance")) { return Fog->StartDistance; }
			if (Parameter == TEXT("Fog.EndDistance")) { return Fog->EndDistance; }
			if (Parameter == TEXT("Fog.DirectionalExponent")) { return Fog->DirectionalInscatteringExponent; }
			if (Parameter == TEXT("Fog.DirectionalStart")) { return Fog->DirectionalInscatteringStartDistance; }
			if (Parameter.StartsWith(TEXT("Fog.Inscatter."))) { return BHGetColorComponent(Fog->FogInscatteringLuminance, SuffixAfter(TEXT("Fog.Inscatter."))); }
			if (Parameter.StartsWith(TEXT("Fog.Directional."))) { return BHGetColorComponent(Fog->DirectionalInscatteringLuminance, SuffixAfter(TEXT("Fog.Directional."))); }
			if (Parameter == TEXT("Volumetric.Extinction")) { return Fog->VolumetricFogExtinctionScale; }
			if (Parameter == TEXT("Volumetric.Distance")) { return Fog->VolumetricFogDistance; }
			if (Parameter == TEXT("Volumetric.Start")) { return Fog->VolumetricFogStartDistance; }
			if (Parameter == TEXT("Volumetric.NearFade")) { return Fog->VolumetricFogNearFadeInDistance; }
			if (Parameter == TEXT("Volumetric.Scattering")) { return Fog->VolumetricFogScatteringDistribution; }
			if (Parameter.StartsWith(TEXT("Volumetric.Albedo."))) { return BHGetColorComponent(FLinearColor(Fog->VolumetricFogAlbedo), SuffixAfter(TEXT("Volumetric.Albedo."))); }
			if (Parameter.StartsWith(TEXT("Volumetric.Emissive."))) { return BHGetColorComponent(Fog->VolumetricFogEmissive, SuffixAfter(TEXT("Volumetric.Emissive."))); }
		}
	}

	if (Parameter.StartsWith(TEXT("LocalFog.")))
	{
		for (TActorIterator<ALocalFogVolume> It(World); It; ++It)
		{
			const ULocalFogVolumeComponent* Fog = It->GetComponent();
			if (!Fog)
			{
				continue;
			}

			if (Parameter == TEXT("LocalFog.Radial")) { return Fog->RadialFogExtinction; }
			if (Parameter == TEXT("LocalFog.Height")) { return Fog->HeightFogExtinction; }
			if (Parameter == TEXT("LocalFog.Falloff")) { return Fog->HeightFogFalloff; }
			if (Parameter == TEXT("LocalFog.Offset")) { return Fog->HeightFogOffset; }
			if (Parameter == TEXT("LocalFog.Phase")) { return Fog->FogPhaseG; }
			if (Parameter.StartsWith(TEXT("LocalFog.Albedo."))) { return BHGetColorComponent(Fog->FogAlbedo, SuffixAfter(TEXT("LocalFog.Albedo."))); }
			if (Parameter.StartsWith(TEXT("LocalFog.Emissive."))) { return BHGetColorComponent(Fog->FogEmissive, SuffixAfter(TEXT("LocalFog.Emissive."))); }
		}
	}

	if (Parameter.StartsWith(TEXT("Sky.")))
	{
		for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
		{
			const USkyAtmosphereComponent* Sky = It->GetComponent();
			if (!Sky)
			{
				continue;
			}

			if (Parameter.StartsWith(TEXT("Sky.Luminance."))) { return BHGetColorComponent(Sky->SkyLuminanceFactor, SuffixAfter(TEXT("Sky.Luminance."))); }
			if (Parameter.StartsWith(TEXT("Sky.Aerial."))) { return BHGetColorComponent(Sky->SkyAndAerialPerspectiveLuminanceFactor, SuffixAfter(TEXT("Sky.Aerial."))); }
			if (Parameter == TEXT("Sky.MultiScattering")) { return Sky->MultiScatteringFactor; }
			if (Parameter == TEXT("Sky.HeightFogContribution")) { return Sky->HeightFogContribution; }
			if (Parameter == TEXT("Sky.AerialDistance")) { return Sky->AerialPespectiveViewDistanceScale; }
			if (Parameter.StartsWith(TEXT("Sky.Ground."))) { return BHGetColorComponent(FLinearColor(Sky->GroundAlbedo), SuffixAfter(TEXT("Sky.Ground."))); }
			if (Parameter == TEXT("Sky.RayleighScale")) { return Sky->RayleighScatteringScale; }
			if (Parameter == TEXT("Sky.MieScale")) { return Sky->MieScatteringScale; }
			if (Parameter == TEXT("Sky.MieAbsorptionScale")) { return Sky->MieAbsorptionScale; }
			if (Parameter == TEXT("Sky.MieAnisotropy")) { return Sky->MieAnisotropy; }
			if (Parameter == TEXT("Sky.AbsorptionScale")) { return Sky->OtherAbsorptionScale; }
		}
	}

	if (Parameter.StartsWith(TEXT("Moon.")))
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			const UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (!Light || !Light->bAtmosphereSunLight)
			{
				continue;
			}

			if (Parameter == TEXT("Moon.Intensity")) { return Light->Intensity; }
			if (Parameter == TEXT("Moon.Indirect")) { return Light->IndirectLightingIntensity; }
			if (Parameter == TEXT("Moon.Volumetric")) { return Light->VolumetricScatteringIntensity; }
			if (Parameter == TEXT("Moon.Pitch")) { return It->GetActorRotation().Pitch; }
			if (Parameter == TEXT("Moon.Yaw")) { return It->GetActorRotation().Yaw; }
			if (Parameter.StartsWith(TEXT("Moon.Color."))) { return BHGetColorComponent(Light->GetLightColor(), SuffixAfter(TEXT("Moon.Color."))); }
			if (Parameter.StartsWith(TEXT("Moon.Disk."))) { return BHGetColorComponent(Light->AtmosphereSunDiskColorScale, SuffixAfter(TEXT("Moon.Disk."))); }
		}
	}

	if (Parameter.StartsWith(TEXT("SkyLight.")))
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			const USkyLightComponent* SkyLight = It->GetLightComponent();
			if (!SkyLight)
			{
				continue;
			}

			if (Parameter == TEXT("SkyLight.Intensity")) { return SkyLight->Intensity; }
			if (Parameter == TEXT("SkyLight.Volumetric")) { return SkyLight->VolumetricScatteringIntensity; }
			if (Parameter.StartsWith(TEXT("SkyLight.Color."))) { return BHGetColorComponent(SkyLight->GetLightColor(), SuffixAfter(TEXT("SkyLight.Color."))); }
			if (Parameter.StartsWith(TEXT("SkyLight.Lower."))) { return BHGetColorComponent(SkyLight->LowerHemisphereColor, SuffixAfter(TEXT("SkyLight.Lower."))); }
		}
	}

	if (Parameter.StartsWith(TEXT("Post.")))
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			const FPostProcessSettings& Settings = It->Settings;
			if (Parameter == TEXT("Post.Exposure")) { return Settings.AutoExposureMinBrightness; }
			if (Parameter == TEXT("Post.ExposureBias")) { return Settings.AutoExposureBias; }
			if (Parameter == TEXT("Post.Vignette")) { return Settings.VignetteIntensity; }
			if (Parameter == TEXT("Post.FilmGrain")) { return Settings.FilmGrainIntensity; }
			if (Parameter.StartsWith(TEXT("Post.Saturation."))) { return BHGetVectorComponent(Settings.ColorSaturation, SuffixAfter(TEXT("Post.Saturation."))); }
			if (Parameter.StartsWith(TEXT("Post.Contrast."))) { return BHGetVectorComponent(Settings.ColorContrast, SuffixAfter(TEXT("Post.Contrast."))); }
			if (Parameter.StartsWith(TEXT("Post.SceneTint."))) { return BHGetColorComponent(Settings.SceneColorTint, SuffixAfter(TEXT("Post.SceneTint."))); }
			if (Parameter == TEXT("Post.IndirectIntensity")) { return Settings.IndirectLightingIntensity; }
			if (Parameter.StartsWith(TEXT("Post.Indirect."))) { return BHGetColorComponent(Settings.IndirectLightingColor, SuffixAfter(TEXT("Post.Indirect."))); }
		}
	}

	if (Parameter.StartsWith(TEXT("Flashlight.")))
	{
		if (const ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
		{
			return ControlledCharacter->GetFlashlightTuningValue(ParameterName);
		}
		for (TActorIterator<ABHCharacter> It(World); It; ++It)
		{
			return It->GetFlashlightTuningValue(ParameterName);
		}
	}

	return DefaultValue;
}

void ABHPlayerController::SetAtmosphereConsoleValue(FName ParameterName, float Value)
{
	if (!IsLocalHostAdminContext())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString Parameter = ParameterName.ToString();
	const auto SuffixAfter = [&Parameter](const TCHAR* Prefix) -> FString
	{
		const FString PrefixString(Prefix);
		return Parameter.StartsWith(PrefixString) ? Parameter.RightChop(PrefixString.Len()) : FString();
	};

	if (Parameter.StartsWith(TEXT("Fog.")) || Parameter.StartsWith(TEXT("Volumetric.")))
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			UExponentialHeightFogComponent* Fog = It->GetComponent();
			if (!Fog)
			{
				continue;
			}

			if (Parameter == TEXT("Fog.Density")) { Fog->SetFogDensity(Value); }
			else if (Parameter == TEXT("Fog.HeightFalloff")) { Fog->SetFogHeightFalloff(Value); }
			else if (Parameter == TEXT("Fog.MaxOpacity")) { Fog->SetFogMaxOpacity(Value); }
			else if (Parameter == TEXT("Fog.StartDistance")) { Fog->SetStartDistance(Value); }
			else if (Parameter == TEXT("Fog.EndDistance")) { Fog->SetEndDistance(Value); }
			else if (Parameter == TEXT("Fog.DirectionalExponent")) { Fog->SetDirectionalInscatteringExponent(Value); }
			else if (Parameter == TEXT("Fog.DirectionalStart")) { Fog->SetDirectionalInscatteringStartDistance(Value); }
			else if (Parameter.StartsWith(TEXT("Fog.Inscatter."))) { Fog->SetFogInscatteringColor(BHSetColorComponent(Fog->FogInscatteringLuminance, SuffixAfter(TEXT("Fog.Inscatter.")), Value)); }
			else if (Parameter.StartsWith(TEXT("Fog.Directional."))) { Fog->SetDirectionalInscatteringColor(BHSetColorComponent(Fog->DirectionalInscatteringLuminance, SuffixAfter(TEXT("Fog.Directional.")), Value)); }
			else if (Parameter == TEXT("Volumetric.Extinction")) { Fog->SetVolumetricFogExtinctionScale(Value); }
			else if (Parameter == TEXT("Volumetric.Distance")) { Fog->SetVolumetricFogDistance(Value); }
			else if (Parameter == TEXT("Volumetric.Start")) { Fog->SetVolumetricFogStartDistance(Value); }
			else if (Parameter == TEXT("Volumetric.NearFade")) { Fog->SetVolumetricFogNearFadeInDistance(Value); }
			else if (Parameter == TEXT("Volumetric.Scattering")) { Fog->SetVolumetricFogScatteringDistribution(Value); }
			else if (Parameter.StartsWith(TEXT("Volumetric.Albedo.")))
			{
				const FLinearColor Color = BHSetColorComponent(FLinearColor(Fog->VolumetricFogAlbedo), SuffixAfter(TEXT("Volumetric.Albedo.")), Value);
				Fog->SetVolumetricFogAlbedo(Color.ToFColor(false));
			}
			else if (Parameter.StartsWith(TEXT("Volumetric.Emissive."))) { Fog->SetVolumetricFogEmissive(BHSetColorComponent(Fog->VolumetricFogEmissive, SuffixAfter(TEXT("Volumetric.Emissive.")), Value)); }
		}
	}
	else if (Parameter.StartsWith(TEXT("LocalFog.")))
	{
		for (TActorIterator<ALocalFogVolume> It(World); It; ++It)
		{
			ULocalFogVolumeComponent* Fog = It->GetComponent();
			if (!Fog)
			{
				continue;
			}

			if (Parameter == TEXT("LocalFog.Radial")) { Fog->SetRadialFogExtinction(Value); }
			else if (Parameter == TEXT("LocalFog.Height")) { Fog->SetHeightFogExtinction(Value); }
			else if (Parameter == TEXT("LocalFog.Falloff")) { Fog->SetHeightFogFalloff(Value); }
			else if (Parameter == TEXT("LocalFog.Offset")) { Fog->SetHeightFogOffset(Value); }
			else if (Parameter == TEXT("LocalFog.Phase")) { Fog->SetFogPhaseG(Value); }
			else if (Parameter.StartsWith(TEXT("LocalFog.Albedo."))) { Fog->SetFogAlbedo(BHSetColorComponent(Fog->FogAlbedo, SuffixAfter(TEXT("LocalFog.Albedo.")), Value)); }
			else if (Parameter.StartsWith(TEXT("LocalFog.Emissive."))) { Fog->SetFogEmissive(BHSetColorComponent(Fog->FogEmissive, SuffixAfter(TEXT("LocalFog.Emissive.")), Value)); }
		}
	}
	else if (Parameter.StartsWith(TEXT("Sky.")))
	{
		for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
		{
			USkyAtmosphereComponent* Sky = It->GetComponent();
			if (!Sky)
			{
				continue;
			}

			if (Parameter.StartsWith(TEXT("Sky.Luminance."))) { Sky->SetSkyLuminanceFactor(BHSetColorComponent(Sky->SkyLuminanceFactor, SuffixAfter(TEXT("Sky.Luminance.")), Value)); }
			else if (Parameter.StartsWith(TEXT("Sky.Aerial."))) { Sky->SetSkyAndAerialPerspectiveLuminanceFactor(BHSetColorComponent(Sky->SkyAndAerialPerspectiveLuminanceFactor, SuffixAfter(TEXT("Sky.Aerial.")), Value)); }
			else if (Parameter == TEXT("Sky.MultiScattering")) { Sky->SetMultiScatteringFactor(Value); }
			else if (Parameter == TEXT("Sky.HeightFogContribution")) { Sky->SetHeightFogContribution(Value); }
			else if (Parameter == TEXT("Sky.AerialDistance")) { Sky->SetAerialPespectiveViewDistanceScale(Value); }
			else if (Parameter.StartsWith(TEXT("Sky.Ground.")))
			{
				const FLinearColor Color = BHSetColorComponent(FLinearColor(Sky->GroundAlbedo), SuffixAfter(TEXT("Sky.Ground.")), Value);
				Sky->SetGroundAlbedo(Color.ToFColor(false));
			}
			else if (Parameter == TEXT("Sky.RayleighScale")) { Sky->SetRayleighScatteringScale(Value); }
			else if (Parameter == TEXT("Sky.MieScale")) { Sky->SetMieScatteringScale(Value); }
			else if (Parameter == TEXT("Sky.MieAbsorptionScale")) { Sky->SetMieAbsorptionScale(Value); }
			else if (Parameter == TEXT("Sky.MieAnisotropy")) { Sky->SetMieAnisotropy(Value); }
			else if (Parameter == TEXT("Sky.AbsorptionScale")) { Sky->SetOtherAbsorptionScale(Value); }
		}
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			if (USkyLightComponent* SkyLight = It->GetLightComponent())
			{
				SkyLight->SetCaptureIsDirty();
			}
		}
	}
	else if (Parameter.StartsWith(TEXT("Moon.")))
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (!Light || !Light->bAtmosphereSunLight)
			{
				continue;
			}

			if (Parameter == TEXT("Moon.Intensity")) { Light->SetIntensity(Value); }
			else if (Parameter == TEXT("Moon.Indirect")) { Light->SetIndirectLightingIntensity(Value); }
			else if (Parameter == TEXT("Moon.Volumetric")) { Light->SetVolumetricScatteringIntensity(Value); }
			else if (Parameter == TEXT("Moon.Pitch"))
			{
				FRotator Rotation = It->GetActorRotation();
				Rotation.Pitch = Value;
				It->SetActorRotation(Rotation);
			}
			else if (Parameter == TEXT("Moon.Yaw"))
			{
				FRotator Rotation = It->GetActorRotation();
				Rotation.Yaw = Value;
				It->SetActorRotation(Rotation);
			}
			else if (Parameter.StartsWith(TEXT("Moon.Color."))) { Light->SetLightColor(BHSetColorComponent(Light->GetLightColor(), SuffixAfter(TEXT("Moon.Color.")), Value), false); }
			else if (Parameter.StartsWith(TEXT("Moon.Disk."))) { Light->SetAtmosphereSunDiskColorScale(BHSetColorComponent(Light->AtmosphereSunDiskColorScale, SuffixAfter(TEXT("Moon.Disk.")), Value)); }
		}
	}
	else if (Parameter.StartsWith(TEXT("SkyLight.")))
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			USkyLightComponent* SkyLight = It->GetLightComponent();
			if (!SkyLight)
			{
				continue;
			}

			if (Parameter == TEXT("SkyLight.Intensity")) { SkyLight->SetIntensity(Value); }
			else if (Parameter == TEXT("SkyLight.Volumetric")) { SkyLight->SetVolumetricScatteringIntensity(Value); }
			else if (Parameter.StartsWith(TEXT("SkyLight.Color."))) { SkyLight->SetLightColor(BHSetColorComponent(SkyLight->GetLightColor(), SuffixAfter(TEXT("SkyLight.Color.")), Value)); }
			else if (Parameter.StartsWith(TEXT("SkyLight.Lower."))) { SkyLight->SetLowerHemisphereColor(BHSetColorComponent(SkyLight->LowerHemisphereColor, SuffixAfter(TEXT("SkyLight.Lower.")), Value)); }
			SkyLight->SetCaptureIsDirty();
		}
	}
	else if (Parameter.StartsWith(TEXT("Post.")))
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			FPostProcessSettings& Settings = It->Settings;
			if (Parameter == TEXT("Post.Exposure"))
			{
				Settings.bOverride_AutoExposureMinBrightness = true;
				Settings.bOverride_AutoExposureMaxBrightness = true;
				Settings.AutoExposureMinBrightness = Value;
				Settings.AutoExposureMaxBrightness = Value;
			}
			else if (Parameter == TEXT("Post.ExposureBias"))
			{
				Settings.bOverride_AutoExposureBias = true;
				Settings.AutoExposureBias = Value;
			}
			else if (Parameter == TEXT("Post.Vignette"))
			{
				Settings.bOverride_VignetteIntensity = true;
				Settings.VignetteIntensity = Value;
			}
			else if (Parameter == TEXT("Post.FilmGrain"))
			{
				Settings.bOverride_FilmGrainIntensity = true;
				Settings.FilmGrainIntensity = Value;
			}
			else if (Parameter.StartsWith(TEXT("Post.Saturation.")))
			{
				Settings.bOverride_ColorSaturation = true;
				Settings.ColorSaturation = BHSetVectorComponent(Settings.ColorSaturation, SuffixAfter(TEXT("Post.Saturation.")), Value);
			}
			else if (Parameter.StartsWith(TEXT("Post.Contrast.")))
			{
				Settings.bOverride_ColorContrast = true;
				Settings.ColorContrast = BHSetVectorComponent(Settings.ColorContrast, SuffixAfter(TEXT("Post.Contrast.")), Value);
			}
			else if (Parameter.StartsWith(TEXT("Post.SceneTint.")))
			{
				Settings.bOverride_SceneColorTint = true;
				Settings.SceneColorTint = BHSetColorComponent(Settings.SceneColorTint, SuffixAfter(TEXT("Post.SceneTint.")), Value);
			}
			else if (Parameter == TEXT("Post.IndirectIntensity"))
			{
				Settings.bOverride_IndirectLightingIntensity = true;
				Settings.IndirectLightingIntensity = Value;
			}
			else if (Parameter.StartsWith(TEXT("Post.Indirect.")))
			{
				Settings.bOverride_IndirectLightingColor = true;
				Settings.IndirectLightingColor = BHSetColorComponent(Settings.IndirectLightingColor, SuffixAfter(TEXT("Post.Indirect.")), Value);
			}
		}
	}
	else if (Parameter.StartsWith(TEXT("Flashlight.")))
	{
		for (TActorIterator<ABHCharacter> It(World); It; ++It)
		{
			It->SetFlashlightTuningValue(ParameterName, Value);
		}
	}
}

void ABHPlayerController::ResetAtmosphereConsoleForMenu()
{
	if (!IsLocalHostAdminContext())
	{
		return;
	}

	if (!bAtmosphereConsoleDefaultsCaptured || AtmosphereConsoleDefaultValues.IsEmpty())
	{
		CaptureAtmosphereConsoleDefaults();
	}

	for (const TPair<FName, float>& DefaultValue : AtmosphereConsoleDefaultValues)
	{
		SetAtmosphereConsoleValue(DefaultValue.Key, DefaultValue.Value);
	}
	ShowLocalStatusMessage(TEXT("Atmosphere console reset to its opening snapshot."), 2.0f);
}

void ABHPlayerController::ApplyPlayableAtmosphereConsoleForMenu()
{
	if (!IsLocalHostAdminContext())
	{
		return;
	}

	const TPair<const TCHAR*, float> Values[] = {
		{TEXT("Fog.Density"), 0.180f},
		{TEXT("Fog.HeightFalloff"), 0.050f},
		{TEXT("Fog.MaxOpacity"), 0.58f},
		{TEXT("Fog.StartDistance"), 110.0f},
		{TEXT("Fog.EndDistance"), 7600.0f},
		{TEXT("Fog.Inscatter.R"), 0.26f},
		{TEXT("Fog.Inscatter.G"), 0.36f},
		{TEXT("Fog.Inscatter.B"), 0.39f},
		{TEXT("Fog.DirectionalExponent"), 8.0f},
		{TEXT("Fog.DirectionalStart"), 0.0f},
		{TEXT("Fog.Directional.R"), 0.36f},
		{TEXT("Fog.Directional.G"), 0.50f},
		{TEXT("Fog.Directional.B"), 0.56f},
		{TEXT("Volumetric.Extinction"), 1.15f},
		{TEXT("Volumetric.Distance"), 7800.0f},
		{TEXT("Volumetric.Start"), 120.0f},
		{TEXT("Volumetric.NearFade"), 220.0f},
		{TEXT("Volumetric.Scattering"), 0.18f},
		{TEXT("Volumetric.Albedo.R"), 0.58f},
		{TEXT("Volumetric.Albedo.G"), 0.68f},
		{TEXT("Volumetric.Albedo.B"), 0.70f},
		{TEXT("Volumetric.Emissive.R"), 0.032f},
		{TEXT("Volumetric.Emissive.G"), 0.044f},
		{TEXT("Volumetric.Emissive.B"), 0.048f},
		{TEXT("LocalFog.Radial"), 1.15f},
		{TEXT("LocalFog.Height"), 0.60f},
		{TEXT("LocalFog.Falloff"), 2600.0f},
		{TEXT("LocalFog.Offset"), -0.34f},
		{TEXT("LocalFog.Phase"), 0.18f},
		{TEXT("LocalFog.Albedo.R"), 0.64f},
		{TEXT("LocalFog.Albedo.G"), 0.74f},
		{TEXT("LocalFog.Albedo.B"), 0.76f},
		{TEXT("LocalFog.Emissive.R"), 0.026f},
		{TEXT("LocalFog.Emissive.G"), 0.036f},
		{TEXT("LocalFog.Emissive.B"), 0.040f},
		{TEXT("Sky.Luminance.R"), 0.86f},
		{TEXT("Sky.Luminance.G"), 1.02f},
		{TEXT("Sky.Luminance.B"), 1.24f},
		{TEXT("Sky.Aerial.R"), 0.80f},
		{TEXT("Sky.Aerial.G"), 0.94f},
		{TEXT("Sky.Aerial.B"), 1.12f},
		{TEXT("Sky.MultiScattering"), 0.78f},
		{TEXT("Sky.HeightFogContribution"), 0.16f},
		{TEXT("Sky.AerialDistance"), 0.34f},
		{TEXT("Moon.Intensity"), 1.25f},
		{TEXT("Moon.Indirect"), 1.00f},
		{TEXT("Moon.Volumetric"), 0.10f},
		{TEXT("SkyLight.Intensity"), 1.00f},
		{TEXT("SkyLight.Volumetric"), 0.10f},
		{TEXT("SkyLight.Lower.R"), 0.12f},
		{TEXT("SkyLight.Lower.G"), 0.15f},
		{TEXT("SkyLight.Lower.B"), 0.17f},
		{TEXT("Post.Exposure"), 1.20f},
		{TEXT("Post.ExposureBias"), 0.34f},
		{TEXT("Post.Vignette"), 0.34f},
		{TEXT("Post.FilmGrain"), 0.045f},
		{TEXT("Post.Saturation.R"), 1.00f},
		{TEXT("Post.Saturation.G"), 1.03f},
		{TEXT("Post.Saturation.B"), 1.03f},
		{TEXT("Post.Contrast.R"), 0.96f},
		{TEXT("Post.Contrast.G"), 0.97f},
		{TEXT("Post.Contrast.B"), 0.97f},
		{TEXT("Post.SceneTint.R"), 1.08f},
		{TEXT("Post.SceneTint.G"), 1.14f},
		{TEXT("Post.SceneTint.B"), 1.18f},
		{TEXT("Post.IndirectIntensity"), 1.00f},
		{TEXT("Post.Indirect.R"), 0.48f},
		{TEXT("Post.Indirect.G"), 0.62f},
		{TEXT("Post.Indirect.B"), 0.78f},
		{TEXT("Flashlight.IntensityScale"), 1.00f},
		{TEXT("Flashlight.RadiusScale"), 1.00f},
		{TEXT("Flashlight.VolumetricScale"), 0.65f},
		{TEXT("Flashlight.BeamLengthScale"), 0.80f},
		{TEXT("Flashlight.BeamOpacityScale"), 0.72f},
		{TEXT("Flashlight.BeamBrightnessScale"), 0.82f},
		{TEXT("Flashlight.ConeScale"), 0.90f},
		{TEXT("Flashlight.RayVisibilityScale"), 1.35f},
		{TEXT("Flashlight.RayWidthScale"), 1.12f},
		{TEXT("Flashlight.RayStartOffset"), 58.0f}
	};

	for (const TPair<const TCHAR*, float>& Pair : Values)
	{
		SetAtmosphereConsoleValue(FName(Pair.Key), Pair.Value);
	}
	ShowLocalStatusMessage(TEXT("Playable fog tuning applied."), 2.0f);
}

void ABHPlayerController::SaveAtmosphereConsoleProfileForMenu()
{
	if (!IsLocalHostAdminContext() || !GConfig)
	{
		return;
	}

	int32 SavedCount = 0;
	BHForEachAtmosphereSliderSpec([this, &SavedCount](const FBHAtmosphereSliderSpec& Spec)
	{
		const FName ParameterName(Spec.ParameterName);
		GConfig->SetFloat(BHAtmosphereProfileConfigSection, Spec.ParameterName, GetAtmosphereConsoleValue(ParameterName), GGameUserSettingsIni);
		++SavedCount;
	});

	GConfig->SetBool(BHAtmosphereProfileConfigSection, TEXT("Enabled"), true, GGameUserSettingsIni);
	GConfig->SetString(BHAtmosphereProfileConfigSection, TEXT("SavedAtUtc"), *FDateTime::UtcNow().ToIso8601(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	ShowLocalStatusMessage(FString::Printf(TEXT("Saved Foggrounds atmosphere profile (%d sliders)."), SavedCount), 2.5f);
}

void ABHPlayerController::LoadAtmosphereConsoleProfileForMenu()
{
	ApplySavedAtmosphereConsoleProfile(true);
}

void ABHPlayerController::ApplySavedAtmosphereConsoleProfileFromTimer()
{
	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const bool bFoggrounds = BHGS && BHGS->ActiveLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	if (bFoggrounds)
	{
		ApplySavedAtmosphereConsoleProfile(false);
	}
}

bool ABHPlayerController::ApplySavedAtmosphereConsoleProfile(bool bShowStatus)
{
	if (!IsLocalHostAdminContext() || !GConfig)
	{
		return false;
	}

	bool bEnabled = false;
	if (!GConfig->GetBool(BHAtmosphereProfileConfigSection, TEXT("Enabled"), bEnabled, GGameUserSettingsIni) || !bEnabled)
	{
		if (bShowStatus)
		{
			ShowLocalStatusMessage(TEXT("No saved Foggrounds atmosphere profile found."), 2.5f);
		}
		return false;
	}

	int32 LoadedCount = 0;
	BHForEachAtmosphereSliderSpec([this, &LoadedCount](const FBHAtmosphereSliderSpec& Spec)
	{
		float SavedValue = 0.0f;
		if (GConfig->GetFloat(BHAtmosphereProfileConfigSection, Spec.ParameterName, SavedValue, GGameUserSettingsIni))
		{
			SetAtmosphereConsoleValue(FName(Spec.ParameterName), SavedValue);
			++LoadedCount;
		}
	});

	if (bShowStatus)
	{
		ShowLocalStatusMessage(
			LoadedCount > 0
				? FString::Printf(TEXT("Loaded Foggrounds atmosphere profile (%d sliders)."), LoadedCount)
				: FString(TEXT("Saved Foggrounds profile exists, but contains no slider values.")),
			2.5f);
	}
	return LoadedCount > 0;
}

FString ABHPlayerController::GetAtmosphereConsoleSummaryForMenu() const
{
	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const FString LevelName = BHGS && !BHGS->ActiveLevelName.IsEmpty() ? BHGS->ActiveLevelName : TEXT("current map");
	const FString NetMode = World
		? (World->GetNetMode() == NM_ListenServer ? TEXT("host") : (World->GetNetMode() == NM_Standalone ? TEXT("standalone") : TEXT("client")))
		: TEXT("no world");
	return FString::Printf(TEXT("%s | %s | local visual tuning"), *LevelName, *NetMode);
}

bool ABHPlayerController::TriggerTargetedJumpscareForMenu(APlayerState* TargetPlayerState, FString& OutMessage)
{
	HideMainMenu();
	ServerTriggerTargetedJumpscare(TargetPlayerState);
	OutMessage = TEXT("");
	return true;
}

bool ABHPlayerController::CycleAvatarForMenu(FString& OutMessage)
{
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	const int32 CurrentAvatar = BHPS ? BHPS->AvatarIndex : 0;
	const UBHAccountSubsystem* ReadOnlyAccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	const int32 XP = ReadOnlyAccountSubsystem ? ReadOnlyAccountSubsystem->GetProgress().XP : TNumericLimits<int32>::Max();
	const int32 NextAvatar = BHCosmeticNextUnlockedIndex(EBHCosmeticCategory::Outfit, CurrentAvatar, XP, ReadOnlyAccountSubsystem ? &ReadOnlyAccountSubsystem->GetProgress().UnlockedAchievements : nullptr);
	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		if (!AccountSubsystem->SetSelectedCosmetic(EBHCosmeticCategory::Outfit, NextAvatar, OutMessage))
		{
			ShowLocalStatusMessage(OutMessage, 4.0f);
			return false;
		}
	}
	PushLocalCosmeticsToServer(false);
	OutMessage = FString::Printf(TEXT("Outfit saved: %s."), BHCosmeticItemName(EBHCosmeticCategory::Outfit, NextAvatar));
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarForMenu(int32 AvatarIndex, FString& OutMessage)
{
	const int32 NormalizedIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Outfit, AvatarIndex);
	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		if (!AccountSubsystem->SetSelectedCosmetic(EBHCosmeticCategory::Outfit, NormalizedIndex, OutMessage))
		{
			ShowLocalStatusMessage(OutMessage, 4.0f);
			return false;
		}
	}
	PushLocalCosmeticsToServer(false);
	OutMessage = FString::Printf(TEXT("Outfit saved: %s."), BHCosmeticItemName(EBHCosmeticCategory::Outfit, NormalizedIndex));
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarColorForMenu(int32 ColorIndex, FString& OutMessage)
{
	const int32 NormalizedIndex = BHCosmeticClampIndex(EBHCosmeticCategory::ShirtColor, ColorIndex);
	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		if (!AccountSubsystem->SetSelectedCosmetic(EBHCosmeticCategory::ShirtColor, NormalizedIndex, OutMessage))
		{
			ShowLocalStatusMessage(OutMessage, 4.0f);
			return false;
		}
	}
	PushLocalCosmeticsToServer(false);
	OutMessage = FString::Printf(TEXT("Shirt saved: %s."), BHCosmeticItemName(EBHCosmeticCategory::ShirtColor, NormalizedIndex));
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarHeadwearForMenu(int32 HeadwearIndex, FString& OutMessage)
{
	const int32 NormalizedIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, HeadwearIndex);
	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		if (!AccountSubsystem->SetSelectedCosmetic(EBHCosmeticCategory::Headwear, NormalizedIndex, OutMessage))
		{
			ShowLocalStatusMessage(OutMessage, 4.0f);
			return false;
		}
	}
	PushLocalCosmeticsToServer(false);
	OutMessage = FString::Printf(TEXT("Headwear saved: %s."), BHCosmeticItemName(EBHCosmeticCategory::Headwear, NormalizedIndex));
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarGearForMenu(int32 GearIndex, FString& OutMessage)
{
	(void)GearIndex;
	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		if (!AccountSubsystem->SetSelectedCosmetic(EBHCosmeticCategory::Gear, 0, OutMessage))
		{
			ShowLocalStatusMessage(OutMessage, 4.0f);
			return false;
		}
	}
	PushLocalCosmeticsToServer(false);
	OutMessage = TEXT("Gear customization has been removed.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetPhysicsTopicsForMenu(const FString& TopicList, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("change classroom question topics")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before changing question focus.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	ServerSetPhysicsTopics(TopicList);
	OutMessage = TEXT("Question focus request sent.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetRevisionDifficultyMixForMenu(EBHRevisionDifficultyMix DifficultyMix, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("change classroom question complexity")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before changing question complexity.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	const FString DifficultyText = BHRevisionDifficultyMixToString(DifficultyMix);
	ServerSetRevisionDifficultyMix(DifficultyText);
	OutMessage = FString::Printf(TEXT("Question complexity request sent: %s."), *DifficultyText);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetRevisionThresholdsForMenu(float ClassPercent, float IndividualPercent, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("change classroom mastery targets")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before changing mastery targets.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	const float ClampedClassPercent = FMath::Clamp(ClassPercent, 0.0f, 100.0f);
	const float ClampedIndividualPercent = FMath::Clamp(IndividualPercent, 0.0f, 100.0f);
	ServerSetRevisionThresholds(ClampedClassPercent, ClampedIndividualPercent);
	OutMessage = FString::Printf(TEXT("Mastery targets request sent: class %.0f%%, individual %.0f%%."), ClampedClassPercent, ClampedIndividualPercent);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetRevisionRoundDurationForMenu(int32 RoundSeconds, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("change classroom round duration")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before changing round duration.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	const int32 ClampedRoundSeconds = FMath::Clamp(RoundSeconds, 60, 3600);
	ServerSetRevisionRoundDuration(ClampedRoundSeconds);
	OutMessage = FString::Printf(TEXT("Round duration request sent: %d seconds."), ClampedRoundSeconds);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetScareIntensityForMenu(int32 Intensity, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("change classroom scare intensity")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before changing scare intensity.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	const int32 ClampedIntensity = FMath::Clamp(Intensity, 0, 3);
	ServerSetScareIntensity(ClampedIntensity);
	OutMessage = FString::Printf(TEXT("Scare intensity request sent: %d."), ClampedIntensity);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ForceReviewForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("force a classroom review")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before forcing review.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	ServerForceReview();
	OutMessage = TEXT("Classroom review request sent.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::RevisionStatusForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("view classroom revision status")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before viewing revision status.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	ServerRevisionStatus();
	OutMessage = TEXT("Revision status requested.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ExportRevisionReportForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("export classroom performance data")))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before exporting performance data.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	ServerExportRevisionReport();
	OutMessage = TEXT("Classroom report export requested.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SendSpectatorEncouragementForMenu(FString& OutMessage)
{
	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHGS || !BHPS || BHPS->PlayerRole != EBHPlayerRole::Spectator)
	{
		OutMessage = TEXT("Spectator support is only available to late-join spectators.");
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	if (BHGS->RoundPhase != EBHRoundPhase::Hunt && BHGS->RoundPhase != EBHRoundPhase::FinalEscape)
	{
		OutMessage = TEXT("Spectator support opens during the Hunt and final escape.");
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	ServerRequestSpectatorEncouragement();
	OutMessage = TEXT("Spectator support request sent.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::QueueSpectatorRolePreferenceForMenu(EBHPlayerRole DesiredRole, FString& OutMessage)
{
	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHGS || !BHPS || BHPS->PlayerRole != EBHPlayerRole::Spectator)
	{
		OutMessage = TEXT("Only late-join spectators can request a next-round role.");
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Lobby)
	{
		OutMessage = TEXT("Use the lobby roster for role assignment now.");
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	ServerSetSpectatorRolePreference(DesiredRole);
	OutMessage = TEXT("Next-round role preference sent to the host roster.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ExportPlaytestTelemetryForMenu(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("export playtest telemetry")))
	{
		return false;
	}

	ServerExportPlaytestTelemetry();
	OutMessage = TEXT("Playtest telemetry export requested.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::GetLessonPresetsForMenu(TArray<FBHLessonPreset>& OutPresets, FString& OutMessage) const
{
	return FBHLessonPresetStore::LoadAllPresets(OutPresets, OutMessage);
}

bool ABHPlayerController::GetSelectedLessonPresetForMenu(FBHLessonPreset& OutPreset, FString& OutMessage) const
{
	return FBHLessonPresetStore::TryFindPreset(FBHLessonPresetStore::GetSelectedPresetId(), OutPreset, OutMessage);
}

FString ABHPlayerController::GetSelectedLessonPresetIdForMenu() const
{
	return FBHLessonPresetStore::GetSelectedPresetId();
}

FString ABHPlayerController::GetLessonPresetStoragePathForMenu() const
{
	return FBHLessonPresetStore::GetStoragePath();
}

bool ABHPlayerController::ApplyLessonPresetForMenu(const FString& PresetId, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("apply lesson presets")))
	{
		return false;
	}

	FBHLessonPreset Preset;
	FString LoadMessage;
	const bool bExactPresetFound = FBHLessonPresetStore::TryFindPreset(PresetId, Preset, LoadMessage);
	if (!bExactPresetFound && !PresetId.IsEmpty())
	{
		OutMessage = LoadMessage;
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	bool bAdjusted = false;
	Preset = FBHLessonPresetStore::ValidatePreset(Preset, &bAdjusted);
	FBHLessonPresetStore::SetSelectedPresetId(Preset.Id);

	EnsureComfortPreferencesLoaded();
	bReducedJumpscares = Preset.bReducedJumpscares;
	bReducedFlash = Preset.bReducedFlash;
	bReducedCameraShake = Preset.bReducedCameraShake;
	bCaptionsEnabled = Preset.bCaptions;
	bHighContrastHud = Preset.bHighContrastHud;
	SaveComfortPreference(TEXT("ReducedJumpscares"), bReducedJumpscares);
	SaveComfortPreference(TEXT("ReducedFlash"), bReducedFlash);
	SaveComfortPreference(TEXT("ReducedCameraShake"), bReducedCameraShake);
	SaveComfortPreference(TEXT("Captions"), bCaptionsEnabled);
	SaveComfortPreference(TEXT("HighContrastHud"), bHighContrastHud);

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	const bool bCanApplyLive = World
		&& World->GetNetMode() == NM_ListenServer
		&& BHGS
		&& BHGS->bRevisionMode
		&& BHGS->RoundPhase == EBHRoundPhase::Lobby;
	if (bCanApplyLive)
	{
		ABHGameMode* BHGM = World ? World->GetAuthGameMode<ABHGameMode>() : nullptr;
		if (!BHGM || !BHGM->ApplyLessonPreset(this, Preset, OutMessage))
		{
			if (OutMessage.IsEmpty())
			{
				OutMessage = TEXT("Could not apply the lesson preset to this lobby.");
			}
			ShowLocalStatusMessage(OutMessage, 4.0f);
			return false;
		}
	}
	else
	{
		OutMessage = BHGS && BHGS->bRevisionMode && BHGS->RoundPhase != EBHRoundPhase::Lobby
			? FString::Printf(TEXT("Selected lesson preset '%s' for the next Live Classroom. Active rounds keep their current settings."), *Preset.DisplayName)
			: FString::Printf(TEXT("Selected lesson preset '%s' for the next Live Classroom."), *Preset.DisplayName);
	}

	if (bAdjusted && !bCanApplyLive)
	{
		OutMessage += TEXT(" Stale values were clamped to current limits.");
	}
	ShowLocalStatusMessage(OutMessage, 4.0f);
	return true;
}

bool ABHPlayerController::SaveCurrentLessonPresetForMenu(const FString& DisplayName, const FString& SelectedMapName, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("save lesson presets")))
	{
		return false;
	}

	FBHLessonPreset Preset = BuildCurrentLessonPresetSnapshot(DisplayName, SelectedMapName);
	const bool bSaved = FBHLessonPresetStore::SaveCustomPreset(Preset, OutMessage);
	ShowLocalStatusMessage(OutMessage, bSaved ? 4.0f : 5.0f);
	return bSaved;
}

bool ABHPlayerController::GenerateManualQuestionSetForMenu(int32 QuestionCount, FString& OutPath, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("generate manual question sets")))
	{
		return false;
	}

	FBHLessonPreset Preset;
	FString LoadMessage;
	const bool bFoundPreset = FBHLessonPresetStore::TryFindPreset(FBHLessonPresetStore::GetSelectedPresetId(), Preset, LoadMessage);
	const bool bSaved = FBHLessonPresetStore::SaveManualQuestionSet(Preset, QuestionCount, OutPath, OutMessage);
	if (bSaved && !bFoundPreset && !LoadMessage.IsEmpty())
	{
		OutMessage = LoadMessage + TEXT(" ") + OutMessage;
	}
	ShowLocalStatusMessage(OutMessage, bSaved ? 5.0f : 6.0f);
	return bSaved;
}

void ABHPlayerController::EnsureGraphicsPreferencesLoaded()
{
	if (bGraphicsPreferencesLoaded)
	{
		return;
	}

	const FBHGraphicsHardwareProfile HardwareProfile = BHScanGraphicsHardwareProfile();
	GraphicsGpuBrand = HardwareProfile.GpuBrand;
	GraphicsSystemMemoryGB = HardwareProfile.SystemMemoryGB;
	GraphicsDedicatedVideoMemoryGB = HardwareProfile.DedicatedVideoMemoryGB;
	GraphicsPhysicalCores = HardwareProfile.PhysicalCores;
	GraphicsLogicalCores = HardwareProfile.LogicalCores;
	GraphicsRecommendedPreset = HardwareProfile.RecommendedPreset;
	GraphicsRecommendedRenderScale = HardwareProfile.RecommendedRenderScale;
	GraphicsRecommendedFpsGoal = HardwareProfile.RecommendedFpsGoal;
	bGraphicsPreferSafeResolution = HardwareProfile.bPreferSafeResolution;
	GraphicsPresetQuality = HardwareProfile.RecommendedPreset;
	GraphicsRenderScalePercent = HardwareProfile.RecommendedRenderScale;
	GraphicsAdaptiveFpsGoal = HardwareProfile.RecommendedFpsGoal;
	GraphicsFrameRateLimit = HardwareProfile.RecommendedFpsGoal;
	bGraphicsLikelyIntegratedGpu = HardwareProfile.bLikelyIntegratedGpu;
	bGraphicsLikelySoftwareGpu = HardwareProfile.bLikelySoftwareGpu;

	if (GConfig)
	{
		GConfig->GetBool(BHGraphicsConfigSection, TEXT("AutoHardware"), bAutoHardwareGraphicsEnabled, GGameUserSettingsIni);
		GConfig->GetBool(BHGraphicsConfigSection, TEXT("AdaptiveEnabled"), bAdaptiveGraphicsEnabled, GGameUserSettingsIni);
		GConfig->GetInt(BHGraphicsConfigSection, TEXT("AdaptiveFpsGoal"), GraphicsAdaptiveFpsGoal, GGameUserSettingsIni);
		GConfig->GetBool(BHGraphicsConfigSection, TEXT("ResolutionOverride"), bGraphicsResolutionOverrideEnabled, GGameUserSettingsIni);
		GConfig->GetBool(BHGraphicsConfigSection, TEXT("ResolutionFullscreen"), bGraphicsFullscreen, GGameUserSettingsIni);
		const bool bLoadedResolutionWidth = GConfig->GetInt(BHGraphicsConfigSection, TEXT("ResolutionWidth"), GraphicsResolutionWidth, GGameUserSettingsIni);
		const bool bLoadedResolutionHeight = GConfig->GetInt(BHGraphicsConfigSection, TEXT("ResolutionHeight"), GraphicsResolutionHeight, GGameUserSettingsIni);
		const bool bLoadedUsableResolution = bLoadedResolutionWidth && bLoadedResolutionHeight && GraphicsResolutionWidth > 0 && GraphicsResolutionHeight > 0;
		bGraphicsHasSavedResolutionPreference = bLoadedUsableResolution;
		bGraphicsResolutionOverrideEnabled = bGraphicsResolutionOverrideEnabled || bLoadedUsableResolution;

		if (!bAutoHardwareGraphicsEnabled)
		{
			GConfig->GetInt(BHGraphicsConfigSection, TEXT("PresetQuality"), GraphicsPresetQuality, GGameUserSettingsIni);
			GConfig->GetInt(BHGraphicsConfigSection, TEXT("RenderScalePercent"), GraphicsRenderScalePercent, GGameUserSettingsIni);
			GConfig->GetInt(BHGraphicsConfigSection, TEXT("FrameRateLimit"), GraphicsFrameRateLimit, GGameUserSettingsIni);
			GConfig->GetInt(BHGraphicsConfigSection, TEXT("TextureQuality"), GraphicsTextureQuality, GGameUserSettingsIni);
			GConfig->GetInt(BHGraphicsConfigSection, TEXT("ShadowQuality"), GraphicsShadowQuality, GGameUserSettingsIni);
			GConfig->GetInt(BHGraphicsConfigSection, TEXT("EffectsQuality"), GraphicsEffectsQuality, GGameUserSettingsIni);
		}
	}

	GraphicsPresetQuality = BHClampGraphicsQuality(GraphicsPresetQuality);
	GraphicsRenderScalePercent = BHClampRenderScale(GraphicsRenderScalePercent);
	GraphicsAdaptiveFpsGoal = BHClampFpsGoal(GraphicsAdaptiveFpsGoal);
	GraphicsFrameRateLimit = GraphicsFrameRateLimit <= 0 ? 0 : FMath::Clamp(GraphicsFrameRateLimit, 30, 360);
	if (bAdaptiveGraphicsEnabled && GraphicsFrameRateLimit > 0 && GraphicsAdaptiveFpsGoal > GraphicsFrameRateLimit)
	{
		GraphicsAdaptiveFpsGoal = GraphicsFrameRateLimit;
	}
	GraphicsTextureQuality = BHClampGraphicsQuality(GraphicsTextureQuality);
	GraphicsShadowQuality = BHClampGraphicsQuality(GraphicsShadowQuality);
	GraphicsEffectsQuality = BHClampGraphicsQuality(GraphicsEffectsQuality);
	if (GraphicsResolutionWidth > 0 || GraphicsResolutionHeight > 0)
	{
		GraphicsResolutionWidth = FMath::Clamp(GraphicsResolutionWidth, 960, 7680);
		GraphicsResolutionHeight = FMath::Clamp(GraphicsResolutionHeight, 540, 4320);
	}
	bGraphicsResolutionOverrideEnabled = bGraphicsResolutionOverrideEnabled && GraphicsResolutionWidth > 0 && GraphicsResolutionHeight > 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	bGraphicsPreferencesLoaded = true;
}

void ABHPlayerController::SaveGraphicsPreference(const TCHAR* Key, int32 Value) const
{
	if (GConfig && Key)
	{
		GConfig->SetInt(BHGraphicsConfigSection, Key, Value, GGameUserSettingsIni);
	}
}

void ABHPlayerController::SaveGraphicsPreference(const TCHAR* Key, bool bValue) const
{
	if (GConfig && Key)
	{
		GConfig->SetBool(BHGraphicsConfigSection, Key, bValue, GGameUserSettingsIni);
	}
}

void ABHPlayerController::SaveGraphicsPreferences() const
{
	if (!GConfig)
	{
		return;
	}

	SaveGraphicsPreference(TEXT("AutoHardware"), bAutoHardwareGraphicsEnabled);
	SaveGraphicsPreference(TEXT("AdaptiveEnabled"), bAdaptiveGraphicsEnabled);
	SaveGraphicsPreference(TEXT("AdaptiveFpsGoal"), GraphicsAdaptiveFpsGoal);
	SaveGraphicsPreference(TEXT("PresetQuality"), GraphicsPresetQuality);
	SaveGraphicsPreference(TEXT("RenderScalePercent"), GraphicsRenderScalePercent);
	SaveGraphicsPreference(TEXT("FrameRateLimit"), GraphicsFrameRateLimit);
	SaveGraphicsPreference(TEXT("TextureQuality"), GraphicsTextureQuality);
	SaveGraphicsPreference(TEXT("ShadowQuality"), GraphicsShadowQuality);
	SaveGraphicsPreference(TEXT("EffectsQuality"), GraphicsEffectsQuality);
	const bool bSaveResolutionPreference = bGraphicsHasSavedResolutionPreference
		|| (!bGraphicsAutoSafeResolutionApplied && bGraphicsResolutionOverrideEnabled && GraphicsResolutionWidth > 0 && GraphicsResolutionHeight > 0);
	if (bSaveResolutionPreference)
	{
		SaveGraphicsPreference(TEXT("ResolutionOverride"), bGraphicsResolutionOverrideEnabled);
		SaveGraphicsPreference(TEXT("ResolutionWidth"), GraphicsResolutionWidth);
		SaveGraphicsPreference(TEXT("ResolutionHeight"), GraphicsResolutionHeight);
		SaveGraphicsPreference(TEXT("ResolutionFullscreen"), bGraphicsFullscreen);
	}
	else
	{
		GConfig->RemoveKey(BHGraphicsConfigSection, TEXT("ResolutionOverride"), GGameUserSettingsIni);
		GConfig->RemoveKey(BHGraphicsConfigSection, TEXT("ResolutionWidth"), GGameUserSettingsIni);
		GConfig->RemoveKey(BHGraphicsConfigSection, TEXT("ResolutionHeight"), GGameUserSettingsIni);
		GConfig->RemoveKey(BHGraphicsConfigSection, TEXT("ResolutionFullscreen"), GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ABHPlayerController::ApplyStartupGraphicsSettings()
{
	if (bGraphicsAppliedAtStartup || !IsLocalController())
	{
		return;
	}

	EnsureGraphicsPreferencesLoaded();
	bGraphicsAppliedAtStartup = true;
	bGraphicsAutoSafeResolutionApplied = false;

	FString Message;
	ApplyGraphicsPresetInternal(GraphicsPresetQuality, false, Message);
	if (bAutoHardwareGraphicsEnabled)
	{
		GraphicsRenderScalePercent = GraphicsRecommendedRenderScale;
		GraphicsFrameRateLimit = FMath::Max(GraphicsRecommendedFpsGoal, GraphicsAdaptiveFpsGoal);
	}
	ApplySavedManualGraphicsTuning();
	if (bAutoHardwareGraphicsEnabled
		&& bGraphicsPreferSafeResolution
		&& !bGraphicsHasSavedResolutionPreference
		&& !bGraphicsResolutionOverrideEnabled)
	{
		GraphicsResolutionWidth = 1280;
		GraphicsResolutionHeight = 720;
		bGraphicsFullscreen = false;
		bGraphicsResolutionOverrideEnabled = true;
		bGraphicsAutoSafeResolutionApplied = true;
	}
	ApplySavedGraphicsResolution();
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -3.0f;

	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt graphics startup: %s"), *GetGraphicsSummaryForMenu());
	if (bGraphicsAutoSafeResolutionApplied)
	{
		UE_LOG(LogTemp, Display, TEXT("BlackoutHunt graphics startup applied 1280x720 windowed safe resolution for low-spec hardware; selecting another resolution in Settings will persist the user's choice."));
	}
}

void ABHPlayerController::MaybeShowWeakDeviceNotice()
{
	// One native pop-up per process on genuinely low-spec machines. Graphics are already auto-set low by
	// EnsureGraphicsPreferencesLoaded(); this makes the "free up resources / don't host here" guidance
	// unmissable on the lab's 4GB boxes. Keyed on SYSTEM RAM (the real constraint here) so the capable
	// 16GB machines are never nagged. Memory presets don't shrink the CPU-side working set, so closing
	// other apps is the single biggest thing a 4GB student can do for a smooth round.
	static bool bWeakDeviceNoticeShown = false;
	if (bWeakDeviceNoticeShown || !IsLocalController())
	{
		return;
	}

	// Never block an automated / headless / dedicated run on a modal dialog (tests, soak, auto-host all
	// run unattended with NullRHI). FApp::CanEverRender() is false on a server with no RHI.
	if (FApp::IsUnattended() || IsRunningCommandlet() || !FApp::CanEverRender())
	{
		return;
	}

	const bool bLowSystemMemory = GraphicsSystemMemoryGB > 0.0f && GraphicsSystemMemoryGB <= 8.5f;
	if (!bLowSystemMemory && !bGraphicsLikelySoftwareGpu)
	{
		return;
	}

	bWeakDeviceNoticeShown = true;

	const FText Title = NSLOCTEXT("BlackoutHunt", "WeakDeviceNoticeTitle", "Blackout Hunt - this computer needs a little help");
	const FText Body = NSLOCTEXT("BlackoutHunt", "WeakDeviceNoticeBody",
		"This computer has limited memory, so graphics were turned down automatically (Low preset, 720p windowed) for a smoother game.\n\n"
		"For the best experience, before you play:\n\n"
		"  - Close all other apps first - especially web browsers (Chrome / Edge), YouTube or video, and Teams.\n"
		"  - Do NOT host the game on this computer - join the teacher's / host's address instead.\n"
		"  - If it still stutters, open Settings and keep graphics on \"Low 4GB\".\n\n"
		"You can change graphics any time in Settings. Click OK to continue.");
	FMessageDialog::Open(EAppMsgType::Ok, Body, Title);
}

void ABHPlayerController::ApplySavedManualGraphicsTuning()
{
	if (!IsLocalController())
	{
		return;
	}

	ConsoleCommand(FString::Printf(TEXT("sg.TextureQuality %d"), BHClampGraphicsQuality(GraphicsTextureQuality)));
	switch (BHClampGraphicsQuality(GraphicsTextureQuality))
	{
	case 0:
		ConsoleCommand(TEXT("r.Streaming.UseFixedPoolSize 1"));
		ConsoleCommand(TEXT("r.Streaming.PoolSize 384"));
		ConsoleCommand(TEXT("r.Streaming.MaxTempMemoryAllowed 64"));
		ConsoleCommand(TEXT("r.MipMapLODBias 1"));
		break;
	case 1:
		ConsoleCommand(TEXT("r.Streaming.UseFixedPoolSize 1"));
		ConsoleCommand(TEXT("r.Streaming.PoolSize 768"));
		ConsoleCommand(TEXT("r.Streaming.MaxTempMemoryAllowed 96"));
		ConsoleCommand(TEXT("r.MipMapLODBias 0"));
		break;
	case 2:
		ConsoleCommand(TEXT("r.Streaming.UseFixedPoolSize 1"));
		ConsoleCommand(TEXT("r.Streaming.PoolSize 2048"));
		ConsoleCommand(TEXT("r.Streaming.MaxTempMemoryAllowed 256"));
		ConsoleCommand(TEXT("r.MipMapLODBias 0"));
		break;
	default:
		ConsoleCommand(TEXT("r.Streaming.UseFixedPoolSize 1"));
		ConsoleCommand(TEXT("r.Streaming.PoolSize 4096"));
		ConsoleCommand(TEXT("r.Streaming.MaxTempMemoryAllowed 512"));
		ConsoleCommand(TEXT("r.MipMapLODBias 0"));
		break;
	}

	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -1.0f;
	ApplyAdaptiveGraphicsState(false);
}

void ABHPlayerController::ApplySavedGraphicsResolution()
{
	if (!IsLocalController() || !bGraphicsResolutionOverrideEnabled || GraphicsResolutionWidth <= 0 || GraphicsResolutionHeight <= 0)
	{
		return;
	}

	const int32 SafeWidth = FMath::Clamp(GraphicsResolutionWidth, 960, 7680);
	const int32 SafeHeight = FMath::Clamp(GraphicsResolutionHeight, 540, 4320);
	ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%d%s"), SafeWidth, SafeHeight, bGraphicsFullscreen ? TEXT("f") : TEXT("w")));
}

void ABHPlayerController::ApplyAdaptiveGraphicsState(bool bAnnounce)
{
	if (!IsLocalController())
	{
		return;
	}

	const int32 SafeRenderScale = BHClampRenderScale(GraphicsRenderScalePercent);
	const int32 SafeGoal = BHClampFpsGoal(GraphicsAdaptiveFpsGoal);
	const int32 EffectiveStep = bAdaptiveGraphicsEnabled ? FMath::Clamp(GraphicsAdaptiveStep, 0, BHAdaptiveMaxStep) : 0;
	const int32 EffectiveRenderScale = BHClampRenderScale(SafeRenderScale - EffectiveStep * 8);
	const int32 EffectiveShadowQuality = FMath::Clamp(GraphicsShadowQuality - (EffectiveStep >= 2 ? 1 : 0) - (EffectiveStep >= 4 ? 1 : 0), 0, 3);
	const int32 EffectiveEffectsQuality = FMath::Clamp(GraphicsEffectsQuality - (EffectiveStep >= 2 ? 1 : 0) - (EffectiveStep >= 3 ? 1 : 0), 0, 3);
	const int32 EffectiveFrameRateLimit = BHResolveGraphicsFrameRateLimit(bAdaptiveGraphicsEnabled, SafeGoal, GraphicsFrameRateLimit);

	ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), EffectiveFrameRateLimit));

	if (bAdaptiveGraphicsEnabled)
	{
		const int32 MinDynamicScale = FMath::Clamp(EffectiveRenderScale - 18, BHAdaptiveMinRenderScale, EffectiveRenderScale);
		ConsoleCommand(TEXT("r.DynamicRes.OperationMode 2"));
		ConsoleCommand(FString::Printf(TEXT("r.DynamicRes.FrameTimeBudget %.2f"), BHFpsGoalToFrameTimeBudgetMs(SafeGoal)));
		ConsoleCommand(FString::Printf(TEXT("r.DynamicRes.MinScreenPercentage %d"), MinDynamicScale));
		ConsoleCommand(FString::Printf(TEXT("r.DynamicRes.MaxScreenPercentage %d"), EffectiveRenderScale));
	}
	else
	{
		ConsoleCommand(TEXT("r.DynamicRes.OperationMode 0"));
	}

	ConsoleCommand(FString::Printf(TEXT("r.ScreenPercentage %d"), EffectiveRenderScale));
	ConsoleCommand(FString::Printf(TEXT("sg.ShadowQuality %d"), EffectiveShadowQuality));
	ConsoleCommand(FString::Printf(TEXT("sg.EffectsQuality %d"), EffectiveEffectsQuality));

	switch (EffectiveShadowQuality)
	{
	case 0:
		ConsoleCommand(TEXT("r.ShadowQuality 2"));
		ConsoleCommand(TEXT("r.Shadow.MaxResolution 512"));
		ConsoleCommand(TEXT("r.Shadow.CSM.MaxCascades 1"));
		break;
	case 1:
		ConsoleCommand(TEXT("r.ShadowQuality 2"));
		ConsoleCommand(TEXT("r.Shadow.MaxResolution 1024"));
		ConsoleCommand(TEXT("r.Shadow.CSM.MaxCascades 1"));
		break;
	case 2:
		ConsoleCommand(TEXT("r.ShadowQuality 4"));
		ConsoleCommand(TEXT("r.Shadow.MaxResolution 2048"));
		ConsoleCommand(TEXT("r.Shadow.CSM.MaxCascades 2"));
		break;
	default:
		ConsoleCommand(TEXT("r.ShadowQuality 5"));
		ConsoleCommand(TEXT("r.Shadow.MaxResolution 4096"));
		ConsoleCommand(TEXT("r.Shadow.CSM.MaxCascades 4"));
		break;
	}

	ConsoleCommand(FString::Printf(TEXT("r.SSR.Quality %d"), EffectiveEffectsQuality <= 0 ? 0 : FMath::Clamp(EffectiveEffectsQuality, 1, 4)));
	ConsoleCommand(FString::Printf(TEXT("r.VolumetricFog %d"), EffectiveEffectsQuality >= 2 ? 1 : 0));
	{
		// Foggrounds is a fog-centric map: force volumetric fog on across every graphics tier so the
		// uniform fog body and the flashlight god-rays render identically from Low through Ultra.
		// The froxel grid is coarsened on low tiers to keep the cost viable on 4GB classroom GPUs.
		// Outside Foggrounds this block is skipped, leaving the gated value above in place.
		const UWorld* GraphicsWorld = GetWorld();
		const ABHGameState* GraphicsGameState = GraphicsWorld ? GraphicsWorld->GetGameState<ABHGameState>() : nullptr;
		if (GraphicsGameState && GraphicsGameState->ActiveLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
		{
			ConsoleCommand(TEXT("r.VolumetricFog 1"));
			ConsoleCommand(FString::Printf(TEXT("r.VolumetricFog.GridPixelSize %d"), EffectiveEffectsQuality >= 2 ? 8 : 16));
		}
	}
	ConsoleCommand(FString::Printf(TEXT("fx.Niagara.QualityLevel %d"), EffectiveEffectsQuality));

	if (EffectiveStep >= 3)
	{
		ConsoleCommand(TEXT("r.SkeletalMeshLODBias 1"));
	}
	else
	{
		ConsoleCommand(FString::Printf(TEXT("r.SkeletalMeshLODBias %d"), GraphicsPresetQuality == 0 ? 1 : 0));
	}
	const float EffectiveFoliageDistanceScale = EffectiveStep >= 3 ? 0.65f : BHGraphicsPresetFoliageDistanceScale(GraphicsPresetQuality);
	const float EffectiveStaticMeshDistanceScale = EffectiveStep >= 3
		? FMath::Max(BHGraphicsPresetStaticMeshDistanceScale(GraphicsPresetQuality), 1.25f)
		: BHGraphicsPresetStaticMeshDistanceScale(GraphicsPresetQuality);
	ConsoleCommand(FString::Printf(TEXT("foliage.LODDistanceScale %.2f"), EffectiveFoliageDistanceScale));
	ConsoleCommand(FString::Printf(TEXT("r.StaticMeshLODDistanceScale %.2f"), EffectiveStaticMeshDistanceScale));

	BHApplyHorrorLookConsoleVariables(this);
	if (EffectiveShadowQuality <= 1)
	{
		ConsoleCommand(TEXT("r.AmbientOcclusionLevels 1"));
	}

	if (bAnnounce)
	{
		const FString State = bAdaptiveGraphicsEnabled
			? FString::Printf(TEXT("Adaptive graphics target %d FPS, render scale %d%%."), GraphicsAdaptiveFpsGoal, EffectiveRenderScale)
			: FString::Printf(TEXT("Adaptive graphics disabled, render scale %d%%."), EffectiveRenderScale);
		ShowLocalStatusMessage(State, 3.0f);
	}
}

void ABHPlayerController::TickAdaptiveGraphics(float DeltaSeconds)
{
	if (!IsLocalController() || !bAdaptiveGraphicsEnabled || bVirtualBoxSafeApplied || DeltaSeconds <= 0.0f || DeltaSeconds > 0.25f)
	{
		return;
	}

	const float FrameTimeMs = FMath::Clamp(DeltaSeconds * 1000.0f, 1.0f, 250.0f);
	GraphicsAverageFrameTimeMs = GraphicsAverageFrameTimeMs <= 0.0f
		? FrameTimeMs
		: FMath::Lerp(GraphicsAverageFrameTimeMs, FrameTimeMs, 0.05f);

	GraphicsAdaptiveEvaluationSeconds += DeltaSeconds;
	if (GraphicsAdaptiveEvaluationSeconds < BHAdaptiveEvaluationIntervalSeconds)
	{
		return;
	}
	GraphicsAdaptiveEvaluationSeconds = 0.0f;

	const float AverageFps = GraphicsAverageFrameTimeMs > KINDA_SMALL_NUMBER ? 1000.0f / GraphicsAverageFrameTimeMs : 0.0f;
	if (AverageFps <= 0.0f)
	{
		return;
	}

	const int32 PreviousStep = GraphicsAdaptiveStep;
	const int32 SafeGoal = BHClampFpsGoal(GraphicsAdaptiveFpsGoal);
	const float TargetRatio = AverageFps / static_cast<float>(SafeGoal);
	if (TargetRatio < BHAdaptiveUnderTargetRatio)
	{
		++GraphicsUnderTargetSamples;
		GraphicsOverTargetSamples = 0;
		if (TargetRatio < BHAdaptiveSevereUnderTargetRatio || GraphicsUnderTargetSamples >= BHAdaptiveUnderTargetSamplesBeforeAdjust)
		{
			const int32 StepDelta = TargetRatio < BHAdaptiveSevereUnderTargetRatio ? 2 : 1;
			GraphicsAdaptiveStep = FMath::Min(GraphicsAdaptiveStep + StepDelta, BHAdaptiveMaxStep);
			GraphicsUnderTargetSamples = 0;
		}
	}
	else if (GraphicsAdaptiveStep > 0 && TargetRatio >= BHAdaptiveStableTargetRatio)
	{
		GraphicsUnderTargetSamples = 0;
		++GraphicsOverTargetSamples;
		const int32 RecoverySamples = TargetRatio >= BHAdaptiveClearlyOverTargetRatio
			? BHAdaptiveOverTargetSamplesBeforeRecover
			: BHAdaptiveStableSamplesBeforeRecover;
		if (GraphicsOverTargetSamples >= RecoverySamples)
		{
			GraphicsAdaptiveStep = FMath::Max(GraphicsAdaptiveStep - 1, 0);
			GraphicsOverTargetSamples = 0;
		}
	}
	else
	{
		GraphicsUnderTargetSamples = 0;
		GraphicsOverTargetSamples = 0;
	}

	if (GraphicsAdaptiveStep != PreviousStep)
	{
		ApplyAdaptiveGraphicsState(false);
		GraphicsAverageFrameTimeMs = 0.0f;
		GraphicsAdaptiveEvaluationSeconds = -0.5f;
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		// The adaptive scaling still happens; we just suppress the on-screen prompt during the guided
		// tutorial so it never steals the status line from a teaching instruction.
		const ABHGameState* AdaptiveBHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		const bool bTutorialActive = AdaptiveBHGS && AdaptiveBHGS->bTutorialMode;
		if (!bTutorialActive && Now - GraphicsLastAdaptiveMessageTime > 12.0f)
		{
			GraphicsLastAdaptiveMessageTime = Now;
			ShowLocalStatusMessage(FString::Printf(TEXT("Adaptive graphics adjusted toward %d FPS after %.0f FPS average."), SafeGoal, AverageFps), 2.5f);
		}
	}
}

bool ABHPlayerController::ApplyGraphicsPresetInternal(int32 Quality, bool bSaveAsManualChoice, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	const int32 ClampedQuality = FMath::Clamp(Quality, 0, 3);
	GraphicsPresetQuality = ClampedQuality;
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -2.0f;

	if (bSaveAsManualChoice)
	{
		bAutoHardwareGraphicsEnabled = false;
	}

	BHApplyScalabilityGroups(this, ClampedQuality);

	switch (ClampedQuality)
	{
	case 0:
	{
		GraphicsRenderScalePercent = 67;
		GraphicsFrameRateLimit = 45;
		GraphicsTextureQuality = 0;
		GraphicsShadowQuality = 0;
		GraphicsEffectsQuality = 0;
		static const FBHConsoleVariableSetting LowSettings[] = {
			{ TEXT("r.ScreenPercentage"), TEXT("67") },
			{ TEXT("r.DynamicRes.OperationMode"), TEXT("2") },
			{ TEXT("r.DynamicRes.MinScreenPercentage"), TEXT("50") },
			{ TEXT("r.DynamicRes.MaxScreenPercentage"), TEXT("75") },
			{ TEXT("r.DynamicRes.FrameTimeBudget"), TEXT("22.2") },
			{ TEXT("t.MaxFPS"), TEXT("45") },
			{ TEXT("r.TextureStreaming"), TEXT("1") },
			{ TEXT("r.Streaming.UseFixedPoolSize"), TEXT("1") },
			{ TEXT("r.Streaming.PoolSize"), TEXT("384") },
			{ TEXT("r.Streaming.MaxTempMemoryAllowed"), TEXT("64") },
			{ TEXT("r.MipMapLODBias"), TEXT("1") },
			{ TEXT("r.StaticMeshLODDistanceScale"), TEXT("1.45") },
			{ TEXT("r.SkeletalMeshLODBias"), TEXT("1") },
			{ TEXT("foliage.LODDistanceScale"), TEXT("0.65") },
			{ TEXT("r.ShadowQuality"), TEXT("2") },
			{ TEXT("r.Shadow.CSM.MaxCascades"), TEXT("1") },
			{ TEXT("r.Shadow.MaxResolution"), TEXT("512") },
			{ TEXT("r.DistanceFieldAO"), TEXT("0") },
			{ TEXT("r.AmbientOcclusionLevels"), TEXT("1") },
			{ TEXT("r.SSR.Quality"), TEXT("0") },
			{ TEXT("r.BloomQuality"), TEXT("3") },
			{ TEXT("r.Tonemapper.Quality"), TEXT("5") },
			{ TEXT("r.EyeAdaptationQuality"), TEXT("2") },
			{ TEXT("r.LocalExposure"), TEXT("1") },
			{ TEXT("r.MotionBlurQuality"), TEXT("0") },
			{ TEXT("r.VolumetricFog"), TEXT("0") },
			{ TEXT("r.Lumen.DiffuseIndirect.Allow"), TEXT("0") },
			{ TEXT("r.Lumen.Reflections.Allow"), TEXT("0") },
			{ TEXT("r.Nanite"), TEXT("0") },
			{ TEXT("r.RayTracing.ForceAllRayTracingEffects"), TEXT("0") }
		};
		BHApplyConsoleVariables(this, LowSettings);
		break;
	}
	case 1:
	{
		GraphicsRenderScalePercent = 82;
		GraphicsFrameRateLimit = 60;
		GraphicsTextureQuality = 1;
		GraphicsShadowQuality = 1;
		GraphicsEffectsQuality = 1;
		static const FBHConsoleVariableSetting MediumSettings[] = {
			{ TEXT("r.ScreenPercentage"), TEXT("82") },
			{ TEXT("r.DynamicRes.OperationMode"), TEXT("2") },
			{ TEXT("r.DynamicRes.MinScreenPercentage"), TEXT("66") },
			{ TEXT("r.DynamicRes.MaxScreenPercentage"), TEXT("90") },
			{ TEXT("r.DynamicRes.FrameTimeBudget"), TEXT("16.7") },
			{ TEXT("t.MaxFPS"), TEXT("60") },
			{ TEXT("r.TextureStreaming"), TEXT("1") },
			{ TEXT("r.Streaming.UseFixedPoolSize"), TEXT("1") },
			{ TEXT("r.Streaming.PoolSize"), TEXT("768") },
			{ TEXT("r.Streaming.MaxTempMemoryAllowed"), TEXT("96") },
			{ TEXT("r.MipMapLODBias"), TEXT("0") },
			{ TEXT("r.StaticMeshLODDistanceScale"), TEXT("1.15") },
			{ TEXT("r.SkeletalMeshLODBias"), TEXT("0") },
			{ TEXT("foliage.LODDistanceScale"), TEXT("0.85") },
			{ TEXT("r.Shadow.MaxResolution"), TEXT("1024") },
			{ TEXT("r.AmbientOcclusionLevels"), TEXT("1") },
			{ TEXT("r.SSR.Quality"), TEXT("1") },
			{ TEXT("r.BloomQuality"), TEXT("3") },
			{ TEXT("r.Tonemapper.Quality"), TEXT("4") },
			{ TEXT("r.MotionBlurQuality"), TEXT("0") },
			{ TEXT("r.VolumetricFog"), TEXT("0") },
			{ TEXT("r.RayTracing.ForceAllRayTracingEffects"), TEXT("0") }
		};
		BHApplyConsoleVariables(this, MediumSettings);
		break;
	}
	case 2:
	{
		GraphicsRenderScalePercent = 100;
		GraphicsFrameRateLimit = 120;
		GraphicsTextureQuality = 2;
		GraphicsShadowQuality = 2;
		GraphicsEffectsQuality = 2;
		static const FBHConsoleVariableSetting HighSettings[] = {
			{ TEXT("r.ScreenPercentage"), TEXT("100") },
			{ TEXT("r.DynamicRes.OperationMode"), TEXT("2") },
			{ TEXT("r.DynamicRes.MinScreenPercentage"), TEXT("80") },
			{ TEXT("r.DynamicRes.MaxScreenPercentage"), TEXT("100") },
			{ TEXT("r.DynamicRes.FrameTimeBudget"), TEXT("13.9") },
			{ TEXT("t.MaxFPS"), TEXT("120") },
			{ TEXT("r.TextureStreaming"), TEXT("1") },
			{ TEXT("r.Streaming.UseFixedPoolSize"), TEXT("1") },
			{ TEXT("r.Streaming.PoolSize"), TEXT("2048") },
			{ TEXT("r.Streaming.MaxTempMemoryAllowed"), TEXT("256") },
			{ TEXT("r.MipMapLODBias"), TEXT("0") },
			{ TEXT("r.StaticMeshLODDistanceScale"), TEXT("1.0") },
			{ TEXT("r.SkeletalMeshLODBias"), TEXT("0") },
			{ TEXT("foliage.LODDistanceScale"), TEXT("1.0") },
			{ TEXT("r.Shadow.MaxResolution"), TEXT("2048") },
			{ TEXT("r.AmbientOcclusionLevels"), TEXT("2") },
			{ TEXT("r.SSR.Quality"), TEXT("3") },
			{ TEXT("r.BloomQuality"), TEXT("4") },
			{ TEXT("r.Tonemapper.Quality"), TEXT("5") },
			{ TEXT("r.MotionBlurQuality"), TEXT("0") },
			{ TEXT("r.VolumetricFog"), TEXT("1") },
			{ TEXT("r.Nanite"), TEXT("1") },
			{ TEXT("r.RayTracing.ForceAllRayTracingEffects"), TEXT("-1") }
		};
		BHApplyConsoleVariables(this, HighSettings);
		break;
	}
	default:
	{
		GraphicsRenderScalePercent = 100;
		GraphicsFrameRateLimit = 120;
		GraphicsTextureQuality = 3;
		GraphicsShadowQuality = 3;
		GraphicsEffectsQuality = 3;
		static const FBHConsoleVariableSetting UltraSettings[] = {
			{ TEXT("r.ScreenPercentage"), TEXT("100") },
			{ TEXT("r.DynamicRes.OperationMode"), TEXT("0") },
			{ TEXT("t.MaxFPS"), TEXT("120") },
			{ TEXT("r.TextureStreaming"), TEXT("1") },
			{ TEXT("r.Streaming.UseFixedPoolSize"), TEXT("1") },
			{ TEXT("r.Streaming.PoolSize"), TEXT("4096") },
			{ TEXT("r.Streaming.MaxTempMemoryAllowed"), TEXT("512") },
			{ TEXT("r.MipMapLODBias"), TEXT("0") },
			{ TEXT("r.StaticMeshLODDistanceScale"), TEXT("1.0") },
			{ TEXT("r.SkeletalMeshLODBias"), TEXT("0") },
			{ TEXT("foliage.LODDistanceScale"), TEXT("1.0") },
			{ TEXT("r.ViewDistanceScale"), TEXT("2.0") },
			{ TEXT("r.Shadow.MaxResolution"), TEXT("4096") },
			{ TEXT("r.Shadow.CSM.MaxCascades"), TEXT("4") },
			{ TEXT("r.DistanceFieldAO"), TEXT("1") },
			{ TEXT("r.AmbientOcclusionLevels"), TEXT("3") },
			{ TEXT("r.SSR.Quality"), TEXT("4") },
			{ TEXT("r.BloomQuality"), TEXT("5") },
			{ TEXT("r.Tonemapper.Quality"), TEXT("5") },
			{ TEXT("r.MotionBlurQuality"), TEXT("0") },
			{ TEXT("r.VolumetricFog"), TEXT("1") },
			{ TEXT("r.VolumetricFog.GridPixelSize"), TEXT("4") },
			{ TEXT("r.Lumen.DiffuseIndirect.Allow"), TEXT("1") },
			{ TEXT("r.Lumen.Reflections.Allow"), TEXT("1") },
			{ TEXT("r.Nanite"), TEXT("1") },
			{ TEXT("r.RayTracing.ForceAllRayTracingEffects"), TEXT("-1") }
		};
		BHApplyConsoleVariables(this, UltraSettings);
		break;
	}
	}

	if (bSaveAsManualChoice)
	{
		GraphicsAdaptiveFpsGoal = GraphicsFrameRateLimit > 0 ? BHClampFpsGoal(GraphicsFrameRateLimit) : 120;
	}

	BHApplyHorrorLookConsoleVariables(this);
	ApplyAdaptiveGraphicsState(false);
	if (bSaveAsManualChoice)
	{
		SaveGraphicsPreferences();
	}

	OutMessage = FString::Printf(TEXT("Local graphics preset applied: %s."), BHGraphicsPresetLabel(ClampedQuality));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ApplyGraphicsPresetForMenu(int32 Quality, FString& OutMessage)
{
	EnsureGraphicsPreferencesLoaded();
	return ApplyGraphicsPresetInternal(Quality, true, OutMessage);
}

bool ABHPlayerController::ApplyAutoGraphicsForMenu(FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	const FBHGraphicsHardwareProfile HardwareProfile = BHScanGraphicsHardwareProfile();
	GraphicsGpuBrand = HardwareProfile.GpuBrand;
	GraphicsSystemMemoryGB = HardwareProfile.SystemMemoryGB;
	GraphicsDedicatedVideoMemoryGB = HardwareProfile.DedicatedVideoMemoryGB;
	GraphicsPhysicalCores = HardwareProfile.PhysicalCores;
	GraphicsLogicalCores = HardwareProfile.LogicalCores;
	GraphicsRecommendedPreset = HardwareProfile.RecommendedPreset;
	GraphicsRecommendedRenderScale = HardwareProfile.RecommendedRenderScale;
	GraphicsRecommendedFpsGoal = HardwareProfile.RecommendedFpsGoal;
	bGraphicsPreferSafeResolution = HardwareProfile.bPreferSafeResolution;
	GraphicsPresetQuality = HardwareProfile.RecommendedPreset;
	GraphicsRenderScalePercent = HardwareProfile.RecommendedRenderScale;
	GraphicsAdaptiveFpsGoal = HardwareProfile.RecommendedFpsGoal;
	GraphicsFrameRateLimit = HardwareProfile.RecommendedFpsGoal;
	bGraphicsLikelyIntegratedGpu = HardwareProfile.bLikelyIntegratedGpu;
	bGraphicsLikelySoftwareGpu = HardwareProfile.bLikelySoftwareGpu;
	bAutoHardwareGraphicsEnabled = true;

	ApplyGraphicsPresetInternal(GraphicsPresetQuality, false, OutMessage);
	GraphicsRenderScalePercent = HardwareProfile.RecommendedRenderScale;
	GraphicsFrameRateLimit = HardwareProfile.RecommendedFpsGoal;
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	ApplyAdaptiveGraphicsState(false);
	if (bGraphicsPreferSafeResolution && !bGraphicsHasSavedResolutionPreference && !bGraphicsResolutionOverrideEnabled)
	{
		GraphicsResolutionWidth = 1280;
		GraphicsResolutionHeight = 720;
		bGraphicsFullscreen = false;
		bGraphicsResolutionOverrideEnabled = true;
		bGraphicsAutoSafeResolutionApplied = true;
		ApplySavedGraphicsResolution();
	}
	SaveGraphicsPreferences();

	OutMessage = FString::Printf(TEXT("Auto graphics selected %s. %s"), BHGraphicsPresetLabel(GraphicsPresetQuality), *GetGraphicsSummaryForMenu());
	ShowLocalStatusMessage(OutMessage, 4.0f);
	return true;
}

bool ABHPlayerController::SetAdaptiveGraphicsForMenu(bool bEnabled, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	EnsureGraphicsPreferencesLoaded();
	bAdaptiveGraphicsEnabled = bEnabled;
	if (bAdaptiveGraphicsEnabled && GraphicsFrameRateLimit > 0 && GraphicsAdaptiveFpsGoal > GraphicsFrameRateLimit)
	{
		GraphicsAdaptiveFpsGoal = GraphicsFrameRateLimit;
	}
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -1.0f;
	ApplyAdaptiveGraphicsState(true);
	SaveGraphicsPreferences();
	OutMessage = bAdaptiveGraphicsEnabled
		? FString::Printf(TEXT("Adaptive graphics enabled at %d FPS."), GraphicsAdaptiveFpsGoal)
		: TEXT("Adaptive graphics disabled.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetAdaptiveFrameRateGoalForMenu(int32 FpsGoal, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	EnsureGraphicsPreferencesLoaded();
	GraphicsAdaptiveFpsGoal = BHClampFpsGoal(FpsGoal);
	bool bRaisedFrameCap = false;
	if (GraphicsFrameRateLimit > 0 && GraphicsFrameRateLimit < GraphicsAdaptiveFpsGoal)
	{
		GraphicsFrameRateLimit = GraphicsAdaptiveFpsGoal;
		bRaisedFrameCap = true;
	}
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -1.0f;
	ApplyAdaptiveGraphicsState(true);
	SaveGraphicsPreferences();
	if (bAdaptiveGraphicsEnabled)
	{
		OutMessage = bRaisedFrameCap
			? FString::Printf(TEXT("Adaptive graphics target set to %d FPS; frame cap raised and effective cap follows the goal."), GraphicsAdaptiveFpsGoal)
			: FString::Printf(TEXT("Adaptive graphics target set to %d FPS; effective cap follows the goal."), GraphicsAdaptiveFpsGoal);
	}
	else
	{
		OutMessage = bRaisedFrameCap
			? FString::Printf(TEXT("Adaptive graphics target set to %d FPS; frame cap raised to match."), GraphicsAdaptiveFpsGoal)
			: FString::Printf(TEXT("Adaptive graphics target set to %d FPS."), GraphicsAdaptiveFpsGoal);
	}
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ApplyRenderScaleForMenu(int32 Percent, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	EnsureGraphicsPreferencesLoaded();
	bAutoHardwareGraphicsEnabled = false;
	GraphicsRenderScalePercent = BHClampRenderScale(Percent);
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -1.0f;
	ApplyAdaptiveGraphicsState(true);
	SaveGraphicsPreferences();
	OutMessage = FString::Printf(TEXT("Render scale set to %d%%."), GraphicsRenderScalePercent);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ApplyTextureQualityForMenu(int32 Quality, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	EnsureGraphicsPreferencesLoaded();
	bAutoHardwareGraphicsEnabled = false;
	GraphicsTextureQuality = BHClampGraphicsQuality(Quality);
	ApplySavedManualGraphicsTuning();
	SaveGraphicsPreferences();
	OutMessage = FString::Printf(TEXT("Texture quality set to %s."), BHGraphicsPresetLabel(GraphicsTextureQuality));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ApplyShadowQualityForMenu(int32 Quality, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	EnsureGraphicsPreferencesLoaded();
	bAutoHardwareGraphicsEnabled = false;
	GraphicsShadowQuality = BHClampGraphicsQuality(Quality);
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -1.0f;
	ApplyAdaptiveGraphicsState(true);
	SaveGraphicsPreferences();
	OutMessage = FString::Printf(TEXT("Shadow quality set to %s."), BHGraphicsPresetLabel(GraphicsShadowQuality));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ApplyEffectsQualityForMenu(int32 Quality, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	EnsureGraphicsPreferencesLoaded();
	bAutoHardwareGraphicsEnabled = false;
	GraphicsEffectsQuality = BHClampGraphicsQuality(Quality);
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -1.0f;
	ApplyAdaptiveGraphicsState(true);
	SaveGraphicsPreferences();
	OutMessage = FString::Printf(TEXT("Effects quality set to %s."), BHGraphicsPresetLabel(GraphicsEffectsQuality));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ApplyResolutionForMenu(int32 Width, int32 Height, bool bFullscreen, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	const int32 SafeWidth = FMath::Clamp(Width, 960, 7680);
	const int32 SafeHeight = FMath::Clamp(Height, 540, 4320);
	EnsureGraphicsPreferencesLoaded();
	GraphicsResolutionWidth = SafeWidth;
	GraphicsResolutionHeight = SafeHeight;
	bGraphicsFullscreen = bFullscreen;
	bGraphicsResolutionOverrideEnabled = true;
	bGraphicsHasSavedResolutionPreference = true;
	bGraphicsAutoSafeResolutionApplied = false;
	ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%d%s"), SafeWidth, SafeHeight, bFullscreen ? TEXT("f") : TEXT("w")));
	SaveGraphicsPreferences();
	OutMessage = FString::Printf(TEXT("Local resolution applied: %dx%d %s."), SafeWidth, SafeHeight, bFullscreen ? TEXT("Fullscreen") : TEXT("Windowed"));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::ApplyFrameRateLimitForMenu(int32 FrameRateLimit, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	const int32 SafeLimit = FrameRateLimit <= 0 ? 0 : FMath::Clamp(FrameRateLimit, 30, 360);
	EnsureGraphicsPreferencesLoaded();
	bAutoHardwareGraphicsEnabled = false;
	GraphicsFrameRateLimit = SafeLimit;
	const bool bLoweredAdaptiveGoal = SafeLimit > 0 && GraphicsAdaptiveFpsGoal > SafeLimit;
	if (bLoweredAdaptiveGoal)
	{
		GraphicsAdaptiveFpsGoal = SafeLimit;
		GraphicsAdaptiveStep = 0;
		GraphicsUnderTargetSamples = 0;
		GraphicsOverTargetSamples = 0;
		GraphicsAverageFrameTimeMs = 0.0f;
		GraphicsAdaptiveEvaluationSeconds = -1.0f;
	}
	ApplyAdaptiveGraphicsState(false);
	SaveGraphicsPreferences();
	if (SafeLimit == 0)
	{
		OutMessage = bAdaptiveGraphicsEnabled
			? FString::Printf(TEXT("Local frame cap removed; adaptive target still caps at %d FPS."), GraphicsAdaptiveFpsGoal)
			: TEXT("Local frame cap removed.");
	}
	else if (bLoweredAdaptiveGoal)
	{
		OutMessage = FString::Printf(TEXT("Local frame cap set to %d FPS; adaptive target lowered to match."), SafeLimit);
	}
	else if (bAdaptiveGraphicsEnabled && SafeLimit != GraphicsAdaptiveFpsGoal)
	{
		OutMessage = FString::Printf(TEXT("Local frame cap set to %d FPS; adaptive target still caps at %d FPS."), SafeLimit, GraphicsAdaptiveFpsGoal);
	}
	else
	{
		OutMessage = FString::Printf(TEXT("Local frame cap set to %d FPS."), SafeLimit);
	}
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

FString ABHPlayerController::GetGraphicsSummaryForMenu() const
{
	const FString ModeText = bAutoHardwareGraphicsEnabled
		? FString::Printf(TEXT("Auto %s"), BHGraphicsPresetLabel(GraphicsPresetQuality))
		: FString::Printf(TEXT("Manual %s"), BHGraphicsPresetLabel(GraphicsPresetQuality));
	const FString VramText = GraphicsDedicatedVideoMemoryGB > 0.0f
		? FString::Printf(TEXT("%.1f GB VRAM"), GraphicsDedicatedVideoMemoryGB)
		: TEXT("VRAM unknown");
	const FString RamText = GraphicsSystemMemoryGB > 0.0f
		? FString::Printf(TEXT("%.1f GB RAM"), GraphicsSystemMemoryGB)
		: TEXT("RAM unknown");
	FString GpuText = GraphicsGpuBrand.IsEmpty() ? TEXT("GPU unknown") : GraphicsGpuBrand;
	if (GpuText.Len() > 48)
	{
		GpuText = GpuText.Left(45) + TEXT("...");
	}
	const FString AdaptiveText = bAdaptiveGraphicsEnabled
		? FString::Printf(TEXT("Adaptive on, %d FPS goal"), GraphicsAdaptiveFpsGoal)
		: TEXT("Adaptive off");
	const FString AverageFpsText = GraphicsAverageFrameTimeMs > KINDA_SMALL_NUMBER
		? FString::Printf(TEXT(", %.0f FPS avg"), 1000.0f / GraphicsAverageFrameTimeMs)
		: FString();
	const int32 EffectiveRenderScale = GetGraphicsEffectiveRenderScalePercentForMenu();
	const FString ScaleText = EffectiveRenderScale == GraphicsRenderScalePercent
		? FString::Printf(TEXT("Scale %d%%"), GraphicsRenderScalePercent)
		: FString::Printf(TEXT("Scale %d%%, effective %d%%"), GraphicsRenderScalePercent, EffectiveRenderScale);
	const int32 EffectiveFrameRateLimit = BHResolveGraphicsFrameRateLimit(bAdaptiveGraphicsEnabled, GraphicsAdaptiveFpsGoal, GraphicsFrameRateLimit);
	const FString BaseCapText = GraphicsFrameRateLimit > 0
		? FString::Printf(TEXT("Cap %d FPS"), GraphicsFrameRateLimit)
		: TEXT("Cap free");
	const FString CapText = EffectiveFrameRateLimit != GraphicsFrameRateLimit
		? FString::Printf(TEXT("%s, effective %d FPS"), *BaseCapText, EffectiveFrameRateLimit)
		: BaseCapText;
	const FString ResolutionText = bGraphicsResolutionOverrideEnabled && GraphicsResolutionWidth > 0 && GraphicsResolutionHeight > 0
		? FString::Printf(
			TEXT("Res %dx%d %s%s"),
			GraphicsResolutionWidth,
			GraphicsResolutionHeight,
			bGraphicsFullscreen ? TEXT("F") : TEXT("W"),
			bGraphicsAutoSafeResolutionApplied ? TEXT(" auto-safe") : TEXT(""))
		: TEXT("Res default");

	return FString::Printf(
		TEXT("%s | %s | %s | %s | %s | %s | %s | %s%s"),
		*GpuText,
		*RamText,
		*VramText,
		*ModeText,
		*AdaptiveText,
		*CapText,
		*ResolutionText,
		*ScaleText,
		*AverageFpsText);
}

bool ABHPlayerController::IsAutoGraphicsEnabledForMenu() const
{
	return bAutoHardwareGraphicsEnabled;
}

bool ABHPlayerController::IsAdaptiveGraphicsEnabledForMenu() const
{
	return bAdaptiveGraphicsEnabled;
}

int32 ABHPlayerController::GetGraphicsPresetQualityForMenu() const
{
	return GraphicsPresetQuality;
}

int32 ABHPlayerController::GetGraphicsRenderScalePercentForMenu() const
{
	return GraphicsRenderScalePercent;
}

int32 ABHPlayerController::GetGraphicsEffectiveRenderScalePercentForMenu() const
{
	const int32 EffectiveStep = bAdaptiveGraphicsEnabled ? FMath::Clamp(GraphicsAdaptiveStep, 0, BHAdaptiveMaxStep) : 0;
	return BHClampRenderScale(GraphicsRenderScalePercent - EffectiveStep * 8);
}

int32 ABHPlayerController::GetGraphicsAdaptiveFpsGoalForMenu() const
{
	return GraphicsAdaptiveFpsGoal;
}

int32 ABHPlayerController::GetGraphicsFrameRateLimitForMenu() const
{
	return GraphicsFrameRateLimit;
}

int32 ABHPlayerController::GetGraphicsTextureQualityForMenu() const
{
	return GraphicsTextureQuality;
}

int32 ABHPlayerController::GetGraphicsShadowQualityForMenu() const
{
	return GraphicsShadowQuality;
}

int32 ABHPlayerController::GetGraphicsEffectsQualityForMenu() const
{
	return GraphicsEffectsQuality;
}

int32 ABHPlayerController::GetGraphicsAdaptiveStepForMenu() const
{
	return GraphicsAdaptiveStep;
}

bool ABHPlayerController::IsGraphicsResolutionSelectedForMenu(int32 Width, int32 Height, bool bFullscreen) const
{
	const int32 SafeWidth = FMath::Clamp(Width, 960, 7680);
	const int32 SafeHeight = FMath::Clamp(Height, 540, 4320);
	return bGraphicsResolutionOverrideEnabled
		&& GraphicsResolutionWidth == SafeWidth
		&& GraphicsResolutionHeight == SafeHeight
		&& bGraphicsFullscreen == bFullscreen;
}

bool ABHPlayerController::ToggleReadyForMenu(FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Ready can only be changed by the local player.");
		return false;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHGS || !BHPS)
	{
		OutMessage = TEXT("Join or host a classroom lobby before readying.");
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	if (BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		OutMessage = TEXT("Round already started. Late joiners observe as survivor spectators until the next lobby.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	const bool bNewReady = !BHPS->bReady;
	ServerSetReady(bNewReady);
	OutMessage = bNewReady ? TEXT("Ready set. Waiting for the classroom roster.") : TEXT("Ready cancelled.");
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetLocalDisplayNameForMenu(const FString& DisplayName, FString& OutMessage)
{
	UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		OutMessage = TEXT("No account subsystem was available.");
		ShowLocalStatusMessage(OutMessage, 4.0f);
		return false;
	}

	const bool bSuccess = AccountSubsystem->SetLocalDisplayName(DisplayName, OutMessage);
	if (bSuccess)
	{
		PushLocalProfileToServer();
	}
	ShowLocalStatusMessage(OutMessage, bSuccess ? 3.0f : 5.0f);
	return bSuccess;
}

FString ABHPlayerController::GetLocalDisplayNameForMenu() const
{
	const UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	if (!AccountSubsystem)
	{
		return FString();
	}

	return BHSanitizeDisplayName(AccountSubsystem->GetProfile().DisplayName);
}

bool ABHPlayerController::HasUsefulLocalDisplayNameForMenu() const
{
	return BHIsUsefulDisplayName(GetLocalDisplayNameForMenu());
}

void ABHPlayerController::PushLocalDisplayNameToServer()
{
	if (!IsLocalController())
	{
		return;
	}

	const FString DisplayName = GetLocalDisplayNameForMenu();
	if (!BHIsUsefulDisplayName(DisplayName))
	{
		return;
	}

	ServerSetPlayerDisplayName(DisplayName);
}

void ABHPlayerController::PushLocalCosmeticsToServer(bool bAnnounce)
{
	if (!IsLocalController())
	{
		return;
	}

	const UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr;
	const FBHAccountProgress* Progress = AccountSubsystem ? &AccountSubsystem->GetProgress() : nullptr;
	const ABHPlayerState* CurrentBHPS = GetPlayerState<ABHPlayerState>();

	const int32 XP = Progress ? Progress->XP : TNumericLimits<int32>::Max();
	const int32 AvatarIndex = Progress
		? BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::Outfit, Progress->SelectedAvatarIndex, XP, &Progress->UnlockedAchievements)
		: BHCosmeticClampIndex(EBHCosmeticCategory::Outfit, CurrentBHPS ? CurrentBHPS->AvatarIndex : 0);
	const int32 ColorIndex = Progress
		? BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::ShirtColor, Progress->SelectedAvatarColorIndex, XP, &Progress->UnlockedAchievements)
		: BHCosmeticClampIndex(EBHCosmeticCategory::ShirtColor, CurrentBHPS ? BHNearestAvatarColorIndex(CurrentBHPS->AvatarColor) : 0);
	const int32 HeadwearIndex = Progress
		? BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::Headwear, Progress->SelectedAvatarHeadwearIndex, XP, &Progress->UnlockedAchievements)
		: BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, CurrentBHPS ? CurrentBHPS->AvatarHeadwearIndex : 0);
	const int32 GearIndex = 0;
	const FLinearColor AvatarColor = BHAvatarPaletteColor(ColorIndex);

	bool bChanged = false;
	if (ABHPlayerState* MutableBHPS = GetPlayerState<ABHPlayerState>())
	{
		bChanged = MutableBHPS->AvatarIndex != AvatarIndex
			|| !MutableBHPS->AvatarColor.Equals(AvatarColor, 0.005f)
			|| MutableBHPS->AvatarHeadwearIndex != HeadwearIndex
			|| MutableBHPS->AvatarGearIndex != GearIndex;
		MutableBHPS->SetAvatarIndex(AvatarIndex);
		MutableBHPS->SetAvatarColor(AvatarColor);
		MutableBHPS->SetAvatarHeadwearIndex(HeadwearIndex);
		MutableBHPS->SetAvatarGearIndex(GearIndex);
	}

	if (bChanged)
	{
		BHApplyLocalAvatarStyle(this);
	}

	ServerApplyAvatarCosmetics(AvatarIndex, AvatarColor, ColorIndex, HeadwearIndex, GearIndex, bAnnounce);
}

void ABHPlayerController::PushLocalProfileToServer()
{
	PushLocalDisplayNameToServer();
	PushLocalCosmeticsToServer(false);
}

void ABHPlayerController::SetDesiredRole(APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole)
{
	ServerSetDesiredRole(TargetPlayerState, DesiredRole);
}

bool ABHPlayerController::KickPlayerForMenu(APlayerState* TargetPlayerState, FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("kick players")))
	{
		return false;
	}

	const ABHPlayerState* TargetBHPS = Cast<ABHPlayerState>(TargetPlayerState);
	if (!TargetBHPS)
	{
		OutMessage = TEXT("Choose a connected student to kick.");
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	if (TargetBHPS->IsABot())
	{
		OutMessage = TEXT("Bots do not need to be kicked from the lobby.");
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	const FString TargetName = TargetBHPS->GetPlayerName().IsEmpty() ? FString(TEXT("Player")) : TargetBHPS->GetPlayerName();
	OutMessage = FString::Printf(TEXT("Kick request sent for %s."), *TargetName);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	ServerKickPlayer(TargetPlayerState);
	return true;
}

void ABHPlayerController::ShowLocalStatusMessage(const FString& Message, float DurationSeconds)
{
	StatusMessage = Message;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	StatusMessageStartTime = Now;
	StatusMessageDuration = FMath::Max(0.05f, DurationSeconds);
	StatusMessageEndTime = Now + StatusMessageDuration;
	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt status: %s"), *Message);
}

const FString& ABHPlayerController::GetStatusMessage() const
{
	return StatusMessage;
}

bool ABHPlayerController::HasActiveStatusMessage() const
{
	const UWorld* World = GetWorld();
	return !StatusMessage.IsEmpty() && World && World->GetTimeSeconds() < StatusMessageEndTime;
}

float ABHPlayerController::GetStatusMessageAlpha() const
{
	const UWorld* World = GetWorld();
	if (StatusMessage.IsEmpty() || !World)
	{
		return 0.0f;
	}

	const float Now = World->GetTimeSeconds();
	const float Remaining = StatusMessageEndTime - Now;
	if (Remaining <= 0.0f)
	{
		return 0.0f;
	}

	const float FadeInAlpha = FMath::Clamp((Now - StatusMessageStartTime) / 0.16f, 0.0f, 1.0f);
	const float FadeOutAlpha = FMath::Clamp(Remaining / 0.38f, 0.0f, 1.0f);
	return FMath::Min(FadeInAlpha, FadeOutAlpha);
}

bool ABHPlayerController::HasActiveRoleIntro() const
{
	const UWorld* World = GetWorld();
	return RoleIntroRole != EBHPlayerRole::Unassigned && World && World->GetTimeSeconds() < RoleIntroEndTime;
}

float ABHPlayerController::GetRoleIntroAlpha() const
{
	const UWorld* World = GetWorld();
	if (RoleIntroRole == EBHPlayerRole::Unassigned || !World)
	{
		return 0.0f;
	}

	const float Now = World->GetTimeSeconds();
	const float Remaining = RoleIntroEndTime - Now;
	if (Remaining <= 0.0f)
	{
		return 0.0f;
	}

	const float FadeIn = FMath::Clamp((Now - RoleIntroStartTime) / 0.25f, 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp(Remaining / 0.5f, 0.0f, 1.0f);
	return FMath::Min(FadeIn, FadeOut);
}

EBHPlayerRole ABHPlayerController::GetRoleIntroRole() const
{
	return RoleIntroRole;
}

bool ABHPlayerController::IsRoleIntroRevisionMode() const
{
	return bRoleIntroRevisionMode;
}

bool ABHPlayerController::HasActiveTutorialCard() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < TutorialCardEndTime && !TutorialCardTitle.IsEmpty();
}

float ABHPlayerController::GetTutorialCardAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	const float Now = World->GetTimeSeconds();
	const float Remaining = TutorialCardEndTime - Now;
	if (Remaining <= 0.0f)
	{
		return 0.0f;
	}
	const float FadeIn = FMath::Clamp((Now - TutorialCardStartTime) / 0.3f, 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp(Remaining / 0.5f, 0.0f, 1.0f);
	return FMath::Min(FadeIn, FadeOut);
}

const FString& ABHPlayerController::GetTutorialCardTitle() const
{
	return TutorialCardTitle;
}

const FString& ABHPlayerController::GetTutorialCardBody() const
{
	return TutorialCardBody;
}

bool ABHPlayerController::HasActiveTutorialPrompt() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < TutorialPromptEndTime && !TutorialPromptMessage.IsEmpty();
}

float ABHPlayerController::GetTutorialPromptAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	const float Now = World->GetTimeSeconds();
	const float Remaining = TutorialPromptEndTime - Now;
	if (Remaining <= 0.0f)
	{
		return 0.0f;
	}
	const float FadeIn = FMath::Clamp((Now - TutorialPromptStartTime) / 0.2f, 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp(Remaining / 0.4f, 0.0f, 1.0f);
	return FMath::Min(FadeIn, FadeOut);
}

const FString& ABHPlayerController::GetTutorialPromptMessage() const
{
	return TutorialPromptMessage;
}

bool ABHPlayerController::HasActiveCCTVReveal() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < CCTVRevealEndTime;
}

float ABHPlayerController::GetCCTVRevealAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const float Now = World->GetTimeSeconds();
	const float Remaining = CCTVRevealEndTime - Now;
	if (Remaining <= 0.0f)
	{
		return 0.0f;
	}

	const float FadeInAlpha = FMath::Clamp((Now - CCTVRevealStartTime) / 0.12f, 0.0f, 1.0f);
	const float FadeOutAlpha = FMath::Clamp(Remaining / 0.45f, 0.0f, 1.0f);
	return FMath::Min(FadeInAlpha, FadeOutAlpha);
}

const FVector& ABHPlayerController::GetCCTVRevealLocation() const
{
	return CCTVRevealLocation;
}

const FString& ABHPlayerController::GetCCTVRevealTargetName() const
{
	return CCTVRevealTargetName;
}

const ABHCharacter* ABHPlayerController::GetCCTVRevealTarget() const
{
	return CCTVRevealTarget.Get();
}

bool ABHPlayerController::IsHudMapVisible() const
{
	return bHudMapVisible;
}

int32 ABHPlayerController::GetCrosshairStyle() const
{
	return CrosshairStyle;
}

float ABHPlayerController::GetHorrorCueFlashAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World || HorrorCueFlashIntensity <= 0.0f || HorrorCueFlashEndTime <= HorrorCueFlashStartTime)
	{
		return 0.0f;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < HorrorCueFlashStartTime || Now >= HorrorCueFlashEndTime)
	{
		return 0.0f;
	}

	const float Duration = HorrorCueFlashEndTime - HorrorCueFlashStartTime;
	const float Age = Now - HorrorCueFlashStartTime;
	// Sharp attack, longer ease-out so the flash spikes hard then bleeds away.
	const float FadeIn = FMath::Clamp(Age / FMath::Max(0.02f, Duration * 0.10f), 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp((HorrorCueFlashEndTime - Now) / FMath::Max(0.06f, Duration * 0.55f), 0.0f, 1.0f);
	return FMath::Clamp(HorrorCueFlashIntensity * FMath::Min(FadeIn, FadeOut), 0.0f, 1.0f);
}

FLinearColor ABHPlayerController::GetHorrorCueFlashColor() const
{
	return HorrorCueFlashColor;
}

float ABHPlayerController::GetHorrorCueBlinkAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World || HorrorCueBlinkIntensity <= 0.0f || HorrorCueBlinkStartTime < 0.0f)
	{
		return 0.0f;
	}

	constexpr float BlinkDuration = 0.085f;
	const float Age = World->GetTimeSeconds() - HorrorCueBlinkStartTime;
	if (Age < 0.0f || Age >= BlinkDuration)
	{
		return 0.0f;
	}

	// Instant onset, fast linear fade — reads as a hard blink right on the hit.
	const float Alpha = 1.0f - (Age / BlinkDuration);
	return FMath::Clamp(HorrorCueBlinkIntensity * Alpha, 0.0f, 1.0f);
}

UTexture2D* ABHPlayerController::GetHorrorCueFaceImage() const
{
	const UWorld* World = GetWorld();
	if (!World || !HorrorCueFaceImage || HorrorCueFaceImageEndTime <= HorrorCueFaceImageStartTime)
	{
		return nullptr;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < HorrorCueFaceImageStartTime || Now >= HorrorCueFaceImageEndTime)
	{
		return nullptr;
	}

	return HorrorCueFaceImage;
}

float ABHPlayerController::GetHorrorCueFaceImageAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World || HorrorCueFaceImageIntensity <= 0.0f || HorrorCueFaceImageEndTime <= HorrorCueFaceImageStartTime)
	{
		return 0.0f;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < HorrorCueFaceImageStartTime || Now >= HorrorCueFaceImageEndTime)
	{
		return 0.0f;
	}

	// Snap in almost instantly, hold, then ease out over the back third — a hard "slam" onto the screen.
	const float Duration = HorrorCueFaceImageEndTime - HorrorCueFaceImageStartTime;
	const float Age = Now - HorrorCueFaceImageStartTime;
	const float FadeIn = FMath::Clamp(Age / FMath::Max(0.04f, Duration * 0.08f), 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp((HorrorCueFaceImageEndTime - Now) / FMath::Max(0.08f, Duration * 0.34f), 0.0f, 1.0f);
	return FMath::Clamp(HorrorCueFaceImageIntensity * FMath::Min(FadeIn, FadeOut), 0.0f, 1.0f);
}

FString ABHPlayerController::GetHorrorDirectiveText() const
{
	const UWorld* World = GetWorld();
	if (!World || HorrorDirectiveText.IsEmpty() || World->GetTimeSeconds() >= HorrorDirectiveEndTime)
	{
		return FString();
	}
	return HorrorDirectiveText;
}

float ABHPlayerController::GetHorrorDirectiveAlpha() const
{
	const UWorld* World = GetWorld();
	if (!World || HorrorDirectiveText.IsEmpty() || HorrorDirectiveEndTime <= HorrorDirectiveStartTime)
	{
		return 0.0f;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < HorrorDirectiveStartTime || Now >= HorrorDirectiveEndTime)
	{
		return 0.0f;
	}

	// Quick fade in, steady hold, short fade out so the directive reads clearly while it is active.
	const float Age = Now - HorrorDirectiveStartTime;
	const float FadeIn = FMath::Clamp(Age / 0.25f, 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp((HorrorDirectiveEndTime - Now) / 0.35f, 0.0f, 1.0f);
	return FMath::Min(FadeIn, FadeOut);
}

void ABHPlayerController::PlayJumpscareImpactAudio(const FBHClientHorrorCue& Cue, float VolumeScale)
{
	if (Cue.AudioAsset.IsNull())
	{
		return;
	}

	USoundBase* Scream = BHLoadGameplaySound(Cue.AudioAsset, TEXT("horror cue audio"));
	if (!Scream)
	{
		return;
	}

	const float BaseVolume = GetEffectiveUiVolume() * FMath::Clamp(Cue.AudioVolume * VolumeScale, 0.0f, 2.0f);

	// Main scream layer with slight random pitch so repeats never sound identical.
	const float MainPitch = FMath::FRandRange(0.93f, 1.06f);
	UGameplayStatics::PlaySound2D(this, Scream, BaseVolume, MainPitch);

	// Optional pitched-down "roar" body layer built from the same asset for extra low-end weight.
	if (Cue.bLayeredImpactAudio)
	{
		const float RoarPitch = FMath::FRandRange(0.55f, 0.66f);
		UGameplayStatics::PlaySound2D(this, Scream, BaseVolume * 0.72f, RoarPitch);
	}

	// Optional designer-supplied stinger (sub-boom / transient) layered on top.
	if (!Cue.ImpactStinger.IsNull())
	{
		if (USoundBase* Stinger = BHLoadGameplaySound(Cue.ImpactStinger, TEXT("horror cue stinger")))
		{
			UGameplayStatics::PlaySound2D(this, Stinger, BaseVolume * 0.95f, FMath::FRandRange(0.92f, 1.0f));
		}
	}

	// Optional ambient duck: push a configured SoundMix for the scare's duration, if one is assigned.
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings && !Settings->JumpscareDuckSoundMix.IsNull())
	{
		if (USoundMix* DuckMix = Settings->JumpscareDuckSoundMix.LoadSynchronous())
		{
			UGameplayStatics::PushSoundMixModifier(this, DuckMix);
			TWeakObjectPtr<ABHPlayerController> WeakThis(this);
			TWeakObjectPtr<USoundMix> WeakMix(DuckMix);
			FTimerDelegate PopDuck;
			PopDuck.BindLambda([WeakThis, WeakMix]()
			{
				if (WeakThis.IsValid() && WeakMix.IsValid())
				{
					UGameplayStatics::PopSoundMixModifier(WeakThis.Get(), WeakMix.Get());
				}
			});
			const float DuckSeconds = FMath::Clamp(Settings->JumpscareDuckSeconds, 0.1f, 4.0f);
			GetWorldTimerManager().SetTimer(HorrorCueDuckHandle, PopDuck, DuckSeconds, false);
		}
	}
}

FBHLessonPreset ABHPlayerController::BuildCurrentLessonPresetSnapshot(const FString& DisplayName, const FString& SelectedMapName) const
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	FBHLessonPreset Preset = FBHLessonPresetStore::MakeDefaultPreset(Settings);
	Preset.DisplayName = FBHLessonPresetStore::SanitizeDisplayName(DisplayName);
	Preset.Id = FBHLessonPresetStore::MakeCustomPresetId(Preset.DisplayName);
	Preset.bBuiltin = false;
	Preset.MapName = FBHLessonPresetStore::NormalizeMapName(SelectedMapName);

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (BHGS)
	{
		if (BHGS->bRevisionMode)
		{
			Preset.TopicMask = BHGS->RevisionTopicMask;
			Preset.DifficultyMix = BHGS->RevisionDifficultyMix;
			Preset.ClassThreshold = BHGS->RevisionClassThreshold;
			Preset.IndividualThreshold = BHGS->RevisionIndividualThreshold;
			Preset.RoundSeconds = BHGS->RevisionRoundDuration;
			Preset.ScareIntensity = BHGS->RevisionScareIntensity;
			if (!BHGS->NextLevelName.IsEmpty())
			{
				Preset.MapName = FBHLessonPresetStore::NormalizeMapName(BHGS->NextLevelName);
			}
			else if (!BHGS->ActiveLevelName.IsEmpty())
			{
				Preset.MapName = FBHLessonPresetStore::NormalizeMapName(BHGS->ActiveLevelName);
			}
		}
		Preset.BotCount = BHGS->TargetBotCount;
		Preset.BotDifficulty = BHGS->BotDifficulty;
	}

	Preset.bReducedJumpscares = bReducedJumpscares;
	Preset.bReducedFlash = bReducedFlash;
	Preset.bReducedCameraShake = bReducedCameraShake;
	Preset.bCaptions = bCaptionsEnabled;
	Preset.bHighContrastHud = bHighContrastHud;
	return FBHLessonPresetStore::ValidatePreset(Preset);
}

void ABHPlayerController::ApplyGameplayInputMode()
{
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}

void ABHPlayerController::SetQuestionCursorMode(bool bEnable)
{
	if (!IsLocalController())
	{
		return;
	}
	// A real UI (menu / atmosphere console / travel loading) owns the cursor; never override it.
	if (bEnable && (MainMenuWidget.IsValid() || AtmosphereConsoleWidget.IsValid() || TravelLoadingScreenWidget.IsValid()))
	{
		return;
	}
	if (bEnable)
	{
		// GameAndUI without a focused Slate widget: mouse moves the cursor (camera stops turning) and
		// clicks fall through to the game InputComponent, so the character's LMB bind hit-tests the
		// HUD-canvas question regions. No widget rebuild -- the panel stays the existing canvas draw.
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else if (!MainMenuWidget.IsValid() && !AtmosphereConsoleWidget.IsValid() && !TravelLoadingScreenWidget.IsValid())
	{
		ApplyGameplayInputMode();
	}
}

void ABHPlayerController::HandleRoundPhaseUiState()
{
	if (!IsLocalController())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS)
	{
		return;
	}

	// Foggrounds forces volumetric fog on for every graphics tier (see ApplyAdaptiveGraphicsState).
	// Re-apply whenever the map becomes Foggrounds or leaves it: the initial graphics pass can run
	// before ActiveLevelName has replicated (and on late-join/direct-load), and leaving must restore
	// the normal quality-gated value.
	const bool bFoggroundsActiveNow = BHGS->ActiveLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	if (bFoggroundsActiveNow != bFoggroundsVolumetricActive)
	{
		bFoggroundsVolumetricActive = bFoggroundsActiveNow;
		ApplyAdaptiveGraphicsState(false);
	}

	const EBHRoundPhase CurrentPhase = BHGS->RoundPhase;
	if (!bRoundPhaseObserved)
	{
		LastObservedRoundPhase = CurrentPhase;
		bRoundPhaseObserved = true;
		return;
	}

	if (LastObservedRoundPhase == EBHRoundPhase::Lobby && CurrentPhase != EBHRoundPhase::Lobby)
	{
		if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
		{
			BHGI->LogAutomationMarkerOnce(TEXT("ROUND_UI_CLOSED"));
		}
		if (MainMenuWidget.IsValid())
		{
			HideMainMenu();
		}
		if (ClassroomBoardWindow.IsValid())
		{
			HideClassroomBoard();
		}
		ShowLocalStatusMessage(TEXT("Round started. Gameplay controls are active. Press Escape for menu or B for board."), 4.0f);
	}

	// When a round resolves to a win, offer the quick end-of-round survey. Arming here (rather than
	// only from ClientRecordRoundResult) keeps the prompt reliable regardless of RPC ordering.
	const bool bEnteredResult = (CurrentPhase == EBHRoundPhase::SurvivorsWin || CurrentPhase == EBHRoundPhase::HunterWin)
		&& LastObservedRoundPhase != EBHRoundPhase::SurvivorsWin
		&& LastObservedRoundPhase != EBHRoundPhase::HunterWin;
	if (bEnteredResult)
	{
		if (UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr)
		{
			Feedback->ArmEndOfRoundSurvey();
			if (Feedback->ShouldAutoPromptEndOfRoundSurvey())
			{
				Feedback->MarkEndOfRoundSurveyShown();
				ShowLocalStatusMessage(TEXT("Round over. Take a quick survey, or press Escape to dismiss."), 6.0f);
				if (!MainMenuWidget.IsValid())
				{
					ShowMainMenu();
				}
			}
		}
	}

	LastObservedRoundPhase = CurrentPhase;
}

void ABHPlayerController::EnsureAudioPreferencesLoaded()
{
	if (bAudioPreferencesLoaded)
	{
		return;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	MasterVolume = BHLoadAudioPreference(TEXT("MasterVolume"), Settings ? Settings->DefaultMasterVolume : 1.0f);
	MusicVolume = BHLoadAudioPreference(TEXT("MusicVolume"), Settings ? Settings->DefaultMusicVolume : 0.85f);
	UiVolume = BHLoadAudioPreference(TEXT("UiVolume"), Settings ? Settings->DefaultUiVolume : 0.9f);
	bAudioPreferencesLoaded = true;
}

void ABHPlayerController::SaveAudioPreference(const TCHAR* Key, float Value) const
{
	if (!Key || !GConfig)
	{
		return;
	}

	GConfig->SetFloat(BHAudioConfigSection, Key, BHClampVolume(Value), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ABHPlayerController::EnsureComfortPreferencesLoaded()
{
	if (bComfortPreferencesLoaded)
	{
		return;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	bReducedJumpscares = BHLoadComfortPreference(TEXT("ReducedJumpscares"), Settings ? Settings->bDefaultReducedJumpscares : false);
	bReducedFlash = BHLoadComfortPreference(TEXT("ReducedFlash"), Settings ? Settings->bDefaultReducedFlash : false);
	bReducedCameraShake = BHLoadComfortPreference(TEXT("ReducedCameraShake"), Settings ? Settings->bDefaultReducedCameraShake : false);
	bCaptionsEnabled = BHLoadComfortPreference(TEXT("Captions"), Settings ? Settings->bDefaultCaptions : true);
	bHighContrastHud = BHLoadComfortPreference(TEXT("HighContrastHud"), Settings ? Settings->bDefaultHighContrastHud : false);
	HudScale = FMath::Clamp(BHLoadComfortScalarPreference(TEXT("HudScale"), Settings ? Settings->DefaultHudScale : 1.0f), 0.75f, 1.5f);
	HudPanelOpacity = FMath::Clamp(BHLoadComfortScalarPreference(TEXT("HudPanelOpacity"), Settings ? Settings->DefaultHudPanelOpacity : 1.0f), 0.35f, 1.0f);
	bColorblindHud = BHLoadComfortPreference(TEXT("ColorblindHud"), Settings ? Settings->bDefaultColorblindHud : false);
	bShowHudMinimap = BHLoadComfortPreference(TEXT("ShowMinimap"), Settings ? Settings->bDefaultShowMinimap : true);
	bShowHudNameplates = BHLoadComfortPreference(TEXT("ShowNameplates"), Settings ? Settings->bDefaultShowNameplates : true);
	bShowHudVitals = BHLoadComfortPreference(TEXT("ShowVitals"), Settings ? Settings->bDefaultShowVitals : true);
	bShowHudObjectiveBeats = BHLoadComfortPreference(TEXT("ShowObjectiveBeats"), Settings ? Settings->bDefaultShowObjectiveBeats : true);
	bShowHudThreatArrow = BHLoadComfortPreference(TEXT("ShowThreatArrow"), Settings ? Settings->bDefaultShowThreatArrow : true);
	bShowHudCrosshairDanger = BHLoadComfortPreference(TEXT("ShowCrosshairDanger"), Settings ? Settings->bDefaultShowCrosshairDanger : true);
	bComfortPreferencesLoaded = true;
}

void ABHPlayerController::SaveComfortPreference(const TCHAR* Key, bool bValue) const
{
	if (!Key || !GConfig)
	{
		return;
	}

	GConfig->SetBool(BHComfortConfigSection, Key, bValue, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ABHPlayerController::SaveComfortScalarPreference(const TCHAR* Key, float Value) const
{
	if (!Key || !GConfig)
	{
		return;
	}

	GConfig->SetFloat(BHComfortConfigSection, Key, Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ABHPlayerController::EnsureAmbientMusic()
{
	UWorld* World = GetWorld();
	if (!World || !IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	EnsureAudioPreferencesLoaded();
	USoundBase* Sound = GetAmbientMusicSound();
	if (!Sound)
	{
		return;
	}

	if (!AmbientMusicComponent)
	{
		AmbientMusicComponent = NewObject<UAudioComponent>(this, TEXT("AmbientMusicComponent"));
		if (AmbientMusicComponent)
		{
			AmbientMusicComponent->bAutoActivate = false;
			AmbientMusicComponent->bAutoDestroy = false;
			AmbientMusicComponent->bStopWhenOwnerDestroyed = true;
			AmbientMusicComponent->bAllowSpatialization = false;
			AmbientMusicComponent->bAlwaysPlay = true;
			AmbientMusicComponent->bIsMusic = true;
			AmbientMusicComponent->SetUISound(true);
			AmbientMusicComponent->SetSound(Sound);
			AddInstanceComponent(AmbientMusicComponent);
			AmbientMusicComponent->RegisterComponentWithWorld(World);
		}
	}

	if (AmbientMusicComponent)
	{
		ApplyAmbientMusicVolume();
		if (!AmbientMusicComponent->IsPlaying())
		{
			AmbientMusicComponent->Play();
			bAmbientMusicStarted = true;
		}
	}
}

void ABHPlayerController::UpdateAmbientMusic()
{
	if (!IsLocalController())
	{
		return;
	}

	const bool bShouldPlay = ShouldPlayAmbientMusic();
	if (bShouldPlay)
	{
		EnsureAmbientMusic();
	}

	if (AmbientMusicComponent)
	{
		ApplyAmbientMusicVolume();
		if (bShouldPlay)
		{
			if (!AmbientMusicComponent->IsPlaying())
			{
				AmbientMusicComponent->Play();
				bAmbientMusicStarted = true;
			}
		}
		else if (AmbientMusicComponent->IsPlaying())
		{
			AmbientMusicComponent->Stop();
			bAmbientMusicStarted = false;
		}
	}
}

bool ABHPlayerController::ShouldPlayAmbientMusic() const
{
	const UWorld* World = GetWorld();
	if (!World || !IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const bool bMainMenuOutsideMatch = MainMenuWidget.IsValid() && World->GetNetMode() == NM_Standalone;
	if (bMainMenuOutsideMatch)
	{
		return true;
	}

	const ABHGameState* BHGS = World->GetGameState<ABHGameState>();
	return BHGS && BHGS->RoundPhase == EBHRoundPhase::Lobby;
}

void ABHPlayerController::ApplyAmbientMusicVolume()
{
	if (AmbientMusicComponent)
	{
		AmbientMusicComponent->SetVolumeMultiplier(GetEffectiveMusicVolume());
	}
}

float ABHPlayerController::GetEffectiveMusicVolume() const
{
	return BHClampVolume(MasterVolume) * BHClampVolume(MusicVolume);
}

float ABHPlayerController::GetEffectiveUiVolume() const
{
	return BHClampVolume(MasterVolume) * BHClampVolume(UiVolume);
}

USoundBase* ABHPlayerController::GetAmbientMusicSound()
{
	if (!AmbientMusicSound)
	{
		AmbientMusicSound = LoadObject<USoundBase>(nullptr, BHAmbientMusicAssetPath);
		if (USoundWave* SoundWave = Cast<USoundWave>(AmbientMusicSound))
		{
			SoundWave->bLooping = true;
		}
		if (!AmbientMusicSound)
		{
			UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt audio: missing lobby music asset %s"), BHAmbientMusicAssetPath);
		}
	}

	return AmbientMusicSound;
}

USoundBase* ABHPlayerController::GetMenuSelectionSound()
{
	if (!MenuSelectionSound)
	{
		MenuSelectionSound = LoadObject<USoundBase>(nullptr, BHMenuClickAssetPath);
		if (!MenuSelectionSound)
		{
			UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt audio: missing menu click asset %s"), BHMenuClickAssetPath);
		}
	}

	return MenuSelectionSound;
}

void ABHPlayerController::PlayMenuSelectionSound()
{
	UWorld* World = GetWorld();
	if (!World || !IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float Now = World->GetRealTimeSeconds();
	if (Now - LastMenuSelectionSoundTime < 0.045f)
	{
		return;
	}

	EnsureAudioPreferencesLoaded();
	LastMenuSelectionSoundTime = Now;

	if (USoundBase* Sound = GetMenuSelectionSound())
	{
		UGameplayStatics::PlaySound2D(this, Sound, GetEffectiveUiVolume(), 1.0f, 0.0f, nullptr, this, true);
	}
}

float ABHPlayerController::GetMasterVolumeForMenu() const
{
	return BHClampVolume(MasterVolume);
}

float ABHPlayerController::GetMusicVolumeForMenu() const
{
	return BHClampVolume(MusicVolume);
}

float ABHPlayerController::GetUiVolumeForMenu() const
{
	return BHClampVolume(UiVolume);
}

void ABHPlayerController::SetMasterVolumeForMenu(float Volume)
{
	EnsureAudioPreferencesLoaded();
	MasterVolume = BHClampVolume(Volume);
	SaveAudioPreference(TEXT("MasterVolume"), MasterVolume);
	ApplyAmbientMusicVolume();
}

void ABHPlayerController::SetMusicVolumeForMenu(float Volume)
{
	EnsureAudioPreferencesLoaded();
	MusicVolume = BHClampVolume(Volume);
	SaveAudioPreference(TEXT("MusicVolume"), MusicVolume);
	ApplyAmbientMusicVolume();
}

void ABHPlayerController::SetUiVolumeForMenu(float Volume)
{
	EnsureAudioPreferencesLoaded();
	UiVolume = BHClampVolume(Volume);
	SaveAudioPreference(TEXT("UiVolume"), UiVolume);
}

bool ABHPlayerController::SetComfortOptionForMenu(FName OptionName, bool bEnabled, FString& OutMessage)
{
	EnsureComfortPreferencesLoaded();

	const FString Option = OptionName.ToString();
	if (Option.Equals(TEXT("ReducedJumpscares"), ESearchCase::IgnoreCase))
	{
		bReducedJumpscares = bEnabled;
		SaveComfortPreference(TEXT("ReducedJumpscares"), bReducedJumpscares);
		OutMessage = bReducedJumpscares ? TEXT("Reduced jumpscares enabled.") : TEXT("Reduced jumpscares disabled.");
	}
	else if (Option.Equals(TEXT("ReducedFlash"), ESearchCase::IgnoreCase))
	{
		bReducedFlash = bEnabled;
		SaveComfortPreference(TEXT("ReducedFlash"), bReducedFlash);
		OutMessage = bReducedFlash ? TEXT("Reduced flash enabled.") : TEXT("Reduced flash disabled.");
	}
	else if (Option.Equals(TEXT("ReducedCameraShake"), ESearchCase::IgnoreCase))
	{
		bReducedCameraShake = bEnabled;
		SaveComfortPreference(TEXT("ReducedCameraShake"), bReducedCameraShake);
		OutMessage = bReducedCameraShake ? TEXT("Reduced camera shake enabled.") : TEXT("Reduced camera shake disabled.");
	}
	else if (Option.Equals(TEXT("Captions"), ESearchCase::IgnoreCase))
	{
		bCaptionsEnabled = bEnabled;
		SaveComfortPreference(TEXT("Captions"), bCaptionsEnabled);
		OutMessage = bCaptionsEnabled ? TEXT("Captions enabled.") : TEXT("Captions disabled.");
	}
	else if (Option.Equals(TEXT("HighContrastHud"), ESearchCase::IgnoreCase))
	{
		bHighContrastHud = bEnabled;
		SaveComfortPreference(TEXT("HighContrastHud"), bHighContrastHud);
		OutMessage = bHighContrastHud ? TEXT("High contrast HUD enabled.") : TEXT("High contrast HUD disabled.");
	}
	else if (Option.Equals(TEXT("ColorblindHud"), ESearchCase::IgnoreCase))
	{
		bColorblindHud = bEnabled;
		SaveComfortPreference(TEXT("ColorblindHud"), bColorblindHud);
		OutMessage = bColorblindHud ? TEXT("Colorblind-safe HUD enabled.") : TEXT("Colorblind-safe HUD disabled.");
	}
	else if (Option.Equals(TEXT("ShowMinimap"), ESearchCase::IgnoreCase))
	{
		bShowHudMinimap = bEnabled;
		SaveComfortPreference(TEXT("ShowMinimap"), bShowHudMinimap);
		OutMessage = bShowHudMinimap ? TEXT("HUD minimap shown.") : TEXT("HUD minimap hidden.");
	}
	else if (Option.Equals(TEXT("ShowNameplates"), ESearchCase::IgnoreCase))
	{
		bShowHudNameplates = bEnabled;
		SaveComfortPreference(TEXT("ShowNameplates"), bShowHudNameplates);
		OutMessage = bShowHudNameplates ? TEXT("HUD nameplates shown.") : TEXT("HUD nameplates hidden.");
	}
	else if (Option.Equals(TEXT("ShowVitals"), ESearchCase::IgnoreCase))
	{
		bShowHudVitals = bEnabled;
		SaveComfortPreference(TEXT("ShowVitals"), bShowHudVitals);
		OutMessage = bShowHudVitals ? TEXT("HUD vitals shown.") : TEXT("HUD vitals hidden.");
	}
	else if (Option.Equals(TEXT("ShowObjectiveBeats"), ESearchCase::IgnoreCase))
	{
		bShowHudObjectiveBeats = bEnabled;
		SaveComfortPreference(TEXT("ShowObjectiveBeats"), bShowHudObjectiveBeats);
		OutMessage = bShowHudObjectiveBeats ? TEXT("HUD objectives shown.") : TEXT("HUD objectives hidden.");
	}
	else if (Option.Equals(TEXT("ShowThreatArrow"), ESearchCase::IgnoreCase))
	{
		bShowHudThreatArrow = bEnabled;
		SaveComfortPreference(TEXT("ShowThreatArrow"), bShowHudThreatArrow);
		OutMessage = bShowHudThreatArrow ? TEXT("HUD threat arrow shown.") : TEXT("HUD threat arrow hidden.");
	}
	else if (Option.Equals(TEXT("ShowCrosshairDanger"), ESearchCase::IgnoreCase))
	{
		bShowHudCrosshairDanger = bEnabled;
		SaveComfortPreference(TEXT("ShowCrosshairDanger"), bShowHudCrosshairDanger);
		OutMessage = bShowHudCrosshairDanger ? TEXT("Crosshair alert shown.") : TEXT("Crosshair alert hidden.");
	}
	else
	{
		OutMessage = FString::Printf(TEXT("Unknown comfort option: %s."), *Option);
		ShowLocalStatusMessage(OutMessage, 3.0f);
		return false;
	}

	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetHudScalarOptionForMenu(FName OptionName, float Value, FString& OutMessage)
{
	EnsureComfortPreferencesLoaded();

	const FString Option = OptionName.ToString();
	if (Option.Equals(TEXT("HudScale"), ESearchCase::IgnoreCase))
	{
		HudScale = FMath::Clamp(Value, 0.75f, 1.5f);
		SaveComfortScalarPreference(TEXT("HudScale"), HudScale);
		OutMessage = FString::Printf(TEXT("HUD size %d%%."), FMath::RoundToInt(HudScale * 100.0f));
	}
	else if (Option.Equals(TEXT("HudPanelOpacity"), ESearchCase::IgnoreCase))
	{
		HudPanelOpacity = FMath::Clamp(Value, 0.35f, 1.0f);
		SaveComfortScalarPreference(TEXT("HudPanelOpacity"), HudPanelOpacity);
		OutMessage = FString::Printf(TEXT("HUD panel opacity %d%%."), FMath::RoundToInt(HudPanelOpacity * 100.0f));
	}
	else
	{
		OutMessage = FString::Printf(TEXT("Unknown HUD option: %s."), *Option);
		return false;
	}

	return true;
}

float ABHPlayerController::GetHudScalarOptionForMenu(FName OptionName) const
{
	const FString Option = OptionName.ToString();
	if (Option.Equals(TEXT("HudScale"), ESearchCase::IgnoreCase))
	{
		return HudScale;
	}
	if (Option.Equals(TEXT("HudPanelOpacity"), ESearchCase::IgnoreCase))
	{
		return HudPanelOpacity;
	}
	return 1.0f;
}

bool ABHPlayerController::IsComfortOptionEnabledForMenu(FName OptionName) const
{
	const FString Option = OptionName.ToString();
	if (Option.Equals(TEXT("ReducedJumpscares"), ESearchCase::IgnoreCase))
	{
		return bReducedJumpscares;
	}
	if (Option.Equals(TEXT("ReducedFlash"), ESearchCase::IgnoreCase))
	{
		return bReducedFlash;
	}
	if (Option.Equals(TEXT("ReducedCameraShake"), ESearchCase::IgnoreCase))
	{
		return bReducedCameraShake;
	}
	if (Option.Equals(TEXT("Captions"), ESearchCase::IgnoreCase))
	{
		return bCaptionsEnabled;
	}
	if (Option.Equals(TEXT("HighContrastHud"), ESearchCase::IgnoreCase))
	{
		return bHighContrastHud;
	}
	if (Option.Equals(TEXT("ColorblindHud"), ESearchCase::IgnoreCase))
	{
		return bColorblindHud;
	}
	if (Option.Equals(TEXT("ShowMinimap"), ESearchCase::IgnoreCase))
	{
		return bShowHudMinimap;
	}
	if (Option.Equals(TEXT("ShowNameplates"), ESearchCase::IgnoreCase))
	{
		return bShowHudNameplates;
	}
	if (Option.Equals(TEXT("ShowVitals"), ESearchCase::IgnoreCase))
	{
		return bShowHudVitals;
	}
	if (Option.Equals(TEXT("ShowObjectiveBeats"), ESearchCase::IgnoreCase))
	{
		return bShowHudObjectiveBeats;
	}
	if (Option.Equals(TEXT("ShowThreatArrow"), ESearchCase::IgnoreCase))
	{
		return bShowHudThreatArrow;
	}
	if (Option.Equals(TEXT("ShowCrosshairDanger"), ESearchCase::IgnoreCase))
	{
		return bShowHudCrosshairDanger;
	}
	return false;
}

bool ABHPlayerController::AreCaptionsEnabled() const
{
	return bCaptionsEnabled;
}

bool ABHPlayerController::IsHighContrastHudEnabled() const
{
	return bHighContrastHud;
}

bool ABHPlayerController::IsReducedFlashEnabled() const
{
	return bReducedFlash;
}

bool ABHPlayerController::IsReducedJumpscaresEnabled() const
{
	return bReducedJumpscares;
}

float ABHPlayerController::GetHudScale() const
{
	return HudScale;
}

float ABHPlayerController::GetHudPanelOpacity() const
{
	return HudPanelOpacity;
}

bool ABHPlayerController::IsColorblindHudEnabled() const
{
	return bColorblindHud;
}

bool ABHPlayerController::IsHudMinimapVisible() const
{
	return bShowHudMinimap;
}

bool ABHPlayerController::IsHudNameplatesVisible() const
{
	return bShowHudNameplates;
}

bool ABHPlayerController::IsHudVitalsVisible() const
{
	return bShowHudVitals;
}

bool ABHPlayerController::IsHudObjectiveBeatsVisible() const
{
	return bShowHudObjectiveBeats;
}

bool ABHPlayerController::IsHudThreatArrowVisible() const
{
	return bShowHudThreatArrow;
}

bool ABHPlayerController::IsHudCrosshairDangerVisible() const
{
	return bShowHudCrosshairDanger;
}

bool ABHPlayerController::IsLocalHostAdminContext() const
{
	const UWorld* World = GetWorld();
	if (!IsLocalController() || !World)
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	return NetMode == NM_Standalone || NetMode == NM_ListenServer;
}

bool ABHPlayerController::RequireLocalHostAdmin(FString& OutMessage, const TCHAR* ActionDescription)
{
	if (IsLocalHostAdminContext())
	{
		return true;
	}

	const FString Action = ActionDescription && FCString::Strlen(ActionDescription) > 0
		? FString(ActionDescription)
		: FString(TEXT("use this classroom control"));
	OutMessage = FString::Printf(TEXT("Only the host machine can %s."), *Action);
	UE_LOG(LogTemp, Warning, TEXT("Denied local host-admin action '%s' from client controller %s."),
		*Action,
		*GetNameSafe(this));
	ShowLocalStatusMessage(OutMessage, 4.0f);
	return false;
}

void ABHPlayerController::CaptureAtmosphereConsoleDefaults()
{
	AtmosphereConsoleDefaultValues.Reset();
	const auto CaptureSpec = [this](const FBHAtmosphereSliderSpec& Spec)
	{
		const FName ParameterName(Spec.ParameterName);
		AtmosphereConsoleDefaultValues.Add(ParameterName, GetAtmosphereConsoleValue(ParameterName));
	};

	BHForEachAtmosphereSliderSpec(CaptureSpec);

	bAtmosphereConsoleDefaultsCaptured = true;
}

bool ABHPlayerController::TryOpenClassroomBoardWindow(FString& OutMessage)
{
	if (!RequireLocalHostAdmin(OutMessage, TEXT("open the classroom board")))
	{
		return false;
	}

	if (!FSlateApplication::IsInitialized())
	{
		OutMessage = TEXT("Slate is not ready to open a classroom board window.");
		return false;
	}

	if (ClassroomBoardWindow.IsValid())
	{
		ClassroomBoardWindow->BringToFront();
		OutMessage = TEXT("Classroom board is already open.");
		return true;
	}

	TSharedRef<SWindow> BoardWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Blackout Hunt Classroom Board")))
		.ClientSize(FVector2D(1280.0f, 720.0f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.SizingRule(ESizingRule::UserSized);

	BoardWindow->SetContent(
		SNew(SBHClassroomBoard)
		.PlayerController(TWeakObjectPtr<ABHPlayerController>(this))
		.bStandaloneWindow(true));
	BoardWindow->SetOnWindowClosed(FOnWindowClosed::CreateUObject(this, &ABHPlayerController::OnClassroomBoardWindowClosed));

	ClassroomBoardWindow = BoardWindow;
	FSlateApplication::Get().AddWindow(BoardWindow, true);

	OutMessage = TEXT("Classroom board opened in a separate window.");
	return true;
}

void ABHPlayerController::OnClassroomBoardWindowClosed(const TSharedRef<SWindow>& ClosedWindow)
{
	(void)ClosedWindow;
	ClassroomBoardWindow.Reset();
}

void ABHPlayerController::BindGameWindowCloseOverride()
{
	if (bGameWindowCloseOverrideBound || !IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	const TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
	if (!GameWindow.IsValid())
	{
		return;
	}

	GameWindow->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateUObject(this, &ABHPlayerController::OnGameWindowCloseRequested));
	bGameWindowCloseOverrideBound = true;
}

void ABHPlayerController::OnGameWindowCloseRequested(const TSharedRef<SWindow>& Window)
{
	(void)Window;
	RequestCleanQuit(TEXT("window-close"));
}

void ABHPlayerController::ApplyVirtualBoxSafeModeIfNeeded()
{
	if (bVirtualBoxSafeApplied || !IsLocalController())
	{
		return;
	}

	const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI || !BHGI->ShouldUseVirtualBoxSafeMode())
	{
		return;
	}

	bVirtualBoxSafeApplied = true;

	FString Message;
	ApplyGraphicsPresetInternal(0, false, Message);
	ApplyResolutionForMenu(1280, 720, false, Message);

	static const FBHConsoleVariableSetting VirtualBoxSafeSettings[] = {
		{ TEXT("r.MotionBlurQuality"), TEXT("0") },
		{ TEXT("r.RHICmdBypass"), TEXT("0") },
		{ TEXT("r.DynamicRes.OperationMode"), TEXT("0") },
		{ TEXT("r.ScreenPercentage"), TEXT("60") },
		{ TEXT("r.TextureStreaming"), TEXT("1") },
		{ TEXT("r.Streaming.UseFixedPoolSize"), TEXT("1") },
		{ TEXT("r.Streaming.PoolSize"), TEXT("384") },
		{ TEXT("r.Streaming.MaxTempMemoryAllowed"), TEXT("64") },
		{ TEXT("r.VolumetricFog"), TEXT("0") },
		{ TEXT("r.Nanite"), TEXT("0") },
		{ TEXT("r.RayTracing.ForceAllRayTracingEffects"), TEXT("0") },
		{ TEXT("t.MaxFPS"), TEXT("30") }
	};
	BHApplyConsoleVariables(this, VirtualBoxSafeSettings);

	ShowLocalStatusMessage(TEXT("VirtualBox-safe graphics applied: 1280x720 windowed low."), 4.0f);
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt VirtualBox-safe graphics applied."));
}

void ABHPlayerController::TickHorrorCueEffects(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// Environmental screen-dim: ease the player's whole view down to a low brightness while the creature
	// looms, then back up. Driven onto the camera-manager colour scale, so the self-lit monster stays the
	// brightest thing on screen and remains visible while the surroundings sink into the dark. Runs before the
	// jitter block below (which can early-return), so it ticks every frame until it expires.
	if (bHorrorEnvDimActive)
	{
		APlayerCameraManager* CamMgr = PlayerCameraManager;
		if (!CamMgr || Now >= HorrorEnvDimEndTime)
		{
			if (CamMgr)
			{
				CamMgr->ColorScale = FVector::OneVector;
				CamMgr->bEnableColorScaling = false;
			}
			bHorrorEnvDimActive = false;
		}
		else
		{
			const float Total = FMath::Max(0.05f, HorrorEnvDimEndTime - HorrorEnvDimStartTime);
			const float Elapsed = FMath::Max(0.0f, Now - HorrorEnvDimStartTime);
			constexpr float RampIn = 0.35f;
			constexpr float RampOut = 0.6f;
			float DimAlpha = 1.0f; // 0 = full brightness, 1 = fully dimmed
			if (Elapsed < RampIn)
			{
				DimAlpha = Elapsed / RampIn;
			}
			else if (Elapsed > Total - RampOut)
			{
				DimAlpha = FMath::Clamp((Total - Elapsed) / RampOut, 0.0f, 1.0f);
			}
			// Comfort: a player with reduced flash gets a much gentler dim (never below 70%).
			const float MinScale = bReducedFlash ? FMath::Max(HorrorEnvDimScale, 0.7f) : HorrorEnvDimScale;
			const float ColorScaleValue = FMath::Lerp(1.0f, MinScale, DimAlpha);
			CamMgr->bEnableColorScaling = true;
			CamMgr->ColorScale = FVector(ColorScaleValue, ColorScaleValue, ColorScaleValue);
		}
	}

	// "Behind you" held scare: spring the payoff once the player turns to face the presence, or when the
	// safety hold elapses (so a player who refuses to turn is never stuck). A short minimum arm time keeps
	// it from triggering instantly if they happened to already be facing that way when it armed.
	if (bBehindYouActive)
	{
		bool bSpring = Now >= BehindYouDeadline;
		if (!bSpring && Now >= BehindYouArmTime + 0.45f)
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			GetPlayerViewPoint(ViewLocation, ViewRotation);
			FVector ToFocus = BehindYouFocusLocation - ViewLocation;
			if (!ToFocus.IsNearlyZero())
			{
				ToFocus.Normalize();
				if (FVector::DotProduct(ViewRotation.Vector(), ToFocus) >= BehindYouTurnCosThreshold)
				{
					bSpring = true;
				}
			}
		}

		if (bSpring)
		{
			TriggerBehindYouPayoff();
		}
	}

	if (HorrorCueJitterIntensity <= 0.0f || Now >= HorrorCueJitterEndTime)
	{
		return;
	}

	const float Interval = 1.0f / FMath::Clamp(HorrorCueJitterFrequency, 8.0f, 90.0f);
	if (Now < HorrorCueNextJitterTime)
	{
		return;
	}

	HorrorCueNextJitterTime = Now + Interval;
	const float RemainingAlpha = FMath::Clamp((HorrorCueJitterEndTime - Now) / FMath::Max(0.1f, HorrorCueJitterEndTime - HorrorCueNextJitterTime + Interval), 0.0f, 1.0f);
	const float Shake = FMath::Clamp(HorrorCueJitterIntensity * RemainingAlpha, 0.0f, 1.0f);
	const FRotator CurrentRotation = GetControlRotation();
	SetControlRotation(CurrentRotation + FRotator(
		FMath::FRandRange(-0.55f, 0.55f) * Shake,
		FMath::FRandRange(-0.92f, 0.92f) * Shake,
		0.0f));
}

void ABHPlayerController::ClientDimEnvironment_Implementation(float DimScale, float DurationSeconds)
{
	if (!IsLocalController() || DurationSeconds <= 0.0f)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	HorrorEnvDimStartTime = Now;
	HorrorEnvDimEndTime = Now + FMath::Clamp(DurationSeconds, 0.3f, 8.0f);
	HorrorEnvDimScale = FMath::Clamp(DimScale, 0.1f, 1.0f);
	bHorrorEnvDimActive = true;
}

void ABHPlayerController::ScheduleAutomation()
{
	if (!IsLocalController())
	{
		return;
	}

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI || !BHGI->IsAutomationEnabled())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AutomationStartupTimerHandle, this, &ABHPlayerController::RunAutomationStartup, 1.0f, false);

		const float QuitSeconds = BHGI->GetAutomationQuitSeconds();
		if (QuitSeconds > 0.0f)
		{
			FTimerDelegate QuitDelegate;
			QuitDelegate.BindUObject(this, &ABHPlayerController::RequestCleanQuit, FString(TEXT("automation")));
			World->GetTimerManager().SetTimer(AutomationQuitTimerHandle, QuitDelegate, QuitSeconds, false);
		}
	}
}

void ABHPlayerController::RunAutomationStartup()
{
	if (bAutomationStartupRan || !IsLocalController())
	{
		return;
	}
	bAutomationStartupRan = true;

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI || !BHGI->IsAutomationEnabled())
	{
		return;
	}

	FString HostMode;
	if (BHGI->ConsumeAutomationHost(HostMode))
	{
		if (!FBHAutomationSupport::IsKnownHostMode(HostMode))
		{
			BHGI->LogAutomationMarker(FString::Printf(TEXT("FAIL:unknown host mode %s"), *HostMode));
			ShowLocalStatusMessage(FString::Printf(TEXT("Automation host failed: unknown mode %s."), *HostMode), 5.0f);
			return;
		}

		if (GetNetMode() != NM_Standalone)
		{
			BHGI->LogAutomationMarker(TEXT("FAIL:auto host requires standalone menu"));
			return;
		}

		FString Message;
		if (HostMode.Equals(TEXT("LiveClassroom"), ESearchCase::CaseSensitive))
		{
			HostLiveClassroomForMenu(Message);
		}
		else if (HostMode.Equals(TEXT("LiveFacility"), ESearchCase::CaseSensitive))
		{
			HostLiveClassroomForMenu(TEXT("Facility"), Message);
		}
		else if (HostMode.Equals(TEXT("LiveSubstation"), ESearchCase::CaseSensitive))
		{
			HostLiveClassroomForMenu(TEXT("Substation"), Message);
		}
		else if (HostMode.Equals(TEXT("LiveFoggrounds"), ESearchCase::CaseSensitive))
		{
			HostLiveClassroomForMenu(TEXT("Foggrounds"), Message);
		}
		else if (HostMode.Equals(TEXT("Substation"), ESearchCase::CaseSensitive))
		{
			HostSubstationGame();
		}
		else if (HostMode.Equals(TEXT("Foggrounds"), ESearchCase::CaseSensitive))
		{
			HostFoggroundsGame();
		}
		else
		{
			HostGame();
		}
		return;
	}

	FString JoinAddress;
	if (BHGI->ConsumeAutomationJoin(JoinAddress))
	{
		const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(JoinAddress);
		if (NormalizedAddress.IsEmpty())
		{
			BHGI->LogAutomationMarker(TEXT("FAIL:invalid join address"));
			ShowLocalStatusMessage(TEXT("Automation join failed: invalid host address."), 5.0f);
			return;
		}

		BHGI->LogAutomationMarkerOnce(TEXT("JOIN_ATTEMPT"));
		bAutomationJoinAttempted = true;
		AutomationJoinAttemptTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		JoinGame(NormalizedAddress);
	}
}

void ABHPlayerController::RunAutomationAtmosphereTests()
{
	if (bAutomationAtmosphereTestsRan || !IsLocalController())
	{
		return;
	}

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI || !BHGI->IsAutomationEnabled())
	{
		return;
	}

	const FBHAutomationConfig& AutomationConfig = BHGI->GetAutomationConfig();
	if (!AutomationConfig.HasAutoAtmosphereTests())
	{
		return;
	}

	UWorld* World = GetWorld();
	const AGameStateBase* BaseGameState = World ? World->GetGameState() : nullptr;
	const int32 CurrentPlayers = FMath::Max(
		BaseGameState ? BaseGameState->PlayerArray.Num() : 0,
		BHCountConnectedPlayerControllers(World));
	const int32 RequiredPlayers = AutomationConfig.GetAutoMinPlayers();
	if (RequiredPlayers > 0 && CurrentPlayers < RequiredPlayers)
	{
		BHGI->LogAutomationMarkerOnce(FString::Printf(TEXT("WAITING_FOR_ATMOSPHERE_PLAYERS:%d/%d"), CurrentPlayers, RequiredPlayers));
		return;
	}

	bAutomationAtmosphereTestsRan = true;
	TArray<FString> Commands;
	AutomationConfig.AutoAtmosphereTests.ParseIntoArray(Commands, TEXT(","), true);
	for (FString& Command : Commands)
	{
		Command.TrimStartAndEndInline();
		if (Command.IsEmpty())
		{
			continue;
		}

		BHGI->LogAutomationMarker(FString::Printf(TEXT("ATMOSPHERE_TEST:%s"), *Command));
		ServerAtmosphereTest(Command);
	}
}

void ABHPlayerController::TickAutomation()
{
	if (!IsLocalController())
	{
		return;
	}

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI || !BHGI->IsAutomationEnabled())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (BHGI->ShouldRequestAutomationCleanExit())
	{
		RequestCleanQuit(TEXT("automation"));
		return;
	}

	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_ListenServer)
	{
		BHGI->LogAutomationMarkerOnce(TEXT("HOST_LISTENING"));
		RunAutomationAtmosphereTests();
	}

	if (!bAutomationJoinedLogged && NetMode == NM_Client && PlayerState)
	{
		bAutomationJoinedLogged = true;
		BHGI->LogAutomationMarkerOnce(TEXT("JOINED"));
	}

	if (bAutomationJoinAttempted && !bAutomationJoinedLogged && AutomationJoinAttemptTime >= 0.0f)
	{
		const float SecondsSinceAttempt = World->GetTimeSeconds() - AutomationJoinAttemptTime;
		if (SecondsSinceAttempt > 30.0f && NetMode != NM_Client)
		{
			bAutomationJoinedLogged = true;
			BHGI->LogAutomationMarkerOnce(TEXT("FAIL:join timeout"));
		}
	}

	ABHGameState* BHGS = World->GetGameState<ABHGameState>();
	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (BHGI->ShouldAutoReady() && BHGS && BHPS && BHGS->RoundPhase == EBHRoundPhase::Lobby && !BHPS->bReady)
	{
		const int32 AutoMinPlayers = BHGI->GetAutomationMinReadyPlayers();
		const AGameStateBase* BaseGameState = World->GetGameState();
		const int32 CurrentPlayers = BaseGameState ? BaseGameState->PlayerArray.Num() : 0;
		if (AutoMinPlayers > 0 && NetMode == NM_ListenServer && CurrentPlayers < AutoMinPlayers)
		{
			BHGI->LogAutomationMarkerOnce(FString::Printf(TEXT("WAITING_FOR_PLAYERS:%d/%d"), CurrentPlayers, AutoMinPlayers));
			return;
		}

		ServerSetReady(true);
		bAutomationReadyLogged = true;
		BHGI->LogAutomationMarkerOnce(TEXT("READY_SET"));
	}

	if (!bAutomationRoundStartedLogged && BHGS && BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		bAutomationRoundStartedLogged = true;
		BHGI->LogAutomationMarkerOnce(TEXT("ROUND_STARTED"));
	}
}

void ABHPlayerController::RunClassroomNetworkPreflight()
{
	if (bClassroomPreflightReported || !IsLocalController() || GetNetMode() != NM_ListenServer)
	{
		return;
	}

	bClassroomPreflightReported = true;
	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	const FString JoinAddress = BHGI ? BHGI->GetPreferredClassroomJoinAddress(7777) : FBHNetworkSupport::ResolveLocalJoinAddress(7777);
	if (BHGI && BHGI->IsAutomationEnabled())
	{
		BHGI->LogAutomationMarkerOnce(FString::Printf(TEXT("JOIN_ADDRESS:%s"), *JoinAddress));
	}
	const FString Status = FString::Printf(
		TEXT("Classroom Playit join address: %s. LAN/direct IP is available from Host LAN only."),
		*JoinAddress);
	ShowLocalStatusMessage(Status, 12.0f);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Status);
}

void ABHPlayerController::RunClassroomFallbackCheck()
{
	if (bClassroomFallbackStarted || !IsLocalController() || GetNetMode() != NM_ListenServer)
	{
		return;
	}

	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const int32 ConnectedPlayers = BaseGameState ? BaseGameState->PlayerArray.Num() : 1;
	if (ConnectedPlayers > 1)
	{
		ShowLocalStatusMessage(TEXT("Classroom network ready: student client reached the host."), 6.0f);
		UE_LOG(LogTemp, Display, TEXT("Classroom network ready: student client reached the host."));
		return;
	}

	bClassroomFallbackStarted = true;
	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	FString TunnelMessage;
	const bool bTunnelStarted = BHGI ? BHGI->TryStartInternetTunnel(TunnelMessage, 7777) : false;
	const FBHInternetTunnelResult TunnelStatus = FBHNetworkSupport::GetInternetTunnelStatus(7777);
	const FString ConfiguredClassroomAddress = BHGI ? BHGI->GetConfiguredClassroomJoinAddress(7777) : FString();
	FString Status;
	if (TunnelStatus.bTunnelReady)
	{
		if (BHGI)
		{
			BHGI->SetPublicJoinAddress(ConfiguredClassroomAddress.IsEmpty() ? TunnelStatus.TunnelAddress : ConfiguredClassroomAddress);
		}
		const FString JoinAddress = ConfiguredClassroomAddress.IsEmpty() ? TunnelStatus.TunnelAddress : ConfiguredClassroomAddress;
		Status = FString::Printf(TEXT("Classroom Playit tunnel ready: students join %s. %s"), *JoinAddress, *TunnelStatus.Message);
	}
	else if (bTunnelStarted)
	{
		const FString JoinAddress = BHGI ? BHGI->GetPreferredClassroomJoinAddress(7777) : ConfiguredClassroomAddress;
		Status = FString::Printf(TEXT("Classroom Playit endpoint: %s. Tunnel status: %s"), *JoinAddress, *TunnelStatus.Message);
	}
	else
	{
		const FString JoinAddress = BHGI ? BHGI->GetPreferredClassroomJoinAddress(7777) : ConfiguredClassroomAddress;
		Status = FString::Printf(TEXT("Classroom Playit endpoint: %s. Tunnel setup pending: %s"), *JoinAddress, *TunnelMessage);
	}

	ShowLocalStatusMessage(Status, bTunnelStarted ? 15.0f : 12.0f);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Status);
}

void ABHPlayerController::RequestCleanQuit(FString Reason)
{
	if (bCleanQuitRequested)
	{
		return;
	}
	bCleanQuitRequested = true;

	HideClassroomBoard();

	if (AmbientMusicComponent)
	{
		AmbientMusicComponent->Stop();
		AmbientMusicComponent->DestroyComponent();
		AmbientMusicComponent = nullptr;
		bAmbientMusicStarted = false;
	}

	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->RequestCleanExit(Reason);
		return;
	}

	FPlatformMisc::RequestExit(false);
}

bool ABHPlayerController::AllowLobbyActionRpc(float MinIntervalSeconds)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastLobbyActionServerTime < MinIntervalSeconds)
	{
		return false;
	}
	LastLobbyActionServerTime = Now;
	return true;
}

void ABHPlayerController::ServerSetReady_Implementation(bool bReady)
{
	// Flood-guard like every other spammable lobby RPC (display name, map/fog vote, avatar/cosmetics);
	// without it a modified client can toggle ready/unready unbounded, forcing repeated AreAllReady() scans
	// and reliable replication churn.
	if (!AllowLobbyActionRpc())
	{
		return;
	}
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetPlayerReady(this, bReady);
	}
}

void ABHPlayerController::ServerSetPlayerDisplayName_Implementation(const FString& DisplayName)
{
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	const FString CleanDisplayName = BHSanitizeDisplayName(DisplayName);
	if (!BHIsUsefulDisplayName(CleanDisplayName))
	{
		return;
	}

	if (APlayerState* BasePlayerState = GetPlayerState<APlayerState>())
	{
		BasePlayerState->SetPlayerName(CleanDisplayName);
	}
}

void ABHPlayerController::ServerForceStartRound_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->ForceStartRound(this);
	}
}

void ABHPlayerController::ServerSetDesiredRole_Implementation(APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetDesiredRole(this, TargetPlayerState, DesiredRole);
	}
}

void ABHPlayerController::ServerKickPlayer_Implementation(APlayerState* TargetPlayerState)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->KickPlayer(this, TargetPlayerState);
	}
}

void ABHPlayerController::ServerSetNextLevel_Implementation(const FString& LevelName)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetNextLevel(this, LevelName);
	}
}

void ABHPlayerController::ServerSetAvatar_Implementation(int32 AvatarIndex)
{
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetPlayerAvatar(this, AvatarIndex);
	}
}

void ABHPlayerController::ServerSetAvatarColor_Implementation(const FLinearColor& AvatarColor, int32 ColorIndex)
{
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS)
	{
		return;
	}

	BHPS->SetAvatarColor(BHSanitizeAvatarColor(AvatarColor));
	if (ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
	{
		ControlledCharacter->ApplyAvatarStyle();
	}

	ClientShowStatusMessage(FString::Printf(TEXT("Avatar color set to %d."), FMath::Clamp(ColorIndex, 0, 7) + 1), 2.5f);
}

void ABHPlayerController::ServerSetAvatarHeadwear_Implementation(int32 HeadwearIndex)
{
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS)
	{
		return;
	}

	const int32 NormalizedIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, HeadwearIndex);
	BHPS->SetAvatarHeadwearIndex(NormalizedIndex);
	if (ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
	{
		ControlledCharacter->ApplyAvatarStyle();
	}

	ClientShowStatusMessage(FString::Printf(TEXT("Headwear set to %s."), BHCosmeticItemName(EBHCosmeticCategory::Headwear, NormalizedIndex)), 2.5f);
}

void ABHPlayerController::ServerSetAvatarGear_Implementation(int32 GearIndex)
{
	(void)GearIndex;
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS)
	{
		return;
	}

	BHPS->SetAvatarGearIndex(0);
	if (ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
	{
		ControlledCharacter->ApplyAvatarStyle();
	}

	ClientShowStatusMessage(TEXT("Gear customization has been removed."), 2.5f);
}

void ABHPlayerController::ServerApplyAvatarCosmetics_Implementation(
	int32 AvatarIndex,
	const FLinearColor& AvatarColor,
	int32 ColorIndex,
	int32 HeadwearIndex,
	int32 GearIndex,
	bool bAnnounce)
{
	(void)GearIndex;
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS)
	{
		return;
	}

	const int32 NormalizedAvatarIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Outfit, AvatarIndex);
	const int32 NormalizedColorIndex = BHCosmeticClampIndex(EBHCosmeticCategory::ShirtColor, ColorIndex);
	const int32 NormalizedHeadwearIndex = BHCosmeticClampIndex(EBHCosmeticCategory::Headwear, HeadwearIndex);
	const int32 NormalizedGearIndex = 0;

	BHPS->SetAvatarIndex(NormalizedAvatarIndex);
	BHPS->SetAvatarColor(BHSanitizeAvatarColor(AvatarColor));
	BHPS->SetAvatarHeadwearIndex(NormalizedHeadwearIndex);
	BHPS->SetAvatarGearIndex(NormalizedGearIndex);

	if (ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
	{
		ControlledCharacter->ApplyAvatarStyle();
	}

	if (bAnnounce)
	{
		ClientShowStatusMessage(
			FString::Printf(
				TEXT("Cosmetics applied: %s, %s."),
				BHCosmeticItemName(EBHCosmeticCategory::Outfit, NormalizedAvatarIndex),
				BHCosmeticItemName(EBHCosmeticCategory::Headwear, NormalizedHeadwearIndex)),
			2.5f);
	}
}

void ABHPlayerController::ServerSetMapVote_Implementation(const FString& LevelName)
{
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetMapVote(this, LevelName);
	}
}

void ABHPlayerController::ServerSetFogPresetVote_Implementation(EBHFogPreset FogPreset)
{
	if (!AllowLobbyActionRpc())
	{
		return;
	}

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetFogPresetVote(this, FogPreset);
	}
}

void ABHPlayerController::ServerSetFogPresetOverride_Implementation(EBHFogPreset FogPreset)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetFogPresetOverride(this, FogPreset);
	}
}

void ABHPlayerController::ServerClearFogPresetOverride_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->ClearFogPresetOverride(this);
	}
}

void ABHPlayerController::ServerSetTargetHunterCount_Implementation(int32 HunterCount)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetTargetHunterCount(this, HunterCount);
	}
}

void ABHPlayerController::ServerSetObjectiveIntensity_Implementation(int32 Intensity)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetObjectiveIntensity(this, Intensity);
	}
}

void ABHPlayerController::ServerSetBotCount_Implementation(int32 BotCount)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetBotCount(this, BotCount);
	}
}

void ABHPlayerController::ServerSetBotDifficulty_Implementation(EBHBotDifficulty Difficulty)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetBotDifficulty(this, Difficulty);
	}
}

void ABHPlayerController::ServerFillBots_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->FillBotsToCapacity(this);
	}
}

void ABHPlayerController::ServerBotStatus_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		if (!BHGM->RequireHostAdmin(this, TEXT("view bot status")))
		{
			return;
		}

		const FString Report = BHGM->GetBotStatusReport();
		UE_LOG(LogTemp, Log, TEXT("%s"), *Report);
		ClientShowStatusMessage(Report, 8.0f);
	}
}

void ABHPlayerController::ServerBotDumpMemory_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		if (!BHGM->RequireHostAdmin(this, TEXT("dump bot memory")))
		{
			return;
		}

		const FString Report = BHGM->GetBotMemoryReport();
		UE_LOG(LogTemp, Log, TEXT("%s"), *Report);
		ClientShowStatusMessage(Report, 10.0f);
	}
}

void ABHPlayerController::ServerBotNavCheck_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		if (!BHGM->RequireHostAdmin(this, TEXT("run bot navigation checks")))
		{
			return;
		}

		FString Summary;
		BHGM->RunBotNavCheck(Summary);
		ClientShowStatusMessage(Summary, 8.0f);
	}
}

void ABHPlayerController::ServerBotForceHunt_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->ForceBotHunt(this);
	}
}

void ABHPlayerController::ServerBotSoak_Implementation(const FString& LevelName, int32 Seconds, int32 BotCount)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->StartBotSoak(this, LevelName, Seconds, BotCount);
	}
}

void ABHPlayerController::ServerSetPhysicsTopics_Implementation(const FString& Topics)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetPhysicsTopics(this, Topics);
	}
}

void ABHPlayerController::ServerSetRevisionDifficultyMix_Implementation(const FString& DifficultyMix)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetRevisionDifficultyMix(this, DifficultyMix);
	}
}

void ABHPlayerController::ServerSetRevisionThresholds_Implementation(float ClassPercent, float IndividualPercent)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetRevisionThresholds(this, ClassPercent, IndividualPercent);
	}
}

void ABHPlayerController::ServerSetRevisionRoundDuration_Implementation(int32 RoundSeconds)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetRevisionRoundDuration(this, RoundSeconds);
	}
}

void ABHPlayerController::ServerSetScareIntensity_Implementation(int32 Intensity)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetScareIntensity(this, Intensity);
	}
}

void ABHPlayerController::ServerForceReview_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->ForceReview(this);
	}
}

void ABHPlayerController::ServerTesterGrantTrainResources_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TesterGrantTrainResources(this);
	}
}

void ABHPlayerController::ServerTesterOpenTrainIntermission_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TesterOpenTrainIntermission(this);
	}
}

void ABHPlayerController::ServerTesterAdvanceTrainPhase_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TesterAdvanceTrainPhase(this);
	}
}

void ABHPlayerController::ServerTesterLoadFinalStation_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TesterLoadFinalStation(this);
	}
}

void ABHPlayerController::ServerTesterTriggerFinalEscape_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TesterTriggerFinalEscape(this);
	}
}

void ABHPlayerController::ServerTesterForceFinalRecap_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TesterForceFinalRecap(this);
	}
}

void ABHPlayerController::ServerRevisionStatus_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		if (!BHGM->RequireHostAdmin(this, TEXT("view revision status")))
		{
			return;
		}

		const FString Report = BHGM->GetRevisionStatusReport();
		UE_LOG(LogTemp, Log, TEXT("%s"), *Report);
		ClientShowStatusMessage(Report, 8.0f);
	}
}

void ABHPlayerController::ServerExportRevisionReport_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		FString Message;
		BHGM->ExportRevisionReport(this, Message);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	}
}

void ABHPlayerController::ServerExportPlaytestTelemetry_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		FString Message;
		BHGM->ExportPlaytestTelemetry(this, Message);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	}
}

void ABHPlayerController::ServerRequestSpectatorEncouragement_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->RequestSpectatorEncouragement(this);
	}
}

void ABHPlayerController::ServerSetSpectatorRolePreference_Implementation(EBHPlayerRole DesiredRole)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->QueueSpectatorRolePreference(this, DesiredRole);
	}
}

void ABHPlayerController::ServerToggleInfectionMode_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->ToggleInfectionMode(this);
	}
}

void ABHPlayerController::ServerTogglePaceMode_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TogglePaceMode(this);
	}
}

void ABHPlayerController::ServerSetPracticeRole_Implementation(EBHPlayerRole NewRole)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetPracticeRole(this, NewRole);
	}
}

void ABHPlayerController::ServerSetPracticeModifier_Implementation(EBHRoundModifier NewModifier)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetPracticeModifier(this, NewModifier);
	}
}

void ABHPlayerController::ServerRefreshPracticeRound_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->RefreshPracticeRound(this);
	}
}

void ABHPlayerController::ServerTriggerPracticeJumpscare_Implementation()
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TriggerPracticeJumpscare(this);
	}
}

void ABHPlayerController::ServerTriggerTargetedJumpscare_Implementation(APlayerState* TargetPlayerState)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->TriggerTargetedJumpscare(this, TargetPlayerState);
	}
}

void ABHPlayerController::ServerJumpscareTest_Implementation(const FString& VariantToken)
{
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->TestJumpscareVariant(this, VariantToken);
	}
}

void ABHPlayerController::ServerAtmosphereTest_Implementation(const FString& Command)
{
	ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
	if (!BHGM || !BHGM->RequireHostAdmin(this, TEXT("run atmosphere test commands")))
	{
		return;
	}

	ABHCharacter* Target = GetPawn<ABHCharacter>();
	const FVector Origin = Target ? Target->GetActorLocation() : FVector::ZeroVector;
	const FString Normalized = Command.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("targetclient") || Normalized == TEXT("clientcharge"))
	{
		ABHPlayerController* RemotePC = nullptr;
		ABHCharacter* RemoteTarget = nullptr;
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				ABHPlayerController* CandidatePC = Cast<ABHPlayerController>(It->Get());
				ABHCharacter* CandidateCharacter = CandidatePC ? CandidatePC->GetPawn<ABHCharacter>() : nullptr;
				if (CandidatePC && CandidatePC != this)
				{
					RemotePC = CandidatePC;
					RemoteTarget = CandidateCharacter;
					break;
				}
			}
		}

		if (RemoteTarget)
		{
			BHGM->TriggerManualScare(RemoteTarget, EBHScareEventType::MonsterCharge);
			ClientShowStatusMessage(TEXT("Atmosphere test: targeted client monster charge."), 2.5f);
		}
		else if (RemotePC)
		{
			FBHClientHorrorCue Cue;
			Cue.EventType = EBHScareEventType::MonsterCharge;
			Cue.FocusLocation = Origin + FVector(320.0f, 0.0f, 120.0f);
			Cue.Message = TEXT("Atmosphere test: targeted client horror cue.");
			Cue.DurationSeconds = 2.2f;
			Cue.LockSeconds = 1.15f;
			Cue.ShakeIntensity = 0.75f;
			Cue.CameraJitterDuration = 1.0f;
			Cue.CameraJitterFrequency = 28.0f;
			Cue.FlashIntensity = 0.85f;
			Cue.FlashColor = FLinearColor(1.0f, 0.08f, 0.04f, 1.0f);
			Cue.AudioVolume = 1.0f;
			Cue.bSnapToFocus = true;
			Cue.bLockInput = true;
			Cue.bCloseRangeFocus = true;
			RemotePC->ClientPlayHorrorCue(Cue);
			RemotePC->ClientShowStatusMessage(Cue.Message, 2.5f);
			BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Manual, Origin, Target, nullptr, 1.0f, TEXT("host targeted client automation cue"));
			ClientShowStatusMessage(TEXT("Atmosphere test: targeted remote controller horror cue."), 2.5f);
			UE_LOG(LogTemp, Display, TEXT("Atmosphere test: targeted remote client controller without pawn."));
		}
		else
		{
			ClientShowStatusMessage(TEXT("Atmosphere test: no remote client target found."), 3.5f);
		}
	}
	else if (Normalized == TEXT("ambient"))
	{
		BHGM->TriggerManualScare(Target, EBHScareEventType::Ambient);
		ClientShowStatusMessage(TEXT("Atmosphere test: ambient scare."), 2.5f);
	}
	else if (Normalized == TEXT("charge") || Normalized == TEXT("monster"))
	{
		BHGM->TriggerManualScare(Target, EBHScareEventType::MonsterCharge);
		ClientShowStatusMessage(TEXT("Atmosphere test: monster charge."), 2.5f);
	}
	else if (Normalized == TEXT("blackout"))
	{
		BHGM->TriggerBlackoutPulse(Origin, 2400.0f, 5.0f);
		ClientShowStatusMessage(TEXT("Atmosphere test: blackout pulse."), 2.5f);
	}
	else if (Normalized == TEXT("cctv"))
	{
		BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::CCTV, Origin, Target, Target, 1.0f, TEXT("host CCTV test"));
		FBHScareEventSpec Spec;
		Spec.EventType = EBHScareEventType::CCTVGlitch;
		Spec.Target = Target;
		Spec.Origin = Origin;
		Spec.Intensity = 0.9f;
		Spec.Message = TEXT("Security feed distortion: host test.");
		Spec.bBypassDirectorBudget = true;
		BHGM->TriggerAtmosphereCue(Spec);
		ClientShowStatusMessage(TEXT("Atmosphere test: CCTV glitch."), 2.5f);
	}
	else if (Normalized == TEXT("footstep") || Normalized == TEXT("noise"))
	{
		BHGM->ReportBotStimulus(EBHBotStimulusType::Noise, Origin, Target, Target, TEXT("host footstep/noise test"), 1.0f);
		BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Footstep, Origin, Target, Target, 1.0f, TEXT("host footstep/noise test"));
		ClientShowStatusMessage(TEXT("Atmosphere test: footstep/noise stimulus."), 2.5f);
	}
	else if (Normalized == TEXT("bots") || Normalized == TEXT("statetree"))
	{
		const FString Report = BHGM->GetBotMemoryReport();
		UE_LOG(LogTemp, Log, TEXT("%s"), *Report);
		ClientShowStatusMessage(Report.Left(420), 6.0f);
	}
	else
	{
		ClientShowStatusMessage(TEXT("Atmosphere tests are available from Escape > Round > Test Commands."), 5.0f);
	}
}

void ABHPlayerController::ClientShowStatusMessage_Implementation(const FString& Message, float DurationSeconds)
{
	ShowLocalStatusMessage(Message, DurationSeconds);
}

void ABHPlayerController::ClientReceiveReconnectToken_Implementation(const FString& Token)
{
	// Store the server-issued reconnect token in the GameInstance, which survives a disconnect and
	// return-to-menu within this process; UBHGameInstance::JoinGame echoes it in the rejoin URL.
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->SetClientReconnectToken(Token);
	}
}

void ABHPlayerController::ClientShowRoleIntro_Implementation(EBHPlayerRole InRole, bool bRevisionMode)
{
	RoleIntroRole = InRole;
	bRoleIntroRevisionMode = bRevisionMode;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	RoleIntroStartTime = Now;
	RoleIntroEndTime = Now + 5.5f;
}

void ABHPlayerController::ClientShowTutorialCard_Implementation(const FString& Title, const FString& Body, float Seconds)
{
	TutorialCardTitle = Title;
	TutorialCardBody = Body;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	TutorialCardStartTime = Now;
	TutorialCardEndTime = Now + FMath::Max(0.5f, Seconds);
}

void ABHPlayerController::ClientShowTutorialPrompt_Implementation(const FString& Message, float Seconds)
{
	TutorialPromptMessage = Message;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	TutorialPromptStartTime = Now;
	TutorialPromptEndTime = Now + FMath::Max(0.5f, Seconds);
}

void ABHPlayerController::ClientShowCCTVReveal_Implementation(ABHCharacter* RevealTarget, const FVector& RevealLocation, const FString& TargetName, float DurationSeconds)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const float NewDuration = FMath::Max(0.1f, DurationSeconds);
	const float NewEndTime = Now + NewDuration;
	const bool bSameActiveTarget = World
		&& Now < CCTVRevealEndTime
		&& CCTVRevealTarget.Get() == RevealTarget
		&& CCTVRevealTargetName.Equals(TargetName.IsEmpty() ? TEXT("Survivor") : TargetName, ESearchCase::IgnoreCase);
	const float ExistingEndTime = CCTVRevealEndTime;

	CCTVRevealLocation = RevealLocation;
	CCTVRevealTargetName = TargetName.IsEmpty() ? TEXT("Survivor") : TargetName;
	CCTVRevealTarget = RevealTarget;
	if (bSameActiveTarget && ExistingEndTime > NewEndTime)
	{
		CCTVRevealDuration = FMath::Max(CCTVRevealDuration, ExistingEndTime - CCTVRevealStartTime);
		CCTVRevealEndTime = ExistingEndTime;
		return;
	}

	CCTVRevealStartTime = Now;
	CCTVRevealDuration = NewDuration;
	CCTVRevealEndTime = NewEndTime;
}

void ABHPlayerController::LockJumpscareInput(float SafetySeconds, bool bLockLook)
{
	if (!IsLocalController())
	{
		return;
	}

	if (MainMenuWidget.IsValid())
	{
		HideMainMenu();
	}
	if (AtmosphereConsoleWidget.IsValid())
	{
		HideAtmosphereConsole();
	}
	// SetIgnoreMoveInput/SetIgnoreLookInput are COUNTED in the engine (each true increments, each false
	// decrements). Overlapping scares (a manual scare during the "behind you" hold, an organic director
	// beat landing mid-payoff, ...) would otherwise push the counter past 1 while the single shared safety
	// timer only releases once, stranding the player with movement+look locked (only jump free). Guard with
	// our own booleans so we push each ignore flag at most once and always pop it exactly once.
	if (!bJumpscareMoveInputLocked)
	{
		SetIgnoreMoveInput(true);
		bJumpscareMoveInputLocked = true;
	}
	// The "behind you" scare locks movement but leaves look free so turning around is possible.
	if (bLockLook && !bJumpscareLookInputLocked)
	{
		SetIgnoreLookInput(true);
		bJumpscareLookInputLocked = true;
	}
	bShowMouseCursor = false;

	// Always arm a self-restoring safety timer so input frees itself even if the explicit
	// unlock RPC is dropped or reordered on a flaky connection. The fast-path unlock clears it.
	const float SafetyDelay = FMath::Clamp(SafetySeconds, 0.0f, 8.0f);
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<ABHPlayerController> WeakThis(this);
		FTimerDelegate RestoreDelegate;
		RestoreDelegate.BindLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ReleaseJumpscareInput();
			}
		});
		World->GetTimerManager().SetTimer(JumpscareInputRestoreHandle, RestoreDelegate, FMath::Max(0.05f, SafetyDelay), false);
	}
}

void ABHPlayerController::ReleaseJumpscareInput()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JumpscareInputRestoreHandle);
	}

	// Pop exactly what we pushed (guarded), so the engine's counted ignore flags return to zero no matter
	// how many overlapping scares armed the lock.
	if (bJumpscareMoveInputLocked)
	{
		SetIgnoreMoveInput(false);
		bJumpscareMoveInputLocked = false;
	}
	if (bJumpscareLookInputLocked)
	{
		SetIgnoreLookInput(false);
		bJumpscareLookInputLocked = false;
	}
	if (!MainMenuWidget.IsValid() && !AtmosphereConsoleWidget.IsValid())
	{
		ApplyGameplayInputMode();
	}
}

void ABHPlayerController::ClientSetJumpscareInputLocked_Implementation(bool bLocked)
{
	if (!IsLocalController())
	{
		return;
	}

	if (bLocked)
	{
		// No explicit duration travels with this RPC, so cap the safety unlock at the max.
		LockJumpscareInput(8.0f);
		return;
	}

	ReleaseJumpscareInput();
}

void ABHPlayerController::PlayGameplayAudioCueLocal(const FBHGameplayAudioCue& Cue)
{
	if (!IsLocalController())
	{
		return;
	}

	EnsureAudioPreferencesLoaded();
	EnsureComfortPreferencesLoaded();

	float DistanceScale = 1.0f;
	if (Cue.bSpatial && Cue.MaxAudibleDistance > 0.0f)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		GetPlayerViewPoint(ViewLocation, ViewRotation);

		const float Distance = FVector::Dist(ViewLocation, Cue.Location);
		if (Distance > Cue.MaxAudibleDistance)
		{
			return;
		}

		const float FullVolumeDistance = FMath::Max(120.0f, Cue.MaxAudibleDistance * 0.18f);
		DistanceScale = FMath::GetMappedRangeValueClamped(
			FVector2D(Cue.MaxAudibleDistance, FullVolumeDistance),
			FVector2D(0.0f, 1.0f),
			Distance);
	}

	if (bCaptionsEnabled && !Cue.Caption.IsEmpty())
	{
		ShowLocalStatusMessage(Cue.Caption, FMath::Max(0.4f, Cue.CaptionSeconds));
	}

	if (Cue.AudioAsset.IsNull())
	{
		return;
	}

	USoundBase* Sound = BHLoadGameplaySound(Cue.AudioAsset, TEXT("gameplay audio"));
	if (!Sound)
	{
		return;
	}

	const float ComfortScale = Cue.bApplyHorrorComfort && bReducedJumpscares ? 0.45f : 1.0f;
	const float EffectiveVolume = BHClampVolume(MasterVolume) * FMath::Clamp(Cue.Volume * DistanceScale * ComfortScale, 0.0f, 2.0f);
	const float EffectivePitch = FMath::Clamp(Cue.Pitch, 0.1f, 3.0f);
	if (EffectiveVolume <= 0.0f)
	{
		return;
	}

	if (Cue.bSpatial)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Cue.Location, EffectiveVolume, EffectivePitch);
	}
	else
	{
		UGameplayStatics::PlaySound2D(this, Sound, EffectiveVolume, EffectivePitch, 0.0f, nullptr, this, true);
	}
}

void ABHPlayerController::ClientPlayGameplayAudioCue_Implementation(const FBHGameplayAudioCue& Cue)
{
	PlayGameplayAudioCueLocal(Cue);
}

void ABHPlayerController::ClientSnapViewToFlatFocus_Implementation(const FVector& FocusLocation)
{
	if (!IsLocalController())
	{
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector FlatDelta = FocusLocation - ViewLocation;
	FlatDelta.Z = 0.0f;
	if (FlatDelta.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		APawn* ControlledPawn = GetPawn();
		FlatDelta = ControlledPawn ? ControlledPawn->GetActorForwardVector() : FVector::ForwardVector;
		FlatDelta.Z = 0.0f;
	}

	if (!FlatDelta.IsNearlyZero())
	{
		const float Yaw = FlatDelta.Rotation().Yaw;
		SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
}

void ABHPlayerController::ClientPlayHorrorCue_Implementation(const FBHClientHorrorCue& Cue)
{
	if (!IsLocalController())
	{
		return;
	}

	EnsureAudioPreferencesLoaded();
	EnsureComfortPreferencesLoaded();
	const float JumpscareScale = bReducedJumpscares ? 0.45f : 1.0f;
	const float FlashScale = bReducedFlash ? 0.25f : 1.0f;
	const float ShakeScale = bReducedCameraShake ? 0.25f : 1.0f;
	// Reduced Jumpscares ("safe mode") no longer hides the close-up entirely — it now shows the gentle
	// abstract proxy in place of the realistic creature (ABHJumpscareMonster swaps itself to the proxy when the
	// local viewer has the option on). The flash/shake/rumble are still toned down via the scales above.
	const bool bSuppressCloseVisual = false;

	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		if (BHGI->IsAutomationEnabled())
		{
			const UEnum* ScareEventEnum = StaticEnum<EBHScareEventType>();
			const FString EventName = ScareEventEnum
				? ScareEventEnum->GetNameStringByValue(static_cast<int64>(Cue.EventType))
				: FString::FromInt(static_cast<int32>(Cue.EventType));
			BHGI->LogAutomationMarker(FString::Printf(
				TEXT("HORROR_CUE:%s lock=%.2f snap=%d"),
				*EventName,
				Cue.bLockInput ? Cue.LockSeconds : 0.0f,
				Cue.bSnapToFocus ? 1 : 0));
		}
	}

	// "Behind you" cues arm a held directive + movement-only lock and defer the sensory impact until the
	// player turns to face the presence (sprung from TickHorrorCueEffects). Everything else falls through
	// to the immediate impact below.
	if (Cue.EventType == EBHScareEventType::BehindYou)
	{
		BeginBehindYouScare(Cue);
		return;
	}

	if (Cue.bDirectivePrompt && !Cue.Message.IsEmpty())
	{
		const float DirectiveNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		HorrorDirectiveText = Cue.Message;
		HorrorDirectiveStartTime = DirectiveNow;
		HorrorDirectiveEndTime = DirectiveNow + FMath::Max(0.4f, Cue.DurationSeconds);
	}
	else if (!Cue.Message.IsEmpty())
	{
		ShowLocalStatusMessage(Cue.Message, FMath::Max(0.25f, Cue.DurationSeconds));
	}
	else if (bCaptionsEnabled && Cue.EventType != EBHScareEventType::Ambient)
	{
		ShowLocalStatusMessage(TEXT("Audio scare cue nearby."), FMath::Max(1.2f, Cue.DurationSeconds * 0.6f));
	}

	PlayJumpscareImpactAudio(Cue, JumpscareScale);

	// Full-screen "PNG in your face" still image, drawn by the HUD over the view for the cue duration.
	if (!Cue.FaceImage.IsNull())
	{
		const FSoftObjectPath FaceImagePath = Cue.FaceImage.ToSoftObjectPath();
		if (BHPlayerControllerSoftObjectPathExists(FaceImagePath))
		{
			if (UTexture2D* FaceTexture = Cue.FaceImage.LoadSynchronous())
			{
				const float ImageNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				HorrorCueFaceImage = FaceTexture;
				HorrorCueFaceImageStartTime = ImageNow;
				HorrorCueFaceImageEndTime = ImageNow + FMath::Clamp(Cue.DurationSeconds, 0.25f, 4.0f);
				const float BaseImageIntensity = Cue.FlashIntensity > 0.0f ? Cue.FlashIntensity : 1.0f;
				HorrorCueFaceImageIntensity = FMath::Clamp(BaseImageIntensity * JumpscareScale, 0.0f, 1.0f);
			}
		}
		else
		{
			BHLogOptionalPlayerAssetFallbackOnce(FaceImagePath, TEXT("horror cue face image"), TEXT("missing"));
		}
	}

	bool bSpawnedCueVisual = false;
	if (!bSuppressCloseVisual && !Cue.VisualActorClass.IsNull())
	{
		const FSoftObjectPath VisualActorPath = Cue.VisualActorClass.ToSoftObjectPath();
		if (BHPlayerControllerSoftObjectPathExists(VisualActorPath))
		{
			if (UClass* VisualClass = Cue.VisualActorClass.LoadSynchronous())
			{
				FVector ViewLocation = FVector::ZeroVector;
				FRotator ViewRotation = FRotator::ZeroRotator;
				GetPlayerViewPoint(ViewLocation, ViewRotation);

				const FRotationMatrix ViewMatrix(ViewRotation);
				const FVector VisualSpawnLocation = Cue.bCloseRangeFocus
					? ViewLocation
						+ ViewMatrix.GetUnitAxis(EAxis::X) * Cue.CloseVisualOffset.X
						+ ViewMatrix.GetUnitAxis(EAxis::Y) * Cue.CloseVisualOffset.Y
						+ ViewMatrix.GetUnitAxis(EAxis::Z) * Cue.CloseVisualOffset.Z
					: Cue.FocusLocation;
				const FRotator SpawnRotation = Cue.bCloseRangeFocus
					? ((ViewLocation - VisualSpawnLocation).Rotation() + Cue.CloseVisualRotation)
					: (ViewRotation + Cue.CloseVisualRotation);

				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = this;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AActor* VisualActor = GetWorld() ? GetWorld()->SpawnActor<AActor>(VisualClass, VisualSpawnLocation, SpawnRotation, SpawnParams) : nullptr)
				{
					bSpawnedCueVisual = true;
					VisualActor->SetActorEnableCollision(false);
					if (Cue.bCloseRangeFocus)
					{
						VisualActor->SetActorScale3D(VisualActor->GetActorScale3D() * Cue.CloseVisualScale);
						BHFitCloseVisualActorToCamera(VisualActor);
					}
					if (Cue.bUpperBodyCloseVisual)
					{
						BHApplyUpperBodyCloseVisualMask(VisualActor);
					}
					TArray<UPrimitiveComponent*> PrimitiveComponents;
					VisualActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
					for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
					{
						if (PrimitiveComponent)
						{
							PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						}
					}
					VisualActor->SetLifeSpan(FMath::Clamp(Cue.DurationSeconds, 0.2f, 5.0f));
				}
			}
			else
			{
				BHLogOptionalPlayerAssetFallbackOnce(VisualActorPath, TEXT("horror cue visual"), TEXT("failed to load"));
			}
		}
		else
		{
			BHLogOptionalPlayerAssetFallbackOnce(VisualActorPath, TEXT("horror cue visual"), TEXT("missing"));
		}
	}

	if (!bSuppressCloseVisual && !bSpawnedCueVisual && Cue.bCloseRangeFocus && !Cue.VariantId.IsNone())
	{
		FBHJumpscareVariant CueVariant;
		if (BHFindJumpscareVariantForCue(Cue.VariantId, CueVariant))
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			GetPlayerViewPoint(ViewLocation, ViewRotation);

			const FRotationMatrix ViewMatrix(ViewRotation);
			const FVector VisualSpawnLocation = ViewLocation
				+ ViewMatrix.GetUnitAxis(EAxis::X) * Cue.CloseVisualOffset.X
				+ ViewMatrix.GetUnitAxis(EAxis::Y) * Cue.CloseVisualOffset.Y
				+ ViewMatrix.GetUnitAxis(EAxis::Z) * Cue.CloseVisualOffset.Z;
			const FRotator SpawnRotation = ((ViewLocation - VisualSpawnLocation).Rotation() + Cue.CloseVisualRotation);

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ABHJumpscareMonster* CloseMonster = GetWorld() ? GetWorld()->SpawnActor<ABHJumpscareMonster>(VisualSpawnLocation, SpawnRotation, SpawnParams) : nullptr)
			{
				CloseMonster->ConfigureCloseupPresentation(CueVariant, FMath::Clamp(Cue.DurationSeconds, 0.2f, 5.0f), Cue.bUpperBodyCloseVisual);
			}
		}
	}

	if (Cue.bSnapToFocus)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		GetPlayerViewPoint(ViewLocation, ViewRotation);

		const FVector Delta = Cue.FocusLocation - ViewLocation;
		if (!Delta.IsNearlyZero())
		{
			FRotator FocusRotation = Delta.Rotation();
			FocusRotation.Roll = 0.0f;
			SetControlRotation(FocusRotation);
		}
	}

	if (Cue.ShakeIntensity > 0.0f && ShakeScale > 0.0f)
	{
		const float Shake = FMath::Clamp(Cue.ShakeIntensity * ShakeScale, 0.0f, 1.0f);
		const FRotator CurrentRotation = GetControlRotation();
		SetControlRotation(CurrentRotation + FRotator(FMath::FRandRange(-1.4f, 1.4f) * Shake, FMath::FRandRange(-2.5f, 2.5f) * Shake, 0.0f));
	}

	if (Cue.CameraJitterDuration > 0.0f && Cue.ShakeIntensity > 0.0f && ShakeScale > 0.0f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		HorrorCueJitterEndTime = FMath::Max(HorrorCueJitterEndTime, Now + FMath::Clamp(Cue.CameraJitterDuration * FMath::Lerp(0.55f, 1.0f, ShakeScale), 0.0f, 4.0f));
		HorrorCueNextJitterTime = Now;
		HorrorCueJitterIntensity = FMath::Max(HorrorCueJitterIntensity, FMath::Clamp(Cue.ShakeIntensity * ShakeScale, 0.0f, 1.0f));
		HorrorCueJitterFrequency = FMath::Clamp(Cue.CameraJitterFrequency, 8.0f, 90.0f);
	}

	if (Cue.FlashIntensity > 0.0f && FlashScale > 0.0f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		HorrorCueFlashStartTime = Now;
		HorrorCueFlashEndTime = Now + FMath::Clamp(Cue.DurationSeconds * 0.42f, 0.22f, 1.65f);
		HorrorCueFlashIntensity = FMath::Clamp(Cue.FlashIntensity * FlashScale * JumpscareScale, 0.0f, 1.0f);
		HorrorCueFlashColor = Cue.FlashColor;

		// A hard black blink on the strongest scares. Suppressed under reduced-flash comfort.
		if (!bReducedFlash && Cue.FlashIntensity >= 0.6f && Cue.bCloseRangeFocus)
		{
			HorrorCueBlinkStartTime = Now;
			HorrorCueBlinkIntensity = FMath::Clamp(Cue.FlashIntensity * JumpscareScale, 0.0f, 1.0f);
		}
	}

	// ---- Impact feel: FOV punch + camera flinch, gamepad rumble, and a brief hitstop. ----
	const float ImpactIntensity = FMath::Clamp(FMath::Max(Cue.ShakeIntensity, Cue.FlashIntensity), 0.0f, 1.0f) * JumpscareScale;

	if (Cue.FOVPunch > 0.0f && ShakeScale > 0.0f)
	{
		if (ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
		{
			ControlledCharacter->PlayJumpscareCameraImpact(ImpactIntensity, Cue.FOVPunch);
		}
	}

	if (Cue.RumbleIntensity > 0.0f && !bReducedJumpscares)
	{
		const float RumbleStrength = FMath::Clamp(Cue.RumbleIntensity * FMath::Max(0.35f, ImpactIntensity), 0.0f, 1.0f);
		PlayDynamicForceFeedback(
			RumbleStrength,
			FMath::Clamp(Cue.DurationSeconds * 0.5f, 0.2f, 1.2f),
			/*bAffectsLeftLarge=*/true,
			/*bAffectsLeftSmall=*/true,
			/*bAffectsRightLarge=*/true,
			/*bAffectsRightSmall=*/true,
			EDynamicForceFeedbackAction::Start,
			HorrorCueRumbleHandle);
	}

	if (Cue.HitStopSeconds > 0.0f && !bReducedJumpscares && GetWorld())
	{
		// Client-local micro slow-motion for the "jolt". A game-time timer fires after
		// HitStopSeconds of real time when the delay is scaled by the dilation factor.
		constexpr float HitStopDilation = 0.12f;
		UGameplayStatics::SetGlobalTimeDilation(this, HitStopDilation);

		TWeakObjectPtr<ABHPlayerController> WeakThis(this);
		FTimerDelegate RestoreDilation;
		RestoreDilation.BindLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				UGameplayStatics::SetGlobalTimeDilation(WeakThis.Get(), 1.0f);
			}
		});
		const float RealStop = FMath::Clamp(Cue.HitStopSeconds, 0.0f, 0.25f);
		GetWorldTimerManager().SetTimer(HorrorCueHitStopHandle, RestoreDilation, FMath::Max(0.001f, RealStop * HitStopDilation), false);
	}

	if (Cue.bLockInput && Cue.LockSeconds > 0.0f)
	{
		// Clamp the lock to a sane max, matching the HitStopSeconds clamp style above.
		// LockJumpscareInput arms the self-restoring safety timer so input always frees itself.
		const float LockSeconds = FMath::Clamp(Cue.LockSeconds, 0.0f, 8.0f);
		LockJumpscareInput(LockSeconds, !Cue.bMoveOnlyLock);
	}
}

void ABHPlayerController::BeginBehindYouScare(const FBHClientHorrorCue& Cue)
{
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// Hold the directive prompt and movement-only lock. The look axis stays free so the player can turn.
	HorrorDirectiveText = Cue.Message.IsEmpty() ? TEXT("DON'T TURN AROUND") : Cue.Message;
	HorrorDirectiveStartTime = Now;

	const float MaxHold = FMath::Clamp(Cue.LockSeconds > 0.0f ? Cue.LockSeconds : Cue.DurationSeconds, 1.5f, 8.0f);
	HorrorDirectiveEndTime = Now + MaxHold + 0.75f;

	bBehindYouActive = true;
	BehindYouFocusLocation = Cue.FocusLocation;
	BehindYouArmTime = Now;
	BehindYouDeadline = Now + MaxHold;
	BehindYouTurnCosThreshold = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Cue.TurnReleaseHalfAngleDegrees, 5.0f, 85.0f)));

	// The payoff is a close-range slam that reuses the full impact path. Strip the directive/behind-you
	// framing so it reads as an ordinary close jumpscare when it fires, and give it a short hard lock.
	BehindYouPayoffCue = Cue;
	BehindYouPayoffCue.EventType = EBHScareEventType::MonsterCharge;
	BehindYouPayoffCue.bDirectivePrompt = false;
	BehindYouPayoffCue.bMoveOnlyLock = false;
	BehindYouPayoffCue.bCloseRangeFocus = true;
	BehindYouPayoffCue.bUpperBodyCloseVisual = true;
	// They turned to face it themselves, so don't yank the view; spawn the slam centred on where they
	// now look, framed near eye level so the face fills the screen rather than spawning at their feet.
	BehindYouPayoffCue.bSnapToFocus = false;
	BehindYouPayoffCue.CloseVisualOffset = FVector(82.0f, 0.0f, -52.0f);
	BehindYouPayoffCue.bLockInput = true;
	BehindYouPayoffCue.LockSeconds = 0.85f;
	// Punchy slam, independent of the (long) directive hold window.
	BehindYouPayoffCue.DurationSeconds = 1.6f;
	BehindYouPayoffCue.Message = FString();

	// Movement-only lock with a safety unlock past the max hold, in case the payoff is somehow missed.
	LockJumpscareInput(MaxHold + 1.5f, /*bLockLook=*/false);
}

void ABHPlayerController::TriggerBehindYouPayoff()
{
	if (!bBehindYouActive)
	{
		return;
	}

	bBehindYouActive = false;
	HorrorDirectiveEndTime = FMath::Min(HorrorDirectiveEndTime, (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + 0.18f);

	// Free the held movement-only lock, then replay the payoff as a normal close-range cue so it runs the
	// full impact path (scream, face slam, flash, blink, FOV punch, brief hard lock that self-restores).
	ReleaseJumpscareInput();
	ClientPlayHorrorCue_Implementation(BehindYouPayoffCue);
}

void ABHPlayerController::ClientRecordRoundResult_Implementation(EBHPlayerRole AccountRole, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase)
{
	// Record the result for session telemetry and arm the end-of-round survey prompt.
	if (UBHFeedbackSubsystem* Feedback = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHFeedbackSubsystem>() : nullptr)
	{
		Feedback->RecordRoundResult(AccountRole, LifeState, ResultPhase);
	}

	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		const int32 PreviousXP = AccountSubsystem->GetProgress().XP;
		AccountSubsystem->RecordRoundResult(AccountRole, LifeState, ResultPhase);
		const int32 CurrentXP = AccountSubsystem->GetProgress().XP;
		if (CurrentXP > PreviousXP)
		{
			TArray<FString> NewlyUnlocked;
			const EBHCosmeticCategory Categories[] = {
				EBHCosmeticCategory::Outfit,
				EBHCosmeticCategory::Headwear
			};
			for (EBHCosmeticCategory Category : Categories)
			{
				for (int32 Index = 0; Index <= BHCosmeticMaxIndex(Category); ++Index)
				{
					const int32 RequiredXP = BHCosmeticRequiredXP(Category, Index);
					if (RequiredXP > PreviousXP && RequiredXP <= CurrentXP)
					{
						NewlyUnlocked.Add(FString::Printf(
							TEXT("%s %s"),
							BHCosmeticCategoryName(Category),
							BHCosmeticItemName(Category, Index)));
					}
				}
			}

			if (!NewlyUnlocked.IsEmpty())
			{
				const int32 MaxShown = FMath::Min(2, NewlyUnlocked.Num());
				FString UnlockText;
				for (int32 Index = 0; Index < MaxShown; ++Index)
				{
					if (!UnlockText.IsEmpty())
					{
						UnlockText += TEXT(", ");
					}
					UnlockText += NewlyUnlocked[Index];
				}
				if (NewlyUnlocked.Num() > MaxShown)
				{
					UnlockText += FString::Printf(TEXT(" +%d more"), NewlyUnlocked.Num() - MaxShown);
				}

				ShowLocalStatusMessage(FString::Printf(TEXT("New cosmetic unlocked: %s."), *UnlockText), 4.0f);
			}
		}
	}
}

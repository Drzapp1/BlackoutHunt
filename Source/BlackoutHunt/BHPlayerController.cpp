#include "BHPlayerController.h"
#include "BHAccountSubsystem.h"
#include "BHAutomationSupport.h"
#include "BHCharacter.h"
#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHNetworkSupport.h"
#include "BHPlayerState.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "SBHClassroomBoard.h"
#include "SBHMainMenu.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
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
constexpr TCHAR BHGraphicsConfigSection[] = TEXT("BlackoutHunt.Graphics");
constexpr TCHAR BHAmbientMusicAssetPath[] = TEXT("/Game/BlackoutHunt/Audio/SW_EerieLobbyLoop.SW_EerieLobbyLoop");
constexpr TCHAR BHMenuClickAssetPath[] = TEXT("/Game/BlackoutHunt/Audio/SW_MenuClick.SW_MenuClick");
constexpr float BHGraphicsGiB = 1024.0f * 1024.0f * 1024.0f;
constexpr int32 BHAdaptiveMinRenderScale = 50;
constexpr int32 BHAdaptiveMaxStep = 4;

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
			TEXT("WARP"),
			TEXT("SwiftShader"),
			TEXT("llvmpipe"),
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
		&& !BHStringContainsAny(GpuBrand, { TEXT("NVIDIA"), TEXT("GeForce"), TEXT("RTX"), TEXT("GTX") });

	const bool bVeryLowSystemMemory = Profile.SystemMemoryGB > 0.0f && Profile.SystemMemoryGB <= 6.25f;
	const bool bLowSystemMemory = Profile.SystemMemoryGB > 0.0f && Profile.SystemMemoryGB <= 10.25f;
	const bool bKnownLowVideoMemory = Profile.DedicatedVideoMemoryGB > 0.0f && Profile.DedicatedVideoMemoryGB <= 4.25f;
	const bool bStrongVideoMemory = Profile.DedicatedVideoMemoryGB >= 7.5f;
	const bool bVeryStrongVideoMemory = Profile.DedicatedVideoMemoryGB >= 10.5f;

	if (Profile.bLikelySoftwareGpu || bVeryLowSystemMemory || bKnownLowVideoMemory)
	{
		Profile.RecommendedPreset = 0;
		Profile.RecommendedFpsGoal = Profile.bLikelySoftwareGpu ? 30 : 45;
		Profile.RecommendedRenderScale = Profile.bLikelySoftwareGpu ? 60 : 67;
	}
	else if (Profile.bLikelyIntegratedGpu || bLowSystemMemory || Profile.PhysicalCores <= 4)
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

bool BHSoftObjectPathExists(const FSoftObjectPath& ObjectPath)
{
	if (ObjectPath.IsNull())
	{
		return false;
	}

	const FString PackageName = ObjectPath.GetLongPackageName();
	return !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
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
		FLinearColor(0.76f, 0.76f, 0.80f, 1.0f)
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
		EnsureAudioPreferencesLoaded();
		EnsureGraphicsPreferencesLoaded();
		ApplyStartupGraphicsSettings();
		ApplyVirtualBoxSafeModeIfNeeded();
		BindGameWindowCloseOverride();
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
		}
		else
		{
			ApplyGameplayInputMode();
		}
		UpdateAmbientMusic();
		GetWorldTimerManager().SetTimer(DisplayNameSyncTimerHandle, this, &ABHPlayerController::PushLocalDisplayNameToServer, 1.0f, false);
		if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
		{
			if (BHGI->IsAutomationEnabled())
			{
				BHGI->LogAutomationMarkerOnce(TEXT("MENU_READY"));
			}
		}
		ScheduleAutomation();
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
		World->GetTimerManager().ClearTimer(DisplayNameSyncTimerHandle);
	}

	HideClassroomBoard();
	HideTravelLoadingScreen();

	if (AmbientMusicComponent)
	{
		AmbientMusicComponent->Stop();
		AmbientMusicComponent->DestroyComponent();
		AmbientMusicComponent = nullptr;
		bAmbientMusicStarted = false;
	}

	Super::EndPlay(EndPlayReason);
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
		InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ABHPlayerController::ToggleClassroomBoard);
		InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ABHPlayerController::TesterGrantTrainResources);
		InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ABHPlayerController::TesterOpenTrainIntermission);
		InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ABHPlayerController::TesterAdvanceTrainPhase);
		InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ABHPlayerController::TesterLoadFinalStation);
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
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, BHMakeListenOptions(TEXT("Facility")));
}

void ABHPlayerController::HostSubstationGame()
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("LOADING SUBSTATION"), TEXT("Opening local lobby."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, BHMakeListenOptions(TEXT("Substation")));
}

void ABHPlayerController::HostFoggroundsGame()
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("LOADING FOGGROUNDS"), TEXT("Opening local lobby."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, BHMakeListenOptions(TEXT("Foggrounds")));
}

void ABHPlayerController::HostPracticeGame()
{
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("PRACTICE LAB"), TEXT("Preparing safe test route."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("listen?BHLevel=Facility?BHPractice=1"));
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

	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("JOINING GAME"), FString::Printf(TEXT("Connecting to %s."), *NormalizedAddress));
	ClientTravel(NormalizedAddress, TRAVEL_Absolute);
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
	const int32 NormalizedIndex = FMath::Clamp(AvatarIndex - 1, 0, 7);
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarIndex(NormalizedIndex);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatar(NormalizedIndex);
}

void ABHPlayerController::SetAvatarColor(int32 ColorIndex)
{
	const int32 NormalizedIndex = FMath::Clamp(ColorIndex - 1, 0, 7);
	const FLinearColor AvatarColor = BHAvatarPaletteColor(NormalizedIndex);
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarColor(AvatarColor);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatarColor(AvatarColor, NormalizedIndex);
}

void ABHPlayerController::SetAvatarHeadwear(int32 HeadwearIndex)
{
	constexpr int32 NormalizedIndex = 0;
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarHeadwearIndex(NormalizedIndex);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatarHeadwear(NormalizedIndex);
}

void ABHPlayerController::SetAvatarGear(int32 GearIndex)
{
	constexpr int32 NormalizedIndex = 0;
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarGearIndex(NormalizedIndex);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatarGear(NormalizedIndex);
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
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting train intermission."), 1.5f);
	ServerTesterOpenTrainIntermission();
}

void ABHPlayerController::TesterAdvanceTrainPhase()
{
	ShowLocalStatusMessage(TEXT("Tester shortcut: requesting next train phase."), 1.5f);
	ServerTesterAdvanceTrainPhase();
}

void ABHPlayerController::TesterLoadFinalStation()
{
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

void ABHPlayerController::ShowMainMenu()
{
	if (!IsLocalController() || MainMenuWidget.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	HideTravelLoadingScreen();

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

void ABHPlayerController::HideMainMenu()
{
	if (MainMenuWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MainMenuWidget.ToSharedRef());
		MainMenuWidget.Reset();
	}

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

bool ABHPlayerController::HostTestRoundForMenu(const FString& LevelName, FString& OutMessage)
{
	const FString NormalizedLevel = BHNormalizeRuntimeLevelName(LevelName);
	const FString Options = BHMakeListenOptions(NormalizedLevel, TEXT("?BHTestMode=1"));
	OutMessage = FString::Printf(TEXT("Starting %s Test Round."), *NormalizedLevel);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HideMainMenu();
	ShowTravelLoadingScreen(FString::Printf(TEXT("TEST %s"), *NormalizedLevel.ToUpper()), TEXT("Loading mechanics sandbox."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, Options);
	return true;
}

bool ABHPlayerController::HostPhysicsClassroomForMenu(FString& OutMessage)
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const int32 StageSeconds = Settings ? FMath::Clamp(Settings->StageOneSeconds, 60, 3600) : 300;
	OutMessage = TEXT("Starting IGCSE Physics Classroom.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	BHApplyClassroomLoopbackBinding(Settings);
	HideMainMenu();
	ShowTravelLoadingScreen(TEXT("PHYSICS CLASSROOM"), TEXT("Preparing revision escape route."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, FString::Printf(TEXT("listen?BHLevel=Facility?BHStageIndex=0?BHRevisionMode=1?BHRevisionTopics=All?BHRevisionDifficultyMix=Adaptive?BHHuntSeconds=%d?BHScareIntensity=3"), StageSeconds));
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
	const int32 StageSeconds = Settings ? FMath::Clamp(Settings->StageOneSeconds, 60, 3600) : 300;
	FString JoinAddress;
	if (UBHGameInstance* BHGI = GetWorld() ? GetWorld()->GetGameInstance<UBHGameInstance>() : nullptr)
	{
		const FString ConfiguredClassroomAddress = BHGI->GetConfiguredClassroomJoinAddress(7777);
		FString TunnelMessage;
		BHGI->TryStartInternetTunnel(TunnelMessage, 7777);
		if (!ConfiguredClassroomAddress.IsEmpty())
		{
			BHGI->SetPublicJoinAddress(ConfiguredClassroomAddress);
		}
		JoinAddress = BHGI->GetPreferredClassroomJoinAddress(7777);
	}
	const FString Options = BHMakeListenOptions(NormalizedLevel, FString::Printf(TEXT("?BHStageIndex=0?BHRevisionMode=1?BHRevisionTopics=All?BHRevisionDifficultyMix=Adaptive?BHHuntSeconds=%d?BHScareIntensity=3?BHLiveClassroom=1"), StageSeconds));
	OutMessage = JoinAddress.IsEmpty()
		? FString::Printf(TEXT("Starting Live Classroom on %s."), *NormalizedLevel)
		: FString::Printf(TEXT("Starting Live Classroom on %s. Students join %s."), *NormalizedLevel, *JoinAddress);
	ShowLocalStatusMessage(OutMessage, 4.0f);
	BHApplyClassroomLoopbackBinding(Settings);
	HideMainMenu();
	ShowTravelLoadingScreen(FString::Printf(TEXT("LIVE %s"), *NormalizedLevel.ToUpper()), TEXT("Opening classroom host."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, Options);
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

	OutMessage = FString::Printf(TEXT("Starting Bot %s with %d %s bots."), *NormalizedLevel, BotCount, *BHBotDifficultyToString(Difficulty));
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HideMainMenu();
	ShowTravelLoadingScreen(FString::Printf(TEXT("BOT %s"), *NormalizedLevel.ToUpper()), TEXT("Spawning offline bot route."));
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, Options);
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
		PushLocalDisplayNameToServer();
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
		PushLocalDisplayNameToServer();
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
		PushLocalDisplayNameToServer();
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
	ShowLocalStatusMessage(OutMessage, bSuccess ? 5.0f : 6.0f);
	return bSuccess;
}

bool ABHPlayerController::OpenClassroomBoardForMenu(FString& OutMessage)
{
	const bool bOpened = TryOpenClassroomBoardWindow(OutMessage);
	ShowLocalStatusMessage(OutMessage, bOpened ? 3.0f : 4.0f);
	return bOpened;
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
	ServerSetBotCount(BotCount);
	OutMessage = FString::Printf(TEXT("Bot count request sent: %d."), FMath::Clamp(BotCount, 0, 11));
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
}

bool ABHPlayerController::SetBotDifficultyForMenu(EBHBotDifficulty Difficulty, FString& OutMessage)
{
	ServerSetBotDifficulty(Difficulty);
	OutMessage = FString::Printf(TEXT("Bot difficulty request sent: %s."), *BHBotDifficultyToString(Difficulty));
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
	const int32 NextAvatar = (CurrentAvatar + 1) % 8;
	if (ABHPlayerState* MutableBHPS = GetPlayerState<ABHPlayerState>())
	{
		MutableBHPS->SetAvatarIndex(NextAvatar);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatar(NextAvatar);
	OutMessage = FString::Printf(TEXT("Avatar request sent: %d."), NextAvatar + 1);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarForMenu(int32 AvatarIndex, FString& OutMessage)
{
	const int32 NormalizedIndex = FMath::Clamp(AvatarIndex, 0, 7);
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarIndex(NormalizedIndex);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatar(NormalizedIndex);
	OutMessage = FString::Printf(TEXT("Avatar look request sent: %d."), NormalizedIndex + 1);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarColorForMenu(int32 ColorIndex, FString& OutMessage)
{
	const int32 NormalizedIndex = FMath::Clamp(ColorIndex, 0, 7);
	const FLinearColor AvatarColor = BHAvatarPaletteColor(NormalizedIndex);
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarColor(AvatarColor);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatarColor(AvatarColor, NormalizedIndex);
	OutMessage = FString::Printf(TEXT("Avatar color request sent: %d."), NormalizedIndex + 1);
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarHeadwearForMenu(int32 HeadwearIndex, FString& OutMessage)
{
	constexpr int32 NormalizedIndex = 0;
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarHeadwearIndex(NormalizedIndex);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatarHeadwear(NormalizedIndex);
	OutMessage = TEXT("Headwear reset.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	return true;
}

bool ABHPlayerController::SetAvatarGearForMenu(int32 GearIndex, FString& OutMessage)
{
	constexpr int32 NormalizedIndex = 0;
	if (ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>())
	{
		BHPS->SetAvatarGearIndex(NormalizedIndex);
	}
	BHApplyLocalAvatarStyle(this);
	ServerSetAvatarGear(NormalizedIndex);
	OutMessage = TEXT("Gear reset.");
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
		bGraphicsResolutionOverrideEnabled = bGraphicsResolutionOverrideEnabled || (bLoadedResolutionWidth && bLoadedResolutionHeight);

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
	SaveGraphicsPreference(TEXT("ResolutionOverride"), bGraphicsResolutionOverrideEnabled);
	SaveGraphicsPreference(TEXT("ResolutionWidth"), GraphicsResolutionWidth);
	SaveGraphicsPreference(TEXT("ResolutionHeight"), GraphicsResolutionHeight);
	SaveGraphicsPreference(TEXT("ResolutionFullscreen"), bGraphicsFullscreen);
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

	FString Message;
	ApplyGraphicsPresetInternal(GraphicsPresetQuality, false, Message);
	if (bAutoHardwareGraphicsEnabled)
	{
		GraphicsRenderScalePercent = GraphicsRecommendedRenderScale;
		GraphicsFrameRateLimit = FMath::Max(GraphicsRecommendedFpsGoal, GraphicsAdaptiveFpsGoal);
	}
	ApplySavedManualGraphicsTuning();
	ApplySavedGraphicsResolution();
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -3.0f;

	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt graphics startup: %s"), *GetGraphicsSummaryForMenu());
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
		ConsoleCommand(TEXT("r.Streaming.UseFixedPoolSize 0"));
		ConsoleCommand(TEXT("r.Streaming.PoolSize 0"));
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
	ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), GraphicsFrameRateLimit));
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
	const int32 EffectiveStep = bAdaptiveGraphicsEnabled ? FMath::Clamp(GraphicsAdaptiveStep, 0, BHAdaptiveMaxStep) : 0;
	const int32 EffectiveRenderScale = BHClampRenderScale(SafeRenderScale - EffectiveStep * 8);
	const int32 EffectiveShadowQuality = FMath::Clamp(GraphicsShadowQuality - (EffectiveStep >= 2 ? 1 : 0) - (EffectiveStep >= 4 ? 1 : 0), 0, 3);
	const int32 EffectiveEffectsQuality = FMath::Clamp(GraphicsEffectsQuality - (EffectiveStep >= 2 ? 1 : 0) - (EffectiveStep >= 3 ? 1 : 0), 0, 3);

	if (bAdaptiveGraphicsEnabled)
	{
		const int32 MinDynamicScale = FMath::Clamp(EffectiveRenderScale - 18, BHAdaptiveMinRenderScale, EffectiveRenderScale);
		ConsoleCommand(TEXT("r.DynamicRes.OperationMode 2"));
		ConsoleCommand(FString::Printf(TEXT("r.DynamicRes.FrameTimeBudget %.2f"), BHFpsGoalToFrameTimeBudgetMs(GraphicsAdaptiveFpsGoal)));
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
	if (GraphicsAdaptiveEvaluationSeconds < 2.0f)
	{
		return;
	}
	GraphicsAdaptiveEvaluationSeconds = 0.0f;

	const float AverageFps = GraphicsAverageFrameTimeMs > KINDA_SMALL_NUMBER ? 1000.0f / GraphicsAverageFrameTimeMs : 0.0f;
	const int32 PreviousStep = GraphicsAdaptiveStep;
	const int32 SafeGoal = BHClampFpsGoal(GraphicsAdaptiveFpsGoal);
	if (AverageFps > 0.0f && AverageFps < static_cast<float>(SafeGoal) * 0.88f)
	{
		++GraphicsUnderTargetSamples;
		GraphicsOverTargetSamples = 0;
		if (GraphicsUnderTargetSamples >= 2)
		{
			GraphicsAdaptiveStep = FMath::Min(GraphicsAdaptiveStep + 1, BHAdaptiveMaxStep);
			GraphicsUnderTargetSamples = 0;
		}
	}
	else if (AverageFps > static_cast<float>(SafeGoal) * 1.20f)
	{
		GraphicsUnderTargetSamples = 0;
		++GraphicsOverTargetSamples;
		if (GraphicsOverTargetSamples >= 4)
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
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now - GraphicsLastAdaptiveMessageTime > 12.0f)
		{
			GraphicsLastAdaptiveMessageTime = Now;
			ShowLocalStatusMessage(FString::Printf(TEXT("Adaptive graphics adjusted render scale for %.0f FPS average."), AverageFps), 2.5f);
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
		GraphicsFrameRateLimit = 0;
		GraphicsTextureQuality = 3;
		GraphicsShadowQuality = 3;
		GraphicsEffectsQuality = 3;
		static const FBHConsoleVariableSetting UltraSettings[] = {
			{ TEXT("r.ScreenPercentage"), TEXT("100") },
			{ TEXT("r.DynamicRes.OperationMode"), TEXT("0") },
			{ TEXT("t.MaxFPS"), TEXT("0") },
			{ TEXT("r.TextureStreaming"), TEXT("1") },
			{ TEXT("r.Streaming.UseFixedPoolSize"), TEXT("0") },
			{ TEXT("r.Streaming.PoolSize"), TEXT("0") },
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
	ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), GraphicsFrameRateLimit));
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
		ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), GraphicsFrameRateLimit));
		bRaisedFrameCap = true;
	}
	GraphicsAdaptiveStep = 0;
	GraphicsUnderTargetSamples = 0;
	GraphicsOverTargetSamples = 0;
	GraphicsAverageFrameTimeMs = 0.0f;
	GraphicsAdaptiveEvaluationSeconds = -1.0f;
	ApplyAdaptiveGraphicsState(true);
	SaveGraphicsPreferences();
	OutMessage = bRaisedFrameCap
		? FString::Printf(TEXT("Adaptive graphics target set to %d FPS; frame cap raised to match."), GraphicsAdaptiveFpsGoal)
		: FString::Printf(TEXT("Adaptive graphics target set to %d FPS."), GraphicsAdaptiveFpsGoal);
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
	ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), SafeLimit));
	ApplyAdaptiveGraphicsState(false);
	SaveGraphicsPreferences();
	if (SafeLimit == 0)
	{
		OutMessage = TEXT("Local frame cap removed.");
	}
	else if (bLoweredAdaptiveGoal)
	{
		OutMessage = FString::Printf(TEXT("Local frame cap set to %d FPS; adaptive target lowered to match."), SafeLimit);
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
	const FString CapText = GraphicsFrameRateLimit > 0
		? FString::Printf(TEXT("Cap %d FPS"), GraphicsFrameRateLimit)
		: TEXT("Cap free");
	const FString ResolutionText = bGraphicsResolutionOverrideEnabled && GraphicsResolutionWidth > 0 && GraphicsResolutionHeight > 0
		? FString::Printf(TEXT("Res %dx%d %s"), GraphicsResolutionWidth, GraphicsResolutionHeight, bGraphicsFullscreen ? TEXT("F") : TEXT("W"))
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
		PushLocalDisplayNameToServer();
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
	const float FadeIn = FMath::Clamp(Age / FMath::Max(0.04f, Duration * 0.18f), 0.0f, 1.0f);
	const float FadeOut = FMath::Clamp((HorrorCueFlashEndTime - Now) / FMath::Max(0.06f, Duration * 0.46f), 0.0f, 1.0f);
	return FMath::Clamp(HorrorCueFlashIntensity * FMath::Min(FadeIn, FadeOut), 0.0f, 1.0f);
}

FLinearColor ABHPlayerController::GetHorrorCueFlashColor() const
{
	return HorrorCueFlashColor;
}

void ABHPlayerController::ApplyGameplayInputMode()
{
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
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

void ABHPlayerController::ServerSetReady_Implementation(bool bReady)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetPlayerReady(this, bReady);
	}
}

void ABHPlayerController::ServerSetPlayerDisplayName_Implementation(const FString& DisplayName)
{
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
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetPlayerAvatar(this, AvatarIndex);
	}
}

void ABHPlayerController::ServerSetAvatarColor_Implementation(const FLinearColor& AvatarColor, int32 ColorIndex)
{
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
	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS)
	{
		return;
	}

	constexpr int32 NormalizedIndex = 0;
	BHPS->SetAvatarHeadwearIndex(NormalizedIndex);
	if (ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
	{
		ControlledCharacter->ApplyAvatarStyle();
	}

	ClientShowStatusMessage(TEXT("Headwear reset."), 2.5f);
}

void ABHPlayerController::ServerSetAvatarGear_Implementation(int32 GearIndex)
{
	ABHPlayerState* BHPS = GetPlayerState<ABHPlayerState>();
	if (!BHPS)
	{
		return;
	}

	constexpr int32 NormalizedIndex = 0;
	BHPS->SetAvatarGearIndex(NormalizedIndex);
	if (ABHCharacter* ControlledCharacter = Cast<ABHCharacter>(GetPawn()))
	{
		ControlledCharacter->ApplyAvatarStyle();
	}

	ClientShowStatusMessage(TEXT("Gear reset."), 2.5f);
}

void ABHPlayerController::ServerSetMapVote_Implementation(const FString& LevelName)
{
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->SetMapVote(this, LevelName);
	}
}

void ABHPlayerController::ServerSetFogPresetVote_Implementation(EBHFogPreset FogPreset)
{
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

void ABHPlayerController::ClientSetJumpscareInputLocked_Implementation(bool bLocked)
{
	if (!IsLocalController())
	{
		return;
	}

	if (bLocked)
	{
		if (MainMenuWidget.IsValid())
		{
			HideMainMenu();
		}
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		bShowMouseCursor = false;
		return;
	}

	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	if (!MainMenuWidget.IsValid())
	{
		ApplyGameplayInputMode();
	}
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

	if (!Cue.Message.IsEmpty())
	{
		ShowLocalStatusMessage(Cue.Message, FMath::Max(0.25f, Cue.DurationSeconds));
	}

	if (!Cue.AudioAsset.IsNull())
	{
		if (BHSoftObjectPathExists(Cue.AudioAsset.ToSoftObjectPath()))
		{
			if (USoundBase* Sound = Cue.AudioAsset.LoadSynchronous())
			{
				UGameplayStatics::PlaySound2D(this, Sound, GetEffectiveUiVolume() * FMath::Clamp(Cue.AudioVolume, 0.0f, 2.0f));
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("BlackoutHunt horror cue audio missing, using silent fallback: %s"), *Cue.AudioAsset.ToSoftObjectPath().ToString());
		}
	}

	if (!Cue.VisualActorClass.IsNull())
	{
		if (BHSoftObjectPathExists(Cue.VisualActorClass.ToSoftObjectPath()))
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
					VisualActor->SetActorEnableCollision(false);
					if (Cue.bCloseRangeFocus)
					{
						VisualActor->SetActorScale3D(VisualActor->GetActorScale3D() * Cue.CloseVisualScale);
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
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("BlackoutHunt horror cue visual actor missing, using client flash fallback: %s"), *Cue.VisualActorClass.ToSoftObjectPath().ToString());
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

	if (Cue.ShakeIntensity > 0.0f)
	{
		const float Shake = FMath::Clamp(Cue.ShakeIntensity, 0.0f, 1.0f);
		const FRotator CurrentRotation = GetControlRotation();
		SetControlRotation(CurrentRotation + FRotator(FMath::FRandRange(-1.4f, 1.4f) * Shake, FMath::FRandRange(-2.5f, 2.5f) * Shake, 0.0f));
	}

	if (Cue.CameraJitterDuration > 0.0f && Cue.ShakeIntensity > 0.0f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		HorrorCueJitterEndTime = FMath::Max(HorrorCueJitterEndTime, Now + FMath::Clamp(Cue.CameraJitterDuration, 0.0f, 4.0f));
		HorrorCueNextJitterTime = Now;
		HorrorCueJitterIntensity = FMath::Max(HorrorCueJitterIntensity, FMath::Clamp(Cue.ShakeIntensity, 0.0f, 1.0f));
		HorrorCueJitterFrequency = FMath::Clamp(Cue.CameraJitterFrequency, 8.0f, 90.0f);
	}

	if (Cue.FlashIntensity > 0.0f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		HorrorCueFlashStartTime = Now;
		HorrorCueFlashEndTime = Now + FMath::Clamp(Cue.DurationSeconds * 0.42f, 0.22f, 1.65f);
		HorrorCueFlashIntensity = FMath::Clamp(Cue.FlashIntensity, 0.0f, 1.0f);
		HorrorCueFlashColor = Cue.FlashColor;
	}

	if (Cue.bLockInput && Cue.LockSeconds > 0.0f)
	{
		if (MainMenuWidget.IsValid())
		{
			HideMainMenu();
		}
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		bShowMouseCursor = false;

		TWeakObjectPtr<ABHPlayerController> WeakThis(this);
		FTimerDelegate RestoreDelegate;
		RestoreDelegate.BindLambda([WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			ABHPlayerController* PC = WeakThis.Get();
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			if (!PC->MainMenuWidget.IsValid())
			{
				PC->ApplyGameplayInputMode();
			}
		});

		FTimerHandle RestoreHandle;
		GetWorldTimerManager().SetTimer(RestoreHandle, RestoreDelegate, Cue.LockSeconds, false);
	}
}

void ABHPlayerController::ClientRecordRoundResult_Implementation(EBHPlayerRole AccountRole, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase)
{
	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		AccountSubsystem->RecordRoundResult(AccountRole, LifeState, ResultPhase);
	}
}

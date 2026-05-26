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
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "SBHClassroomBoard.h"
#include "SBHMainMenu.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

namespace
{
constexpr TCHAR BHAudioConfigSection[] = TEXT("BlackoutHunt.Audio");
constexpr TCHAR BHAmbientMusicAssetPath[] = TEXT("/Game/BlackoutHunt/Audio/SW_EerieLobbyLoop.SW_EerieLobbyLoop");
constexpr TCHAR BHMenuClickAssetPath[] = TEXT("/Game/BlackoutHunt/Audio/SW_MenuClick.SW_MenuClick");

struct FBHConsoleVariableSetting
{
	const TCHAR* Name;
	const TCHAR* Value;
};

float BHClampVolume(float Volume)
{
	return FMath::Clamp(Volume, 0.0f, 1.0f);
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
	}
}

void ABHPlayerController::HostGame()
{
	HideMainMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, BHMakeListenOptions(TEXT("Facility")));
}

void ABHPlayerController::HostSubstationGame()
{
	HideMainMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, BHMakeListenOptions(TEXT("Substation")));
}

void ABHPlayerController::HostFoggroundsGame()
{
	HideMainMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, BHMakeListenOptions(TEXT("Foggrounds")));
}

void ABHPlayerController::HostPracticeGame()
{
	HideMainMenu();
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

void ABHPlayerController::RevisionStatus()
{
	ServerRevisionStatus();
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

void ABHPlayerController::ShowMainMenu()
{
	if (!IsLocalController() || MainMenuWidget.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

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
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true);
}

void ABHPlayerController::QuitGame()
{
	RequestCleanQuit(TEXT("menu"));
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
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, Options);
	return true;
}

bool ABHPlayerController::HostPhysicsClassroomForMenu(FString& OutMessage)
{
	OutMessage = TEXT("Starting IGCSE Physics Classroom.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HideMainMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("listen?BHLevel=Facility?BHRevisionMode=1?BHRevisionTopics=All?BHRevisionDifficultyMix=Adaptive?BHHuntSeconds=600?BHScareIntensity=2"));
	return true;
}

bool ABHPlayerController::HostLiveClassroomForMenu(FString& OutMessage)
{
	OutMessage = TEXT("Starting Live Classroom.");
	ShowLocalStatusMessage(OutMessage, 2.5f);
	HideMainMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("listen?BHLevel=Facility?BHRevisionMode=1?BHRevisionTopics=All?BHRevisionDifficultyMix=Adaptive?BHHuntSeconds=600?BHScareIntensity=2?BHLiveClassroom=1"));
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

bool ABHPlayerController::ApplyGraphicsPresetForMenu(int32 Quality, FString& OutMessage)
{
	if (!IsLocalController())
	{
		OutMessage = TEXT("Display settings can only be changed on the local machine.");
		return false;
	}

	const int32 ClampedQuality = FMath::Clamp(Quality, 0, 3);

	BHApplyScalabilityGroups(this, ClampedQuality);

	switch (ClampedQuality)
	{
	case 0:
	{
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
			{ TEXT("r.Shadow.CSM.MaxCascades"), TEXT("1") },
			{ TEXT("r.Shadow.MaxResolution"), TEXT("512") },
			{ TEXT("r.DistanceFieldAO"), TEXT("0") },
			{ TEXT("r.AmbientOcclusionLevels"), TEXT("0") },
			{ TEXT("r.SSR.Quality"), TEXT("0") },
			{ TEXT("r.BloomQuality"), TEXT("1") },
			{ TEXT("r.Tonemapper.Quality"), TEXT("2") },
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

	static const TCHAR* Labels[] = { TEXT("Low 4GB"), TEXT("Medium"), TEXT("High 16GB"), TEXT("Ultra") };
	OutMessage = FString::Printf(TEXT("Local graphics preset applied: %s."), Labels[ClampedQuality]);
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
	ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%d%s"), SafeWidth, SafeHeight, bFullscreen ? TEXT("f") : TEXT("w")));
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
	ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), SafeLimit));
	OutMessage = SafeLimit == 0 ? TEXT("Local frame cap removed.") : FString::Printf(TEXT("Local frame cap set to %d FPS."), SafeLimit);
	ShowLocalStatusMessage(OutMessage, 3.0f);
	return true;
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
	ApplyGraphicsPresetForMenu(0, Message);
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
	const FString JoinAddress = BHGI ? BHGI->GetPreferredJoinAddress(7777) : FBHNetworkSupport::ResolveLocalJoinAddress(7777);
	if (BHGI && BHGI->IsAutomationEnabled())
	{
		BHGI->LogAutomationMarkerOnce(FString::Printf(TEXT("JOIN_ADDRESS:%s"), *JoinAddress));
	}
	const FString Status = FString::Printf(
		TEXT("Classroom join address: %s. Direct LAN uses UDP 7777; if nobody connects, tunnel fallback starts automatically."),
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
		ShowLocalStatusMessage(TEXT("LAN ready: student client reached the host."), 6.0f);
		UE_LOG(LogTemp, Display, TEXT("LAN ready: student client reached the host."));
		return;
	}

	bClassroomFallbackStarted = true;
	FString TunnelMessage;
	const bool bTunnelStarted = StartInternetTunnelForMenu(TunnelMessage, 7777);
	const FBHInternetTunnelResult TunnelStatus = FBHNetworkSupport::GetInternetTunnelStatus(7777);
	FString Status;
	if (TunnelStatus.bTunnelReady)
	{
		if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
		{
			BHGI->SetPublicJoinAddress(TunnelStatus.TunnelAddress);
		}
		Status = FString::Printf(TEXT("LAN blocked, tunnel ready: students join %s. %s"), *TunnelStatus.TunnelAddress, *TunnelStatus.Message);
	}
	else if (bTunnelStarted)
	{
		Status = FString::Printf(TEXT("network setup required: %s"), *TunnelStatus.Message);
	}
	else
	{
		Status = FString::Printf(TEXT("network setup required: %s"), *TunnelMessage);
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

void ABHPlayerController::ClientRecordRoundResult_Implementation(EBHPlayerRole AccountRole, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase)
{
	if (UBHAccountSubsystem* AccountSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBHAccountSubsystem>() : nullptr)
	{
		AccountSubsystem->RecordRoundResult(AccountRole, LifeState, ResultPhase);
	}
}

#include "BHGameInstance.h"
#include "BHGameSettings.h"
#include "BHNetworkSupport.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"

namespace
{
	const FName BHOnlineLevelSetting(TEXT("BHLEVEL"));
	const FName BHOnlineBuildSetting(TEXT("BHBUILD"));
	const FString BHOnlineBuildId(TEXT("BlackoutHunt-0.2.0-beta.2"));
	constexpr int32 BHOnlineMaxSearchResults = 25;

	FString NormalizeRuntimeLevelName(FString LevelName)
	{
		LevelName.TrimStartAndEndInline();
		if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || LevelName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
		{
			return TEXT("Foggrounds");
		}
		return LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? TEXT("Substation") : TEXT("Facility");
	}

	FString MakeListenOptions(const FString& LevelName)
	{
		return FString::Printf(TEXT("listen?BHLevel=%s?BHFogPreset=Heavy"), *NormalizeRuntimeLevelName(LevelName));
	}
}

void UBHGameInstance::Init()
{
	Super::Init();

	AutomationConfig = FBHAutomationSupport::ParseCommandLine(FCommandLine::Get());

	const FString GpuBrand = FPlatformMisc::GetPrimaryGPUBrand();
	if (GpuBrand.Contains(TEXT("VirtualBox"), ESearchCase::IgnoreCase))
	{
		AutomationConfig.bVirtualBoxSafeDetected = true;
		UE_LOG(LogTemp, Display, TEXT("BlackoutHunt VirtualBox-safe graphics detected from GPU: %s"), *GpuBrand);
	}

	if (AutomationConfig.ShouldUseVirtualBoxSafeMode())
	{
		UE_LOG(LogTemp, Display, TEXT("BlackoutHunt VM-safe graphics mode requested."));
	}

	if (AutomationConfig.bEnabled)
	{
		LogAutomationMarker(TEXT("BH_AUTOMATION_BOOT"));
	}
}

void UBHGameInstance::Shutdown()
{
	if (!GameHotspotSsid.IsEmpty())
	{
		FBHNetworkSupport::StopGameHotspot(GameHotspotSsid);
		GameHotspotSsid.Reset();
		GameHotspotPassphrase.Reset();
	}

	FBHNetworkSupport::StopInternetTunnel();
	Super::Shutdown();
}

void UBHGameInstance::HostGame()
{
	OpenListenLevel(TEXT("Facility"));
}

void UBHGameInstance::HostSubstationGame()
{
	OpenListenLevel(TEXT("Substation"));
}

void UBHGameInstance::HostFoggroundsGame()
{
	OpenListenLevel(TEXT("Foggrounds"));
}

void UBHGameInstance::JoinGame(const FString& Address)
{
	const FString NormalizedAddress = FBHNetworkSupport::NormalizeJoinAddress(Address);
	if (NormalizedAddress.IsEmpty())
	{
		SetLastNetworkMessage(TEXT("Usage: JoinGame <host-ip-or-domain>:7777"));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->ClientTravel(NormalizedAddress, TRAVEL_Absolute);
		}
	}
}

void UBHGameInstance::HostOnlineGame()
{
	FString Message;
	TryHostOnlineGame(TEXT("Facility"), Message);
}

void UBHGameInstance::HostOnlineSubstationGame()
{
	FString Message;
	TryHostOnlineGame(TEXT("Substation"), Message);
}

void UBHGameInstance::HostOnlineFoggroundsGame()
{
	FString Message;
	TryHostOnlineGame(TEXT("Foggrounds"), Message);
}

void UBHGameInstance::FindOnlineGames()
{
	FString Message;
	TryFindOnlineGames(Message);
}

void UBHGameInstance::JoinOnlineGame(int32 SessionIndex)
{
	FString Message;
	TryJoinOnlineGame(SessionIndex, Message);
}

void UBHGameInstance::DestroyOnlineSession()
{
	FString Message;
	TryDestroyOnlineSession(Message);
}

void UBHGameInstance::CreateGameHotspot()
{
	FString Message;
	TryCreateGameHotspot(Message);
}

void UBHGameInstance::StopGameHotspot()
{
	FString Message;
	TryStopGameHotspot(Message);
}

void UBHGameInstance::StartInternetTunnel(int32 LocalPort)
{
	FString Message;
	TryStartInternetTunnel(Message, LocalPort);
}

void UBHGameInstance::StopInternetTunnel()
{
	FString Message;
	TryStopInternetTunnel(Message);
}

void UBHGameInstance::OpenInternetTunnelSetup(int32 LocalPort)
{
	FString Message;
	TryOpenInternetTunnelSetup(Message, LocalPort);
}

bool UBHGameInstance::TryHostOnlineGame(const FString& LevelName, FString& OutMessage)
{
	if (!IsHostNetworkContext(OutMessage, TEXT("host online sessions")))
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	if (bOnlineSessionBusy)
	{
		OutMessage = TEXT("An online session operation is already in progress.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(OutMessage);
	if (!Sessions.IsValid())
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		OutMessage = TEXT("An online session already exists. Leave the current session or run DestroyOnlineSession first.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 12;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bIsLANMatch = false;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUsesStats = false;
	Settings.BuildUniqueId = GetTypeHash(BHOnlineBuildId);
	Settings.Set(SETTING_MAPNAME, FString(TEXT("/Engine/Maps/Entry")), EOnlineDataAdvertisementType::ViaOnlineService);
	const FString NormalizedLevel = NormalizeRuntimeLevelName(LevelName);
	Settings.Set(BHOnlineLevelSetting, NormalizedLevel, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(BHOnlineBuildSetting, BHOnlineBuildId, EOnlineDataAdvertisementType::ViaOnlineService);

	PendingOnlineLevelName = NormalizedLevel;
	CreateOnlineSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UBHGameInstance::OnCreateOnlineSessionComplete));

	bOnlineSessionBusy = true;
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateOnlineSessionCompleteHandle);
		bOnlineSessionBusy = false;
		OutMessage = FString::Printf(TEXT("%s could not start online session creation."), *GetOnlineSubsystemName());
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Creating %s online session through %s..."), *PendingOnlineLevelName, *GetOnlineSubsystemName());
	SetLastNetworkMessage(OutMessage);
	return true;
}

bool UBHGameInstance::TryFindOnlineGames(FString& OutMessage)
{
	if (bOnlineSessionBusy)
	{
		OutMessage = TEXT("An online session operation is already in progress.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(OutMessage);
	if (!Sessions.IsValid())
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	FoundOnlineSessions.Reset();
	OnlineSessionSummaries.Reset();
	ActiveOnlineSessionSearch = MakeShared<FOnlineSessionSearch>();
	ActiveOnlineSessionSearch->MaxSearchResults = BHOnlineMaxSearchResults;
	ActiveOnlineSessionSearch->bIsLanQuery = false;
	ActiveOnlineSessionSearch->TimeoutInSeconds = 12.0f;
	ActiveOnlineSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	FindOnlineSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UBHGameInstance::OnFindOnlineSessionsComplete));

	bOnlineSessionBusy = true;
	if (!Sessions->FindSessions(0, ActiveOnlineSessionSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindOnlineSessionsCompleteHandle);
		ActiveOnlineSessionSearch.Reset();
		bOnlineSessionBusy = false;
		OutMessage = FString::Printf(TEXT("%s could not start online session search."), *GetOnlineSubsystemName());
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Searching for online sessions through %s..."), *GetOnlineSubsystemName());
	SetLastNetworkMessage(OutMessage);
	return true;
}

bool UBHGameInstance::TryJoinOnlineGame(int32 SessionIndex, FString& OutMessage)
{
	if (bOnlineSessionBusy)
	{
		OutMessage = TEXT("An online session operation is already in progress.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	if (!FoundOnlineSessions.IsValidIndex(SessionIndex))
	{
		OutMessage = FString::Printf(TEXT("No online session at index %d. Run FindOnlineGames first."), SessionIndex);
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(OutMessage);
	if (!Sessions.IsValid())
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	JoinOnlineSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UBHGameInstance::OnJoinOnlineSessionComplete));

	bOnlineSessionBusy = true;
	if (!Sessions->JoinSession(0, NAME_GameSession, FoundOnlineSessions[SessionIndex]))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinOnlineSessionCompleteHandle);
		bOnlineSessionBusy = false;
		OutMessage = FString::Printf(TEXT("%s could not start joining online session %d."), *GetOnlineSubsystemName(), SessionIndex);
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Joining online session %d through %s..."), SessionIndex, *GetOnlineSubsystemName());
	SetLastNetworkMessage(OutMessage);
	return true;
}

bool UBHGameInstance::TryDestroyOnlineSession(FString& OutMessage)
{
	if (!IsHostNetworkContext(OutMessage, TEXT("destroy online sessions")))
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	if (bOnlineSessionBusy)
	{
		OutMessage = TEXT("An online session operation is already in progress.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(OutMessage);
	if (!Sessions.IsValid())
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	if (!Sessions->GetNamedSession(NAME_GameSession))
	{
		OutMessage = TEXT("No online session is active.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	DestroyOnlineSessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UBHGameInstance::OnDestroyOnlineSessionComplete));

	bOnlineSessionBusy = true;
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyOnlineSessionCompleteHandle);
		bOnlineSessionBusy = false;
		OutMessage = TEXT("Could not start online session cleanup.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	OutMessage = TEXT("Leaving online session...");
	SetLastNetworkMessage(OutMessage);
	return true;
}

bool UBHGameInstance::TryCreateGameHotspot(FString& OutMessage)
{
	if (!IsHostNetworkContext(OutMessage, TEXT("create the game hotspot")))
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings || !Settings->bAllowHotspotHelper)
	{
		OutMessage = TEXT("Game hotspot helper is disabled in classroom settings.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const FBHHotspotLaunchResult Result = FBHNetworkSupport::StartGameHotspot();
	SetLastNetworkMessage(Result.Message);
	OutMessage = LastNetworkMessage;

	if (Result.bSuccess)
	{
		GameHotspotSsid = Result.Ssid;
		GameHotspotPassphrase = Result.Passphrase;
	}

	return Result.bSuccess;
}

bool UBHGameInstance::TryStopGameHotspot(FString& OutMessage)
{
	if (!IsHostNetworkContext(OutMessage, TEXT("stop the game hotspot")))
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const FBHHotspotLaunchResult Result = FBHNetworkSupport::StopGameHotspot(GameHotspotSsid);
	SetLastNetworkMessage(Result.Message);
	OutMessage = LastNetworkMessage;

	if (Result.bSuccess)
	{
		GameHotspotSsid.Reset();
		GameHotspotPassphrase.Reset();
	}

	return Result.bSuccess;
}

bool UBHGameInstance::TryStartInternetTunnel(FString& OutMessage, int32 LocalPort)
{
	if (!IsHostNetworkContext(OutMessage, TEXT("start the internet tunnel helper")))
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings || !Settings->bAllowTunnelHelper)
	{
		OutMessage = TEXT("Internet tunnel helper is disabled in classroom settings.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const FBHInternetTunnelResult Result = FBHNetworkSupport::StartInternetTunnel(LocalPort);
	SetLastNetworkMessage(Result.Message);
	OutMessage = LastNetworkMessage;
	return Result.bSuccess;
}

bool UBHGameInstance::TryStopInternetTunnel(FString& OutMessage)
{
	if (!IsHostNetworkContext(OutMessage, TEXT("stop the internet tunnel helper")))
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const FBHInternetTunnelResult Result = FBHNetworkSupport::StopInternetTunnel();
	SetLastNetworkMessage(Result.Message);
	OutMessage = LastNetworkMessage;
	return Result.bSuccess;
}

bool UBHGameInstance::TryOpenInternetTunnelSetup(FString& OutMessage, int32 LocalPort)
{
	if (!IsHostNetworkContext(OutMessage, TEXT("open internet tunnel setup")))
	{
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings || !Settings->bAllowTunnelHelper)
	{
		OutMessage = TEXT("Internet tunnel helper is disabled in classroom settings.");
		SetLastNetworkMessage(OutMessage);
		return false;
	}

	const FBHInternetTunnelResult Result = FBHNetworkSupport::OpenInternetTunnelSetup(LocalPort);
	SetLastNetworkMessage(Result.Message);
	OutMessage = LastNetworkMessage;
	return Result.bSuccess;
}

bool UBHGameInstance::IsOnlineSessionBusy() const
{
	return bOnlineSessionBusy;
}

const TArray<FBHOnlineSessionSummary>& UBHGameInstance::GetOnlineSessionSummaries() const
{
	return OnlineSessionSummaries;
}

FString UBHGameInstance::GetOnlineSubsystemName() const
{
	if (const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
	{
		return OnlineSubsystem->GetSubsystemName().ToString();
	}

	return TEXT("None");
}

const FString& UBHGameInstance::GetGameHotspotSsid() const
{
	return GameHotspotSsid;
}

const FString& UBHGameInstance::GetGameHotspotPassphrase() const
{
	return GameHotspotPassphrase;
}

const FString& UBHGameInstance::GetLastNetworkMessage() const
{
	return LastNetworkMessage;
}

const FBHAutomationConfig& UBHGameInstance::GetAutomationConfig() const
{
	return AutomationConfig;
}

bool UBHGameInstance::IsAutomationEnabled() const
{
	return AutomationConfig.bEnabled;
}

bool UBHGameInstance::ShouldUseVirtualBoxSafeMode() const
{
	return AutomationConfig.ShouldUseVirtualBoxSafeMode();
}

bool UBHGameInstance::ConsumeAutomationHost(FString& OutHostMode)
{
	if (bAutomationHostConsumed || !AutomationConfig.HasAutoHost())
	{
		return false;
	}

	bAutomationHostConsumed = true;
	OutHostMode = AutomationConfig.AutoHost;
	return true;
}

bool UBHGameInstance::ConsumeAutomationJoin(FString& OutAddress)
{
	if (bAutomationJoinConsumed || !AutomationConfig.HasAutoJoin())
	{
		return false;
	}

	bAutomationJoinConsumed = true;
	OutAddress = AutomationConfig.AutoJoin;
	return true;
}

bool UBHGameInstance::ShouldAutoReady() const
{
	return AutomationConfig.ShouldAutoReady();
}

float UBHGameInstance::GetAutomationQuitSeconds() const
{
	return AutomationConfig.ShouldAutoQuit() ? AutomationConfig.AutoQuitSeconds : 0.0f;
}

void UBHGameInstance::LogAutomationMarker(const FString& Marker) const
{
	if (!AutomationConfig.bEnabled || Marker.IsEmpty())
	{
		return;
	}

	const FString MarkerLine = FBHAutomationSupport::MakeMarkerLine(AutomationConfig, Marker);
	UE_LOG(LogTemp, Display, TEXT("%s"), *MarkerLine);

	const FString LogDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs")));
	IFileManager::Get().MakeDirectory(*LogDir, true);
	const FString MarkerLogPath = FPaths::Combine(LogDir, TEXT("BlackoutHuntAutomation.log"));
	FFileHelper::SaveStringToFile(
		MarkerLine + LINE_TERMINATOR,
		*MarkerLogPath,
		FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(),
		FILEWRITE_Append);
}

bool UBHGameInstance::LogAutomationMarkerOnce(const FString& Marker)
{
	if (!AutomationConfig.bEnabled || Marker.IsEmpty() || AutomationMarkersLogged.Contains(Marker))
	{
		return false;
	}

	AutomationMarkersLogged.Add(Marker);
	LogAutomationMarker(Marker);
	return true;
}

void UBHGameInstance::RequestCleanExit(const FString& Reason)
{
	LogAutomationMarkerOnce(TEXT("CLEAN_QUIT"));

	if (!GameHotspotSsid.IsEmpty())
	{
		FBHNetworkSupport::StopGameHotspot(GameHotspotSsid);
		GameHotspotSsid.Reset();
		GameHotspotPassphrase.Reset();
	}

	FBHNetworkSupport::StopInternetTunnel();
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt clean exit requested: %s"), *Reason);
	FPlatformMisc::RequestExit(false);
}

IOnlineSessionPtr UBHGameInstance::GetOnlineSessionInterface(FString& OutMessage) const
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		OutMessage = TEXT("No OnlineSubsystem is loaded. The project defaults to OnlineSubsystemNull for local tests; configure EOS or Steam for relay/lobby internet play.");
		return nullptr;
	}

	IOnlineSessionPtr Sessions = OnlineSubsystem->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		OutMessage = FString::Printf(TEXT("Online subsystem %s does not provide session support."), *OnlineSubsystem->GetSubsystemName().ToString());
		return nullptr;
	}

	return Sessions;
}

bool UBHGameInstance::IsHostNetworkContext(FString& OutMessage, const TCHAR* ActionDescription) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		OutMessage = TEXT("No world is available for this network operation.");
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_Client)
	{
		const FString Action = ActionDescription && FCString::Strlen(ActionDescription) > 0
			? FString(ActionDescription)
			: FString(TEXT("use this classroom control"));
		OutMessage = FString::Printf(TEXT("Only the host machine can %s."), *Action);
		UE_LOG(LogTemp, Warning, TEXT("Denied host-only network action '%s' on a client instance."), *Action);
		return false;
	}

	return true;
}

void UBHGameInstance::SetLastNetworkMessage(const FString& Message)
{
	LastNetworkMessage = Message;
	UE_LOG(LogTemp, Display, TEXT("%s"), *LastNetworkMessage);
}

void UBHGameInstance::OpenListenLevel(const FString& LevelName)
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, MakeListenOptions(LevelName));
}

void UBHGameInstance::RebuildOnlineSessionSummaries()
{
	OnlineSessionSummaries.Reset();

	for (const FOnlineSessionSearchResult& Result : FoundOnlineSessions)
	{
		FBHOnlineSessionSummary Summary;
		Summary.HostName = Result.Session.OwningUserName.IsEmpty() ? TEXT("Unknown host") : Result.Session.OwningUserName;
		Summary.SubsystemName = GetOnlineSubsystemName();
		Summary.SessionId = Result.GetSessionIdStr();
		Summary.PingMs = Result.PingInMs;
		Summary.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		Summary.CurrentPlayers = Summary.MaxPlayers - Result.Session.NumOpenPublicConnections;

		if (!Result.Session.SessionSettings.Get(BHOnlineLevelSetting, Summary.LevelName) || Summary.LevelName.IsEmpty())
		{
			Summary.LevelName = TEXT("Facility");
		}

		OnlineSessionSummaries.Add(Summary);
	}
}

void UBHGameInstance::ClearOnlineDelegates(const IOnlineSessionPtr& Sessions)
{
	if (!Sessions.IsValid())
	{
		return;
	}

	if (CreateOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateOnlineSessionCompleteHandle);
		CreateOnlineSessionCompleteHandle.Reset();
	}

	if (StartOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartOnlineSessionCompleteHandle);
		StartOnlineSessionCompleteHandle.Reset();
	}

	if (FindOnlineSessionsCompleteHandle.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindOnlineSessionsCompleteHandle);
		FindOnlineSessionsCompleteHandle.Reset();
	}

	if (JoinOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinOnlineSessionCompleteHandle);
		JoinOnlineSessionCompleteHandle.Reset();
	}

	if (DestroyOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyOnlineSessionCompleteHandle);
		DestroyOnlineSessionCompleteHandle.Reset();
	}
}

void UBHGameInstance::OnCreateOnlineSessionComplete(FName SessionName, bool bWasSuccessful)
{
	FString Message;
	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(Message);
	if (!Sessions.IsValid())
	{
		bOnlineSessionBusy = false;
		SetLastNetworkMessage(Message);
		return;
	}

	if (CreateOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateOnlineSessionCompleteHandle);
		CreateOnlineSessionCompleteHandle.Reset();
	}

	if (!bWasSuccessful)
	{
		bOnlineSessionBusy = false;
		SetLastNetworkMessage(FString::Printf(TEXT("%s failed to create the online session."), *GetOnlineSubsystemName()));
		return;
	}

	StartOnlineSessionCompleteHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &UBHGameInstance::OnStartOnlineSessionComplete));

	if (!Sessions->StartSession(SessionName))
	{
		if (StartOnlineSessionCompleteHandle.IsValid())
		{
			Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartOnlineSessionCompleteHandle);
			StartOnlineSessionCompleteHandle.Reset();
		}

		bOnlineSessionBusy = false;
		SetLastNetworkMessage(TEXT("Online session was created, but could not be started."));
		return;
	}

	SetLastNetworkMessage(TEXT("Online session created. Starting session..."));
}

void UBHGameInstance::OnStartOnlineSessionComplete(FName SessionName, bool bWasSuccessful)
{
	FString Message;
	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(Message);
	if (Sessions.IsValid() && StartOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartOnlineSessionCompleteHandle);
		StartOnlineSessionCompleteHandle.Reset();
	}

	bOnlineSessionBusy = false;
	if (!bWasSuccessful)
	{
		SetLastNetworkMessage(TEXT("Online session was created, but start failed."));
		return;
	}

	SetLastNetworkMessage(FString::Printf(TEXT("Online session hosted through %s. Opening %s..."), *GetOnlineSubsystemName(), *PendingOnlineLevelName));
	OpenListenLevel(PendingOnlineLevelName);
}

void UBHGameInstance::OnFindOnlineSessionsComplete(bool bWasSuccessful)
{
	FString Message;
	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(Message);
	if (Sessions.IsValid() && FindOnlineSessionsCompleteHandle.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindOnlineSessionsCompleteHandle);
		FindOnlineSessionsCompleteHandle.Reset();
	}

	bOnlineSessionBusy = false;
	FoundOnlineSessions.Reset();

	if (bWasSuccessful && ActiveOnlineSessionSearch.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : ActiveOnlineSessionSearch->SearchResults)
		{
			if (Result.IsValid() || Result.IsSessionInfoValid())
			{
				FoundOnlineSessions.Add(Result);
			}
		}
	}

	ActiveOnlineSessionSearch.Reset();
	RebuildOnlineSessionSummaries();

	if (!bWasSuccessful)
	{
		SetLastNetworkMessage(FString::Printf(TEXT("%s online session search failed."), *GetOnlineSubsystemName()));
		return;
	}

	if (FoundOnlineSessions.IsEmpty())
	{
		SetLastNetworkMessage(FString::Printf(TEXT("No online sessions found through %s."), *GetOnlineSubsystemName()));
		return;
	}

	SetLastNetworkMessage(FString::Printf(TEXT("Found %d online session(s). Use JoinOnlineGame <index> or the menu join button."), FoundOnlineSessions.Num()));
}

void UBHGameInstance::OnJoinOnlineSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	FString Message;
	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(Message);
	if (!Sessions.IsValid())
	{
		bOnlineSessionBusy = false;
		SetLastNetworkMessage(Message);
		return;
	}

	if (JoinOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinOnlineSessionCompleteHandle);
		JoinOnlineSessionCompleteHandle.Reset();
	}

	bOnlineSessionBusy = false;
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		SetLastNetworkMessage(TEXT("Online session join failed."));
		return;
	}

	FString ConnectString;
	if (!Sessions->GetResolvedConnectString(SessionName, ConnectString) || ConnectString.IsEmpty())
	{
		SetLastNetworkMessage(TEXT("Joined the session, but no travel address was resolved by the online subsystem."));
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		SetLastNetworkMessage(TEXT("Joined the session, but no local player controller was available for travel."));
		return;
	}

	SetLastNetworkMessage(FString::Printf(TEXT("Joined online session. Traveling to %s"), *ConnectString));
	PC->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UBHGameInstance::OnDestroyOnlineSessionComplete(FName SessionName, bool bWasSuccessful)
{
	FString Message;
	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(Message);
	if (Sessions.IsValid() && DestroyOnlineSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyOnlineSessionCompleteHandle);
		DestroyOnlineSessionCompleteHandle.Reset();
	}

	bOnlineSessionBusy = false;
	SetLastNetworkMessage(bWasSuccessful ? TEXT("Online session cleaned up.") : TEXT("Online session cleanup failed."));
}

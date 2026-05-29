#include "BHGameInstance.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHNetworkSupport.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"

#include <initializer_list>

#ifndef SEARCH_PRESENCE
#define SEARCH_PRESENCE FName(TEXT("PRESENCESEARCH"))
#endif

namespace
{
	const FName BHOnlineLevelSetting(TEXT("BHLEVEL"));
	const FName BHOnlineBuildSetting(TEXT("BHBUILD"));
	const FString BHOnlineBuildId(TEXT("BlackoutHunt-0.5.0-beta.1"));
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
		// Pin a fresh hosted game to stage 0. Without an explicit BHStageIndex option the GameMode
		// falls back to the GameInstance's persisted stage index, which only resets when a train run
		// reaches its final recap — so abandoning a run mid-way and hosting again would otherwise
		// inherit the stale stage (wrong durations / inflated targets / "final" framing).
		return FString::Printf(TEXT("listen?BHLevel=%s?BHFogPreset=Heavy?BHStageIndex=0"), *NormalizeRuntimeLevelName(LevelName));
	}

	void ShowTravelLoadingScreen(UWorld* World, const FString& Title, const FString& Detail)
	{
		if (ABHPlayerController* PC = World ? Cast<ABHPlayerController>(World->GetFirstPlayerController()) : nullptr)
		{
			PC->ShowTravelLoadingScreen(Title, Detail);
		}
	}

	FString TravelStableIdForPlayerState(const APlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return TEXT("");
		}

		const FString UniqueId = PlayerState->GetUniqueId().IsValid() ? PlayerState->GetUniqueId()->ToString() : FString();
		if (!UniqueId.IsEmpty())
		{
			return UniqueId;
		}

		return PlayerState->GetPlayerName();
	}

	// Match a stored travel entry to a player. When BOTH sides carry a real stable id (the online /
	// unique-net-id case) they must match exactly — a coinciding display name must NOT bind a player
	// to a different account's saved progress. The display-name fallback is allowed only when a stable
	// id is unavailable on at least one side (the OSS-Null/LAN case), and never matches a blank name.
	bool TravelEntryMatches(const FBHTravelPlayerProgress& Progress, const FString& StableId, const FString& PlayerName)
	{
		if (!StableId.IsEmpty() && !Progress.StableId.IsEmpty())
		{
			return Progress.StableId == StableId;
		}
		return !PlayerName.IsEmpty() && Progress.PlayerName == PlayerName;
	}

	FString BHTelemetryCsvEscape(const FString& Input)
	{
		FString Escaped = Input;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	FString BHTelemetrySanitizeToken(FString Token)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			return TEXT("Unknown");
		}

		for (TCHAR& Ch : Token)
		{
			if (!FChar::IsAlnum(Ch) && Ch != TEXT('-') && Ch != TEXT('_'))
			{
				Ch = TEXT('_');
			}
		}
		return Token.Left(48);
	}

	bool BHSessionMatchesBuild(const FOnlineSessionSearchResult& Result)
	{
		FString BuildId;
		return Result.Session.SessionSettings.Get(BHOnlineBuildSetting, BuildId)
			&& BuildId.Equals(BHOnlineBuildId, ESearchCase::IgnoreCase);
	}
}

void UBHGameInstance::Init()
{
	Super::Init();

	// Surface connection/travel failures to the player. The engine otherwise bounces a failed join back
	// to the menu map silently, so a student who mistypes the address, joins before the host is up, or
	// runs a mismatched build just sees a black screen then the menu with no explanation.
	if (GEngine)
	{
		EngineNetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UBHGameInstance::HandleEngineNetworkFailure);
		EngineTravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UBHGameInstance::HandleEngineTravelFailure);
	}

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
		AutomationStartTimeSeconds = FPlatformTime::Seconds();
		LogAutomationMarker(TEXT("BH_AUTOMATION_BOOT"));
	}
}

void UBHGameInstance::Shutdown()
{
	if (GEngine)
	{
		if (EngineNetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(EngineNetworkFailureHandle);
			EngineNetworkFailureHandle.Reset();
		}
		if (EngineTravelFailureHandle.IsValid())
		{
			GEngine->OnTravelFailure().Remove(EngineTravelFailureHandle);
			EngineTravelFailureHandle.Reset();
		}
	}

	if (!GameHotspotSsid.IsEmpty())
	{
		FBHNetworkSupport::StopGameHotspot(GameHotspotSsid);
		GameHotspotSsid.Reset();
		GameHotspotPassphrase.Reset();
	}

	FBHNetworkSupport::StopInternetTunnel();

	FString OnlineMessage;
	if (IOnlineSessionPtr Sessions = GetOnlineSessionInterface(OnlineMessage))
	{
		ClearOnlineDelegates(Sessions);
	}

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
			if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(PC))
			{
				BHPC->ShowTravelLoadingScreen(TEXT("JOINING GAME"), FString::Printf(TEXT("Connecting to %s."), *NormalizedAddress));
			}
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

void UBHGameInstance::SetClassroomJoinAddress(const FString& Address)
{
	SetPublicJoinAddress(Address);
	if (PublicJoinAddress.IsEmpty())
	{
		SetLastNetworkMessage(TEXT("Usage: SetClassroomJoinAddress <host-or-domain>:7777"));
		return;
	}

	SetLastNetworkMessage(FString::Printf(TEXT("Classroom join address set to %s."), *PublicJoinAddress));
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

	if (!IsOnlineIdentityReadyForSessions(OutMessage))
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

	if (!IsOnlineIdentityReadyForSessions(OutMessage))
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
	ActiveOnlineSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

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

	if (!IsOnlineIdentityReadyForSessions(OutMessage))
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

void UBHGameInstance::LeaveOnlineSessionIfActive()
{
	if (bOnlineSessionBusy)
	{
		return;
	}

	FString IgnoredMessage;
	IOnlineSessionPtr Sessions = GetOnlineSessionInterface(IgnoredMessage);
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		return;
	}

	DestroyOnlineSessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UBHGameInstance::OnDestroyOnlineSessionComplete));

	bOnlineSessionBusy = true;
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyOnlineSessionCompleteHandle);
		bOnlineSessionBusy = false;
	}
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
	if (!Result.TunnelAddress.IsEmpty())
	{
		SetPublicJoinAddress(Result.TunnelAddress);
	}
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

FString UBHGameInstance::ConsumePendingNetworkFailureMessage()
{
	FString Message = PendingNetworkFailureMessage;
	PendingNetworkFailureMessage.Reset();
	return Message;
}

void UBHGameInstance::HandleEngineNetworkFailure(UWorld* FailedWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// On a listen server the host is also notified when a student disconnects; don't overwrite the
	// host's own status line with a client-side failure message.
	const bool bIsServer = NetDriver ? (NetDriver->GetNetMode() != NM_Client) : false;
	if (bIsServer)
	{
		return;
	}

	FString Message;
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionTimeout:
		Message = TEXT("Could not reach the host (timed out). Check the join address and that the teacher has started hosting.");
		break;
	case ENetworkFailure::PendingConnectionFailure:
		Message = TEXT("The host refused the connection or is not reachable yet. Make sure hosting has started and the tunnel/port is open.");
		break;
	case ENetworkFailure::ConnectionLost:
		Message = TEXT("Lost connection to the host.");
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		Message = TEXT("Your game build does not match the host. Make sure every device runs the same BlackoutHunt version, then try again.");
		break;
	case ENetworkFailure::NetGuidMismatch:
	case ENetworkFailure::NetChecksumMismatch:
		Message = TEXT("Content mismatch with the host. Make sure you are on the same build, then try again.");
		break;
	default:
		Message = FString::Printf(TEXT("Could not connect to the host (%s)."), ENetworkFailure::ToString(FailureType));
		break;
	}

	PendingNetworkFailureMessage = Message;
	SetLastNetworkMessage(Message);
	UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt network failure: %s (%s)"), *Message, *ErrorString);
}

void UBHGameInstance::HandleEngineTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	FString Message;
	switch (FailureType)
	{
	case ETravelFailure::InvalidURL:
		Message = TEXT("That join address is not valid. Use the host's address as host-or-domain:7777.");
		break;
	case ETravelFailure::PackageMissing:
	case ETravelFailure::PackageVersion:
		Message = TEXT("Missing or mismatched map/content versus the host. Update to the same build.");
		break;
	case ETravelFailure::PendingNetGameCreateFailure:
	case ETravelFailure::ClientTravelFailure:
		Message = TEXT("Could not connect to the host. Check the address and that the host is running.");
		break;
	default:
		Message = FString::Printf(TEXT("Travel to the host failed (%s)."), ETravelFailure::ToString(FailureType));
		break;
	}

	PendingNetworkFailureMessage = Message;
	SetLastNetworkMessage(Message);
	UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt travel failure: %s (%s)"), *Message, *ErrorString);
}

const FString& UBHGameInstance::GetPublicJoinAddress() const
{
	return PublicJoinAddress;
}

FString UBHGameInstance::GetPreferredJoinAddress(int32 LocalPort) const
{
	if (!PublicJoinAddress.IsEmpty())
	{
		return PublicJoinAddress;
	}

	return FBHNetworkSupport::ResolveLocalJoinAddress(LocalPort);
}

FString UBHGameInstance::GetConfiguredClassroomJoinAddress(int32 LocalPort) const
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings)
	{
		return FString();
	}

	return FBHNetworkSupport::NormalizePreferredJoinEndpoint(Settings->ClassroomJoinEndpoints, LocalPort);
}

FString UBHGameInstance::GetPreferredClassroomJoinAddress(int32 LocalPort) const
{
	if (!PublicJoinAddress.IsEmpty())
	{
		return PublicJoinAddress;
	}

	const FString ConfiguredEndpoint = GetConfiguredClassroomJoinAddress(LocalPort);
	if (!ConfiguredEndpoint.IsEmpty())
	{
		return ConfiguredEndpoint;
	}

	return FBHNetworkSupport::ResolveLocalJoinAddress(LocalPort);
}

void UBHGameInstance::SetPublicJoinAddress(const FString& Address)
{
	PublicJoinAddress = FBHNetworkSupport::NormalizeJoinAddress(Address);
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

int32 UBHGameInstance::GetAutomationMinReadyPlayers() const
{
	return AutomationConfig.GetAutoMinPlayers();
}

float UBHGameInstance::GetAutomationQuitSeconds() const
{
	return AutomationConfig.ShouldAutoQuit() ? AutomationConfig.AutoQuitSeconds : 0.0f;
}

bool UBHGameInstance::ShouldRequestAutomationCleanExit() const
{
	const float QuitSeconds = GetAutomationQuitSeconds();
	return QuitSeconds > 0.0f
		&& AutomationStartTimeSeconds > 0.0
		&& FPlatformTime::Seconds() - AutomationStartTimeSeconds >= static_cast<double>(QuitSeconds);
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

void UBHGameInstance::PersistTravelPlayerState(const ABHPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	const FString StableId = TravelStableIdForPlayerState(PlayerState);
	const FString PlayerName = PlayerState->GetPlayerName();
	if (StableId.IsEmpty() && PlayerName.IsEmpty())
	{
		return;
	}

	FBHTravelPlayerProgress* Existing = TravelPlayerProgress.FindByPredicate([&](const FBHTravelPlayerProgress& Progress)
	{
		return TravelEntryMatches(Progress, StableId, PlayerName);
	});

	FBHTravelPlayerProgress& Progress = Existing ? *Existing : TravelPlayerProgress.AddDefaulted_GetRef();
	Progress.StableId = StableId;
	Progress.PlayerName = PlayerName;
	Progress.PlayerRole = PlayerState->PlayerRole;
	Progress.DesiredRole = PlayerState->DesiredRole;
	Progress.SpectatorRolePreference = PlayerState->SpectatorRolePreference;
	Progress.LifeState = PlayerState->LifeState;
	Progress.bFakeHunterEligible = PlayerState->bFakeHunterEligible;
	Progress.RevisionStats = PlayerState->RevisionStats;
	Progress.QuestionPoints = PlayerState->QuestionPoints;
	Progress.LifetimeQuestionPoints = PlayerState->LifetimeQuestionPoints;
	Progress.HunterPoints = PlayerState->HunterPoints;
	Progress.LifetimeHunterPoints = PlayerState->LifetimeHunterPoints;
	Progress.Powerups = PlayerState->Powerups;
}

bool UBHGameInstance::RestoreTravelPlayerState(ABHPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}

	const FString StableId = TravelStableIdForPlayerState(PlayerState);
	const FString PlayerName = PlayerState->GetPlayerName();
	const FBHTravelPlayerProgress* Existing = TravelPlayerProgress.FindByPredicate([&](const FBHTravelPlayerProgress& Progress)
	{
		return TravelEntryMatches(Progress, StableId, PlayerName);
	});

	if (!Existing)
	{
		return false;
	}

	PlayerState->SetRole(Existing->PlayerRole);
	PlayerState->SetDesiredRole(Existing->DesiredRole);
	PlayerState->SetSpectatorRolePreference(Existing->SpectatorRolePreference);
	PlayerState->SetLifeState(EBHPlayerLifeState::Alive);
	PlayerState->SetFakeHunterEligible(false);
	PlayerState->RevisionStats = Existing->RevisionStats;
	PlayerState->QuestionPoints = FMath::Max(0, Existing->QuestionPoints);
	PlayerState->LifetimeQuestionPoints = FMath::Max(PlayerState->QuestionPoints, Existing->LifetimeQuestionPoints);
	PlayerState->HunterPoints = FMath::Max(0, Existing->HunterPoints);
	PlayerState->LifetimeHunterPoints = FMath::Max(PlayerState->HunterPoints, Existing->LifetimeHunterPoints);
	PlayerState->Powerups = Existing->Powerups;
	for (FBHPowerupInventoryEntry& Entry : PlayerState->Powerups)
	{
		Entry.CooldownEndServerTime = 0.0f;
	}
	return true;
}

void UBHGameInstance::MarkTravelPlayerLeftForReconnect(const ABHPlayerState* PlayerState, float ServerTimeSeconds)
{
	// The caller's world time is intentionally ignored for the grace stamp: it resets on ServerTravel.
	// We stamp a process-wide monotonic clock so elapsed time stays valid across travel boundaries.
	(void)ServerTimeSeconds;
	if (!PlayerState)
	{
		return;
	}

	// Persist the player's current state, then stamp the leave time on the matching entry so a
	// rejoin within the grace window can be recognized and restored.
	PersistTravelPlayerState(PlayerState);

	const FString StableId = TravelStableIdForPlayerState(PlayerState);
	const FString PlayerName = PlayerState->GetPlayerName();
	FBHTravelPlayerProgress* Existing = TravelPlayerProgress.FindByPredicate([&](const FBHTravelPlayerProgress& Progress)
	{
		return TravelEntryMatches(Progress, StableId, PlayerName);
	});

	if (Existing)
	{
		Existing->LeftServerWorldTime = FPlatformTime::Seconds();
	}
}

bool UBHGameInstance::TryGetReconnectProgress(const ABHPlayerState* PlayerState, float NowServerTimeSeconds, float GraceSeconds, FBHTravelPlayerProgress& OutProgress) const
{
	if (!PlayerState || GraceSeconds <= 0.0f)
	{
		return false;
	}

	const FString StableId = TravelStableIdForPlayerState(PlayerState);
	const FString PlayerName = PlayerState->GetPlayerName();
	const FBHTravelPlayerProgress* Existing = TravelPlayerProgress.FindByPredicate([&](const FBHTravelPlayerProgress& Progress)
	{
		return TravelEntryMatches(Progress, StableId, PlayerName);
	});

	if (!Existing || Existing->LeftServerWorldTime < 0.0)
	{
		return false;
	}

	// Compare against the same process-wide monotonic clock the leave time was stamped with, so the
	// grace window survives ServerTravel. The caller's world time is ignored on purpose.
	(void)NowServerTimeSeconds;
	const double ElapsedSinceLeft = FPlatformTime::Seconds() - Existing->LeftServerWorldTime;
	if (ElapsedSinceLeft < 0.0 || ElapsedSinceLeft > static_cast<double>(GraceSeconds))
	{
		return false;
	}

	OutProgress = *Existing;
	return true;
}

void UBHGameInstance::ClearReconnectMark(const ABHPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	const FString StableId = TravelStableIdForPlayerState(PlayerState);
	const FString PlayerName = PlayerState->GetPlayerName();
	FBHTravelPlayerProgress* Existing = TravelPlayerProgress.FindByPredicate([&](const FBHTravelPlayerProgress& Progress)
	{
		return TravelEntryMatches(Progress, StableId, PlayerName);
	});

	if (Existing)
	{
		Existing->LeftServerWorldTime = -1.0f;
	}
}

void UBHGameInstance::ResetPersistentHunterPoints()
{
	for (FBHTravelPlayerProgress& Progress : TravelPlayerProgress)
	{
		Progress.HunterPoints = 0;
		Progress.LifetimeHunterPoints = 0;
	}
}

void UBHGameInstance::ResetPersistentTrainRunProgress()
{
	for (FBHTravelPlayerProgress& Progress : TravelPlayerProgress)
	{
		Progress.LifeState = EBHPlayerLifeState::Alive;
		Progress.bFakeHunterEligible = false;
		Progress.QuestionPoints = 0;
		Progress.LifetimeQuestionPoints = 0;
		Progress.HunterPoints = 0;
		Progress.LifetimeHunterPoints = 0;
		Progress.Powerups.Reset();
	}
}

void UBHGameInstance::RecordQuestionAttempt(const FBHQuestionAttemptRecord& Attempt)
{
	QuestionAttemptHistory.Add(Attempt);
	if (QuestionAttemptHistory.Num() > 4096)
	{
		QuestionAttemptHistory.RemoveAt(0, QuestionAttemptHistory.Num() - 4096);
	}
}

const TArray<FBHQuestionAttemptRecord>& UBHGameInstance::GetQuestionAttemptHistory() const
{
	return QuestionAttemptHistory;
}

void UBHGameInstance::ClearQuestionAttemptHistory()
{
	QuestionAttemptHistory.Reset();
}

FString UBHGameInstance::GetAnonymousTelemetryPlayerTag(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return TEXT("");
	}

	if (PlaytestTelemetrySessionId.IsEmpty())
	{
		PlaytestTelemetrySessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	const int32 PlayerId = PlayerState->GetPlayerId();
	// Uses only session/runtime identifiers, never names or online account IDs.
	const FString PlayerKey = PlayerId >= 0
		? FString::Printf(TEXT("pid:%d"), PlayerId)
		: FString::Printf(TEXT("obj:%u"), PlayerState->GetUniqueID());

	if (const FString* Existing = PlaytestTelemetryPlayerTagsById.Find(PlayerKey))
	{
		return *Existing;
	}

	const ABHPlayerState* BHPS = Cast<ABHPlayerState>(PlayerState);
	const TCHAR* Prefix = (BHPS && BHPS->IsABot()) ? TEXT("B") : TEXT("P");
	const FString NewTag = FString::Printf(TEXT("%s%03d"), Prefix, NextPlaytestTelemetryPlayerOrdinal++);
	PlaytestTelemetryPlayerTagsById.Add(PlayerKey, NewTag);
	return NewTag;
}

void UBHGameInstance::RecordPlaytestTelemetryEvent(const FBHPlaytestTelemetryEvent& Event)
{
	if (PlaytestTelemetrySessionId.IsEmpty())
	{
		PlaytestTelemetrySessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	FBHPlaytestTelemetryEvent Sanitized = Event;
	Sanitized.EventType = BHTelemetrySanitizeToken(Sanitized.EventType);
	Sanitized.RuntimeLevelName = Sanitized.RuntimeLevelName.Left(48);
	Sanitized.RoundPhase = Sanitized.RoundPhase.Left(48);
	Sanitized.EventDetail = Sanitized.EventDetail.Left(160);
	Sanitized.PlayerTag = Sanitized.PlayerTag.Left(16);
	Sanitized.TargetTag = Sanitized.TargetTag.Left(16);
	Sanitized.PlayerRole = Sanitized.PlayerRole.Left(48);
	Sanitized.TargetRole = Sanitized.TargetRole.Left(48);
	Sanitized.QuestionId = Sanitized.QuestionId.Left(80);
	Sanitized.Topic = Sanitized.Topic.Left(48);
	Sanitized.Difficulty = Sanitized.Difficulty.Left(48);
	Sanitized.Count = FMath::Max(1, Sanitized.Count);

	PlaytestTelemetryEvents.Add(Sanitized);
	if (PlaytestTelemetryEvents.Num() > 8192)
	{
		PlaytestTelemetryEvents.RemoveAt(0, PlaytestTelemetryEvents.Num() - 8192);
	}
}

bool UBHGameInstance::ExportPlaytestTelemetry(FString& OutMessage, bool bClearAfterExport)
{
	if (PlaytestTelemetryEvents.IsEmpty())
	{
		OutMessage = TEXT("No playtest telemetry has been recorded yet.");
		return false;
	}

	if (PlaytestTelemetrySessionId.IsEmpty())
	{
		PlaytestTelemetrySessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	const FString TelemetryDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PlaytestTelemetry")));
	IFileManager::Get().MakeDirectory(*TelemetryDir, true);

	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString SessionToken = BHTelemetrySanitizeToken(PlaytestTelemetrySessionId.Left(12));
	const FString Prefix = FString::Printf(TEXT("BlackoutHuntTelemetry_%s_%s"), *SessionToken, *Timestamp);
	const FString EventsPath = FPaths::Combine(TelemetryDir, Prefix + TEXT("_events.csv"));
	const FString SummaryPath = FPaths::Combine(TelemetryDir, Prefix + TEXT("_summary.csv"));

	const auto MakeCsvRow = [](std::initializer_list<FString> Cells)
	{
		TArray<FString> Escaped;
		Escaped.Reserve(static_cast<int32>(Cells.size()));
		for (const FString& Cell : Cells)
		{
			Escaped.Add(BHTelemetryCsvEscape(Cell));
		}
		return FString::Join(Escaped, TEXT(","));
	};

	const FString ExportedUtc = FDateTime::UtcNow().ToIso8601();
	TArray<FString> EventLines;
	EventLines.Add(MakeCsvRow({
		TEXT("SessionId"), TEXT("ExportedUtc"), TEXT("ServerTimeSeconds"), TEXT("Map"), TEXT("Stage"), TEXT("Phase"),
		TEXT("EventType"), TEXT("Detail"), TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("RoundSeed"), TEXT("RevisionMode"),
		TEXT("PlayerTag"), TEXT("PlayerRole"), TEXT("TargetTag"), TEXT("TargetRole"), TEXT("QuestionId"),
		TEXT("Topic"), TEXT("Difficulty"), TEXT("Count")
	}));

	TMap<FString, int32> CountsByType;
	TMap<FString, int32> CountsByTypeAndDetail;
	for (const FBHPlaytestTelemetryEvent& Event : PlaytestTelemetryEvents)
	{
		const FString EventType = Event.EventType.IsEmpty() ? TEXT("unknown") : Event.EventType;
		const FString Detail = Event.EventDetail;
		const int32 Count = FMath::Max(1, Event.Count);
		CountsByType.FindOrAdd(EventType) += Count;
		CountsByTypeAndDetail.FindOrAdd(EventType + TEXT("|") + Detail) += Count;

		EventLines.Add(MakeCsvRow({
			PlaytestTelemetrySessionId,
			ExportedUtc,
			FString::Printf(TEXT("%.2f"), Event.ServerTimeSeconds),
			Event.RuntimeLevelName,
			FString::FromInt(Event.StageIndex),
			Event.RoundPhase,
			EventType,
			Detail,
			FString::Printf(TEXT("%.1f"), Event.Location.X),
			FString::Printf(TEXT("%.1f"), Event.Location.Y),
			FString::Printf(TEXT("%.1f"), Event.Location.Z),
			FString::FromInt(Event.RoundSeed),
			Event.bRevisionMode ? TEXT("Yes") : TEXT("No"),
			Event.PlayerTag,
			Event.PlayerRole,
			Event.TargetTag,
			Event.TargetRole,
			Event.QuestionId,
			Event.Topic,
			Event.Difficulty,
			FString::FromInt(Count)
		}));
	}

	TArray<FString> SummaryLines;
	SummaryLines.Add(MakeCsvRow({TEXT("Metric"), TEXT("Value")}));
	SummaryLines.Add(MakeCsvRow({TEXT("ExportedUtc"), ExportedUtc}));
	SummaryLines.Add(MakeCsvRow({TEXT("SessionId"), PlaytestTelemetrySessionId}));
	SummaryLines.Add(MakeCsvRow({TEXT("EventRows"), FString::FromInt(PlaytestTelemetryEvents.Num())}));

	TArray<FString> EventTypes;
	CountsByType.GetKeys(EventTypes);
	EventTypes.Sort();
	for (const FString& EventType : EventTypes)
	{
		SummaryLines.Add(MakeCsvRow({FString::Printf(TEXT("EventType.%s"), *EventType), FString::FromInt(CountsByType[EventType])}));
	}

	TArray<FString> DetailKeys;
	CountsByTypeAndDetail.GetKeys(DetailKeys);
	DetailKeys.Sort();
	for (const FString& DetailKey : DetailKeys)
	{
		FString EventType;
		FString Detail;
		DetailKey.Split(TEXT("|"), &EventType, &Detail);
		SummaryLines.Add(MakeCsvRow({FString::Printf(TEXT("EventDetail.%s.%s"), *EventType, *Detail.Left(48)), FString::FromInt(CountsByTypeAndDetail[DetailKey])}));
	}

	const bool bSavedEvents = FFileHelper::SaveStringToFile(FString::Join(EventLines, LINE_TERMINATOR), *EventsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedSummary = FFileHelper::SaveStringToFile(FString::Join(SummaryLines, LINE_TERMINATOR), *SummaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSavedEvents || !bSavedSummary)
	{
		OutMessage = FString::Printf(TEXT("Playtest telemetry export failed. Check write access to %s."), *TelemetryDir);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Playtest telemetry exported to %s"), *TelemetryDir);
	UE_LOG(LogTemp, Display, TEXT("%s (%s, %s)"), *OutMessage, *EventsPath, *SummaryPath);

	if (bClearAfterExport)
	{
		ClearPlaytestTelemetry();
	}
	return true;
}

void UBHGameInstance::ClearPlaytestTelemetry()
{
	PlaytestTelemetryEvents.Reset();
	PlaytestTelemetryPlayerTagsById.Reset();
	PlaytestTelemetrySessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	NextPlaytestTelemetryPlayerOrdinal = 1;
}

int32 UBHGameInstance::GetPlaytestTelemetryEventCount() const
{
	return PlaytestTelemetryEvents.Num();
}

void UBHGameInstance::SetPersistentStageIndex(int32 NewStageIndex)
{
	PersistentStageIndex = FMath::Max(0, NewStageIndex);
}

int32 UBHGameInstance::GetPersistentStageIndex() const
{
	return PersistentStageIndex;
}

FString UBHGameInstance::BuildTrainRecapOverview() const
{
	int32 Attempts = 0;
	int32 Correct = 0;
	int32 Points = 0;
	for (const FBHQuestionAttemptRecord& Attempt : QuestionAttemptHistory)
	{
		++Attempts;
		if (Attempt.bCorrect)
		{
			++Correct;
		}
		Points += FMath::Max(0, Attempt.PointsEarned);
	}

	float ClassAverage = Attempts > 0 ? (100.0f * static_cast<float>(Correct) / static_cast<float>(Attempts)) : 0.0f;
	if (TravelPlayerProgress.Num() > 0)
	{
		float MasterySum = 0.0f;
		int32 MasteryCount = 0;
		for (const FBHTravelPlayerProgress& Progress : TravelPlayerProgress)
		{
			if (Progress.PlayerRole == EBHPlayerRole::Survivor || Progress.PlayerRole == EBHPlayerRole::FakeHunter)
			{
				MasterySum += Progress.RevisionStats.MasteryPercent;
				++MasteryCount;
			}
		}
		if (MasteryCount > 0)
		{
			ClassAverage = MasterySum / MasteryCount;
		}
	}

	return FString::Printf(TEXT("CLASS REVIEW\nAverage: %.0f%% (%s 70%% threshold)\nQuestions answered: %d\nCorrect answers: %d\nQuestion points earned: %d\nStation integrity: %s"),
		ClassAverage,
		ClassAverage >= 70.0f ? TEXT("met") : TEXT("below"),
		Attempts,
		Correct,
		Points,
		ClassAverage >= 70.0f ? TEXT("boarding approved") : TEXT("remediation route selected"));
}

FString UBHGameInstance::BuildTrainRecapTopics() const
{
	struct FTopicRollup
	{
		int32 Attempts = 0;
		int32 Correct = 0;
	};

	// Must match the number of entries in EBHPhysicsTopic (see BHTypes.h). The enum has no
	// sentinel, so this literal is kept in sync manually and the printf below still lists each
	// topic explicitly.
	constexpr int32 PhysicsTopicCount = 4;

	FTopicRollup Topics[PhysicsTopicCount];
	for (const FBHQuestionAttemptRecord& Attempt : QuestionAttemptHistory)
	{
		const int32 TopicIndex = FMath::Clamp(static_cast<int32>(Attempt.Topic), 0, PhysicsTopicCount - 1);
		++Topics[TopicIndex].Attempts;
		if (Attempt.bCorrect)
		{
			++Topics[TopicIndex].Correct;
		}
	}

	int32 WeakIndex = 0;
	int32 StrongIndex = 0;
	float WeakScore = 101.0f;
	float StrongScore = -1.0f;
	for (int32 Index = 0; Index < PhysicsTopicCount; ++Index)
	{
		const float Score = Topics[Index].Attempts > 0 ? 100.0f * Topics[Index].Correct / Topics[Index].Attempts : 50.0f;
		if (Score < WeakScore)
		{
			WeakScore = Score;
			WeakIndex = Index;
		}
		if (Score > StrongScore)
		{
			StrongScore = Score;
			StrongIndex = Index;
		}
	}

	return FString::Printf(TEXT("TOPIC DIAGNOSTICS\nStrong area: %s (%.0f%%)\nWeak area: %s (%.0f%%)\nForces %d/%d  Electricity %d/%d\nWaves %d/%d  Energy %d/%d"),
		*FBHRevisionQuestionBank::TopicToString(static_cast<EBHPhysicsTopic>(StrongIndex)),
		StrongScore,
		*FBHRevisionQuestionBank::TopicToString(static_cast<EBHPhysicsTopic>(WeakIndex)),
		WeakScore,
		Topics[0].Correct, Topics[0].Attempts,
		Topics[1].Correct, Topics[1].Attempts,
		Topics[2].Correct, Topics[2].Attempts,
		Topics[3].Correct, Topics[3].Attempts);
}

FString UBHGameInstance::BuildTrainRecapMissedQuestions() const
{
	TArray<FString> Lines;
	for (int32 Index = QuestionAttemptHistory.Num() - 1; Index >= 0 && Lines.Num() < 3; --Index)
	{
		const FBHQuestionAttemptRecord& Attempt = QuestionAttemptHistory[Index];
		if (!Attempt.bCorrect)
		{
			Lines.Add(FString::Printf(TEXT("%s\nAnswer: %s\nWhy: %s"),
				*Attempt.QuestionText.Left(118),
				*Attempt.CorrectAnswer.Left(80),
				*Attempt.Explanation.Left(120)));
		}
	}

	if (Lines.IsEmpty())
	{
		return TEXT("MISSED QUESTIONS\nNo missed question data yet.\nThe recap boards will fill after students answer revision nodes or train bonus questions.");
	}

	return FString::Printf(TEXT("MISSED QUESTIONS\n%s"), *FString::Join(Lines, TEXT("\n\n")));
}

FString UBHGameInstance::BuildTrainRecapTips(const FString& NextDestination) const
{
	return FString::Printf(TEXT("NEXT ROUTE\nDestination: %s\nClass performance review in progress.\nWeak areas detected. Bonus questions target the weakest topic.\nMonitors may provide unreliable guidance.\nSpend points before the doors close."),
		NextDestination.IsEmpty() ? TEXT("Next Station") : *NextDestination);
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

bool UBHGameInstance::IsOnlineIdentityReadyForSessions(FString& OutMessage)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		OutMessage = TEXT("No OnlineSubsystem is loaded. The project defaults to OnlineSubsystemNull for local tests; configure EOS or Steam for relay/lobby internet play.");
		return false;
	}

	const FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
	if (SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
	{
		IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();
		if (!Identity.IsValid())
		{
			OutMessage = TEXT("Steam is selected, but the Steam identity interface is unavailable. Start the Steam client and verify OnlineSubsystemSteam is packaged.");
			return false;
		}

		if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			return true;
		}

		if (!bSteamAutoLoginAttempted)
		{
			bSteamAutoLoginAttempted = true;
			const bool bAutoLoginStarted = Identity->AutoLogin(0);
			OutMessage = bAutoLoginStarted
				? TEXT("Steam sign-in is starting. Keep Steam running with an account that owns this App ID, then press the online button again.")
				: TEXT("Steam is not ready. Start Steam, sign into an account that owns this App ID, then press the online button again.");
			return false;
		}

		OutMessage = TEXT("Steam is not signed in yet. Start Steam, sign into an account that owns this App ID, then press the online button again.");
		return false;
	}

	if (SubsystemName.Equals(TEXT("EOS"), ESearchCase::IgnoreCase))
	{
		IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();
		if (!Identity.IsValid())
		{
			OutMessage = TEXT("EOS is selected, but the EOS identity interface is unavailable. Verify OnlineSubsystemEOS, SocketSubsystemEOS, and the EOS values file are packaged.");
			return false;
		}

		if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			return true;
		}

		if (!bEOSAutoLoginAttempted)
		{
			bEOSAutoLoginAttempted = true;
			const bool bAutoLoginStarted = Identity->AutoLogin(0);
			OutMessage = bAutoLoginStarted
				? TEXT("EOS sign-in is starting. Complete the Epic account prompt if it appears, then press the online button again.")
				: TEXT("EOS is not ready. Fill Config\\EOS\\EOSValues.local.ini, package with the EOS profile, then press the online button again.");
			return false;
		}

		OutMessage = TEXT("EOS is not signed in yet. Complete the Epic account prompt if it appears, then press the online button again.");
		return false;
	}

	return true;
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
	ShowTravelLoadingScreen(GetWorld(), FString::Printf(TEXT("LOADING %s"), *NormalizeRuntimeLevelName(LevelName).ToUpper()), TEXT("Opening online lobby."));
	UGameplayStatics::OpenLevel(this, FName(BHResolveLevelMapPackage(LevelName)), true, MakeListenOptions(LevelName));
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
		Summary.CurrentPlayers = FMath::Clamp(Summary.MaxPlayers - Result.Session.NumOpenPublicConnections, 0, Summary.MaxPlayers);

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
			if ((Result.IsValid() || Result.IsSessionInfoValid()) && BHSessionMatchesBuild(Result))
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
	if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(PC))
	{
		BHPC->ShowTravelLoadingScreen(TEXT("JOINING ONLINE"), TEXT("Connecting to lobby host."));
	}
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

#pragma once

#include "CoreMinimal.h"
#include "BHAutomationSupport.h"
#include "BHTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "BHGameInstance.generated.h"

class UNetDriver;

USTRUCT()
struct FBHOnlineSessionSummary
{
	GENERATED_BODY()

	FString HostName;
	FString LevelName;
	FString SubsystemName;
	FString SessionId;
	int32 CurrentPlayers = 0;
	int32 MaxPlayers = 0;
	int32 PingMs = 0;
};

class ABHPlayerState;
class APlayerState;

struct FBHTravelPlayerProgress
{
	FString StableId;
	FString PlayerName;
	EBHPlayerRole PlayerRole = EBHPlayerRole::Unassigned;
	EBHPlayerRole DesiredRole = EBHPlayerRole::Unassigned;
	EBHPlayerRole SpectatorRolePreference = EBHPlayerRole::Unassigned;
	EBHPlayerLifeState LifeState = EBHPlayerLifeState::Alive;
	bool bFakeHunterEligible = false;
	FBHPlayerRevisionStats RevisionStats;
	int32 QuestionPoints = 0;
	int32 LifetimeQuestionPoints = 0;
	int32 HunterPoints = 0;
	int32 LifetimeHunterPoints = 0;
	TArray<FBHPowerupInventoryEntry> Powerups;
	// Monotonic wall-clock time (FPlatformTime::Seconds) this player disconnected during an active
	// round, or < 0 if this entry is a normal travel snapshot rather than a pending mid-round
	// reconnect. Wall-clock (not world time) so the grace window stays correct across ServerTravel,
	// where the new world's GetTimeSeconds() resets to ~0 and would otherwise make the elapsed time
	// negative and silently bypass the grace check.
	double LeftServerWorldTime = -1.0;
};

USTRUCT()
struct FBHPlaytestTelemetryEvent
{
	GENERATED_BODY()

	FString EventType;
	FString EventDetail;
	FString RuntimeLevelName;
	FString RoundPhase;
	FString PlayerTag;
	FString PlayerRole;
	FString TargetTag;
	FString TargetRole;
	FString QuestionId;
	FString Topic;
	FString Difficulty;
	FVector Location = FVector::ZeroVector;
	float ServerTimeSeconds = 0.0f;
	int32 StageIndex = 0;
	int32 RoundSeed = 0;
	int32 Count = 1;
	bool bRevisionMode = false;
};

UCLASS()
class BLACKOUTHUNT_API UBHGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(Exec)
	void HostGame();

	UFUNCTION(Exec)
	void HostSubstationGame();

	UFUNCTION(Exec)
	void HostFoggroundsGame();

	UFUNCTION(Exec)
	void JoinGame(const FString& Address);

	UFUNCTION(Exec)
	void HostOnlineGame();

	UFUNCTION(Exec)
	void HostOnlineSubstationGame();

	UFUNCTION(Exec)
	void HostOnlineFoggroundsGame();

	UFUNCTION(Exec)
	void FindOnlineGames();

	UFUNCTION(Exec)
	void JoinOnlineGame(int32 SessionIndex);

	UFUNCTION(Exec)
	void DestroyOnlineSession();

	UFUNCTION(Exec)
	void CreateGameHotspot();

	UFUNCTION(Exec)
	void StopGameHotspot();

	UFUNCTION(Exec)
	void StartInternetTunnel(int32 LocalPort = 7777);

	UFUNCTION(Exec)
	void StopInternetTunnel();

	UFUNCTION(Exec)
	void OpenInternetTunnelSetup(int32 LocalPort = 7777);

	UFUNCTION(Exec)
	void SetClassroomJoinAddress(const FString& Address);

	bool TryHostOnlineGame(const FString& LevelName, FString& OutMessage);
	bool TryFindOnlineGames(FString& OutMessage);
	bool TryJoinOnlineGame(int32 SessionIndex, FString& OutMessage);
	bool TryDestroyOnlineSession(FString& OutMessage);
	// Destroy any active NAME_GameSession when leaving to the menu, for BOTH host and client (unlike
	// TryDestroyOnlineSession, which is host-gated). The GameInstance survives map travel, so without
	// this the stale session blocks re-hosting and blocks a client from ever joining again.
	void LeaveOnlineSessionIfActive();
	bool TryCreateGameHotspot(FString& OutMessage);
	bool TryStopGameHotspot(FString& OutMessage);
	bool TryStartInternetTunnel(FString& OutMessage, int32 LocalPort = 7777);
	bool TryStopInternetTunnel(FString& OutMessage);
	bool TryOpenInternetTunnelSetup(FString& OutMessage, int32 LocalPort = 7777);
	bool IsOnlineSessionBusy() const;
	const TArray<FBHOnlineSessionSummary>& GetOnlineSessionSummaries() const;
	FString GetOnlineSubsystemName() const;
	const FString& GetGameHotspotSsid() const;
	const FString& GetGameHotspotPassphrase() const;
	const FString& GetLastNetworkMessage() const;
	// Returns and clears a connection/travel failure message stashed by the engine failure delegates,
	// so the controller can surface it after a failed join bounces the student back to the menu map.
	FString ConsumePendingNetworkFailureMessage();
	const FString& GetPublicJoinAddress() const;
	FString GetPreferredJoinAddress(int32 LocalPort = 7777) const;
	FString GetConfiguredClassroomJoinAddress(int32 LocalPort = 7777) const;
	FString GetPreferredClassroomJoinAddress(int32 LocalPort = 7777) const;
	void SetPublicJoinAddress(const FString& Address);
	const FBHAutomationConfig& GetAutomationConfig() const;
	bool IsAutomationEnabled() const;
	bool ShouldUseVirtualBoxSafeMode() const;
	bool ConsumeAutomationHost(FString& OutHostMode);
	bool ConsumeAutomationJoin(FString& OutAddress);
	bool ShouldAutoReady() const;
	int32 GetAutomationMinReadyPlayers() const;
	float GetAutomationQuitSeconds() const;
	bool ShouldRequestAutomationCleanExit() const;
	void LogAutomationMarker(const FString& Marker) const;
	bool LogAutomationMarkerOnce(const FString& Marker);
	void RequestCleanExit(const FString& Reason);
	void PersistTravelPlayerState(const ABHPlayerState* PlayerState);
	bool RestoreTravelPlayerState(ABHPlayerState* PlayerState) const;
	// Mid-round reconnect support. MarkTravelPlayerLeftForReconnect persists the player's current
	// state and stamps the leave time; TryGetReconnectProgress returns a copy if a matching entry
	// is still within the grace window; ClearReconnectMark consumes it after a successful rejoin.
	void MarkTravelPlayerLeftForReconnect(const ABHPlayerState* PlayerState, float ServerTimeSeconds);
	bool TryGetReconnectProgress(const ABHPlayerState* PlayerState, float NowServerTimeSeconds, float GraceSeconds, FBHTravelPlayerProgress& OutProgress) const;
	void ClearReconnectMark(const ABHPlayerState* PlayerState);
	void ResetPersistentHunterPoints();
	void ResetPersistentTrainRunProgress();
	void RecordQuestionAttempt(const FBHQuestionAttemptRecord& Attempt);
	const TArray<FBHQuestionAttemptRecord>& GetQuestionAttemptHistory() const;
	void ClearQuestionAttemptHistory();
	FString GetAnonymousTelemetryPlayerTag(const APlayerState* PlayerState);
	void RecordPlaytestTelemetryEvent(const FBHPlaytestTelemetryEvent& Event);
	bool ExportPlaytestTelemetry(FString& OutMessage, bool bClearAfterExport = false);
	void ClearPlaytestTelemetry();
	int32 GetPlaytestTelemetryEventCount() const;
	void SetPersistentStageIndex(int32 NewStageIndex);
	int32 GetPersistentStageIndex() const;
	FString BuildTrainRecapOverview() const;
	FString BuildTrainRecapTopics() const;
	FString BuildTrainRecapMissedQuestions() const;
	FString BuildTrainRecapTips(const FString& NextDestination) const;

private:
	IOnlineSessionPtr GetOnlineSessionInterface(FString& OutMessage) const;
	bool IsOnlineIdentityReadyForSessions(FString& OutMessage);
	bool IsHostNetworkContext(FString& OutMessage, const TCHAR* ActionDescription) const;
	void SetLastNetworkMessage(const FString& Message);
	void OpenListenLevel(const FString& LevelName);
	void RebuildOnlineSessionSummaries();
	void ClearOnlineDelegates(const IOnlineSessionPtr& Sessions);

	void OnCreateOnlineSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartOnlineSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindOnlineSessionsComplete(bool bWasSuccessful);
	void OnJoinOnlineSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroyOnlineSessionComplete(FName SessionName, bool bWasSuccessful);

	// Engine-level connection/travel failure handlers so a student whose join fails (timeout, refused,
	// version mismatch, full server) sees a reason instead of a silent bounce to a black screen/menu.
	void HandleEngineNetworkFailure(UWorld* FailedWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleEngineTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	FDelegateHandle EngineNetworkFailureHandle;
	FDelegateHandle EngineTravelFailureHandle;
	FString PendingNetworkFailureMessage;

	FDelegateHandle CreateOnlineSessionCompleteHandle;
	FDelegateHandle StartOnlineSessionCompleteHandle;
	FDelegateHandle FindOnlineSessionsCompleteHandle;
	FDelegateHandle JoinOnlineSessionCompleteHandle;
	FDelegateHandle DestroyOnlineSessionCompleteHandle;
	TSharedPtr<FOnlineSessionSearch> ActiveOnlineSessionSearch;
	TArray<FOnlineSessionSearchResult> FoundOnlineSessions;
	TArray<FBHOnlineSessionSummary> OnlineSessionSummaries;
	FString PendingOnlineLevelName;
	bool bOnlineSessionBusy = false;
	bool bSteamAutoLoginAttempted = false;
	bool bEOSAutoLoginAttempted = false;

	FString GameHotspotSsid;
	FString GameHotspotPassphrase;
	FString LastNetworkMessage;
	FString PublicJoinAddress;
	FBHAutomationConfig AutomationConfig;
	double AutomationStartTimeSeconds = 0.0;
	bool bAutomationHostConsumed = false;
	bool bAutomationJoinConsumed = false;
	TSet<FString> AutomationMarkersLogged;
	TArray<FBHTravelPlayerProgress> TravelPlayerProgress;
	TArray<FBHQuestionAttemptRecord> QuestionAttemptHistory;
	TArray<FBHPlaytestTelemetryEvent> PlaytestTelemetryEvents;
	TMap<FString, FString> PlaytestTelemetryPlayerTagsById;
	FString PlaytestTelemetrySessionId;
	int32 NextPlaytestTelemetryPlayerOrdinal = 1;
	int32 PersistentStageIndex = 0;
};

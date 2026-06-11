// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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
class APawn;
class APlayerState;

struct FBHTravelPlayerProgress
{
	FString StableId;
	FString PlayerName;
	// Secret per-client token the server issued to this player. A mid-round reconnect is matched on
	// this token (not the spoofable display name), so two students who type the same lobby name on the
	// id-less direct-IP/Playit path can no longer be restored into each other's role/points.
	FString ReconnectToken;
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
	// Prop-hunt match bookkeeping (P6): the cumulative match score and how many rounds this player has STARTED as
	// the seeker (drives the fewest-first rotation). Both zeroed by ResetPropHuntMatch when a new match begins.
	int32 PropHuntScore = 0;
	int32 PropHuntTimesSeeker = 0;
	// True when this snapshot belongs to a bot PlayerState. Bots are persisted alongside humans (they share
	// PlayerArray), but the AutoPrep roster gate must count only HUMANS still expected to re-login after the
	// lobby->hunt travel — a bot never re-logs-in, so counting it would always force the timeout fallback.
	bool bIsBot = false;
	// Combat-log containment: where the leaver's pawn stood when they dropped during a LIVE round (Hunt/
	// FinalEscape). A grace reconnect respawns them at (near) this spot instead of a fresh spawn, so
	// plug-pulling mid-chase is never a free cross-map teleport. bHasPawnTransform stays false for a
	// Prep/Intermission leave (or a pawn already torn down) — those reconnect at a normal spawn.
	bool bHasPawnTransform = false;
	FVector PawnLocation = FVector::ZeroVector;
	FRotator PawnRotation = FRotator::ZeroRotator;
	// PlayerId of the PlayerState that disconnected mid-round. A reconnect mints a brand-new PlayerState
	// (fresh PlayerId), which would silently reset the objective stations' once-per-node answer bookkeeping;
	// the GameMode remaps the stations' stored ids to the new id on a grace reconnect. INDEX_NONE until a
	// mid-round leave stamps it.
	int32 OldPlayerId = INDEX_NONE;
	TArray<FBHPowerupInventoryEntry> Powerups;
	// Monotonic wall-clock time (FPlatformTime::Seconds) this player disconnected during an active
	// round, or < 0 if this entry is a normal travel snapshot rather than a pending mid-round
	// reconnect. Wall-clock (not world time) so the grace window stays correct across ServerTravel,
	// where the new world's GetTimeSeconds() resets to ~0 and would otherwise make the elapsed time
	// negative and silently bypass the grace check.
	double LeftServerWorldTime = -1.0;
	// Monotonic wall-clock time (FPlatformTime::Seconds) this entry was last persisted. Used to bound the
	// TravelPlayerProgress list over a long, churny session: a connected player is re-persisted on every
	// ServerTravel, so the oldest LastPersistedWallTime values are departed players safe to evict.
	double LastPersistedWallTime = -1.0;
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
	// bApplyRoleAndLifeState gates the identity override (Role/DesiredRole/SpectatorRolePreference/
	// LifeState/FakeHunterEligible). Pass false for a fresh late join that lands mid-round (Hunt/
	// FinalEscape) so the spectator/captured state already assigned in PostLogin is preserved while
	// banked progress (points/powerups) is still carried over. Legitimate cross-travel relogins land
	// in a non-active phase (Lobby/Prep/Intermission) and pass true for a full restore.
	bool RestoreTravelPlayerState(ABHPlayerState* PlayerState, bool bApplyRoleAndLifeState = true) const;
	// Mid-round reconnect support. MarkTravelPlayerLeftForReconnect persists the player's current
	// state and stamps the leave time; TryGetReconnectProgress returns a copy if a matching entry
	// is still within the grace window; ClearReconnectMark consumes it after a successful rejoin.
	// LeaverPawn (optional) additionally snapshots the pawn's spot + the old PlayerId for the
	// combat-log containment restore — pass it only for a LIVE-round (Hunt/FinalEscape) leave.
	void MarkTravelPlayerLeftForReconnect(const ABHPlayerState* PlayerState, float ServerTimeSeconds, const APawn* LeaverPawn = nullptr);
	bool TryGetReconnectProgress(const ABHPlayerState* PlayerState, float NowServerTimeSeconds, float GraceSeconds, FBHTravelPlayerProgress& OutProgress) const;
	void ClearReconnectMark(const ABHPlayerState* PlayerState);
	// Drop EVERY pending reconnect mark (banked-progress snapshots are kept). Called by EndRound: the marks
	// are round-scoped state, so a round-N Teacher rejoining during round N+1 must NOT be restored as a
	// second hunter, and a stale mark must not keep deferring round N+1's win checks. Safe relative to the
	// train-hop rescue: the travel-teardown marks are written during Logout AFTER EndRound has latched.
	void InvalidateAllReconnectMarks();
	// Number of players who dropped with an alive Survivor (or Tester) state and still hold a live reconnect
	// mark within GraceSeconds. The GameMode consults this on Logout so a transient classroom Wi-Fi blip that
	// drops the last alive survivor(s) together does not instantly hand the round to the Teacher before the
	// 120 s reconnect grace can bring them back. Uses the same process-wide monotonic clock as the grace check.
	int32 CountReconnectableAliveSurvivors(float GraceSeconds) const;
	// Hunter mirror of the above: players who dropped with an alive Hunter state and still hold a live
	// reconnect mark. The no-hunter win checks (TickRoundTimer / Logout) defer the SurvivorsWin while this
	// is > 0, so a Teacher Wi-Fi blip no longer ends the round instantly.
	int32 CountReconnectableAliveHunters(float GraceSeconds) const;
	// Number of HUMAN (non-bot) snapshots persisted within the last WindowSeconds. PersistPlayersForTravel
	// refreshes every connected player's entry immediately before a ServerTravel, so on the arrival map this
	// is the departure roster — the AutoPrep gate holds the warmup start until that many humans have
	// re-logged-in. The window only needs to cover the HOST's own map load (the gap between the departure
	// persist and the new GameMode's BeginPlay); older entries belong to earlier travels/departed players.
	int32 CountRecentlyPersistedHumanPlayers(double WindowSeconds) const;
	// EXACT departure roster: PersistPlayersForTravel stamps the count of connected humans at the moment of
	// each ServerTravel. Preferred over the windowed count above by the AutoPrep gate — the window can sweep
	// in a player who quit the lobby shortly before departure (their entry was refreshed on lobby arrival),
	// which would force every such first round onto the timeout path.
	void SetExpectedReturningHumanCount(int32 Count) { ExpectedReturningHumanCount = FMath::Max(0, Count); }
	int32 GetExpectedReturningHumanCount() const { return ExpectedReturningHumanCount; }
	// True when a session snapshot exists for this exact reconnect token — i.e. the identity played in this
	// session. Used by the win-window round-result resend to exclude brand-new joiners.
	bool HasTravelSnapshotForToken(const FString& ReconnectToken) const;
	// Per-client reconnect token storage (Bug-9 secure reconnect). The server issues a token at join and
	// pushes it to the owning client via ClientReceiveReconnectToken; the client stores it here and the
	// JoinGame URL echoes it on a later rejoin, so the reconnect is keyed on this unguessable token.
	void SetClientReconnectToken(const FString& Token) { ClientReconnectToken = Token; }
	const FString& GetClientReconnectToken() const { return ClientReconnectToken; }
	// Test-only override for the monotonic reconnect clock. When >= 0 it is used in place of
	// FPlatformTime::Seconds() so the grace-window expiry path can be exercised deterministically.
	// Production never sets this, so behaviour is unchanged (real monotonic time that survives ServerTravel).
	void SetReconnectClockOverrideForTest(double NowSeconds);
	void ResetPersistentHunterPoints();
	void ResetPersistentTrainRunProgress();
	// Prop-hunt match wrapper (P6): rounds completed this match, surviving the per-round ServerTravel the same way
	// the train run does. ResetPropHuntMatch starts a fresh match: zeroes the counter AND every player's persisted
	// prop-hunt score/seeker history (called when the session enters the train lobby).
	int32 GetPropHuntRoundsPlayed() const { return PropHuntRoundsPlayed; }
	int32 IncrementPropHuntRoundsPlayed() { return ++PropHuntRoundsPlayed; }
	void ResetPropHuntMatch();
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

	// Cosmetic cold-boot terminal (SBHBootConsole) is shown at most once per process. The GameInstance
	// survives non-seamless level travel, so this flag keeps the intro from replaying every time a player
	// returns to the menu after a match.
	bool HasPlayedBootSequence() const { return bHasPlayedBootSequence; }
	void MarkBootSequencePlayed() { bHasPlayedBootSequence = true; }

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
	// Set when the user leaves to the menu while a host create/start is still in flight. The async
	// create/start completion handlers honor it by tearing the session down instead of travelling the
	// host into a lobby they already chose to leave (and leaving the session created behind them).
	bool bPendingOnlineHostCancel = false;
	// This client's current reconnect token (server-issued, stored here so it survives a disconnect and
	// is echoed in the next JoinGame URL). Empty until the server pushes one after a join.
	FString ClientReconnectToken;
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
	// Prop-hunt rounds completed this match (P6); reset by ResetPropHuntMatch on entering the lobby.
	int32 PropHuntRoundsPlayed = 0;
	// Humans connected at the most recent PersistPlayersForTravel — the AutoPrep gate's exact expected roster.
	int32 ExpectedReturningHumanCount = 0;
	// >= 0 only in automated tests; otherwise the reconnect grace clock uses FPlatformTime::Seconds().
	double ReconnectClockOverrideSeconds = -1.0;
	TArray<FBHQuestionAttemptRecord> QuestionAttemptHistory;
	TArray<FBHPlaytestTelemetryEvent> PlaytestTelemetryEvents;
	TMap<FString, FString> PlaytestTelemetryPlayerTagsById;
	FString PlaytestTelemetrySessionId;
	int32 NextPlaytestTelemetryPlayerOrdinal = 1;
	int32 PersistentStageIndex = 0;
	// See HasPlayedBootSequence / MarkBootSequencePlayed.
	bool bHasPlayedBootSequence = false;
};

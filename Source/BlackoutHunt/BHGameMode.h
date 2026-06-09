// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "BHBlockActor.h"
#include "BHTypes.h"
#include "BHGameMode.generated.h"

class ABHCharacter;
class ABHCCTVZone;
class ABHBotController;
class ABHBreaker;
class ABHDoor;
class ABHExitGate;
class ABHEscapeStationManager;
class ABHFlickerLight;
class ABHGameState;
class ABHObjectiveStation;
class ABHPlayerController;
class ABHPlayerState;
class ABHRuntimeMeshPropActor;
class ABHSecurityCamera;
class ABHStaticBlockField;
class ABHTrainIntermissionManager;
class ANavMeshBoundsVolume;
class UBHAtmosphereDirector;
struct FBHLessonPreset;

// Shared travel-map resolver usable outside ABHGameMode (e.g. BHPlayerController/BHGameInstance host and
// launch paths): returns the authored /Game/BlackoutHunt/Maps/<Level> package when bUseAuthoredLevels is
// enabled and that asset exists, otherwise the stock runtime base map /Engine/Maps/Entry. Never reroutes
// the special TrainIntermission level. ABHGameMode::ResolveTravelMapForLevel delegates to this.
BLACKOUTHUNT_API FString BHResolveLevelMapPackage(const FString& LevelName);

// True when the bh.PropHunt console variable is set. Defined in BHGameModePropHunt.cpp so the cvar object stays
// file-local; BuildRuntimeFacility folds this into the parsed bPropHuntMode (opt-in, reversible Prop Hunt mode).
BLACKOUTHUNT_API bool BHIsPropHuntCVarEnabled();

// Resolves a prop-hunt arena logical name (BHPropHunt::FArenaSpec registry) to the .umap package to travel to: the
// authored arena bake (Tools/Setup-PropHuntArena.py output) when it exists, otherwise the raw imported pack demo map,
// otherwise empty (not a registered arena, or nothing on disk/in the cook). Defined in BHGameModePropHunt.cpp.
BLACKOUTHUNT_API FString BHResolvePropHuntArenaPackage(const FString& Token);

// Prop-hunt seeker-kit tunables (the bh.PropHuntScan*/MissSlow* cvars live file-local in BHGameModePropHunt.cpp;
// these accessors let the pawn code in BHCharacter.cpp read them without the cvar objects leaking out).
BLACKOUTHUNT_API float BHPropHuntScanRadius();
BLACKOUTHUNT_API float BHPropHuntScanCooldownSeconds();
// The wrong-hit self-slow duration for the Nth consecutive miss (folds the bh.PropHuntMissSlow* cvars into
// BHPropHunt::SeekerMissSlowSeconds). 0 when misses <= 0.
BLACKOUTHUNT_API float BHPropHuntMissSlowSecondsFor(int32 ConsecutiveMisses);
// The movement-speed multiplier applied while the wrong-hit slow is active.
BLACKOUTHUNT_API float BHPropHuntMissSlowFactor();

// Indoor (non-Foggrounds) auto-exposure tuning, shared by two paths that must stay in lockstep:
//  * ABHGameMode::AddMoodPass bakes these into the authored .umap's post-process volume at export time, and
//  * ABHPlayerController::ClampIndoorAutoExposure re-applies them on every client at runtime, repairing the
//    already-baked volume in maps that were exported before this fix (re-exporting the art-passed .umaps to
//    rebake the value is not an option, as it would discard the mesh/lighting pass layered on the seed).
// MaxBrightness is the ceiling the eye may adapt TO: the old 0.95 let a flashlit near wall fill the frame,
// drag adaptation toward the bright end, and crush the whole scene to near-black (creeping back only at the
// engine's slow default SpeedDown). A tight ceiling caps that darkening; the floor (0.030, kept in AddMoodPass)
// stays well below normal-play luminance so ordinary dark-room play is visually unchanged, and the brisk
// symmetric speed makes any residual swing snap back instead of crawling.
inline constexpr float BHIndoorAutoExposureMaxBrightness = 0.20f;
inline constexpr float BHAutoExposureAdaptSpeed = 4.0f;

UCLASS()
class BLACKOUTHUNT_API ABHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABHGameMode();

	virtual void BeginPlay() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	// Editor-only export entry used by UBHExportLevelCommandlet: builds one logical level's geometry and
	// gameplay actors into the current world with no round/timer/atmosphere state, so the result can be
	// saved as an authored /Game/BlackoutHunt/Maps/<Level>.umap seed. Not used at runtime.
	void BuildLevelForExport(const FString& InLevelName);

	// Authored-export bake. Set only for the duration of BuildLevelForExport. When true, AddStaticBlock and
	// SpawnRuntimeMeshProp emit real, hard-referenced AStaticMeshActors (Static mobility) instead of the
	// runtime ABHStaticBlockField specs / replicated prop actors, so the saved .umap holds visible, editable,
	// auto-cooking geometry artists can polish in place (the runtime block field is invisible until BeginPlay).
	// Never set at runtime, so the procedural generator is byte-for-byte unchanged.
	bool bAuthoringExport = false;
	class AStaticMeshActor* SpawnAuthoredBlockActor(const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation, bool bCollides, EBHBlockMaterial Material, bool bStartHidden);
	class AStaticMeshActor* SpawnAuthoredMeshActor(const FVector& Location, const FRotator& Rotation, const FString& MeshAssetPath, const FString& MaterialAssetPath, const FVector& MeshScale, bool bCollides);
	void SpawnAuthoredPlayerStarts();
	// v2 industrial look (Facility/Substation): lay ContainersHouseCH modular metal wall panels over a
	// wall-shaped blockout block, on both faces, as NON-collision visual cladding. Additive and fail-safe:
	// the cube wall keeps collision/nav, so a missing kit or a wrong orientation can only fail to ADD metal,
	// never break the map. Tiled from the catalog dimensions (SM_Wall_Metal_6m = 600x8.5x230, end pivot).
	void CladAuthoredWall(const FVector& Center, const FVector& Scale, const FRotator& Rotation);
#endif

	void SetPlayerReady(ABHPlayerController* Controller, bool bReady);
	void NotifyBreakerRepaired(const FVector& BreakerLocation = FVector::ZeroVector);
	void NotifyObjectiveStationCompleted(ABHObjectiveStation* Station);
	void NotifySurvivorCaptured(ABHCharacter* Survivor, ABHCharacter* CapturingHunter = nullptr);
	void NotifySurvivorEscaped(ABHCharacter* Survivor);
	// Prop hunt (public: the pawn drives both). Manual taunt = risk/reward bonus on a personal cooldown; the clang
	// is the audible half of the wrong-hit penalty. Defined in BHGameModePropHunt.cpp; no-ops outside the mode.
	void HandlePropHuntManualTaunt(ABHCharacter* Prop);
	void PlayPropHuntWrongHitClang(const FVector& Location);
	void ToggleLightCircuit(int32 CircuitId);
	void OpenSecurityCircuit(int32 CircuitId);
	bool ToggleSecurityCircuit(int32 CircuitId);
	void NotifyLoudNoise(const FVector& Location, const FString& Reason);
	void BroadcastGameplayAudioCue(const FBHGameplayAudioCue& Cue);
	void NotifyCCTVDetection(ABHSecurityCamera* Camera, AActor* ZoneActor, ABHCharacter* Survivor, const FString& AlertLabel);
	int32 NotifyHallMonitorMisdirection(const FVector& Location, const FString& SenderName);
	void ReportAtmosphereStimulus(EBHAtmosphereStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, float Strength, const FString& Reason);
	bool TriggerAtmosphereCue(const FBHScareEventSpec& Spec);
	bool TriggerBlackoutPulse(const FVector& Location, float Radius, float DurationSeconds);
	bool TriggerManualScare(ABHCharacter* Target, EBHScareEventType ScareType);
	bool TriggerStudentScareSwitch(ABHCharacter* Activator, const FVector& SourceLocation, const FString& ScareTitle, int32 Severity);
	// bPreferRealMesh restricts the pick to imported creature meshes (never the procedural "simple render"
	// proxies) when at least one is eligible — used by the showcased close-up scares (in-your-face, behind-you,
	// corner peek, SCP-096) so they always present a genuine monster.
	FBHJumpscareVariant ChooseJumpscareVariant(EBHScareEventType EventType, bool bPreferRealMesh = false) const;
	float GetScareSensoryScale() const;
	void TestJumpscareVariant(ABHPlayerController* RequestingController, const FString& VariantToken);
	FString GetJumpscareVariantTestReport() const;
	void ForceStartRound(ABHPlayerController* RequestingController);
	// Recovery: teleport a player to the centre of the carriage they're currently in if they get stuck on the
	// train (wedged in interior geometry, or out on the roof in the lobby where nothing auto-boards them). Only
	// valid on the train lobby / intermission; returns false elsewhere so it can't be used to escape a live hunt.
	bool ResetPlayerToTrainInterior(AController* Controller);
	// Index into a TrainCabinCenters-style array of the cabin centre nearest to X (cars run along the train's
	// length, so only X matters). Returns INDEX_NONE for an empty array. Pure helper, unit-tested.
	static int32 NearestCabinCenterIndex(const TArray<FVector>& Centers, float X);
	void SetDesiredRole(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole);
	void QueueSpectatorRolePreference(ABHPlayerController* RequestingController, EBHPlayerRole DesiredRole);
	void RequestSpectatorEncouragement(ABHPlayerController* RequestingController);
	void KickPlayer(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState);
	void SetNextLevel(ABHPlayerController* RequestingController, const FString& LevelName);
	void SetMapVote(ABHPlayerController* RequestingController, const FString& LevelName);
	void SetFogPresetOverride(ABHPlayerController* RequestingController, EBHFogPreset FogPreset);
	void ClearFogPresetOverride(ABHPlayerController* RequestingController);
	void SetFogPresetVote(ABHPlayerController* RequestingController, EBHFogPreset FogPreset);
	void SetPlayerAvatar(ABHPlayerController* RequestingController, int32 AvatarIndex);
	void SetTargetHunterCount(ABHPlayerController* RequestingController, int32 NewHunterCount);
	void SetObjectiveIntensity(ABHPlayerController* RequestingController, int32 NewObjectiveIntensity);
	void ToggleInfectionMode(ABHPlayerController* RequestingController);
	void TogglePaceMode(ABHPlayerController* RequestingController);
	void SetPracticeRole(ABHPlayerController* RequestingController, EBHPlayerRole NewRole);
	void SetPracticeModifier(ABHPlayerController* RequestingController, EBHRoundModifier NewModifier);
	void RefreshPracticeRound(ABHPlayerController* RequestingController);
	void TriggerPracticeJumpscare(ABHPlayerController* RequestingController);
	void TriggerTargetedJumpscare(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState);
	void TriggerHunterBlackout(const FVector& SourceLocation, int32 SurgeCharges = 0);
	// Returns true if a travel was initiated (or one is already pending); false if it could not act
	// (no authority/world) so the caller can retry instead of leaving the session stuck at the train.
	bool CompleteTrainIntermission(const FString& NextMapName, bool bFinalRecap);

	// Final stage: instead of travelling onward, the train arrives at its terminal and the game ends in place.
	// Posts the top-5 leaderboard + class summary to the carriage boards, exports the class CSV + a per-student
	// status file (and logs both), and holds here so the host can review results before returning to the lobby.
	void EnterFinalResultsHold();
	bool bFinalResultsHold = false;
	// Top-5 students by combined score (question mastery + hunter/capture points + a survival bonus), plus a
	// short class summary line, formatted for the carriage leaderboard board.
	FString BuildFinalLeaderboardText() const;
	// Writes the class telemetry CSV (via the GameInstance) and one per-student status file to Saved/ClassResults.
	// Fills OutSummary with a human-readable result and logs every file written.
	void ExportFinalClassResults(FString& OutSummary);

	void NotifyFinalEscapeExpired();
	// Issue a discretionary ServerTravel (tester shortcuts, bot soak) only if no round-end/other
	// travel is already committed. Returns false (and does nothing) if a travel is already in flight,
	// preventing a second travel from overwriting the pending post-round destination.
	bool RequestServerTravel(const FString& URL, bool bAbsolute = false);
	// Which solo tutorial is running (the map-baked ABHTutorialDirector reads this on activation).
	EBHTutorialPhase GetTutorialPhase() const { return RuntimeTutorialPhase; }
	// True when the tutorials chain Survivor -> Teacher -> Monitor on reaching each exit (the full course);
	// false when the player selected a single tutorial, which returns to the menu when its exit is reached.
	bool IsTutorialChained() const { return bTutorialChain; }
	// The map's hunter/teacher spawn point. Exposed for the baked ABHTutorialDirector, which spawns the
	// scripted Teacher there in the Teacher/Monitor lessons. A real runtime accessor (unlike the
	// test-only DebugGetHunterSpawnForTest), so it stays available in a Shipping build.
	FVector GetHunterSpawnLocation() const { return HunterSpawn; }
	// Called by ABHTutorialDirector when the student reaches the exit: ServerTravel-reload the Tutorial map
	// into the next phase (Survivor -> Teacher -> Monitor), or return to the main menu after Monitor.
	void AdvanceTutorialPhase();
	// Tutorial atmosphere gate. The director flips this ON only for the scripted Survivor-phase Teacher chase
	// (Encounter/Escape) so ambient dread (whispers, dread audio, light flicker/cuts) plays for that climax but is
	// fully suppressed during the calm teaching steps. IsTutorialHorrorSuppressed() is the single predicate the
	// presence/atmosphere chokepoints read; it is false (never suppresses) outside tutorial mode, so live matches
	// are unaffected. The random monster-charge jumpscare is gated separately on bTutorialMode alone, so it never
	// fires in ANY tutorial state - even the chase.
	void SetTutorialHorrorAllowed(bool bAllowed) { bTutorialHorrorAllowed = bAllowed; }
	bool IsTutorialHorrorSuppressed() const { return bTutorialMode && !bTutorialHorrorAllowed; }
	void SetBotCount(ABHPlayerController* RequestingController, int32 NewBotCount);
	void SetBotDifficulty(ABHPlayerController* RequestingController, EBHBotDifficulty NewDifficulty);
	// Top up the bot roster so humans + bots fill every slot (MaxPlayers). Bots still yield
	// slots as more humans join, so a full lobby naturally makes room for late arrivals.
	void FillBotsToCapacity(ABHPlayerController* RequestingController);
	bool IsRevisionMode() const;
	// True for the opt-in, reversible Prop Hunt mode (Seeker = Hunter, Props = Survivors). See BHGameModePropHunt.cpp.
	bool IsPropHuntMode() const;
	// The per-round master seed (see PrepareRoundDirector). Exposed so other actors can derive
	// reproducible per-round variety from it (e.g. revision question selection).
	int32 GetRoundSeed() const { return RoundSeed; }
	EBHRevisionDifficultyMix GetRevisionDifficultyMix() const;
	// Host lobby customization accessors (read by the question bank / objective stations during selection).
	EBHQuestionDifficulty GetRevisionStartingDifficulty() const { return RevisionStartingDifficulty; }
	EBHQuestionDifficulty GetRevisionMinDifficulty() const { return RevisionMinDifficulty; }
	EBHQuestionDifficulty GetRevisionMaxDifficulty() const { return RevisionMaxDifficulty; }
	// Clamp a difficulty tier into the host-configured [Min, Max] band.
	EBHQuestionDifficulty ClampRevisionDifficulty(EBHQuestionDifficulty Difficulty) const
	{
		return static_cast<EBHQuestionDifficulty>(FMath::Clamp<uint8>(
			static_cast<uint8>(Difficulty),
			static_cast<uint8>(RevisionMinDifficulty),
			static_cast<uint8>(RevisionMaxDifficulty)));
	}
	// Non-empty when the host pinned an exact question set; selection draws only from these ids.
	const TArray<FString>& GetRevisionAllowedQuestionIds() const { return RevisionAllowedQuestionIds; }
	const FString& GetRevisionQuestionSetId() const { return RevisionQuestionSetId; }
	// Procedural layout knobs (0 = generator default) parsed from the host preset.
	int32 GetLayoutSeed() const { return LayoutSeed; }
	int32 GetLayoutBreakerCount() const { return LayoutBreakerCount; }
	int32 GetLayoutDensity() const { return LayoutDensity; }
	bool ShouldForceProceduralLayout() const { return bForceProceduralLayout; }
	TArray<EBHPhysicsTopic> GetRevisionWeakTopics() const;
	int32 GetRevisionQuestionTargetPerNode() const;
	int32 GetRevisionAnswerTeamTargetSize() const;
	// True when the per-node "answer this node only once per round" rule should be enforced. That rule
	// spreads a populated class across nodes for coverage, but it is only SATISFIABLE when there are at
	// least as many live human revision students as a node needs distinct correct answers
	// (GetRevisionQuestionTargetPerNode). With fewer humans -- a lone player, or a bot-populated lobby
	// (bots are excluded from every revision score, so their answers can never make up the shortfall) --
	// the cap makes each node unwinnable and the exit mathematically unreachable. In that case this
	// returns false so the present human(s) can re-answer a node (each press loads a fresh question) and
	// complete it themselves. Always true outside revision mode. Reads live state on purpose: if a class
	// shrinks below the target mid-round it only ever relaxes (never makes a populated class harder).
	bool ShouldEnforceRevisionNodeAnswerCap() const;
	// Returns the FROZEN per-round hall-monitor contribution gate target (the snapshot in
	// RevisionContributionGateTarget). Stable for the whole round; recomputed only by
	// RefreshRevisionContributionGateTarget at round start and on intentional admin changes.
	int32 GetRevisionMinimumContributionTarget() const;
	// Live formula for the contribution gate target (derived from the current active-student count and
	// stage). Only RefreshRevisionContributionGateTarget should call this to take a fresh snapshot.
	int32 ComputeLiveRevisionContributionTarget() const;
	// Recompute the frozen contribution gate target from current live state and push it to the replicated
	// ABHGameState::RevisionContributionTarget. Call at round start and whenever an admin intentionally
	// changes inputs that affect the target.
	void RefreshRevisionContributionGateTarget();
	bool CanUseHallMonitorTools(const ABHPlayerState* PlayerState, FString& OutBlockReason) const;
	// Revision-mode round-end enforcement (see docs/ROADMAP_MOVEMENT_REVISION.md, WS2): count hall monitors
	// (caught survivors) still below the per-round contribution target; manage the "all survivors caught" grace
	// window (returns true to DEFER the round-end while monitors finish, ticked once per second); and apply the
	// proportional question-point dock to monitors who did not finish by round-end.
	int32 CountMonitorsBelowRevisionTarget() const;
	bool TickAllCaughtRevisionGrace();
	void ApplyUnfinishedMonitorRevisionDock();

	// --- Catch-pressure loop (docs/CATCH_PRESSURE.md). CVar-reading wrappers around the pure BHCompute* helpers
	// in BHTypes.h, plus the climb-back contribution target derived from the monitor gate. ----------------------
	float ComputeRecaptureTaxFraction(int32 InTimesCaught) const;
	float ComputeDetentionHoldSeconds(int32 InTimesCaught) const;
	int32 GetClimbBackContributionTarget() const;
	static bool IsRevisionParticipantRole(EBHPlayerRole Role);
	static bool IsValidSpectatorRolePreference(EBHPlayerRole Role);
	static EBHPlayerRole SanitizeSpectatorRolePreference(EBHPlayerRole Role);
	static int32 ResolveRevisionQuestionTargetFor(int32 StudentCount, int32 StageIndex);
	static int32 ResolveRevisionNodeTargetFor(int32 StudentCount, int32 StageIndex, int32 StationCount);
	// Pure rule behind ShouldEnforceRevisionNodeAnswerCap: the once-per-node cap is enforced only when
	// there are at least PerNodeAnswerTarget live human students (enough to fill a node via distinct
	// answers). Fewer humans => relax. Static so it can be unit-tested without a live world.
	static bool RevisionNodeAnswerCapApplies(int32 HumanStudentCount, int32 PerNodeAnswerTarget);
	static FVector ResolveFoggroundsDoorFrameOrigin(const FVector& DoorLocation);
	// Records a revision answer (mastery, spaced-repetition queue, telemetry) and returns the
	// shop points awarded for the answer (0 when wrong or when not in revision mode).
	// bCountsAsContribution: team-station answers count toward the hall-monitor tool gate;
	//   bonus-terminal answers pass false so they build mastery but do not satisfy that gate.
	// bBonusPoints: award the 1.25x "bonus question" point value (train bonus terminals).
	int32 RecordRevisionAnswer(ABHCharacter* Character, const FBHRevisionQuestion& Question, bool bCorrect, bool bCorrection, const FString& SelectedAnswer = FString(), const FString& TeamSummary = FString(), bool bCountsAsContribution = true, bool bBonusPoints = false);
	void GetAdaptiveRevisionPlan(const ABHPlayerState* PlayerState, bool bLastAnswerCorrect, EBHPhysicsTopic& OutTopic, EBHQuestionDifficulty& OutDifficulty, FString& OutReason) const;
	bool BuildRevisionAnswerTeam(ABHObjectiveStation* Station, ABHCharacter* RequestingCharacter, TSet<int32>& OutPlayerIds, FString& OutSummary) const;
	void TriggerTeacherCounterJumpscare(ABHObjectiveStation* Station, EBHRevisionCounterNodeType CounterType);
	void ActivateStudentScareRelay(ABHCharacter* Activator, ABHObjectiveStation* SourceNode);
	void SetPhysicsTopics(ABHPlayerController* RequestingController, const FString& TopicList);
	void SetRevisionDifficultyMix(ABHPlayerController* RequestingController, const FString& DifficultyMix);
	void SetRevisionThresholds(ABHPlayerController* RequestingController, float NewClassThreshold, float NewIndividualThreshold);
	void SetRevisionRoundDuration(ABHPlayerController* RequestingController, int32 NewRoundSeconds);
	void SetScareIntensity(ABHPlayerController* RequestingController, int32 NewScareIntensity);
	bool ApplyLessonPreset(ABHPlayerController* RequestingController, const FBHLessonPreset& Preset, FString& OutMessage);
	void ForceReview(ABHPlayerController* RequestingController);
	void TesterGrantTrainResources(ABHPlayerController* RequestingController);
	void TesterOpenTrainIntermission(ABHPlayerController* RequestingController);
	void TesterAdvanceTrainPhase(ABHPlayerController* RequestingController);
	void TesterLoadFinalStation(ABHPlayerController* RequestingController);
	void TesterTriggerFinalEscape(ABHPlayerController* RequestingController);
	void TesterForceFinalRecap(ABHPlayerController* RequestingController);
	FString GetRevisionStatusReport() const;
	bool ExportRevisionReport(ABHPlayerController* RequestingController, FString& OutMessage);
	void RecordPlaytestTelemetryMarker(const FString& EventType, const FVector& Location, const FString& Detail = FString(), const ABHPlayerState* PlayerState = nullptr, const ABHPlayerState* TargetState = nullptr, const FString& QuestionId = FString(), const FString& Topic = FString(), const FString& Difficulty = FString(), int32 Count = 1);
	bool ExportPlaytestTelemetry(ABHPlayerController* RequestingController, FString& OutMessage);
	bool IsBotMode() const;
	EBHBotDifficulty GetBotDifficulty() const;
	bool IsRuntimeNavigationReady() const;
	bool GetLatestBotNoiseLocation(float MaxAgeSeconds, FVector& OutLocation) const;
	FVector GetRandomBotPatrolPoint() const;
	void ReportBotStimulus(EBHBotStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, const FString& Reason, float Strength = 1.0f);
	bool GetBotWorldMemorySnapshot(FBHBotMemory& OutMemory, float MaxAgeSeconds = 18.0f) const;
	bool ClaimBotObjective(AController* Claimant, AActor* Target, EBHBotIntent Intent, float ClaimSeconds = 10.0f);
	void ReleaseBotObjective(AController* Claimant, AActor* Target = nullptr);
	int32 CountBotClaimsForTarget(const AActor* Target) const;
	bool IsBotTargetOnCooldown(const AController* Claimant, const AActor* Target) const;
	void AddBotTargetCooldown(AController* Claimant, AActor* Target, float CooldownSeconds, const FString& Reason);
	bool GetBotApproachPoint(AActor* Target, const FVector& FromLocation, float DesiredDistance, FVector& OutLocation);
	int32 RunBotNavCheck(FString& OutSummary);
	FString GetBotStatusReport() const;
	FString GetBotMemoryReport() const;
	FString GetAtmosphereDebugStatus() const;
	void ForceBotHunt(ABHPlayerController* RequestingController);
	void StartBotSoak(ABHPlayerController* RequestingController, const FString& LevelName, int32 DurationSeconds, int32 NewBotCount);
	bool IsHostAdminController(const ABHPlayerController* RequestingController) const;
	bool RequireHostAdmin(ABHPlayerController* RequestingController, const TCHAR* ActionDescription) const;

protected:
	friend class UBHAtmosphereDirector;

	void BuildRuntimeFacility();
	// Authored-level path: when a placed ABHLevelMarker is found, collect the gameplay actors already
	// in the loaded .umap instead of running the runtime block generator. Returns true if the level was
	// authored (and generation should be skipped); false leaves the procedural generator to run as before.
	bool DiscoverAuthoredLevel();
	void BuildTrainIntermissionLevel();
	// Records the centre of every carriage (along the train's length) when a lobby / intermission train is built.
	// Cars sit at a fixed 1500cm pitch centred in the tube, so this is derived from the tube's X bounds and covers
	// every car in either build. Consumed by ResetPlayerToTrainInterior (the O-key cabin-reset unstick).
	void RecordTrainCabinCenters(float TubeMinX, float TubeMaxX, float FloorZ);
	// The pre-game LOBBY built as a parked subway train: a longer carriage with a large social/seating lounge so
	// joining players can see each other and mingle while they wait. Self-contained (no intermission manager, no
	// auto-advance); the round starts via TravelFromLobbyToFirstHunt() on ready / host force-start.
	void BuildTrainLobbyLevel();
	// Server-travels out of the train lobby to the chosen first hunt level (stage 0), auto-starting its warmup.
	void TravelFromLobbyToFirstHunt();
	// Shared rooftop "fun" extras (stargazing starfield + festoon lights, calm decorations, and a short parkour
	// course whose finish gate grants the roof_parkour achievement). Called from both train roof easter-egg
	// blocks so the intermission and lobby roofs match; the tube extent + hatch arrival fit the layout to each.
	void BuildTrainRoofExtras(float TubeMinX, float TubeMaxX, const FVector& HatchLocation);
	// Procedural geometry/actor builders, one per logical level. Each is self-contained (sets spawns and
	// spawns all gameplay actors); BuildRuntimeFacility() parses travel options then dispatches to one.
	void BuildFacilityLevel();
	// Spawn ~5 hidden ABHCollectable relics at the given (already-valid) world locations with stable per-site ids
	// ("<prefix>_0".."<prefix>_N"). Called from each Build*Level. IDs must stay stable so found-state persists.
	void SpawnMapCollectables(const FString& MapPrefix, const TArray<FVector>& Locations);
	// Whole-map "dark liminal backrooms" layout for the Facility: a connected maze of tight rooms (randomized
	// DFS spanning tree + extra loops, so every room is reachable), sparse dim lighting, mono concrete, with
	// objectives/breakers/spawns/one exit distributed across the grid. Replaces the legacy hand-authored
	// industrial layout when bBuildBackroomsFacility is set.
	void BuildBackroomsFacility();
	bool bBuildBackroomsFacility = true;
	void BuildSubstationLevel();
	void BuildFoggroundsLevel();
	// Compact, multi-room greybox for the self-serve solo tutorial. Authored-only (no procedural runtime
	// fallback path needs it): baked to /Game/BlackoutHunt/Maps/Tutorial.umap via the export commandlet,
	// and it bakes in an ABHTutorialDirector that drives the guided lesson at runtime.
	void BuildTutorialLevel();
	// Hidden easter-egg HUB (reached by Escape at the main menu, EBHTutorialPhase::EasterEgg): a clear central
	// gallery ringed by themed sector rooms (trophy/stats, arcade, dev/credits, an eerie nook) and skill-training
	// rooms (movement, teacher-evasion), plus a secret room mapping the hidden eggs. Built procedurally here.
	void BuildEasterEggRoom();
	void BuildFoggroundsFinalStation();
	void BuildMapSubwayExitStation(const FVector& GateLocation, float DirectionSign, const FString& StationName, const FString& DestinationText);
	void BuildRuntimeNavigation();
	// Parametrized core of BuildRuntimeNavigation: places/sizes the runtime NavMeshBoundsVolume explicitly, for maps
	// whose bounds are not one of the hardcoded level-name presets (prop-hunt arenas size this from their geometry).
	void BuildRuntimeNavigationBounds(const FVector& Center, const FVector& Size);
	void CompleteRuntimeNavigationBuild();
	void SetSecurityCircuitCCTVActive(int32 CircuitId, bool bCircuitOpen);
	void ApplyRoundCCTVVisibility(EBHRoundModifier ActiveModifier);
	int32 SelectHiddenCCTVIndex(int32 CandidateCount, int32 Salt) const;
	ABHCCTVZone* SpawnCCTVZoneForCamera(ABHSecurityCamera* Camera, int32 CircuitId, const FString& AlertLabel, bool bVisible);
	ABHRuntimeMeshPropActor* SpawnRuntimeMeshProp(
		const FVector& Location,
		const FRotator& Rotation,
		const FString& MeshAssetPath,
		const FString& MaterialAssetPath,
		const FVector& MeshScale,
		bool bCollides,
		const FVector& FallbackScale,
		const FLinearColor& FallbackTint,
		EBHBlockMaterial FallbackMaterial = EBHBlockMaterial::PaintedMetal,
		bool bGroundToMeshBounds = false);
	void AddFacilityVerticalSlicePass();
	void AddFacilityDetailPass();
	void AddClassroomHorrorPass();
	void AddHorrorVariationPass(FRandomStream& Stream);
	void AddSurfaceDetailGrid(float HalfX, float HalfY, const FLinearColor& LineTint);
	void AddIndustrialClutter(const TArray<FVector>& Centers, const FLinearColor& Tint);
	void AddMoodPass(const FLinearColor& FogColor, float FogDensity, float VignetteIntensity, float FilmGrainIntensity);
	void AddFoggroundsLightingPass();
	void AddFoggroundsMoodPass();
	void AddFoggroundsModeledFog();
	void AddStaticBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint = FLinearColor(0.38f, 0.42f, 0.45f, 1.0f), const FRotator& Rotation = FRotator::ZeroRotator, bool bCollides = true, EBHBlockMaterial Material = EBHBlockMaterial::Tinted, bool bStartHidden = false);
	ABHBlockActor* SpawnDynamicBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint = FLinearColor(0.38f, 0.42f, 0.45f, 1.0f), const FRotator& Rotation = FRotator::ZeroRotator, bool bCollides = true, EBHBlockMaterial Material = EBHBlockMaterial::Tinted);
	ABHBlockActor* SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint = FLinearColor(0.38f, 0.42f, 0.45f, 1.0f), const FRotator& Rotation = FRotator::ZeroRotator, bool bCollides = true, EBHBlockMaterial Material = EBHBlockMaterial::Tinted);
	class ABHBreakableGlassPane* SpawnBreakableGlassPane(const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FText& Label = FText());
	class ABHFootstepSurfaceVolume* SpawnSurfacePatch(const FVector& Location, const FVector& Extent, EBHFootstepSurface Surface);
	void SpawnHiddenBlocker(const FVector& Location, const FVector& Scale);
	void RegisterStationSignalBlock(ABHBlockActor* Block);
	void RegisterStationSignalLight(ABHFlickerLight* Light);
	void UpdateStationSignalState(bool bExitOpen);
	void AddMapContainment(float HalfX, float HalfY);
	// Containment for maps not centred at the origin (prop-hunt arenas): perimeter blockers around an explicit
	// centre, with an explicit wall height in metres (the classic overload assumes origin + 4.2 m walls).
	void AddMapContainmentAt(const FVector& Center, float HalfX, float HalfY, float WallHeightMeters);
	ABHStaticBlockField* EnsureStaticBlockField();
	void ResetStaticBlockField();
	void FinalizeStaticBlockField();
	void RecoverPlayersFromVoid();
	FVector GetVoidRecoveryLocationFor(const ABHCharacter* Character) const;
	void SpawnAmbient(const FVector& Location, float Frequency, float Volume, float Noise, float Pulse, float LifeSpan = 0.0f);
	void PrepareRoundDirector();
	void StartDirectorTimer();
	void TickDirector();
	// Periodically (on a jittered cooldown) hands a single-use decoy to a small random subset of alive human
	// survivors who aren't already holding one, so decoys stay scarce and unpredictable instead of being a
	// tap-spam tool every player carries. Called from TickDirector during the Hunt.
	void MaybeHandOutDecoys();
	void TriggerScareEvent();
	bool TriggerRevisionThemedAmbientScare(ABHCharacter* Target);
	// Occasional dread builder: a flicker + faint whisper/footstep near the target that resolves to
	// nothing. Makes the real scares land harder. Returns true if a fake-out was emitted this tick.
	bool TriggerFakeOutTensionCue(ABHCharacter* Target);
	void TriggerMonsterChargeJumpscare(ABHCharacter* Target);
	// bForceRevealSequence runs the classic SCP-096 choreography (spawn far with a faint red glow, hold/pause,
	// then sprint in with audio and cut the lights afterwards) even for a modern imported monster variant.
	void TriggerMonsterChargeJumpscareWithVariant(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FString& Message, float FearAmount, float DreadAmount, bool bForceRevealSequence = false);
	// The classic distant-reveal -> pause -> charge -> blackout scare, now driven by a real creature mesh.
	void TriggerScp096Scare(ABHCharacter* Target);
	// Same flow as above but driven by the genuine imported SCP-096 model (its own mesh/skeleton/anims). A
	// standalone scare layered on the shared flow — does not modify TriggerScp096Scare or any other scare.
	void TriggerRealScp096Scare(ABHCharacter* Target);
	// A teacher-initiated scare picks at random from the showcased set (super chain, behind-you, corner peek,
	// in-your-face slam, SCP-096) so "the Teacher chose you" never feels the same twice.
	void TriggerTeacherChosenScare(ABHCharacter* Target, ABHPlayerController* TargetPC);
	// Scares aimed AT the teacher (student-triggered or the periodic beat) are restricted to the three "proper"
	// scares only: the turn-around (behind-you), the super-jumpscare chain, or an SCP-096 variation. Never the
	// lighter face/peek/ambient cues.
	void TriggerProperScareOnTeacher(ABHCharacter* Teacher);
	// True if a Teacher/Hunter pawn is within Radius of Location. Ambient/director jumpscares are suppressed
	// near the teacher so their own approach is not stepped on by a random monster; deliberate teacher scares
	// and test-menu scares bypass this.
	bool IsTeacherNearby(const FVector& Location, float Radius = 1800.0f) const;
	// Slams a random full-screen "face" still into the player's view with a scream/flash (the "PNG in your
	// face" scare). No 3D actor is spawned; the image + impact are delivered as a client horror cue.
	void TriggerFaceImageJumpscare(ABHCharacter* Target);
	// "Something is behind you": a directive prompt + movement-only lock that springs the payoff jumpscare
	// when the player turns to face the presence (handled client-side via the BehindYou cue).
	void TriggerBehindYouScare(ABHCharacter* Target);
	// Passive corner-peek: spawns a figure leaning out at a wall edge in the player's periphery that
	// retreats when looked at or after a short linger. Tension, not a contact scare.
	void TriggerCornerPeek(ABHCharacter* Target);
	bool ChooseWhisperJumpscareVariant(FBHJumpscareVariant& OutVariant) const;
	void TriggerTeacherFlatScare(ABHCharacter* Target, const FVector& FocusLocation, const FString& Message, float LockSeconds = 1.65f);
	void FreezeTargetForJumpscare(ABHCharacter* Target, float DurationSeconds);
	void CutLightsForJumpscare(const FVector& TargetLocation, const FVector& MonsterLocation, float Radius, float RestoreDelaySeconds = 9.5f);
	// Dims (does not kill) lit flicker-lights near the target/monster down to DimScale (e.g. 0.15 = 15%) for a
	// few seconds when the creature appears — the eerie "lights sink low as it manifests" beat, distinct from
	// the hard blackout CutLightsForJumpscare does on the final impact.
	void DimLightsForJumpscare(const FVector& TargetLocation, const FVector& MonsterLocation, float Radius, float DimScale, float DurationSeconds);
	int32 GetEffectiveScareIntensity() const;
	bool ResolveJumpscareVariantToken(const FString& VariantToken, FBHJumpscareVariant& OutVariant, int32& OutVariantIndex, FString& OutError) const;
	void TriggerTesterJumpscareVariant(ABHPlayerController* RequestingController, const FBHJumpscareVariant& Variant, const FString& TestLabel);
	void TriggerTesterSuperJumpscare(ABHPlayerController* RequestingController, const FBHJumpscareVariant* ForcedVariant = nullptr);
	bool ResolveSuperJumpscareRoute(ABHCharacter* Target, ABHPlayerController* TargetPC, FVector& OutPeekStart, FVector& OutPeekEnd, FVector& OutCrossStart, FVector& OutCrossEnd, FVector& OutChargeBendStart, FVector& OutChargeStart) const;
	struct FBHResolvedJumpscareSpawn
	{
		FVector SpawnLocation = FVector::ZeroVector;
		FVector FocusLocation = FVector::ZeroVector;
		FRotator SpawnRotation = FRotator::ZeroRotator;
		float Score = -1.0f;
	};
	bool ResolveVisibleJumpscareSpawn(ABHCharacter* Target, ABHPlayerController* TargetPC, const TArray<FVector>& Candidates, float FocusHeight, float MinDistance, float MaxDistance, float PathRadius, FBHResolvedJumpscareSpawn& OutSpawn) const;
	// Resolves a spawn that does NOT need to be in the player's view (used for the behind / ceiling-drop
	// approaches). ZOffset raises the candidate above the floor; bPreferClose scores nearer spots higher.
	bool ResolveDirectionalJumpscareSpawn(ABHCharacter* Target, const TArray<FVector>& Candidates, float FocusHeight, float MinDistance, float MaxDistance, float PathRadius, float ZOffset, bool bPreferClose, FBHResolvedJumpscareSpawn& OutSpawn) const;
	// Picks a monster-charge approach with an even mix, avoiding an immediate repeat of the last one.
	EBHJumpscareApproach ChooseMonsterChargeApproach();
	void SendJumpscareChargeCue(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FVector& FocusLocation, const FString& Message, float HoldDuration, float AudioVolume = 1.0f, bool bCloseRangeFocus = false) const;
	void TriggerCloseOverlayJumpscare(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FString& Message, float HoldDuration, float FearAmount, float DreadAmount);
#if WITH_DEV_AUTOMATION_TESTS
public:
	bool DebugValidateJumpscareSpawnCandidate(const FVector& ViewLocation, const FVector& ViewForward, const FVector& TargetLocation, const FVector& CandidateLocation, float FocusHeight, float PathRadius, float* OutScore = nullptr) const;
	bool DebugReserveDirectorCue(ABHCharacter* Target, EBHScareEventType ScareType, float Intensity, FString& OutReason);
protected:
#endif
	void TriggerColdCallEvent();
	void UpdatePresenceDirector();
	void ApplyPresenceSpike(const FVector& SourceLocation, float SpikeLevel, const FString& PresenceText);
	void PublishObjectiveBeats();
	void PublishDangerObjectiveBeat(const FVector& Location, const FString& Label);
	void BroadcastStatus(const FString& Message, float DurationSeconds = 3.0f) const;
	void UpdateDirectorGameState(const FString& ObjectiveText);
	// bBroadcastBlockReason=false performs a silent re-check (used on the per-tick path) so the
	// "exit locked" status is not re-broadcast every second; completion events keep the default.
	void UpdateExitUnlockState(bool bBroadcastBlockReason = true);
	void RefreshNextLevelFromVotes();
	void StartPracticeMode(ABHPlayerController* RequestingController);
	void RefreshPracticeDirector(const FString& Reason);
	void StartTestMode(ABHPlayerController* RequestingController);
	void RefreshTestDirector(const FString& Reason);
	void ResetRoleWarmupForLiveRoundStart();
	EBHRoundModifier ChooseRoundModifier(FRandomStream& Stream) const;
	FString GetRoundModifierText(EBHRoundModifier Modifier) const;
	void StartPrepPhase();
	void StartHuntPhaseImmediately();
	void StartHuntPhase();
	void AssignRoles();
	// --- Prop Hunt (opt-in, reversible). All gated by bPropHuntMode; defined in BHGameModePropHunt.cpp. ----------
	// Destroy the classroom objective actors (question stations, breakers, exit gates, escape managers) the level
	// builders / authored maps spawned, so a prop-hunt arena has no tasks and no exit to run to. Called after the
	// level is built; no-op when prop hunt is off.
	void StripPropHuntObjectives();
	// HIDE phase (rides Prep): freeze + screen-black the seeker, let props disguise. The Prep->Hunt timer is the release.
	void BeginPropHuntHidePhase();
	// SEEK phase (rides Hunt): release the seeker + start the seek clock + taunts. Called from StartHuntPhase.
	void BeginPropHuntHunt();
	// A caught prop joins the seekers (infection); the last caught prop ends the round as a seeker win.
	void HandlePropHuntCapture(ABHCharacter* Survivor, ABHCharacter* CapturingHunter);
	// Once-per-second service (called from TickRoundTimer's Hunt branch): drives the forced-taunt cadence, the
	// auto reveal-pulse cadence, and the replicated "props left / total" HUD counters.
	void TickPropHunt();
	// Force every still-hidden prop to emit a noise so the seeker gets a fair directional hint.
	void ForcePropHuntTaunt();
	// Auto reveal pulse (P4): briefly surface EVERY alive prop's position to every seeker (range-unlimited), on a
	// cadence that tightens as the seek timer runs down. The fairness valve that guarantees endgame closure.
	void ForcePropHuntRevealPulse();
	// Round-end win/lose sting + caption (gated to prop hunt; called from EndRound so every end path plays it once).
	void PlayPropHuntRoundEndSting(EBHRoundPhase ResultPhase);
	// Recompute + replicate the prop-hunt HUD state (props remaining/total, next-taunt countdown).
	void RefreshPropHuntGameState();
	// Count props (role == Survivor) and how many are still hidden (alive). Caught props keep the Survivor role
	// in prop hunt (the Hall-Monitor conversion is suppressed), so found = total - remaining is exact.
	void CountPropHuntProps(int32& OutTotal, int32& OutRemaining) const;
	// P3 arena loader: when the loaded world IS a registered prop-hunt arena package (an imported pack map), build it
	// as an arena -- collect placed PlayerStarts or floor-scatter spawns, size the runtime nav + containment + void
	// recovery to the geometry bounds -- and skip the classroom generators entirely. Returns false when the current
	// map is not a registered arena (the normal generators/authored discovery run as before).
	bool TryBuildPropHuntArena();
	// Deterministic spawn scatter for arena maps with too few placed PlayerStarts: Halton candidate points over the
	// bounds, floor-traced and clearance-checked. Static so the automation test can exercise it on a bare test world.
	static TArray<FVector> ScatterPropHuntSpawnPoints(UWorld* World, const FBox& Bounds, int32 DesiredCount, int32 Salt);
	// The blocking static-geometry bounds of the loaded arena map (sky spheres/oversized shells excluded).
	FBox ComputePropHuntArenaBounds() const;
	void TickRoundTimer();
	void EndRound(EBHRoundPhase ResultPhase);
	void ResetRoundByTravel();
	void TravelToTrainIntermission(EBHRoundPhase ResultPhase);
	void PersistPlayersForTravel();
	void RestorePlayersAfterTravel(AController* Controller);
	void ConvertMonitorsBackToSurvivors(const FString& Reason);
	void TriggerFinalEscapeIfNeeded();
	bool CanUseTesterShortcut(ABHPlayerController* RequestingController, const TCHAR* ActionDescription) const;
	bool IsFinalStage() const;
	int32 GetConfiguredStageIndex() const;
	FString GetDefaultMapForStage(int32 StageIndex) const;
	FString GetNextMapAfterStage(int32 StageIndex) const;
	// Variable-length host map route helpers (route empty => the default 3-stage Facility/Substation/Foggrounds).
	int32 GetRouteStageCount() const { return RuntimeMapRoute.Num() > 0 ? RuntimeMapRoute.Num() : 3; }
	int32 GetMaxStageIndex() const { return FMath::Max(0, GetRouteStageCount() - 1); }
	// Resolve RevisionAllowedQuestionIds from RevisionQuestionSetId (empty id => no restriction).
	void ResolveAllowedQuestionIds();
	FString BuildTravelOptionsForLevel(const FString& LevelName, bool bIntermission, int32 StageIndex, EBHRoundPhase ResultPhase) const;
	// Returns the .umap package to travel to for a logical level. When bUseAuthoredLevels is enabled and an
	// authored /Game/BlackoutHunt/Maps/<Level> asset exists it is used; otherwise the stock runtime base
	// map (/Engine/Maps/Entry) is returned so the procedural generator runs as before.
	FString ResolveTravelMapForLevel(const FString& NormalizedLevel) const;
	bool AreAllReady() const;
	int32 CountAliveSurvivors() const;
	int32 CountAliveHunters() const;
	int32 CountEscapedSurvivors() const;
	void RefreshBotRoster(ABHPlayerController* RequestingController);
	bool RemoveOneBot();
	void TrimBotRosterToCapacity();
	int32 CountHumanPlayers() const;
	// Public so a stuck-player reset (ABHCharacter::ServerResetFromTreeStuckZone) can fetch the authoritative,
	// role-aware, collision-resolved spawn transform without duplicating spawn logic.
public:
	FTransform GetSpawnTransformFor(AController* Controller) const;
protected:
	void SweepExpiredBotTacticalState();
	bool IsExclusiveBotClaimIntent(EBHBotIntent Intent) const;
	bool IsBotTargetStillUseful(const AActor* Target) const;
	bool IsRevisionAdmin(const ABHPlayerController* RequestingController) const;
	void ResetRevisionStats();
	void UpdateRevisionSummary(const FString& ReviewText = FString());
	FBHClassRevisionSummary ComputeRevisionSummary() const;
	bool CanUnlockRevisionExit(FString& OutReason) const;
	FString GetRevisionObjectiveText() const;
	EBHPhysicsTopic GetWeakestRevisionTopic() const;
	bool ExportRevisionReportToDisk(EBHRoundPhase ResultPhase, bool bAutomatic, FString& OutMessage);
	void RecordPlaytestTelemetryMapSnapshot();
	ABHCharacter* FindCharacterForPlayerState(APlayerState* TargetPlayerState) const;

#if WITH_DEV_AUTOMATION_TESTS
public:
	void DebugRestorePlayersAfterTravelForTest(AController* Controller) { RestorePlayersAfterTravel(Controller); }
	void DebugSetTrainIntermissionLevelForTest(bool bEnabled) { bTrainIntermissionLevel = bEnabled; }
	bool DebugDiscoverAuthoredLevelForTest() { return DiscoverAuthoredLevel(); }
	int32 DebugGetBreakerCountForTest() const { return BreakerActors.Num(); }
	int32 DebugGetObjectiveStationCountForTest() const { return ObjectiveStations.Num(); }
	int32 DebugGetExitGateCountForTest() const { return ExitGates.Num(); }
	int32 DebugGetSurvivorSpawnCountForTest() const { return SurvivorSpawns.Num(); }
	FVector DebugGetHunterSpawnForTest() const { return HunterSpawn; }
	void DebugRecordTrainCabinCentersForTest(float MinX, float MaxX, float FloorZ) { RecordTrainCabinCenters(MinX, MaxX, FloorZ); }
	const TArray<FVector>& DebugGetTrainCabinCentersForTest() const { return TrainCabinCenters; }
	FString DebugResolveTravelMapForLevelForTest(const FString& Level) const { return ResolveTravelMapForLevel(Level); }
	// Defined in BHGameMode.cpp: NormalizeBHLevelName is file-local (anonymous namespace) there.
	static FString DebugNormalizeBHLevelNameForTest(const FString& Level);
	void DebugSetPropHuntModeForTest(bool bEnabled) { bPropHuntMode = bEnabled; }
	FString DebugBuildTravelOptionsForLevelForTest(const FString& Level, int32 StageIndex) const { return BuildTravelOptionsForLevel(Level, false, StageIndex, EBHRoundPhase::Lobby); }
	static TArray<FVector> DebugScatterPropHuntSpawnPointsForTest(UWorld* World, const FBox& Bounds, int32 DesiredCount, int32 Salt) { return ScatterPropHuntSpawnPoints(World, Bounds, DesiredCount, Salt); }
	bool DebugTryBuildPropHuntArenaForTest() { return TryBuildPropHuntArena(); }
protected:
#endif

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 MinPlayers;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 MaxPlayers;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 PrepSeconds;

	// Teaching: longer first-play warmup. Effective warmup length uses ExtendedPrepSeconds when
	// bExtendedFirstWarmup is set OR this install has never hosted a class (auto-enable first session).
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	bool bExtendedFirstWarmup;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 ExtendedPrepSeconds;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	bool bHasHostedClassBefore;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 HuntSeconds;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 RequiredBreakers;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	bool bAllowHostForceStart;

	TArray<FVector> SurvivorSpawns;
	// Centre of each train carriage along the train's length, recorded by RecordTrainCabinCenters when the lobby /
	// intermission train is built. ResetPlayerToTrainInterior snaps a stuck player to the nearest one by X.
	TArray<FVector> TrainCabinCenters;
	TArray<FVector> ScarePoints;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHBreaker>> BreakerActors;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHDoor>> DoorActors;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHExitGate>> ExitGates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHFlickerLight>> FlickerLights;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHBlockActor>> StationSignalBlocks;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHFlickerLight>> StationSignalLights;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHObjectiveStation>> ObjectiveStations;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHEscapeStationManager>> EscapeStationManagers;
	UPROPERTY(Transient)
	TObjectPtr<ABHTrainIntermissionManager> TrainIntermissionManager;
	// The active block-field shard that AddStaticBlock currently appends to. Once it reaches the per-shard
	// spec cap a new shard is spawned (see EnsureStaticBlockField). StaticBlockFields holds every shard for
	// reset/finalize. Sharding keeps each actor's replicated spec array well under the engine's ~64KB single
	// bunch limit, so a dense procedural level (e.g. the train intermission) replicates to all clients instead
	// of erroring out and leaving everyone in empty geometry.
	UPROPERTY(Transient)
	TObjectPtr<ABHStaticBlockField> StaticBlockField;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHStaticBlockField>> StaticBlockFields;
	UPROPERTY(Transient)
	TObjectPtr<UBHAtmosphereDirector> AtmosphereDirector;
	FVector HunterSpawn;
	FString RuntimeLevelName;
	FString NextRuntimeLevelName;
	EBHFogPreset RuntimeFogPreset;
	EBHFogPreset NextFogPreset;
	bool bFogPresetOverride;
	bool bFacilityBuilt;
	FTimerHandle RoundTimerHandle;
	FTimerHandle DirectorTimerHandle;
	FTimerHandle VoidRecoveryTimerHandle;
	// Armed only on the FinalEscape fallback path (no EscapeStationManager present) so the match
	// cannot hang forever; the normal path uses the manager's own escape timer.
	FTimerHandle FinalEscapeFallbackTimerHandle;
	// True once a ServerTravel has been committed/scheduled for this GameMode instance (e.g. the
	// post-round reset). Discretionary travels routed through RequestServerTravel() bail when set, so
	// a tester shortcut cannot overwrite the pending destination during the post-round window.
	bool bServerTravelInProgress = false;
	int32 RoundSeed;
	int32 TargetHunterCount;
	int32 ObjectiveIntensity;
	int32 ActiveBreakerCount;
	int32 ActiveSideObjectiveCount;
	bool bInfectionMode;
	bool bPartyPace;
	bool bPracticeMode;
	// The solo guided tutorial rides on the practice sandbox but additionally hands beat ownership to the
	// map-baked ABHTutorialDirector, which points a single marker at the one next object the lesson needs.
	bool bTutorialMode;
	// Set true by ABHTutorialDirector ONLY during the scripted Survivor-phase Teacher chase so ambient dread runs
	// for that climax; false (the default) during every calm teaching step and every other phase. See
	// IsTutorialHorrorSuppressed(). Reset to its default on each phase ServerTravel (fresh game mode instance).
	bool bTutorialHorrorAllowed = false;
	// Which tutorial the director runs (Survivor -> Teacher -> Monitor). Parsed from ?BHTutorialPhase=.
	EBHTutorialPhase RuntimeTutorialPhase = EBHTutorialPhase::Survivor;
	// Whether to chain to the next tutorial on reaching the exit (full course) or return to the menu (single
	// selected tutorial). Parsed from ?BHTutorialChain= (default true). Preserved across the chaining travels.
	bool bTutorialChain = true;
	bool bTestMode;
	// Opt-in, reversible Prop Hunt mode (parsed from ?BHPropHunt=1 / the bh.PropHunt cvar in BuildRuntimeFacility).
	// Mutually exclusive with the practice/test/train sandboxes and revision mode. When false the game is unchanged.
	bool bPropHuntMode = false;
	// Server time of the last forced prop taunt (drives the shrinking taunt cadence). See BHGameModePropHunt.cpp.
	float PropHuntLastTauntServerTime = -1000.0f;
	// Server time of the last auto reveal pulse (P4); seeded at seek start so the first pulse lands a full interval in.
	float PropHuntLastPulseServerTime = -1000.0f;
	// Below this Z an alive pawn counts as fallen out of the world (RecoverPlayersFromVoid teleports it back). The
	// BlackoutHunt maps are built around Z=0 so -650 is right for them; a prop-hunt arena (imported pack map) sets
	// this from its own geometry bounds, which may sit far from the origin.
	float VoidRecoveryZThreshold = -650.0f;
	EBHRevisionMode RevisionMode;
	bool bRevisionMode;
	int32 RevisionTopicMask;
	EBHRevisionDifficultyMix RevisionDifficultyMix;
	float RevisionClassThreshold;
	float RevisionIndividualThreshold;
	int32 RevisionRoundDuration;
	int32 RevisionScareIntensity;
	// --- Host lobby customization (parsed from URL options; reconstructed on every ServerTravel) ---
	EBHQuestionDifficulty RevisionStartingDifficulty = EBHQuestionDifficulty::Easy;
	EBHQuestionDifficulty RevisionMinDifficulty = EBHQuestionDifficulty::Easy;
	EBHQuestionDifficulty RevisionMaxDifficulty = EBHQuestionDifficulty::Hard;
	// Non-empty when the host pinned an exact question set; RevisionAllowedQuestionIds is then resolved from it.
	FString RevisionQuestionSetId;
	TArray<FString> RevisionAllowedQuestionIds;
	// Ordered host map route (variable length, repeats allowed). Empty = the default 3-stage sequence.
	TArray<FString> RuntimeMapRoute;
	// Procedural layout knobs (0 = generator default). Applied only when the round runs the generator.
	int32 LayoutSeed = 0;
	int32 LayoutBreakerCount = 0;
	int32 LayoutDensity = 0;
	bool bForceProceduralLayout = false;
	int32 RevisionReviewTimeRemaining;
	bool bRevisionReportExported;
	bool bBotMode;
	int32 TargetBotCount;
	EBHBotDifficulty BotDifficulty;
	EBHRoundModifier PracticeRoundModifier;
	float NoiseRadiusMultiplier;
	float LastDirectorScareTime;
	float LastMonsterChargeTime;
	float LastFakeOutTime = -1000.0f;
	// Anti-repetition state so back-to-back monster scares don't reuse the same variant/approach.
	FName LastJumpscareVariantId;
	EBHJumpscareApproach LastJumpscareApproach = EBHJumpscareApproach::HeadOn;
	float LastColdCallTime;
	// Cooldown clock for the random single-use decoy hand-out (see MaybeHandOutDecoys).
	float LastDecoyHandoutTime;
	float LastPresenceWhisperTime;
	float LastWhisperJumpscareTime;
	float LastPresenceSpikeTime;
	// Last UpdatePresenceDirector wall-clock time, so the presence decay can be time-based instead of a flat
	// per-tick drop (the director interval is mode-dependent, ~7s normal vs ~4s revision intensity-3).
	float LastPresenceDirectorTime = -1.0f;
	float LastTeacherCounterScareTime;
	// Cooldown clock for the occasional "scare the Teacher too" beat (a proper scare on a random alive hunter).
	float LastTeacherProperScareTime = -999.0f;
	float LastObjectiveDangerBeatTime;
	float LastSpectatorEncouragementTime;
	FVector LastBotNoiseLocation;
	float LastBotNoiseTime;
	bool bRuntimeNavigationReady;
	bool bTrainIntermissionLevel;
	// True when this map is the pre-game train LOBBY (built by BuildTrainLobbyLevel). LobbyFirstLevelName is the
	// hunt level the lobby travels to when the round starts.
	bool bLobbyLevel;
	FString LobbyFirstLevelName;
	int32 RuntimeStageIndex;
	// Hall-monitor contribution gate target, FROZEN once per round (see RefreshRevisionContributionGateTarget).
	// The enforced gate (CanUseHallMonitorTools) and the displayed/replicated requirement both read this single
	// snapshot so the threshold cannot drift mid-round when students join/leave. Per-node question targets
	// (GetRevisionQuestionTargetPerNode) deliberately stay live — that is a different concept.
	int32 RevisionContributionGateTarget;
	EBHRoundPhase PendingIntermissionResult;
	UPROPERTY(Transient)
	TObjectPtr<ANavMeshBoundsVolume> RuntimeNavBounds;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHBotController>> BotControllers;
	TArray<FBHBotStimulus> BotWorldStimuli;
	TArray<FBHBotObjectiveClaim> BotObjectiveClaims;
	TArray<FBHBotTargetCooldown> BotTargetCooldowns;
	TMap<TObjectKey<APlayerState>, float> SpectatorEncouragementTimes;
	// Last server time a host-admin denial reply was sent to each controller, so a misbehaving
	// client spamming host-only Server RPCs can't make us flood it with reliable status messages.
	// Mutable because RequireHostAdmin is const but needs to record the throttle timestamp.
	mutable TMap<TObjectKey<APlayerController>, float> HostAdminDenialReplyTimes;
	TSet<FString> LoggedBotTacticalWarnings;
	TSet<FString> TelemetryUsedLockerKeys;
	TSet<FString> TelemetryStartedObjectiveKeys;
	TSet<FString> TelemetryCompletedObjectiveKeys;
	TSet<FString> TelemetrySnapshotKeys;
};

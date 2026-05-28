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

UCLASS()
class BLACKOUTHUNT_API ABHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABHGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetPlayerReady(ABHPlayerController* Controller, bool bReady);
	void NotifyBreakerRepaired(const FVector& BreakerLocation = FVector::ZeroVector);
	void NotifyObjectiveStationCompleted(ABHObjectiveStation* Station);
	void NotifySurvivorCaptured(ABHCharacter* Survivor, ABHCharacter* CapturingHunter = nullptr);
	void NotifySurvivorEscaped(ABHCharacter* Survivor);
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
	FBHJumpscareVariant ChooseJumpscareVariant(EBHScareEventType EventType) const;
	float GetScareSensoryScale() const;
	void TestJumpscareVariant(ABHPlayerController* RequestingController, const FString& VariantToken);
	FString GetJumpscareVariantTestReport() const;
	void ForceStartRound(ABHPlayerController* RequestingController);
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
	void CompleteTrainIntermission(const FString& NextMapName, bool bFinalRecap);
	void NotifyFinalEscapeExpired();
	void SetBotCount(ABHPlayerController* RequestingController, int32 NewBotCount);
	void SetBotDifficulty(ABHPlayerController* RequestingController, EBHBotDifficulty NewDifficulty);
	bool IsRevisionMode() const;
	EBHRevisionDifficultyMix GetRevisionDifficultyMix() const;
	TArray<EBHPhysicsTopic> GetRevisionWeakTopics() const;
	int32 GetRevisionQuestionTargetPerNode() const;
	int32 GetRevisionAnswerTeamTargetSize() const;
	int32 GetRevisionMinimumContributionTarget() const;
	bool CanUseHallMonitorTools(const ABHPlayerState* PlayerState, FString& OutBlockReason) const;
	static bool IsRevisionParticipantRole(EBHPlayerRole Role);
	static bool IsValidSpectatorRolePreference(EBHPlayerRole Role);
	static EBHPlayerRole SanitizeSpectatorRolePreference(EBHPlayerRole Role);
	static int32 ResolveRevisionQuestionTargetFor(int32 StudentCount, int32 StageIndex);
	static int32 ResolveRevisionNodeTargetFor(int32 StudentCount, int32 StageIndex, int32 StationCount);
	static FVector ResolveFoggroundsDoorFrameOrigin(const FVector& DoorLocation);
	void RecordRevisionAnswer(ABHCharacter* Character, const FBHRevisionQuestion& Question, bool bCorrect, bool bCorrection, const FString& SelectedAnswer = FString(), const FString& TeamSummary = FString());
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
	void BuildTrainIntermissionLevel();
	void BuildSubstationLevel();
	void BuildFoggroundsLevel();
	void BuildFoggroundsFinalStation();
	void BuildMapSubwayExitStation(const FVector& GateLocation, float DirectionSign, const FString& StationName, const FString& DestinationText);
	void BuildRuntimeNavigation();
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
		EBHBlockMaterial FallbackMaterial = EBHBlockMaterial::PaintedMetal);
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
	ABHStaticBlockField* EnsureStaticBlockField();
	void ResetStaticBlockField();
	void FinalizeStaticBlockField();
	void RecoverPlayersFromVoid();
	FVector GetVoidRecoveryLocationFor(const ABHCharacter* Character) const;
	void SpawnAmbient(const FVector& Location, float Frequency, float Volume, float Noise, float Pulse, float LifeSpan = 0.0f);
	void PrepareRoundDirector();
	void StartDirectorTimer();
	void TickDirector();
	void TriggerScareEvent();
	bool TriggerRevisionThemedAmbientScare(ABHCharacter* Target);
	// Occasional dread builder: a flicker + faint whisper/footstep near the target that resolves to
	// nothing. Makes the real scares land harder. Returns true if a fake-out was emitted this tick.
	bool TriggerFakeOutTensionCue(ABHCharacter* Target);
	void TriggerMonsterChargeJumpscare(ABHCharacter* Target);
	void TriggerMonsterChargeJumpscareWithVariant(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FString& Message, float FearAmount, float DreadAmount);
	bool ChooseWhisperJumpscareVariant(FBHJumpscareVariant& OutVariant) const;
	void TriggerTeacherFlatScare(ABHCharacter* Target, const FVector& FocusLocation, const FString& Message, float LockSeconds = 1.65f);
	void FreezeTargetForJumpscare(ABHCharacter* Target, float DurationSeconds);
	void CutLightsForJumpscare(const FVector& TargetLocation, const FVector& MonsterLocation, float Radius, float RestoreDelaySeconds = 9.5f);
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
	void UpdateExitUnlockState();
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
	FString BuildTravelOptionsForLevel(const FString& LevelName, bool bIntermission, int32 StageIndex, EBHRoundPhase ResultPhase) const;
	bool AreAllReady() const;
	int32 CountAliveSurvivors() const;
	int32 CountEscapedSurvivors() const;
	void RefreshBotRoster(ABHPlayerController* RequestingController);
	bool RemoveOneBot();
	void TrimBotRosterToCapacity();
	int32 CountHumanPlayers() const;
	FTransform GetSpawnTransformFor(AController* Controller) const;
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
protected:
#endif

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 MinPlayers;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 MaxPlayers;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 PrepSeconds;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 HuntSeconds;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	int32 RequiredBreakers;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	bool bAllowHostForceStart;

	TArray<FVector> SurvivorSpawns;
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
	UPROPERTY(Transient)
	TObjectPtr<ABHStaticBlockField> StaticBlockField;
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
	int32 RoundSeed;
	int32 TargetHunterCount;
	int32 ObjectiveIntensity;
	int32 ActiveBreakerCount;
	int32 ActiveSideObjectiveCount;
	bool bInfectionMode;
	bool bPartyPace;
	bool bPracticeMode;
	bool bTestMode;
	EBHRevisionMode RevisionMode;
	bool bRevisionMode;
	int32 RevisionTopicMask;
	EBHRevisionDifficultyMix RevisionDifficultyMix;
	float RevisionClassThreshold;
	float RevisionIndividualThreshold;
	int32 RevisionRoundDuration;
	int32 RevisionScareIntensity;
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
	float LastPresenceWhisperTime;
	float LastWhisperJumpscareTime;
	float LastPresenceSpikeTime;
	float LastTeacherCounterScareTime;
	float LastObjectiveDangerBeatTime;
	float LastSpectatorEncouragementTime;
	FVector LastBotNoiseLocation;
	float LastBotNoiseTime;
	bool bRuntimeNavigationReady;
	bool bTrainIntermissionLevel;
	int32 RuntimeStageIndex;
	EBHRoundPhase PendingIntermissionResult;
	UPROPERTY(Transient)
	TObjectPtr<ANavMeshBoundsVolume> RuntimeNavBounds;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHBotController>> BotControllers;
	TArray<FBHBotStimulus> BotWorldStimuli;
	TArray<FBHBotObjectiveClaim> BotObjectiveClaims;
	TArray<FBHBotTargetCooldown> BotTargetCooldowns;
	TMap<TObjectKey<APlayerState>, float> SpectatorEncouragementTimes;
	TSet<FString> LoggedBotTacticalWarnings;
	TSet<FString> TelemetryUsedLockerKeys;
	TSet<FString> TelemetryStartedObjectiveKeys;
	TSet<FString> TelemetryCompletedObjectiveKeys;
	TSet<FString> TelemetrySnapshotKeys;
};

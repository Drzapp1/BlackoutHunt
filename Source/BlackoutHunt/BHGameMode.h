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
class ABHSecurityCamera;
class ABHTrainIntermissionManager;
class ANavMeshBoundsVolume;
class UBHAtmosphereDirector;

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

	void SetPlayerReady(ABHPlayerController* Controller, bool bReady);
	void NotifyBreakerRepaired(const FVector& BreakerLocation = FVector::ZeroVector);
	void NotifyObjectiveStationCompleted(ABHObjectiveStation* Station);
	void NotifySurvivorCaptured(ABHCharacter* Survivor);
	void NotifySurvivorEscaped(ABHCharacter* Survivor);
	void ToggleLightCircuit(int32 CircuitId);
	void OpenSecurityCircuit(int32 CircuitId);
	bool ToggleSecurityCircuit(int32 CircuitId);
	void NotifyLoudNoise(const FVector& Location, const FString& Reason);
	void NotifyCCTVDetection(ABHSecurityCamera* Camera, AActor* ZoneActor, ABHCharacter* Survivor, const FString& AlertLabel);
	void ReportAtmosphereStimulus(EBHAtmosphereStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, float Strength, const FString& Reason);
	bool TriggerAtmosphereCue(const FBHScareEventSpec& Spec);
	bool TriggerBlackoutPulse(const FVector& Location, float Radius, float DurationSeconds);
	bool TriggerManualScare(ABHCharacter* Target, EBHScareEventType ScareType);
	bool TriggerStudentScareSwitch(ABHCharacter* Activator, const FVector& SourceLocation, const FString& ScareTitle, int32 Severity);
	FBHJumpscareVariant ChooseJumpscareVariant(EBHScareEventType EventType) const;
	void TestJumpscareVariant(ABHPlayerController* RequestingController, const FString& VariantToken);
	FString GetJumpscareVariantTestReport() const;
	void ForceStartRound(ABHPlayerController* RequestingController);
	void SetDesiredRole(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole);
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
	void TriggerHunterBlackout(const FVector& SourceLocation);
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
	void SetScareIntensity(ABHPlayerController* RequestingController, int32 NewScareIntensity);
	void ForceReview(ABHPlayerController* RequestingController);
	void TesterGrantTrainResources(ABHPlayerController* RequestingController);
	void TesterOpenTrainIntermission(ABHPlayerController* RequestingController);
	void TesterAdvanceTrainPhase(ABHPlayerController* RequestingController);
	void TesterLoadFinalStation(ABHPlayerController* RequestingController);
	void TesterTriggerFinalEscape(ABHPlayerController* RequestingController);
	void TesterForceFinalRecap(ABHPlayerController* RequestingController);
	FString GetRevisionStatusReport() const;
	bool ExportRevisionReport(ABHPlayerController* RequestingController, FString& OutMessage);
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
	void ApplyRoundCCTVVisibility();
	int32 SelectHiddenCCTVIndex(int32 CandidateCount, int32 Salt) const;
	ABHCCTVZone* SpawnCCTVZoneForCamera(ABHSecurityCamera* Camera, int32 CircuitId, const FString& AlertLabel, bool bVisible);
	void AddFacilityDetailPass();
	void AddClassroomHorrorPass();
	void AddSurfaceDetailGrid(float HalfX, float HalfY, const FLinearColor& LineTint);
	void AddIndustrialClutter(const TArray<FVector>& Centers, const FLinearColor& Tint);
	void AddMoodPass(const FLinearColor& FogColor, float FogDensity, float VignetteIntensity, float FilmGrainIntensity);
	void AddFoggroundsLightingPass();
	void AddFoggroundsMoodPass();
	void AddFoggroundsModeledFog();
	void SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint = FLinearColor(0.38f, 0.42f, 0.45f, 1.0f), const FRotator& Rotation = FRotator::ZeroRotator, bool bCollides = true, EBHBlockMaterial Material = EBHBlockMaterial::Tinted);
	void SpawnHiddenBlocker(const FVector& Location, const FVector& Scale);
	void AddMapContainment(float HalfX, float HalfY);
	void RecoverPlayersFromVoid();
	FVector GetVoidRecoveryLocationFor(const ABHCharacter* Character) const;
	void SpawnAmbient(const FVector& Location, float Frequency, float Volume, float Noise, float Pulse, float LifeSpan = 0.0f);
	void PrepareRoundDirector();
	void StartDirectorTimer();
	void TickDirector();
	void TriggerScareEvent();
	void TriggerRevisionThemedAmbientScare(ABHCharacter* Target);
	void TriggerMonsterChargeJumpscare(ABHCharacter* Target);
	void TriggerTeacherFlatScare(ABHCharacter* Target, const FVector& FocusLocation, const FString& Message, float LockSeconds = 1.65f);
	void FreezeTargetForJumpscare(ABHCharacter* Target, float DurationSeconds);
	void CutLightsForJumpscare(const FVector& TargetLocation, const FVector& MonsterLocation, float Radius, float RestoreDelaySeconds = 9.5f);
	int32 GetEffectiveScareIntensity() const;
	bool ResolveJumpscareVariantToken(const FString& VariantToken, FBHJumpscareVariant& OutVariant, int32& OutVariantIndex, FString& OutError) const;
	void TriggerTesterJumpscareVariant(ABHPlayerController* RequestingController, const FBHJumpscareVariant& Variant, const FString& TestLabel);
	void TriggerTesterSuperJumpscare(ABHPlayerController* RequestingController, const FBHJumpscareVariant* ForcedVariant = nullptr);
	bool ResolveSuperJumpscareRoute(ABHCharacter* Target, ABHPlayerController* TargetPC, FVector& OutPeekStart, FVector& OutPeekEnd, FVector& OutCrossStart, FVector& OutCrossEnd, FVector& OutChargeBendStart, FVector& OutChargeStart) const;
	void TriggerColdCallEvent();
	void UpdatePresenceDirector();
	void ApplyPresenceSpike(const FVector& SourceLocation, float SpikeLevel, const FString& PresenceText);
	void BroadcastStatus(const FString& Message, float DurationSeconds = 3.0f) const;
	void UpdateDirectorGameState(const FString& ObjectiveText);
	void UpdateExitUnlockState();
	void RefreshNextLevelFromVotes();
	void StartPracticeMode(ABHPlayerController* RequestingController);
	void RefreshPracticeDirector(const FString& Reason);
	void StartTestMode(ABHPlayerController* RequestingController);
	void RefreshTestDirector(const FString& Reason);
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
	ABHCharacter* FindCharacterForPlayerState(APlayerState* TargetPlayerState) const;

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
	TArray<TObjectPtr<ABHBreaker>> BreakerActors;
	TArray<TObjectPtr<ABHDoor>> DoorActors;
	TArray<TObjectPtr<ABHExitGate>> ExitGates;
	TArray<TObjectPtr<ABHFlickerLight>> FlickerLights;
	TArray<TObjectPtr<ABHObjectiveStation>> ObjectiveStations;
	TArray<TObjectPtr<ABHEscapeStationManager>> EscapeStationManagers;
	TObjectPtr<ABHTrainIntermissionManager> TrainIntermissionManager;
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
	float LastColdCallTime;
	float LastPresenceWhisperTime;
	float LastPresenceSpikeTime;
	float LastTeacherCounterScareTime;
	FVector LastBotNoiseLocation;
	float LastBotNoiseTime;
	bool bRuntimeNavigationReady;
	bool bTrainIntermissionLevel;
	int32 RuntimeStageIndex;
	EBHRoundPhase PendingIntermissionResult;
	TObjectPtr<ANavMeshBoundsVolume> RuntimeNavBounds;
	TArray<TObjectPtr<ABHBotController>> BotControllers;
	TArray<FBHBotStimulus> BotWorldStimuli;
	TArray<FBHBotObjectiveClaim> BotObjectiveClaims;
	TArray<FBHBotTargetCooldown> BotTargetCooldowns;
	TMap<TObjectKey<AActor>, FVector> BotApproachPointCache;
	TSet<FString> LoggedBotTacticalWarnings;
};

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "GameFramework/Character.h"
#include "BHCharacter.generated.h"

class ABHLocker;
class ABHGameState;
class ABHObjectiveStation;
class ABHPlayerState;
class ABHTrainServiceLight;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UBHPowerupComponent;
class UMeshComponent;
class UPointLightComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USpotLightComponent;

UCLASS()
class BLACKOUTHUNT_API ABHCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABHCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	void EnterLocker(ABHLocker* Locker);
	// bAllowMovementExit: true only for a voluntary player exit (not capture/escape), enabling the roll-out-of-locker
	// link (bh.LockerRollExit) when Sprint is held. Capture/escape pass false so a downed player never rolls.
	void ExitLocker(bool bAllowMovementExit = false);
	void MarkCaptured();
	void MarkEscaped();
	// Tutorial-only capture shield: the guided-tutorial director sets this TRUE on the student for the whole teaching
	// half so a stray/early Teacher swing (or the Loom spawn race) can't capture them before the scripted Encounter,
	// and clears it at the climax. Server-side gate only (capture is decided on authority); default false in live play.
	void SetTutorialCaptureImmune(bool bImmune) { bTutorialCaptureImmune = bImmune; }
	bool IsTutorialCaptureImmune() const { return bTutorialCaptureImmune; }
	void RefillFlashlight(float Amount);
	void RecoverStamina(float Amount);
	void AddFear(float Amount);
	void AddDread(float Amount);
	void ApplyDetentionMark(float DurationSeconds);
	void ClearDetentionMark();
	void ResetRoleWarmupStateForRoundStart();
	void ApplyAvatarStyle();
	ABHPlayerState* GetBHPlayerState() const;
	// Social emotes: if this player played an emote within the last few seconds, returns true + its label so the
	// HUD can draw a bubble over their head. GetEmoteLabel maps an emote id to its short text.
	bool GetActiveEmote(FString& OutLabel) const;
	static FString GetEmoteLabel(int32 Id);
	float GetFlashlightTuningValue(FName ParameterName) const;
	void SetFlashlightTuningValue(FName ParameterName, float Value);
	bool BotBeginInteract(AActor* Target);
	void BotEndInteract(AActor* Target = nullptr);
	void BotExitCurrentLocker();
	bool BotTryCapture();
	bool BotUseScan();
	bool BotUseHunterPower();
	bool BotDropDecoyOrTrap();
	bool BotSubmitAnswer(ABHObjectiveStation* Station, int32 AnswerIndex);
	bool UsePowerupByType(EBHPowerupType Type);
	bool InterruptTeacherCaptureAttack(const FString& Reason, float RecoverySeconds);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsHiddenInLocker() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	float GetFlashlightBattery() const;

	// True when this character is a student standing inside an active Teacher blackout, so their flashlight is
	// being weakened: faster battery drain, a flickering near-dead beam, and no teacher-stagger. The Teacher
	// and far-away students are never affected.
	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsInTeacherBlackout() const;

	// Per-second flashlight battery drain for this character right now, including the surge while caught in a
	// Teacher blackout (see IsInTeacherBlackout).
	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	float GetEffectiveFlashlightDrainPerSecond() const;

	// Public read of the flashlight on/off state (the replicated field itself is protected). Used by the
	// tutorial director to detect when a student has tried the flashlight.
	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsFlashlightOn() const { return bFlashlightOn; }

	// Tutorial movement lesson: a bitmask of which of the four move directions the player has actually
	// driven (forward 0x01 / back 0x02 / right 0x04 / left 0x08). Accumulated in MoveForward/MoveRight on the
	// listen-server host, so the tutorial director can keep the WASD prompt up until all four are pressed.
	static constexpr uint8 TutorialMoveForwardBit = 0x01;
	static constexpr uint8 TutorialMoveBackBit = 0x02;
	static constexpr uint8 TutorialMoveRightBit = 0x04;
	static constexpr uint8 TutorialMoveLeftBit = 0x08;
	static constexpr uint8 TutorialMoveAllMask = 0x0F;
	uint8 GetTutorialMovementMask() const { return TutorialMovementMask; }
	void ResetTutorialMovementMask() { TutorialMovementMask = 0; }

	// Movement-tutorial telemetry (server-side, listen-server host): latches the advanced links the player has
	// performed -- the transient ones (roll/slide/dive) a coarse 0.5s tutorial poll would otherwise miss, plus the
	// bunny-hop chain. Mirrors TutorialMovementMask. Set authoritatively; read by ABHTutorialDirector.
	static constexpr int32 TutorialActRollBit = 1 << 0;
	static constexpr int32 TutorialActSlideBit = 1 << 1;
	static constexpr int32 TutorialActDiveBit = 1 << 2;
	static constexpr int32 TutorialActBhopBit = 1 << 3;
	// Advanced micro-technique latches (each set at its specific code path so the tutorial detects the TECHNIQUE,
	// not just "a roll"): drop-roll on landing, slide-stop early brake, a clean (quiet) roll, locker roll-out, and a
	// frame-perfect momentum flow-chain.
	static constexpr int32 TutorialActDropRollBit = 1 << 4;
	static constexpr int32 TutorialActSlideStopBit = 1 << 5;
	static constexpr int32 TutorialActQuietRollBit = 1 << 6;
	static constexpr int32 TutorialActLockerRollBit = 1 << 7;
	static constexpr int32 TutorialActFlowChainBit = 1 << 8;
	// Sprint latch: set authoritatively while actually running at sprint speed (the same gate the stamina drain
	// uses), so the Sprint tutorial step detects from the real, map-scale-correct speed rather than a fixed cm/s.
	static constexpr int32 TutorialActSprintBit = 1 << 9;
	int32 GetTutorialActionMask() const { return TutorialActionMask; }
	// Live count of consecutive sprint-speed hops (0 when not chaining). The Movement tutorial reads this for
	// progress coaching (e.g. "2 in a row!") before the 3-hop TutorialActBhopBit latch trips.
	int32 GetTutorialBhopChain() const { return TutorialBhopChain; }
	void ResetTutorialActionMask() { TutorialActionMask = 0; TutorialBhopChain = 0; LastTutorialJumpServerTime = -999.0f; }
	void MarkTutorialAction(int32 Bit) { TutorialActionMask |= Bit; }

	// Last server time (World->GetTimeSeconds) the Hunter/Monitor used each ability. The tutorial director
	// compares these against the step start time to detect that the student just pressed Q / R / G.
	float GetLastScanTime() const { return LastScanTime; }
	float GetLastHunterPowerTime() const { return LastHunterPowerTime; }
	float GetLastDecoyTime() const { return LastDecoyTime; }

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	float GetStamina() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	float GetStaminaPercent() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	float GetFear() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	float GetDread() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsDetentionMarked() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	float GetDetentionMarkRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	EBHMovementSpecialState GetMovementSpecialState() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	bool IsProne() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	bool IsSpecialMoveActive() const;

	// True while the active (or most-recently-finished, until the next move starts) roll/slide/dive clipped a wall
	// (bSpecialMoveHitWall). The Movement tutorial reads this to coach clean spacing ("you clipped a wall").
	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	bool IsSpecialMoveWallBonk() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	EBHMovementSpecialState GetCosmeticMovementSpecialState() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	float GetRemainingSpecialMoveCooldown() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	FString GetLastMovementFailureReason() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Movement")
	float GetMovementSightProfileMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Counterplay")
	bool IsTeacherCaptureAttackActive() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Counterplay")
	bool IsTeacherCaptureAttackInWindup() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Counterplay")
	float GetTeacherCaptureAttackProgress() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Counterplay")
	float GetTeacherCaptureCooldownRemaining() const;

	// Local-only: fires a transient FOV punch + camera flinch on a jumpscare impact.
	// FOVPunchDegrees<=0 uses the default punch; Intensity (0..1) scales magnitude. Honors reduced-camera-shake comfort.
	void PlayJumpscareCameraImpact(float Intensity, float FOVPunchDegrees = -1.0f);

	// Current cosmetic first-person VIEW ROLL in degrees -- the lateral barrel roll during a dodge roll. The camera
	// uses bUsePawnControlRotation (which clobbers component-relative roll), so ABHPlayerCameraManager reads this in
	// ProcessViewRotation and stamps it onto the view rotation. 0 whenever no roll effect is active.
	float GetViewRollOffsetDeg() const { return ViewRollOffsetDeg; }

	// Server-authoritative crawl-space entry assist: if this is an eligible survivor not already in a low-profile
	// pose, auto-drop to prone so a player sprinting at a crawl mouth flows into cover instead of being bounced off
	// the lip by the volume. Returns true if now (or already) in a sheltering pose. Called by ABHCrawlSpaceVolume.
	bool TryEnterCrawlSpacePose();

#if WITH_DEV_AUTOMATION_TESTS
	bool TryStartSpecialMoveForTest(EBHMovementSpecialState RequestedState, bool bEndProne);
	bool TrySetProneForTest(bool bNewProne);
	// Exposes IsTeacherCaptureCandidateAuthority so a test can assert a prone survivor sheltering in a crawl is
	// uncapturable (i.e. the Teacher cannot tag them).
	bool DebugIsTeacherCaptureCandidateForTest(const ABHCharacter* Target) const;
	float DebugGetAntiCampIdleSecondsForTest() const;
	float DebugGetAntiCampMoveBurstSecondsForTest() const { return AntiCampMoveBurstSeconds; }
	FString DebugGetInteractionFailureReasonForTest(AActor* Target) const { return GetInteractionFailureReason(Target); }
	bool DebugBeginInteractForTest(AActor* Target) { return BeginInteractAuthority(Target, false, false); }
	AActor* DebugGetServerInteractTargetForTest() const { return CurrentServerInteractTarget; }
	void DebugEndStaleHeldInteractionForTest() { EndStaleHeldInteractionAuthority(); }
#endif

protected:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void StartKeyboardTurnLeft();
	void StopKeyboardTurnLeft();
	void StartKeyboardTurnRight();
	void StopKeyboardTurnRight();
	void StartKeyboardLookUp();
	void StopKeyboardLookUp();
	void StartKeyboardLookDown();
	void StopKeyboardLookDown();
	void ApplyKeyboardLook(float DeltaSeconds);
	void ToggleReady();
	void StartInteract();
	void StopInteract();
	void ToggleFlashlight();
	void StartJump();
	void StopJump();
	virtual void OnJumped_Implementation() override;
	void StartSprint();
	void StopSprint();
	void StartCrouch();
	void StopCrouch();
	void StartProne();
	void StopProne();
	void TryCapture();
	void UseScan();
	void UseHunterPower();
	void DropDecoy();
	// bVisual = answered via the interactive visual element (a clicked diagram region); choice rows
	// and number keys pass false. Visual answers earn extra mastery (server-side).
	void SubmitAnswer(int32 AnswerIndex, bool bVisual = false);
	void SubmitAnswerOne();
	void SubmitAnswerTwo();
	void SubmitAnswerThree();
	void SubmitAnswerFour();
	// Typed numeric entry for Calculation questions. Digit keys build a local buffer that
	// the owning client submits with Enter; multiple-choice questions are unaffected.
	void NumericEntryDigit(int32 Digit);
	void NumericEntryDecimal();
	void NumericEntryMinus();
	void NumericEntryBackspace();
	void ConfirmNumericAnswer();
	// Param-less key wrappers (UInputComponent::BindKey requires member functions).
	void NumericEntryZero() { NumericEntryDigit(0); }
	void NumericEntryFive() { NumericEntryDigit(5); }
	void NumericEntrySix() { NumericEntryDigit(6); }
	void NumericEntrySeven() { NumericEntryDigit(7); }
	void NumericEntryEight() { NumericEntryDigit(8); }
	void NumericEntryNine() { NumericEntryDigit(9); }
	bool IsCalculationEntryActive() const;
	void SetClientFocusedQuestionStation(class ABHObjectiveStation* Station);
	// --- Mouse-driven question interaction (client-only) ---
	// Toggle the "answer with the mouse" cursor over the focused question panel. Number keys 1-4 and
	// typed numeric entry keep working whether or not this is on, so keyboard/bots are unaffected.
	void ToggleQuestionCursor();
	void UseNodeMarker();
	// Recovery: ask the server to teleport this player back inside the train if they get stuck outside (e.g. up
	// on the lobby roof). Local handler -> ServerResetToTrainInterior; the GameMode gates it to train levels.
	void RequestResetToTrainInterior();
	UFUNCTION(Server, Reliable)
	void ServerResetToTrainInterior();
	// LeftMouseButton press/release while the question cursor is active: select a choice, pick up or
	// drop an arrangement piece, or tap a keypad key, by hit-testing the HUD's question regions.
	void OnQuestionPointerDown();
	void OnQuestionPointerUp();
	// Per-tick upkeep: drops the cursor when no answerable question is focused, and resets the local
	// drag arrangement when the focused question's piece set changes (e.g. a new question loads).
	void UpdateQuestionInteractionState();
public:
	// Read by the HUD to render the in-progress typed answer for calculation questions.
	const FString& GetNumericAnswerEntry() const { return NumericAnswerEntry; }
	// Read by the HUD to render the interactive question overlay (cursor mode, in-progress drag).
	bool IsQuestionCursorActive() const { return bQuestionCursorActive; }
	int32 GetQuestionDraggedPiece() const { return QuestionDraggedPiece; }
	const TArray<int32>& GetQuestionArrangement() const { return QuestionArrangement; }

	// Easter-egg hook (PUBLIC so external interactables can call it -- e.g. the train-roof hatch): the server
	// tells the owning client to unlock a (cosmetic) achievement and toast it. Cosmetic only; account progress
	// is client-local, so the server can't write it directly -- it asks the owning client to.
	UFUNCTION(Client, Reliable)
	void ClientGrantAchievement(FName AchievementId, const FString& ToastMessage);

	// Easter-egg hook: the server tells the owning client it completed a train activity (type 0..3) -> Tourist progress.
	UFUNCTION(Client, Reliable)
	void ClientRecordTrainActivity(uint8 ActivityIndex);

	// Achievement hook: the server tells the owning (human) Teacher it just made a capture -> lifetime-capture
	// count behind the Truant Officer achievement. Account progress is client-local, so the server asks the client.
	UFUNCTION(Client, Reliable)
	void ClientRecordCapture();

	// Easter-egg hook (PUBLIC so the train-roof breaker can call it): the server tells the owning client to
	// toggle ITS per-player roof service lights. Per-player lighting is client-local, so the server can't flip
	// it directly -- it asks the owning client to, here.
	UFUNCTION(Client, Reliable)
	void ClientToggleRoofServiceLights();

	// Educational hook: the server tells the owning client a graded answer's result (physics topic 0..3,
	// correct?) -> per-topic mastery + the Honor Roll / Polymath achievements. Cosmetic/local.
	UFUNCTION(Client, Reliable)
	void ClientRecordQuestionResult(uint8 TopicIndex, bool bCorrect);
	// Public so the tutorial director can read where the hall-monitor false marker resolves (to steer the scripted
	// Teacher toward it in the Monitor lesson). Pure read; returns false if no valid marker target is found.
	bool ResolveHallMonitorMarkerLocation(FVector& OutLocation) const;
protected:
	void UsePowerupSlotOne();
	void UsePowerupSlotTwo();
	void UsePowerupSlotThree();
	void UsePowerupSlotFour();
	void UsePowerupSlotFive();
	void UsePowerupSlotSix();
	void TesterGrantTrainResources();
	void TesterOpenTrainIntermission();
	void TesterAdvanceTrainPhase();
	void TesterLoadFinalStation();
	void TesterTriggerFinalEscape();
	void TesterForceFinalRecap();

	bool CanAct() const;
	bool TraceForInteractable(AActor*& OutActor) const;
	bool FindInteractableFromView(AActor*& OutActor, float ExtraDistance = 0.0f) const;
	bool IsValidInteractionTarget(AActor* Target) const;
	bool HasInteractionLineOfSight(AActor* Target) const;
	FString GetInteractionFailureReason(AActor* Target) const;
	void SendStatusMessage(const FString& Message) const;
	void SendFakeHunterHint(bool bRealHint);
	bool BeginInteractAuthority(AActor* Target, bool bUseViewFallback, bool bShowFailureMessages);
	void EndInteractAuthority(AActor* Target);
	// Server-side guard for hold-style interactions (objective stations, breakers): the worker is only
	// dropped on key release (StopInteract), so holding E and walking out of reach used to keep driving
	// the task to completion. Ends the active interaction once the target leaves interaction range,
	// reusing the same distance gate BeginInteractAuthority applied to start it.
	void EndStaleHeldInteractionAuthority();
	void ExitCurrentLockerAuthority();
	bool TryCaptureAuthority(bool bShowFailureMessages);
	bool UseScanAuthority(bool bShowFailureMessages);
	bool UseHunterPowerAuthority(bool bShowFailureMessages);
	bool DropDecoyAuthority(bool bShowFailureMessages);
	bool SubmitAnswerAuthority(ABHObjectiveStation* Station, int32 AnswerIndex, bool bUseViewFallback, bool bShowFailureMessages, bool bVisualAnswer = false);
	bool SubmitNumericAnswerAuthority(float Value);
	void EmitFootstepStimulus(float Strength, const FString& Reason, EBHFootstepSurface Surface = EBHFootstepSurface::Default);
	EBHFootstepSurface ResolveFootstepSurface(const FHitResult* KnownGroundHit = nullptr) const;
	FBHFootstepSurfaceProfile GetFootstepSurfaceProfile(EBHFootstepSurface Surface) const;
	void BroadcastFlashlightAudioCue(bool bNewOn, bool bBatteryDied = false);
	// Server-side stuttering "the light is fighting the dark" cue, emitted on a short cadence while a student
	// is caught in a Teacher blackout (reuses the flashlight click sound, no new asset).
	void BroadcastFlashlightStruggleAudioCue();
	void SendTeacherProximityAudioCue(float DistanceAlpha, bool bHunterHasSight);
	void UpdateAntiCampPressureAuthority(float DeltaSeconds, float Speed2D, const class ABHGameState* BHGS, const ABHPlayerState* BHPS);
	void ResetAntiCampTrackingAuthority();
	void UpdateViewFeel(float DeltaSeconds);
	void UpdatePOVAnimation(float DeltaSeconds);
	// Transient jumpscare-impact envelope (0..1) shared by the FOV punch and camera flinch.
	float GetJumpscareImpactEnvelope() const;
	float ComputeJumpscareFOVPunch() const;
	FRotator ComputeJumpscareCameraFlinch() const;
	bool IsReducedCameraShakeLocal() const;
	// Resolve the player's dodge-roll camera style (motion-sickness setting), honoring the cvar override and the
	// reduced-camera-shake comfort clamp. See EBHRollCamStyle.
	EBHRollCamStyle ResolveRollCamStyle() const;
	void UpdateFlashlightFeel(float DeltaSeconds);
	// Instantaneous [~0.02..0.55] beam-strength multiplier while caught in a Teacher blackout: a low, violently
	// flickering value driven by FlashlightPulseTime so the beam stutters and nearly dies.
	float ComputeBlackoutFlashlightFlicker() const;
	void TryBHopJump();
	bool TryStartSpecialMoveAuthority(EBHMovementSpecialState RequestedState, bool bEndProne, bool bEndProneRequiresInput);
	bool ValidateSpecialMoveSpaceAuthority(EBHMovementSpecialState RequestedState, const FBHMovementSpecialTuning& Tuning, FString& OutFailureReason) const;
	// Vault/mantle helper (ships dark behind bh.VaultTech): if a special move's forward hit is a low, vaultable ledge
	// with clear headroom + floor on the far side, hop over it (consuming Curve.VerticalImpulse) and return true.
	bool TryVaultOverObstacleAuthority(const FBHMovementSpecialTuning& Tuning, const FVector& StepDir, const FHitResult& WallHit);
	void UpdateSpecialMoveAuthority(float DeltaSeconds);
	void EmitSpecialMoveNoiseAuthority(EBHMovementSpecialState State, FName NoiseEvent, float EventMultiplier);
	void UpdateTeacherCaptureAttackAuthority(float DeltaSeconds);
	void ResolveTeacherCaptureAttackAuthority();
	void NotifyNearbySurvivorsOfTeacherCaptureWindup(float Now);
	bool IsTeacherCaptureCandidateAuthority(const ABHCharacter* Target, float& OutScore) const;
	bool HasDirectVisibilityToCharacter(const ABHCharacter* Target) const;
	float GetTeacherCaptureClockSeconds() const;
	bool IsTimedCaptureEvasionActive(float Now) const;
	bool CanStaggerTeacherWithFlashlight(const ABHCharacter* Survivor) const;
	void SpendFlashlightForTeacherStagger(float Now);
	void StartTeacherCaptureRecoveryAuthority(float Now, float RecoverySeconds);
	void StartCosmeticSpecialMove(EBHMovementSpecialState State);
	void ClearCosmeticSpecialMove();
	void SetMovementFailureReason(const FString& Reason);
	void FinishSpecialMoveAuthority();
	bool SetProneAuthority(bool bNewProne, bool bShowFailureMessages);
	bool CanStandFromProne() const;
	void ApplyMovementSpecialState();
	void RefreshMovementSpeedFromState();
	void SetProneCollisionApplied(bool bApplied);
	// Functional first-person sit (a lobby relax pose): lowers the eye height and freezes movement
	// server-side; pressing a movement key stands you back up. No body animation (the game is first-person).
	void ToggleSit();
	void SetSeatedAuthority(bool bNewSeated);
	// Social emote: cycles through a small set of text emotes shown over your head to nearby players.
	void CycleEmote();
	// Cosmetic third-person seated pose (no sit animation needed): hide the leg bones and drop the torso to
	// the seat so other players see a seated upper body instead of a standing-locked one.
	void ApplySeatedAvatar(bool bSeatedNow);
	void ApplyFlashlightState();
	void ApplyHiddenState();
	void UpdateHunterVisualCue();
	void ConfigureLowPolyAvatar();
	void UpdateLowPolyAvatar(float DeltaSeconds);
	void UpdateRoleSkeletalAnimation(float Speed2D, float MoveAlpha, float SprintAlpha, bool bGrounded);
	void ApplyRoleModelVisuals(const ABHPlayerState* BHPS, const FLinearColor& ShirtColor, const FLinearColor& SkinColor);
	void ApplyTeacherWeaponVisuals(const ABHPlayerState* BHPS);
	void PlayTeacherMeleeSwingLocal(bool bConfirmedHit);
	void UpdateTeacherWeaponSwingVisuals();
	void SetLowPolyAvatarVisible(bool bVisible);

	UFUNCTION(Server, Reliable)
	void ServerSetFlashlight(bool bNewOn);

	UFUNCTION(Server, Reliable)
	void ServerBeginInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerEndInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerExitCurrentLocker();

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerStartSpecialMove(EBHMovementSpecialState RequestedState, bool bEndProne, bool bEndProneRequiresInput);

	UFUNCTION(Server, Reliable)
	void ServerSetProne(bool bNewProne);

	UFUNCTION(Server, Reliable)
	void ServerSetProneInputHeld(bool bHeld);

	// Mirror crouch-held intent to the server so the authority-side drop-roll (Landed: bSprintInputHeld &&
	// bCrouchInputHeld) fires for remote clients, not just the listen-server host.
	UFUNCTION(Server, Reliable)
	void ServerSetCrouchInputHeld(bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerSetSeated(bool bNewSeated);

	UFUNCTION(Server, Reliable)
	void ServerEmote(int32 InEmoteId);

	UFUNCTION(Client, Reliable)
	void ClientSpecialMoveRejected(EBHMovementSpecialState RejectedState, const FString& Reason);

	// Momentum tech: the server tells the owning client they nailed a frame-perfect chain, so the client can
	// unlock the (cosmetic) perfect_chain achievement locally and show a brief cue.
	UFUNCTION(Client, Reliable)
	void ClientNotifyPerfectChain(int32 ChainCount);

	UFUNCTION(Server, Reliable)
	void ServerTryCapture();

	UFUNCTION(Server, Reliable)
	void ServerUseScan();

	UFUNCTION(Server, Reliable)
	void ServerUseHunterPower();

	UFUNCTION(Server, Reliable)
	void ServerDropDecoy();

	UFUNCTION(Server, Reliable)
	void ServerSubmitAnswer(int32 AnswerIndex, bool bVisualAnswer);

	UFUNCTION(Server, Reliable)
	void ServerSubmitNumericAnswer(float Value);

	// Drag/drop answer for matching/ordering questions: SlotToPiece[slot] is the chosen piece index.
	UFUNCTION(Server, Reliable)
	void ServerSubmitArrangement(const TArray<int32>& SlotToPiece);

	UFUNCTION(Server, Reliable)
	void ServerUsePowerup(EBHPowerupType Type);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFootstepCue(EBHFootstepSurface Surface, FVector Location, float Volume);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTeacherMeleeSwing(bool bConfirmedHit);

	UFUNCTION(Client, Reliable)
	void ClientReceiveScanResult(const FVector& TargetLocation, bool bTargetHidden, bool bFoundTarget);

	UFUNCTION()
	void OnRep_FlashlightOn();

	UFUNCTION()
	void OnRep_HiddenInLocker();

	UFUNCTION()
	void OnRep_OutOfPlay();

	UFUNCTION()
	void OnRep_MovementSpecialState();

	UFUNCTION()
	void OnRep_TeacherCaptureAttackActive();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Flashlight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> HunterHueLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBHPowerupComponent> PowerupComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FlashlightBeamOuter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FlashlightBeamCore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> AvatarRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RoleModelRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> RoleSkeletalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleHeadwearMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleHeadwearAccentMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleHeadwearDetailMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleGearMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleGearAccentMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleGearLeftStrapMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleGearRightStrapMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleGearDetailMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> TeacherWeaponRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TeacherWeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TeacherWeaponHandleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TeacherWeaponHeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TeacherWeaponBladeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TeacherWeaponGripMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TeacherWeaponTrailMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HeadJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LeftShoulderJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RightShoulderJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LeftElbowJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RightElbowJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LeftHipJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RightHipJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LeftKneeJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RightKneeJoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WaistMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HairMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftEyeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightEyeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MouthMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftUpperArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftLowerArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightUpperArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightLowerArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftUpperLegMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftLowerLegMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftFootMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightUpperLegMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightLowerLegMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightFootMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackpackMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoleBadgeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float MaxStamina;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float InteractDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float CaptureDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horror")
	float FlashlightDrainPerSecond;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashlightBeamOuterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashlightBeamCoreMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TeacherWeaponTrailMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horror")
	float ScanCooldownSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horror")
	float DecoyCooldownSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float StaminaDrainPerSecond;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float StaminaRecoveryPerSecond;

	UPROPERTY(ReplicatedUsing = OnRep_MovementSpecialState, BlueprintReadOnly, Category = "Movement")
	EBHMovementSpecialState MovementSpecialState;

	// Server-only scratch (listen-server host) for the tutorial WASD lesson; see GetTutorialMovementMask.
	uint8 TutorialMovementMask = 0;
	// Server-only scratch for the Movement tutorial's advanced-link detection; see GetTutorialActionMask.
	int32 TutorialActionMask = 0;
	int32 TutorialBhopChain = 0;
	float LastTutorialJumpServerTime = -999.0f;

	UPROPERTY(ReplicatedUsing = OnRep_TeacherCaptureAttackActive, BlueprintReadOnly, Category = "Counterplay")
	bool bTeacherCaptureAttackActive;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Counterplay")
	float TeacherCaptureAttackStartServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Counterplay")
	float TeacherCaptureAttackResolveServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Counterplay")
	float TeacherCaptureAttackEndServerTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Counterplay")
	float TeacherCaptureNextAllowedServerTime;

	UPROPERTY(ReplicatedUsing = OnRep_FlashlightOn, BlueprintReadOnly, Category = "Horror")
	bool bFlashlightOn;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Horror")
	float FlashlightBattery;

	bool bFlashlightEmptyTelemetryReported;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
	float Stamina;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Horror")
	float Fear;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Horror")
	float Dread;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Horror")
	float DetentionMarkRemaining;

	UPROPERTY(ReplicatedUsing = OnRep_HiddenInLocker, BlueprintReadOnly, Category = "Stealth")
	bool bHiddenInLocker;

	// Server-only tutorial capture shield (see SetTutorialCaptureImmune). Not replicated -- capture is authority-decided.
	bool bTutorialCaptureImmune = false;

	UPROPERTY(ReplicatedUsing = OnRep_OutOfPlay, BlueprintReadOnly, Category = "Round")
	bool bOutOfPlay;

	// Functional first-person sit: replicated so the lowered eye height applies on the owning client
	// (read each frame in UpdateViewFeel). Movement is frozen server-side while true.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
	bool bSeated = false;

	// Cosmetic seated-pose tracking for the third-person avatar (not replicated; derived from bSeated).
	bool bSeatedAvatarApplied = false;
	bool bRoleMeshStandCaptured = false;
	FVector RoleMeshStandOffset = FVector::ZeroVector;

	// Social emotes: the last-played emote id + when (server time), replicated so the HUD can draw a short
	// emote bubble over nearby players. -1 = none.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Social")
	int32 EmoteId = -1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Social")
	float EmoteStartServerTime = 0.0f;

	// Owning-client local cycle counter (not replicated).
	int32 LocalEmoteCycle = 0;

	UPROPERTY()
	TObjectPtr<AActor> CurrentInteractTarget;

	UPROPERTY()
	TObjectPtr<AActor> CurrentServerInteractTarget;

	UPROPERTY()
	TObjectPtr<ABHLocker> CurrentLocker;

	// Owning-client only: the calculation question the local player is looking at, pushed by
	// the HUD each frame, plus the digits they have typed but not yet submitted.
	TWeakObjectPtr<class ABHObjectiveStation> ClientFocusedQuestionStation;
	FString NumericAnswerEntry;

	// Per-player roof service lights (client-local; never replicated). Spawned/destroyed when the owning client
	// throws the train-roof breaker so each passenger lights the roof for themselves.
	void SetRoofServiceLightsLocal(bool bOn);
	bool bRoofServiceLightsOn = false;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABHTrainServiceLight>> LocalRoofLights;

	// Comfort: hide OTHER players' bodies (locally, for this viewer only) when they crowd right up against the
	// camera, so a knot of people around a node/breaker doesn't block the view. Driven each frame from the local
	// viewpoint; never hides the active threat (Hunter), and never touches gameplay or collision.
	void UpdateProximityPlayerHideFromLocalView();
	void SetProximityHiddenLocal(bool bShouldHide);
	bool bProximityHiddenLocal = false;
	TArray<TWeakObjectPtr<UMeshComponent>> ProximityHiddenMeshes;

	// Mouse-driven question interaction (client/owning-only; never replicated). The cursor frees the
	// mouse over the focused station's question panel; the drag fields build a matching/ordering answer.
	bool bQuestionCursorActive = false;
	// Piece index (into the focused station's InteractivePieces) currently held by the cursor, or -1.
	int32 QuestionDraggedPiece = INDEX_NONE;
	// slot -> piece index the player has placed (-1 = empty). Sized to the focused station's slots.
	TArray<int32> QuestionArrangement;
	// Identity of the arrangement the local QuestionArrangement was built for; a change resets it.
	FString QuestionArrangementKey;

	float LastScanTime;
	float LastHunterPowerTime;
	float LastDecoyTime;
	float LastSprintNoiseTime;
	float LastFootstepStimulusTime;
	float LastFlashlightAudioCueTime;
	float LastFlashlightStruggleAudioTime = -100.0f;
	float LastTeacherProximityAudioTime;
	float FootstepStimulusDistanceAccumulator;
	float StaminaRecoveryLockedUntil;
	float LastStaminaWarningTime;
	float LastHidingPanicMessageTime;
	float LastLockerNoiseTime;
	float LockerExitTime;
	float LastForcedBreathNoiseTime;
	float LastPanicBreathNoiseTime;
	float LastDetentionNoiseTime;
	float AntiCampMoveBurstSeconds;
	float AntiCampMoveBurstDistance;
	float AntiCampLastSatisfiedTime;
	float AntiCampLastMeaningfulMoveTime;
	FVector AntiCampLastSampleLocation;
	float LastAntiCampWarningTime;
	float LastAntiCampNoiseTime;
	float HiddenSeconds;
	float DefaultCameraFOV;
	FVector DefaultCameraLocation;
	float CameraBobTime;
	float AvatarAnimTime;
	float AvatarBreathTime;
	float AvatarMoveAlpha;
	float AvatarSprintAlpha;
	float SmoothedMoveAlpha;
	float SmoothedStrafeAlpha;
	float SmoothedSprintAlpha;
	FVector ViewFeelCameraLocation;
	FRotator POVAnimRotationCurrent;
	FVector POVAnimLocationCurrent;
	// Cosmetic first-person view-roll (degrees), updated each frame in UpdatePOVAnimation and read by
	// ABHPlayerCameraManager (the camera's bUsePawnControlRotation ignores component-relative roll). See GetViewRollOffsetDeg.
	float ViewRollOffsetDeg = 0.0f;
	float SmoothedBaseFOV = 0.0f;
	float JumpscareImpactStartTime = -1.0f;
	float JumpscareImpactIntensity = 0.0f;
	float JumpscareImpactFOVPunch = 0.0f;
	float LocalSpecialAnimStartTime;
	EBHMovementSpecialState LocalSpecialAnimState;
	float FlashlightPulseTime;
	float FlashlightTuningIntensityScale = 1.0f;
	float FlashlightTuningRadiusScale = 1.0f;
	float FlashlightTuningVolumetricScale = 1.0f;
	float FlashlightTuningBeamLengthScale = 1.0f;
	float FlashlightTuningBeamOpacityScale = 1.0f;
	float FlashlightTuningBeamBrightnessScale = 1.0f;
	float FlashlightTuningConeScale = 1.0f;
	float FlashlightTuningRayVisibilityScale = 1.0f;
	float FlashlightTuningRayWidthScale = 1.0f;
	float FlashlightTuningRayStartOffset = 48.0f;
	float LastBHopJumpInputTime;
	float SpecialMoveStartTime;
	float SpecialMoveEndTime;
	float SpecialMoveCooldownEndTime;
	// Momentum "flow chain" tech (survivor-side, bh.MomentumTech): a frame-perfect transient move right as the
	// previous one ends bypasses the cooldown once and preserves momentum. See Docs/EASTER_EGGS.md.
	float LastSpecialMoveEndedTime = -999.0f;
	int32 PerfectChainCount = 0;
	float SpecialMoveMomentumScale = 1.0f;
	// Quiet-roll discipline (bh.QuietRoll): set true if the active roll/slide/dive bonked a wall (MoveHit blocked),
	// so the roll-impact stimulus fires LOUD (sloppy) while a clean, full-distance roll fires quiet (a stealth reward).
	bool bSpecialMoveHitWall = false;
	// Flow-chain noise amnesty (bh.MomentumChainNoiseScale): captured once at move start from the chain depth, then
	// applied to every noise event of THIS move (the later Tick events fire after PerfectChainCount could change).
	float SpecialMoveNoiseScale = 1.0f;
	// Silent slide-stop (bh.SlideStopTech): set when the player releases Prone early in a Slide to brake into a quiet
	// crouch instead of standing/proning. FinishSpecialMoveAuthority reads it to choose the Crouch end-state.
	bool bSpecialMoveEarlyBrake = false;
	// Crawl-gap dash (bh.CrawlDashNoiseScale): set when the active Dive was started FROM prone, so its launch AND
	// landing noise are softened to a localizable scrape (quieter than a loud standing dive).
	bool bSpecialMoveFromProne = false;
	// Drop-roll (bh.DropRollTech): server time a roll was requested while airborne (buffered, not rejected). Landed()
	// consumes a fresh stamp to suppress the hard-landing noise and roll out on touchdown.
	float LastDropRollInputTime = -999.0f;
	float SpecialMoveDistanceTravelled;
	FVector SpecialMoveDirection;
	float LastCaptureEvasionTime;
	float LastTeacherCounterplayHintTime;
	float DefaultCapsuleHalfHeight;
	float DefaultCapsuleRadius;
	// Auto-squeeze: when walking into a narrow slot (walls close on both sides), the standing capsule smoothly
	// shrinks so you can thread the gap, and walk speed drops. Restores when the space opens up. Server + owning
	// client run it identically from world geometry so networked movement stays consistent.
	void UpdateAutoSqueeze(float DeltaSeconds);
	bool bAutoSqueezing = false;

	// While the train intermission is active, players pass through each other (and the teacher) so the packed
	// carriage never jams. Toggles the capsule's Pawn-channel response on the intermission boundary only.
	void UpdateTrainPawnCollision();
	bool bTrainPawnCollisionOff = false;
	float MovementFailurePulse;
	float LastMovementFailureTime;
	FString LastMovementFailureReason;
	bool bKeyboardLookLeft;
	bool bKeyboardLookRight;
	bool bKeyboardLookUp;
	bool bKeyboardLookDown;
	bool bBHopJumpQueued;
	bool bSprintInputHeld;
	bool bProneInputHeld;
	// True while the Crouch key is physically held (set in StartCrouch / cleared in StopCrouch), so a drop-roll can
	// fire on landing for a player holding Sprint+Crouch through the fall, rather than relying on a tight tap window.
	bool bCrouchInputHeld = false;
	bool bSpecialMoveEndsProne;
	bool bSpecialMoveEndProneRequiresInput;
	bool bProneCollisionApplied;
	// Server-only: true while we have authoritatively stopped this pawn's movement for an input
	// freeze (final-escape cutscene / jumpscare). Tracks ownership so the freeze restores movement
	// only if it was the one that disabled it. Not replicated.
	bool bMovementFrozenByServer = false;
	bool bTeacherCaptureAttackResolved;
	int32 SpecialMoveNoiseEventMask;
	EBHMovementSpecialState CosmeticMovementSpecialState;
	bool bUsingRoleModel;
	FName LastRoleAnimationName;
	float TeacherMeleeSwingStartTime;
	float TeacherMeleeSwingEndTime;
	float TeacherMeleeHitFlashEndTime;
	bool bTeacherMeleeSwingHit;
	int32 LastAppliedAvatarIndex;
	EBHPlayerRole LastAppliedAvatarRole;
	FLinearColor LastAppliedAvatarColor;
	int32 LastAppliedAvatarHeadwearIndex;
	int32 LastAppliedAvatarGearIndex;
};

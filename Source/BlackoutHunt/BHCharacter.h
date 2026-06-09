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
class USkeletalMesh;
class USkeletalMeshComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USpotLightComponent;

UCLASS()
class BLACKOUTHUNT_API ABHCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABHCharacter(const FObjectInitializer& ObjectInitializer);

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
	// True if a capture swing RIGHT NOW would actually land on Target (the same range/angle/alive/not-immune gate the
	// real grab uses). Public so the tutorial director can tell a genuine "too far" swing from a valid close one.
	bool CanTeacherCaptureTargetNow(const ABHCharacter* Target) const { float Score = 0.0f; return IsTeacherCaptureCandidateAuthority(Target, Score); }
	void RefillFlashlight(float Amount);
	void RecoverStamina(float Amount);
	void AddFear(float Amount);
	void AddDread(float Amount);
	void ApplyDetentionMark(float DurationSeconds);
	void ClearDetentionMark();
	void ResetRoleWarmupStateForRoundStart();
	void ApplyAvatarStyle();
	ABHPlayerState* GetBHPlayerState() const;
	// True while seated -- either the free-sit toggle (C) or locked onto an interactable chair. Chair seats read
	// this to avoid re-seating an already-seated player.
	bool IsSeated() const { return bSeated; }
	// Seat the player ONTO an interactable chair: teleport onto the seat, enter the seated pose, and LOCK them there
	// so movement input no longer stands you up -- only Jump does. Server authority (called from ABHTrainSeat).
	void SitOnSeatAuthority(const FVector& SeatLocation, float FacingYaw);
	// Social emotes: if this player played an emote within the last few seconds, returns true + its label, the
	// emote id, and how long ago it started, so the HUD can draw a bubble over their head (the shake emote
	// thrashes its bubble using OutAge). GetEmoteLabel maps an emote id to its short text; GetEmoteCount is the
	// size of the set (the emote wheel lays out one wedge per entry); IsShakeEmote flags the inside-joke emote.
	bool GetActiveEmote(FString& OutLabel, int32& OutId, float& OutAge) const;
	static FString GetEmoteLabel(int32 Id);
	static int32 GetEmoteCount();
	static bool IsShakeEmote(int32 Id);
	// Emote-wheel state for the HUD radial: whether it is open, the current selection stick (unit disc, +Y up),
	// and the live wedge id (-1 = aim is in the centre dead zone = cancel).
	bool IsEmoteWheelActive() const { return bEmoteWheelActive; }
	void GetEmoteWheelSelection(float& OutX, float& OutY) const { OutX = EmoteWheelSelX; OutY = EmoteWheelSelY; }
	int32 GetEmoteWheelHighlightedId() const;
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

	// --- Prop Hunt (opt-in, reversible). Read by the HUD; all default to "not a prop" when the mode is off. -------
	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Prop Hunt")
	bool IsDisguisedAsProp() const { return bDisguisedAsProp; }
	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Prop Hunt")
	bool IsPropLockedInPlace() const { return bPropLockedInPlace; }
	// Server-authoritative reset of the disguise (also re-shows the real body). Public so round-reset / capture /
	// escape paths can clear a lingering disguise. No-op off the server or when not disguised.
	void ClearPropDisguiseAuthority();
	// Seeker-kit client presentation state (P4), read by DrawPropHuntOverlay on the seeker's HUD: the through-wall
	// sonar/pulse markers with their expiry, the local sonar-ready clock, and the reveal-pulse flash stamp.
	const TArray<FVector>& GetPropHuntSonarMarkers() const { return PropHuntSonarMarkers; }
	float GetPropHuntSonarMarkersExpireClientTime() const { return PropHuntSonarMarkersExpireClientTime; }
	float GetPropHuntSonarReadyClientTime() const { return PropHuntSonarReadyClientTime; }
	float GetPropHuntPulseFlashClientTime() const { return PropHuntPulseFlashClientTime; }

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

	// Local-only inside-joke effect: when the player plays the "mister ke~" emote, thrash THEIR first-person view
	// up and down for a couple of seconds. Purely cosmetic -- it rides the same additive camera-component transform
	// as the jumpscare flinch, so the capsule/collision and control rotation never move. Honors reduced-camera-shake.
	void PlayEmoteShake(float Intensity = 1.0f);

	// Current cosmetic first-person VIEW ROLL in degrees -- the lateral barrel roll during a dodge roll. The camera
	// uses bUsePawnControlRotation (which clobbers component-relative roll), so ABHPlayerCameraManager reads this in
	// ProcessViewRotation and stamps it onto the view rotation. 0 whenever no roll effect is active.
	float GetViewRollOffsetDeg() const { return ViewRollOffsetDeg; }

	// Current cosmetic first-person VIEW LEAN in degrees -- a bank (roll about the view forward axis) into strafes and
	// into the direction you turn, part of the "raw" camera layer. Like the somersault above, bUsePawnControlRotation
	// clobbers component-relative roll, so ABHPlayerCameraManager reads this and rolls the final POV. 0 when no lean.
	float GetViewLeanRollDeg() const { return ViewLeanRollDeg; }

	// Current cosmetic view-PITCH wobble (degrees) for the "mister ke~" emote shake. Like the roll above, the camera's
	// bUsePawnControlRotation clobbers component rotation, so ABHPlayerCameraManager reads this and pitches the final
	// POV (after the control-rotation pitch limits, so the player's aim is never disturbed). 0 when no shake is active.
	float GetEmoteShakePitchOffsetDeg() const;

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
	// --- Prop Hunt (opt-in, reversible) input handlers. Each is a no-op unless this player is a live prop (a
	// Survivor in prop-hunt mode), so the keys are inert in every other mode. See the prop-hunt block lower down. ---
	void TogglePropDisguise();   // become the prop you are looking at (re-disguise allowed)
	void TogglePropLockInPlace(); // freeze perfectly still / unfreeze
	void RotatePropLeft();
	void RotatePropRight();
	void TogglePropCamera();     // flip the local prop view between 1st and 3rd person
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
	// Same bridge for collectable relics: the server tells the owning client to record a found relic in its
	// (client-local) account and toast it. Called by ABHCollectable on collect.
	UFUNCTION(Client, Reliable)
	void ClientRecordCollectable(FName CollectableId, const FString& ToastMessage);

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

	// "Stuck in a tree" easter egg: the owning client detects (in Tick) when it jumps up into the lobby greenhouse
	// tree, grants the (client-local) achievement, and toasts "press O to reset". O routes here to ask the server
	// to lift the pawn back down to the ground next to the trunk (server-validated against the replicated tree set).
	void RequestResetFromTreeStuckZone();
	UFUNCTION(Server, Reliable)
	void ServerResetFromTreeStuckZone();
	// True on the owning client while the pawn is wedged up in a lobby greenhouse tree (see UpdateTreeStuckDetection).
	// Lets the O-key handler prefer the tree climb-down over the roof "return to the cabin" when both could apply.
	bool IsTreeStuckActive() const { return bTreeStuckActive; }

	// Recovery: ask the server to teleport this player back inside the train carriage when they are out on the
	// roof (PUBLIC so ABHPlayerController's context-aware O handler can route here). Local handler ->
	// ServerResetToTrainInterior; the GameMode gates it to train levels (lobby / intermission).
	void RequestResetToTrainInterior();
	UFUNCTION(Server, Reliable)
	void ServerResetToTrainInterior();

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
	// Local-only first-person legs: mirror the third-person role skeletal mesh onto an only-owner-see component with the
	// whole upper body hidden, so the owning player sees their own legs (not a camera-filling torso) when they look
	// down. Gated by ResolveFirstPersonBodyEnabled(); re-applies when the avatar mesh changes; hidden while seated / hidden.
	void UpdateFirstPersonBodyMesh();
	bool ResolveFirstPersonBodyEnabled() const;
	// Transient jumpscare-impact envelope (0..1) shared by the FOV punch and camera flinch.
	float GetJumpscareImpactEnvelope() const;
	float ComputeJumpscareFOVPunch() const;
	FRotator ComputeJumpscareCameraFlinch() const;
	// Transient "mister ke~" emote-shake envelope (0..1) + the additive camera-component LOCATION offset it drives
	// (vertical-dominant; survives bUsePawnControlRotation). Both return zero unless a shake is active. The matching
	// view-pitch wobble is exposed via GetEmoteShakePitchOffsetDeg() and applied in the camera manager.
	float GetEmoteShakeEnvelope() const;
	FVector ComputeEmoteShakeLocationOffset() const;
	bool IsReducedCameraShakeLocal() const;
	// Resolve the player's dodge-roll camera style (motion-sickness setting), honoring the cvar override and the
	// reduced-camera-shake comfort clamp. See EBHRollCamStyle.
	EBHRollCamStyle ResolveRollCamStyle() const;
	void UpdateFlashlightFeel(float DeltaSeconds);
	// Instantaneous [~0.02..0.55] beam-strength multiplier while caught in a Teacher blackout: a low, violently
	// flickering value driven by FlashlightPulseTime so the beam stutters and nearly dies.
	float ComputeBlackoutFlashlightFlicker() const;
	void TryBHopJump();
	// bLandingRoll: the call originates from Landed() (a confirmed touchdown), where the CharacterMovement mode has
	// NOT yet flipped from Falling to Walking (UE calls Landed() before SetPostLandedPhysics), so IsMovingOnGround()
	// is still false. Treat it as grounded so the drop-roll actually fires instead of being rejected/re-buffered.
	bool TryStartSpecialMoveAuthority(EBHMovementSpecialState RequestedState, bool bEndProne, bool bEndProneRequiresInput, bool bLandingRoll = false);
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
	// Social emote wheel (hold X): a radial selector of text emotes shown over your head to nearby players.
	// Open on press (the cursor snaps to the screen centre), aim a wedge by moving the mouse out from centre
	// (look is frozen while open), play it on release. The selection is the cursor's absolute offset from the
	// centre, sampled each frame by UpdateEmoteWheelSelectionFromMouse.
	void OpenEmoteWheel();
	void CloseEmoteWheelAndEmote();
	void UpdateEmoteWheelSelectionFromMouse();
	// Cosmetic third-person seated pose (no sit animation needed): hide the leg bones and drop the torso to
	// the seat so other players see a seated upper body instead of a standing-locked one.
	void ApplySeatedAvatar(bool bSeatedNow);
	void ApplyFlashlightState();
	void ApplyHiddenState();
	// --- Prop Hunt (opt-in, reversible) internals. -----------------------------------------------------------------
	// True when this player should be acting as a prop right now (a live Survivor while the GameState is in prop-hunt
	// mode). The single gate every prop-hunt input handler / RPC checks, so the keys are inert in any other mode.
	bool IsPropHuntProp() const;
	// Server-side: line-trace from the pawn's (replicated) aim for a static-mesh world actor to copy. Returns its mesh
	// path, material path, world scale and a short label. False when nothing copyable is in view.
	bool ResolvePropDisguiseTargetFromView(FString& OutMeshPath, FString& OutMaterialPath, FVector& OutScale, FString& OutLabel) const;
	// Apply the current replicated disguise state to the meshes (hide the body + show the prop, or restore the body).
	// Runs on the server and (via OnRep_PropDisguise) on every client.
	void ApplyPropDisguiseVisuals();
	// Owner-only: reparent the first-person Camera onto the 3rd-person boom (or back to the capsule eye) per
	// bPropThirdPerson, and flip the disguise mesh owner-visibility so a 3rd-person prop sees its own disguise.
	void ApplyPropCameraMode();
	// Server-side: freeze the pawn dead-still (or release it). Reuses the locker/seat movement-freeze idiom.
	void SetPropLockedAuthority(bool bNewLocked);
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

	// Point the owning client's view at the chair's facing the moment they sit (server -> owning client).
	UFUNCTION(Client, Reliable)
	void ClientFaceYaw(float Yaw);

	UFUNCTION(Server, Reliable)
	void ServerEmote(int32 InEmoteId);

	UFUNCTION(Client, Reliable)
	void ClientSpecialMoveRejected(EBHMovementSpecialState RejectedState, const FString& Reason);

	// Momentum tech: the server tells the owning client they nailed a frame-perfect chain, so the client can
	// unlock the (cosmetic) perfect_chain achievement locally and show a brief cue.
	UFUNCTION(Client, Reliable)
	void ClientNotifyPerfectChain(int32 ChainCount, EBHMovementSpecialState ChainedMove);

	UFUNCTION(Server, Reliable)
	void ServerTryCapture();

	UFUNCTION(Server, Reliable)
	void ServerUseScan();

	UFUNCTION(Server, Reliable)
	void ServerUseHunterPower();

	UFUNCTION(Server, Reliable)
	void ServerDropDecoy();

	// --- Prop Hunt (opt-in, reversible) server RPCs. The owning client resolves the prop it is looking at (it has the
	// real camera) and sends the asset paths + scale; the server validates the prop role + clamps the scale before
	// replicating. Worst-case client trust here is cosmetic (which mesh you wear), never a gameplay advantage. ---
	UFUNCTION(Server, Reliable)
	void ServerSetPropDisguise(const FString& MeshPath, const FString& MaterialPath, FVector Scale);
	UFUNCTION(Server, Reliable)
	void ServerSetPropLocked(bool bNewLocked);
	UFUNCTION(Server, Reliable)
	void ServerRotateProp(float DeltaYaw);

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

	// Prop Hunt: re-apply the disguise visuals on clients whenever any disguise field changes (mirrors
	// ABHRuntimeMeshPropActor::OnRep_PropVisuals -- all the disguise fields point here so a late field still re-applies).
	UFUNCTION()
	void OnRep_PropDisguise();

	// Prop Hunt: apply / release the dead-still freeze on clients when the lock flag changes (so the owning client's
	// predicted movement actually stops, and a captured/escaped/unlocked prop isn't left frozen).
	UFUNCTION()
	void OnRep_PropLockedInPlace();

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

	// Prop Hunt: the disguise mesh shown in place of the body while disguised. Attached to the CAPSULE (not the
	// bobbing AvatarRoot) so a hidden prop sits dead-still, and owner-no-see so the disguised player isn't blinded by
	// their own prop. No collision (visual only) -- the capsule keeps collision so the seeker can still capture them.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PropDisguiseMesh;

	// Prop Hunt 3rd-person camera boom: a spring arm the first-person Camera reparents onto while a prop is in 3rd
	// person, so they can see + position their disguise. Capsule-attached, control-rotation driven, wall-probing.
	// Unused (camera stays first-person on the capsule) for the seeker and in every other mode.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> PropCameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> RoleSkeletalMesh;

	// Only-owner-see first-person legs: a clone of RoleSkeletalMesh with the whole upper body hidden, rendered ONLY for
	// the owning player so they see their own legs looking down (others still see the full third-person RoleSkeletalMesh,
	// which stays owner-no-see). Configured in UpdateFirstPersonBodyMesh; empty/hidden when the feature is off.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonBodyMesh;

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

	// --- Catch-pressure detention hold (docs/CATCH_PRESSURE.md; gated by bh.DetentionEnabled) ------------------
	// True while this pawn is a caught survivor pinned in the detention cell awaiting rescue-or-timeout. Drives
	// the "FREE (E)" world marker, the CanInteract rescue gate, and keeps the captive frozen-but-visible.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Catch Pressure")
	bool bInDetentionHold = false;

	// Rescue hold ring 0..1 (a teammate holding E on this captive). Server-written each tick while a rescuer is
	// registered; replicated so the captive and the rescuer both see the bar climb.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Catch Pressure")
	float DetentionRescueProgress = 0.0f;

	// Single-rescuer lock (server-only): the one teammate currently holding the rescue interact on this captive.
	TWeakObjectPtr<ABHCharacter> CurrentRescuer;

	// Where this captive was teleported for the hold (server-only).
	FVector DetentionCellLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_HiddenInLocker, BlueprintReadOnly, Category = "Stealth")
	bool bHiddenInLocker;

	// --- Prop Hunt (opt-in, reversible). All disguise fields share OnRep_PropDisguise so a late-arriving field still
	// re-applies the visuals (mirrors ABHRuntimeMeshPropActor). Replicated to everyone -- the seeker must see the prop.
	UPROPERTY(ReplicatedUsing = OnRep_PropDisguise, BlueprintReadOnly, Category = "Blackout Hunt|Prop Hunt")
	bool bDisguisedAsProp = false;

	UPROPERTY(ReplicatedUsing = OnRep_PropDisguise)
	FString PropDisguiseMeshPath;

	UPROPERTY(ReplicatedUsing = OnRep_PropDisguise)
	FString PropDisguiseMaterialPath;

	UPROPERTY(ReplicatedUsing = OnRep_PropDisguise)
	FVector PropDisguiseScale = FVector::OneVector;

	UPROPERTY(ReplicatedUsing = OnRep_PropDisguise)
	float PropDisguiseYaw = 0.0f;

	// Frozen-still flag. ReplicatedUsing so the freeze/unfreeze is APPLIED on every client (incl. the owning remote
	// client, whose predicted movement must stop too), mirroring bHiddenInLocker -- not just shown on the HUD.
	UPROPERTY(ReplicatedUsing = OnRep_PropLockedInPlace, BlueprintReadOnly, Category = "Blackout Hunt|Prop Hunt")
	bool bPropLockedInPlace = false;

	// Client-local prop camera view (NOT replicated -- the camera is the local player's own). Defaults to 3rd person
	// so a prop sees its disguise the moment it transforms; the toggle key flips it, the choice is then respected.
	bool bPropThirdPerson = true;
	// Base relative Z of the disguise mesh (floor alignment), captured in ApplyPropDisguiseVisuals; the idle bob rides
	// on top of it each frame so a walking (unlocked) prop reads as alive while a locked prop sits perfectly still.
	float PropDisguiseBaseZ = 0.0f;

	// --- Prop Hunt seeker kit (P4). ---------------------------------------------------------------------------------
	// Consecutive whiffed capture swings (server-only); a landed catch resets it. Drives the escalating wrong-hit
	// self-slow via BHPropHunt::SeekerMissSlowSeconds, so flailing at furniture is punished but one honest miss
	// barely stings.
	int32 PropHuntConsecutiveMisses = 0;
	// Server-clock time (GetTeacherCaptureClockSeconds domain) the wrong-hit slow ends. Owner-only replicated so the
	// seeker's own client recomputes its move speed in lockstep with the server (everyone else never reads it).
	UPROPERTY(ReplicatedUsing = OnRep_PropHuntMissSlow)
	float PropHuntMissSlowUntilServerTime = -1000.0f;
	FTimerHandle PropHuntMissSlowExpireHandle;
	UFUNCTION()
	void OnRep_PropHuntMissSlow();
	// Escalate the miss counter + apply the timed self-slow (server). Called from the swing whiff/dodge branches.
	void ApplyPropHuntMissPenaltyAuthority();
	// Q in prop hunt: the prop SONAR -- a through-wall ping of every alive prop within bh.PropHuntScanRadius, on the
	// bh.PropHuntScanCooldown clock. Replaces the classroom heartbeat scan for the seeker (UseScanAuthority branches).
	bool UsePropHuntSonarAuthority(bool bShowFailureMessages);
public:
	// Reveal delivery: marker world-positions + how long the HUD shows them. bIsPulse marks the periodic auto reveal
	// (full-screen flash + no cooldown touch); CooldownSeconds (sonar only) mirrors the server cooldown for the HUD.
	// Public: the GameMode's reveal pulse (ForcePropHuntRevealPulse) drives it for every seeker.
	UFUNCTION(Client, Reliable)
	void ClientReceivePropHuntSonar(const TArray<FVector>& PropLocations, float RevealSeconds, bool bIsPulse, float CooldownSeconds);
protected:
	// Client-local presentation state (read by the HUD via the public getters).
	TArray<FVector> PropHuntSonarMarkers;
	float PropHuntSonarMarkersExpireClientTime = -1000.0f;
	float PropHuntSonarReadyClientTime = -1000.0f;
	float PropHuntPulseFlashClientTime = -1000.0f;

	// Server-only tutorial capture shield (see SetTutorialCaptureImmune). Not replicated -- capture is authority-decided.
	bool bTutorialCaptureImmune = false;

	UPROPERTY(ReplicatedUsing = OnRep_OutOfPlay, BlueprintReadOnly, Category = "Round")
	bool bOutOfPlay;

	// Functional first-person sit: replicated so the lowered eye height applies on the owning client
	// (read each frame in UpdateViewFeel). Movement is frozen server-side while true.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
	bool bSeated = false;

	// Set when seated via an interactable chair (ABHTrainSeat): movement input no longer stands you up; only Jump
	// does. Replicated so the owning client's "move to stand" shortcut respects the lock.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
	bool bSeatLocked = false;

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

	// Emote wheel (owning client only, never replicated): whether the radial is open and the current
	// selection stick (a point in the unit disc; +X right, +Y up). Mouse motion feeds it while open.
	bool bEmoteWheelActive = false;
	float EmoteWheelSelX = 0.0f;
	float EmoteWheelSelY = 0.0f;

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

	// Owning-client per-tick check for the "Stuck in a Tree" egg (jumped up into the lobby greenhouse canopy).
	void UpdateTreeStuckDetection(float DeltaSeconds);

	// "Stuck in a tree" easter egg (owning-client detection state): edge flag so the achievement + prompt fire once
	// per time the player jumps up into the lobby greenhouse tree, plus a timer to re-toast the "press O" hint while
	// they stay wedged. Pure client-local; the reset itself is a server RPC.
	bool bTreeStuckActive = false;
	float TreeStuckPromptTimer = 0.0f;
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
	// Server-only cooldown stamp for the O "reset to spawn" unstick (see ServerResetFromTreeStuckZone). Not
	// replicated: the server is the sole arbiter; the owning client learns the result via a status toast.
	float LastResetToSpawnServerTime = -100000.0f;
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
	// Cosmetic first-person view LEAN (degrees), updated in UpdateViewFeel and read by ABHPlayerCameraManager. See
	// GetViewLeanRollDeg. Banks the view into strafes/turns; the supporting smoothed turn-rate state backs it.
	float ViewLeanRollDeg = 0.0f;
	float SmoothedTurnRate = 0.0f;
	float LastControlYaw = 0.0f;
	bool bHasLastControlYaw = false;
	// First-person body (local owner only): whether the clone is currently shown, and the asset it was last
	// configured for, so UpdateFirstPersonBodyMesh only rebuilds (and re-hides the head/neck) when the avatar changes.
	bool bFirstPersonBodyVisible = false;
	TWeakObjectPtr<USkeletalMesh> FirstPersonBodyConfiguredAsset;
	float SmoothedBaseFOV = 0.0f;
	float JumpscareImpactStartTime = -1.0f;
	float JumpscareImpactIntensity = 0.0f;
	float JumpscareImpactFOVPunch = 0.0f;
	// "mister ke~" emote camera shake (local-only, never replicated; like the jumpscare impact above).
	float EmoteShakeStartTime = -1.0f;
	float EmoteShakeIntensity = 0.0f;
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
	// Remote-client flow-chain buffer (server-only, never replicated): a chain press from a REMOTE client arrives
	// while the previous move is still active server-side -- it was pressed during the ~half-RTT replication tail
	// where the owning client still shows the move running. Rather than drop it (the listen-server host never hits
	// this because RTT is 0), the server records the requested link here and fires it the instant the current move
	// ends (FinishSpecialMoveAuthority), so a remote player chains with the same rhythm as the host. Auto-expires.
	EBHMovementSpecialState BufferedChainMove = EBHMovementSpecialState::None;
	float BufferedChainMoveServerTime = -999.0f;
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

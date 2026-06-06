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
class UCameraComponent;
class UMaterialInstanceDynamic;
class UBHPowerupComponent;
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
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	void EnterLocker(ABHLocker* Locker);
	void ExitLocker();
	void MarkCaptured();
	void MarkEscaped();
	void RefillFlashlight(float Amount);
	void RecoverStamina(float Amount);
	void AddFear(float Amount);
	void AddDread(float Amount);
	void ApplyDetentionMark(float DurationSeconds);
	void ClearDetentionMark();
	void ResetRoleWarmupStateForRoundStart();
	void ApplyAvatarStyle();
	ABHPlayerState* GetBHPlayerState() const;
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

#if WITH_DEV_AUTOMATION_TESTS
	bool TryStartSpecialMoveForTest(EBHMovementSpecialState RequestedState, bool bEndProne);
	bool TrySetProneForTest(bool bNewProne);
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
	bool ResolveHallMonitorMarkerLocation(FVector& OutLocation) const;
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
	void UpdateFlashlightFeel(float DeltaSeconds);
	// Instantaneous [~0.02..0.55] beam-strength multiplier while caught in a Teacher blackout: a low, violently
	// flickering value driven by FlashlightPulseTime so the beam stutters and nearly dies.
	float ComputeBlackoutFlashlightFlicker() const;
	void TryBHopJump();
	bool TryStartSpecialMoveAuthority(EBHMovementSpecialState RequestedState, bool bEndProne, bool bEndProneRequiresInput);
	bool ValidateSpecialMoveSpaceAuthority(EBHMovementSpecialState RequestedState, const FBHMovementSpecialTuning& Tuning, FString& OutFailureReason) const;
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

	UPROPERTY(ReplicatedUsing = OnRep_OutOfPlay, BlueprintReadOnly, Category = "Round")
	bool bOutOfPlay;

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

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

#if WITH_DEV_AUTOMATION_TESTS
	bool TryStartSpecialMoveForTest(EBHMovementSpecialState RequestedState, bool bEndProne);
	bool TrySetProneForTest(bool bNewProne);
	float DebugGetAntiCampIdleSecondsForTest() const;
	float DebugGetAntiCampMoveBurstSecondsForTest() const { return AntiCampMoveBurstSeconds; }
	FString DebugGetInteractionFailureReasonForTest(AActor* Target) const { return GetInteractionFailureReason(Target); }
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
	void SubmitAnswer(int32 AnswerIndex);
	void SubmitAnswerOne();
	void SubmitAnswerTwo();
	void SubmitAnswerThree();
	void SubmitAnswerFour();
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
	FString GetInteractionFailureReason(AActor* Target) const;
	void SendStatusMessage(const FString& Message) const;
	void SendFakeHunterHint(bool bRealHint);
	bool ResolveHallMonitorMarkerLocation(FVector& OutLocation) const;
	bool BeginInteractAuthority(AActor* Target, bool bUseViewFallback, bool bShowFailureMessages);
	void EndInteractAuthority(AActor* Target);
	void ExitCurrentLockerAuthority();
	bool TryCaptureAuthority(bool bShowFailureMessages);
	bool UseScanAuthority(bool bShowFailureMessages);
	bool UseHunterPowerAuthority(bool bShowFailureMessages);
	bool DropDecoyAuthority(bool bShowFailureMessages);
	bool SubmitAnswerAuthority(ABHObjectiveStation* Station, int32 AnswerIndex, bool bUseViewFallback, bool bShowFailureMessages);
	void EmitFootstepStimulus(float Strength, const FString& Reason, EBHFootstepSurface Surface = EBHFootstepSurface::Default);
	EBHFootstepSurface ResolveFootstepSurface(const FHitResult* KnownGroundHit = nullptr) const;
	FBHFootstepSurfaceProfile GetFootstepSurfaceProfile(EBHFootstepSurface Surface) const;
	void BroadcastFlashlightAudioCue(bool bNewOn, bool bBatteryDied = false);
	void SendTeacherProximityAudioCue(float DistanceAlpha, bool bHunterHasSight);
	void UpdateAntiCampPressureAuthority(float DeltaSeconds, float Speed2D, const class ABHGameState* BHGS, const ABHPlayerState* BHPS);
	void ResetAntiCampTrackingAuthority();
	void UpdateViewFeel(float DeltaSeconds);
	void UpdateFlashlightFeel(float DeltaSeconds);
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

	UFUNCTION(Client, Reliable)
	void ClientSpecialMoveRejected(EBHMovementSpecialState RejectedState, const FString& Reason);

	UFUNCTION(Server, Reliable)
	void ServerTryCapture();

	UFUNCTION(Server, Reliable)
	void ServerUseScan();

	UFUNCTION(Server, Reliable)
	void ServerUseHunterPower();

	UFUNCTION(Server, Reliable)
	void ServerDropDecoy();

	UFUNCTION(Server, Reliable)
	void ServerSubmitAnswer(int32 AnswerIndex);

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

	float LastScanTime;
	float LastHunterPowerTime;
	float LastDecoyTime;
	float LastSprintNoiseTime;
	float LastFootstepStimulusTime;
	float LastFlashlightAudioCueTime;
	float LastTeacherProximityAudioTime;
	float FootstepStimulusDistanceAccumulator;
	float StaminaRecoveryLockedUntil;
	float LastStaminaWarningTime;
	float LastHidingPanicMessageTime;
	float LastLockerNoiseTime;
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
	float FlashlightPulseTime;
	float LastBHopJumpInputTime;
	float SpecialMoveStartTime;
	float SpecialMoveEndTime;
	float SpecialMoveCooldownEndTime;
	float SpecialMoveDistanceTravelled;
	FVector SpecialMoveDirection;
	float LastCaptureEvasionTime;
	float LastTeacherCounterplayHintTime;
	float DefaultCapsuleHalfHeight;
	float DefaultCapsuleRadius;
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

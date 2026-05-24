#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "GameFramework/Character.h"
#include "BHCharacter.generated.h"

class ABHLocker;
class ABHObjectiveStation;
class ABHPlayerState;
class UCameraComponent;
class UMaterialInstanceDynamic;
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
	void TryCapture();
	void UseScan();
	void UseHunterPower();
	void DropDecoy();
	void SubmitAnswer(int32 AnswerIndex);
	void SubmitAnswerOne();
	void SubmitAnswerTwo();
	void SubmitAnswerThree();
	void SubmitAnswerFour();

	bool CanAct() const;
	bool TraceForInteractable(AActor*& OutActor) const;
	bool FindInteractableFromView(AActor*& OutActor, float ExtraDistance = 0.0f) const;
	bool IsValidInteractionTarget(AActor* Target) const;
	FString GetInteractionFailureReason(AActor* Target) const;
	void SendStatusMessage(const FString& Message) const;
	void SendFakeHunterHint(bool bRealHint);
	bool BeginInteractAuthority(AActor* Target, bool bUseViewFallback, bool bShowFailureMessages);
	void EndInteractAuthority(AActor* Target);
	void ExitCurrentLockerAuthority();
	bool TryCaptureAuthority(bool bShowFailureMessages);
	bool UseScanAuthority(bool bShowFailureMessages);
	bool UseHunterPowerAuthority(bool bShowFailureMessages);
	bool DropDecoyAuthority(bool bShowFailureMessages);
	bool SubmitAnswerAuthority(ABHObjectiveStation* Station, int32 AnswerIndex, bool bUseViewFallback, bool bShowFailureMessages);
	void UpdateViewFeel(float DeltaSeconds);
	void UpdateFlashlightFeel(float DeltaSeconds);
	void TryBHopJump();
	void ApplyFlashlightState();
	void ApplyHiddenState();
	void ConfigureLowPolyAvatar();
	void UpdateLowPolyAvatar(float DeltaSeconds);
	void UpdateRoleSkeletalAnimation(float Speed2D, float MoveAlpha, float SprintAlpha, bool bGrounded);
	void ApplyRoleModelVisuals(const ABHPlayerState* BHPS, const FLinearColor& ShirtColor, const FLinearColor& SkinColor);
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
	void ServerTryCapture();

	UFUNCTION(Server, Reliable)
	void ServerUseScan();

	UFUNCTION(Server, Reliable)
	void ServerUseHunterPower();

	UFUNCTION(Server, Reliable)
	void ServerDropDecoy();

	UFUNCTION(Server, Reliable)
	void ServerSubmitAnswer(int32 AnswerIndex);

	UFUNCTION(Client, Reliable)
	void ClientReceiveScanResult(const FVector& TargetLocation, bool bTargetHidden, bool bFoundTarget);

	UFUNCTION()
	void OnRep_FlashlightOn();

	UFUNCTION()
	void OnRep_HiddenInLocker();

	UFUNCTION()
	void OnRep_OutOfPlay();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Flashlight;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horror")
	float ScanCooldownSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Horror")
	float DecoyCooldownSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float StaminaDrainPerSecond;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float StaminaRecoveryPerSecond;

	UPROPERTY(ReplicatedUsing = OnRep_FlashlightOn, BlueprintReadOnly, Category = "Horror")
	bool bFlashlightOn;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Horror")
	float FlashlightBattery;

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
	float LastStaminaWarningTime;
	float LastHidingPanicMessageTime;
	float LastLockerNoiseTime;
	float LastForcedBreathNoiseTime;
	float LastDetentionNoiseTime;
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
	bool bKeyboardLookLeft;
	bool bKeyboardLookRight;
	bool bKeyboardLookUp;
	bool bKeyboardLookDown;
	bool bBHopJumpHeld;
	bool bUsingRoleModel;
	FName LastRoleAnimationName;
	int32 LastAppliedAvatarIndex;
	EBHPlayerRole LastAppliedAvatarRole;
	FLinearColor LastAppliedAvatarColor;
	int32 LastAppliedAvatarHeadwearIndex;
	int32 LastAppliedAvatarGearIndex;
};

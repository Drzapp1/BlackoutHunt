#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "GameFramework/Actor.h"
#include "BHJumpscareMonster.generated.h"

class ABHCharacter;
class UAnimSequence;
class UChildActorComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class USoundBase;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;

UCLASS()
class BLACKOUTHUNT_API ABHJumpscareMonster : public AActor
{
	GENERATED_BODY()

public:
	ABHJumpscareMonster();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(ABHCharacter* NewTarget, float NewSpeed = 2200.0f, float NewLifetime = 5.0f, float NewHoldSeconds = 0.0f, bool bNewPlayChargeEffects = true);
	void ConfigureScriptedPath(const TArray<FVector>& NewPathPoints, float NewSpeed = 3200.0f, float NewLifetime = 2.0f, AActor* NewLookAtTarget = nullptr, bool bNewFaceLookAtTarget = false, bool bNewPlayChargeEffects = true);
	void ConfigurePresentation(USkeletalMesh* NewSkeletalMesh, UAnimSequence* NewRunAnimation, UStaticMesh* NewStaticMesh, UMaterialInterface* NewMaterial, USoundBase* NewLaunchSound, const FLinearColor& NewLightColor, float NewFocusHeight);
	void ConfigureVariant(const FBHJumpscareVariant& NewVariant);
	void ConfigureCloseupPresentation(const FBHJumpscareVariant& NewVariant, float NewLifetime = 1.35f, bool bUpperBodyOnly = true);
	// Passive "corner peek": the entity stands at a wall edge facing the target, plays no charge, and
	// retreats (destroys) when the player looks straight at it or after LingerSeconds. Builds tension
	// without a contact jumpscare.
	void ConfigurePeek(ABHCharacter* NewTarget, const FBHJumpscareVariant& NewVariant, float LingerSeconds = 2.6f);
	float GetCameraFocusHeight() const { return CameraFocusHeight; }
	FName GetJumpscareVariantId() const { return JumpscareVariantId; }
	// When set, the homing charge moves in full 3D toward the target (used for the ceiling-drop approach).
	void SetChargeDescend(bool bNewDescend) { bChargeDescend = bNewDescend; }

protected:
	virtual void BeginPlay() override;

	void ApplyVisuals();
	void ApplyConfiguredPresentation();
	void ApplyConfiguredVariant();
	void UseProxyFallbackVisual();
	void SetProxyPartsVisible(bool bVisible);
	void SetCloseupUpperBodyOnly();
	void TriggerContactJumpscare();
	void TickScriptedPath(float DeltaSeconds);
	FVector GetEffectiveVariantVisualScale() const;
	FVector FitVisualScaleToTargetHeight(const FVector& RequestedScale, float UnscaledHeight) const;
	void ApplyVisualRootScale();

	UFUNCTION()
	void OnRep_JumpscareVariantId();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MonsterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SkeletalMonsterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Chest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftLeg;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightLeg;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftEye;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightEye;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mouth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> EyeLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> CoreLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UChildActorComponent> VariantVisualActor;

	void StartChargeEffects();
	void SpawnLaunchScream();

	// Reduced-flash comfort: mutes the eye/core light pulse oscillation for a local player with
	// Reduced flash enabled. Client-side only; cached once at BeginPlay (monster is short-lived).
	float ComputeLocalReducedFlashScale() const;

	// Comfort / "safe mode": true if the local viewer has Reduced Jumpscares enabled, in which case this
	// monster renders as the abstract proxy instead of an imported photoreal creature. Client-side, per-player.
	bool LocalViewerWantsProxyJumpscare() const;

	TWeakObjectPtr<ABHCharacter> Target;
	float ChargeSpeed;
	float MaxLifetime;
	float HoldSeconds;
	float SpawnTime;
	bool bChargeStarted;
	bool bContactJumpscareTriggered;
	bool bUseScriptedPath;
	bool bPeekMode;
	bool bChargeDescend;
	bool bPlayChargeEffects;
	bool bScriptedFaceLookAtTarget;
	bool bScriptedPlayChargeEffects;
	int32 ScriptedPathIndex;
	bool bUsingImportedVisual;
	bool bCloseupPresentation;
	// When a close-up only frames the upper body (e.g. the behind-you payoff), the whole mesh is fit to a
	// short target so the visible torso fills the screen. When it shows the full creature (the in-your-face
	// slam, the super-chain finale), it is fit to a taller target so the complete monster looms instead of
	// reading as a stumpy legless torso.
	bool bCloseupUpperBodyOnly = true;
	float CloseupStartTime;
	float CameraFocusHeight;
	TWeakObjectPtr<AActor> ScriptedLookAtTarget;
	TArray<FVector> ScriptedPathPoints;
	FLinearColor PresentationLightColor;
	FVector PresentationVisualOffset;
	FRotator PresentationVisualRotation;
	FVector PresentationVisualScale;
	FVector BaseVisualRootScale;

	// 1.0 = full pulse; <1.0 attenuates the strobe for a local Reduced-flash player.
	float CachedReducedFlashScale = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_JumpscareVariantId)
	FName JumpscareVariantId;

	UPROPERTY(Transient)
	FBHJumpscareVariant ActiveVariant;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LaunchSound;
};

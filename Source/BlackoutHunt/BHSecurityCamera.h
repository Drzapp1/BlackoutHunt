#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "BHSecurityCamera.generated.h"

class ABHCharacter;
class ABHCCTVZone;
class UPointLightComponent;
class USceneComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

UCLASS()
class BLACKOUTHUNT_API ABHSecurityCamera : public AActor
{
	GENERATED_BODY()

public:
	ABHSecurityCamera();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ConfigureCamera(float NewRange, float NewConeDegrees, int32 NewCircuitId);
	void SetCameraDisabled(bool bNewDisabled);
	void ResetCCTVState();
	void TriggerManualGlitch(ABHCharacter* InstigatorCharacter);
	bool CanSeeSurvivor(const ABHCharacter* Survivor) const;
	bool TryTriggerZoneAlert(ABHCharacter* Survivor, AActor* ZoneActor, const FString& AlertLabel);
	UTextureRenderTarget2D* GetOrCreateLiveFeedTarget(int32 Resolution);
	void SetLiveFeedEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Security")
	int32 GetCircuitId() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Security")
	float GetDetectionRange() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Security")
	float GetDetectionConeDegrees() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Security")
	bool IsCameraDisabled() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Security")
	bool IsGlitching() const;

protected:
	UFUNCTION()
	void OnRep_CameraState();

	void TriggerGlitch(ABHCharacter* Target, const FString& Reason);
	void ApplyCameraVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Lens;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneCaptureComponent2D> FeedCapture;

	UPROPERTY(ReplicatedUsing = OnRep_CameraState, EditAnywhere, Category = "Security")
	bool bDisabled;

	UPROPERTY(ReplicatedUsing = OnRep_CameraState, BlueprintReadOnly, Category = "Security")
	bool bGlitching;

	UPROPERTY(Replicated, EditAnywhere, Category = "Security")
	int32 CircuitId;

	UPROPERTY(Replicated, EditAnywhere, Category = "Security")
	float DetectionRange;

	UPROPERTY(Replicated, EditAnywhere, Category = "Security")
	float DetectionConeDegrees;

	float LastDetectionTime;
	float LastGlitchTime;
	float LastZoneAlertTime;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> FeedRenderTarget;

	FTimerHandle GlitchTimerHandle;
};

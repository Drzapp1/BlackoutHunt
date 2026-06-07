// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTrainServiceLight.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * A small ceiling "service light" module for the subway intermission/lobby car. Many of these are spaced
 * frequently down each carriage to give a comfortable, flashlight-free baseline ("half on, but not perfect").
 *
 * Boot sequence (purely client-side and deterministic from BeginPlay, so every viewer sees it without any
 * replicated state): the module first powers up on RED "backup power" for a few seconds, then ramps to a warm,
 * relaxing "mains power" white so passengers can settle in. The interior modules use the class-default red
 * boot; the per-player ROOF lights (spawned client-locally by the breaker) call SetRoofModeImmediate() to skip
 * the red phase and fade straight up to a cooler service white.
 */
UCLASS()
class BLACKOUTHUNT_API ABHTrainServiceLight : public AActor
{
	GENERATED_BODY()

public:
	ABHTrainServiceLight();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Tunes the fixture for the per-player roof lights: skip the red "backup" phase and fade straight up to a
	// cooler service white. Call right after a client-local spawn (these lights are never replicated).
	void SetRoofModeImmediate();

protected:
	void ApplyLightLevel(const FLinearColor& Color, float Intensity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FixtureMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FixtureMaterial;

	// Look/boot tuning (plain members; interior modules use these defaults, roof lights override locally).
	FLinearColor StartupColor;   // "backup power" colour shown during the boot delay
	FLinearColor PoweredColor;   // comfortable "mains power" colour after boot
	float StartupIntensity;
	float PoweredIntensity;
	float BootDelaySeconds;      // how long to sit on backup power before ramping up
	float BootFadeSeconds;       // ramp duration from backup -> mains
	bool bFlickerDuringStartup;

	float BootStartTime;
	bool bBootComplete;
};

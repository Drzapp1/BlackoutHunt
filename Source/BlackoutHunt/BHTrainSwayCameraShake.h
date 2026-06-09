// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "BHTrainSwayCameraShake.generated.h"

/**
 * Continuous, looping sway pattern that sells "you are riding a moving subway car": a dominant side-to-side
 * roll/rock, a gentle vertical bob over the rails, small pitch/yaw drift, and a faint high-frequency engine
 * rumble. A slow, non-repeating envelope makes the motion swell and ease ("rougher track at times") instead of
 * reading as a perfectly periodic loop. Amplitudes are authored for the exposed ROOF ride at ShakeScale = 1; the
 * calmer interior ride plays the same shake at a lower ShakeScale set live by ABHPlayerController.
 *
 * Self-contained in the Engine module (subclasses UCameraShakePattern directly and manages its own
 * FCameraShakeState, mirroring USimpleCameraShakePattern) so it needs no EngineCameras plugin dependency.
 */
UCLASS()
class BLACKOUTHUNT_API UBHTrainSwayCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UBHTrainSwayCameraShakePattern(const FObjectInitializer& ObjectInitializer);

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

	// Seconds the shake has been running; drives every oscillator term.
	float ElapsedSeconds = 0.0f;

	// Occasional "rough rail joint / points crossing" JOLT layered over the continuous sway: a brief, sharp,
	// damped lurch every ~17-36s (sometimes a quick cluster) so the ride feels like real track instead of a
	// constant hum. Scheduled and shaped entirely in UpdateShakePatternImpl; it rides the same ShakeScale
	// (interior/roof/reduced comfort) and blend as the base sway, so no separate plumbing is needed.
	float NextJoltTime = 8.0f;        // ElapsedSeconds at which the next jolt fires
	float JoltStartTime = -1.0f;      // start time of the active jolt, or < 0 when idle
	float JoltDuration = 0.0f;        // length of the active jolt
	float JoltStrength = 0.0f;        // per-jolt magnitude (some bumps notably bigger than others)
	float JoltLateralSign = 1.0f;     // which way the car lurches this time (+/-1)
	int32 PendingQuickBumps = 0;      // queued quick follow-up bumps in the current cluster

	// Lifecycle/blend bookkeeping, mirroring USimpleCameraShakePattern (kept local to avoid the plugin dep).
	FCameraShakeState State;
};

/**
 * The playable shake wrapper. Plays a single looping instance whose intensity (interior vs. roof, comfort
 * scaling) is driven live through UCameraShakeBase::ShakeScale by ABHPlayerController::TickTrainMotion.
 */
UCLASS()
class BLACKOUTHUNT_API UBHTrainSwayCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UBHTrainSwayCameraShake(const FObjectInitializer& ObjectInitializer);
};

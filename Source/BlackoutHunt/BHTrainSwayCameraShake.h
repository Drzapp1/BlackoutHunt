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

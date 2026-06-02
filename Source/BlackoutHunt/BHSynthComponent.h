// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "BHSynthComponent.generated.h"

UCLASS(ClassGroup = Audio, meta = (BlueprintSpawnableComponent))
class BLACKOUTHUNT_API UBHSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UBHSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

	void Configure(float InBaseFrequency, float InVolume, float InNoiseAmount, float InPulseSpeed);

	// Switches the synth to an organic "person nearby" voice: alternating footstep thumps at a walking
	// cadence plus a slow inhale/exhale breath bed. Used by the noise decoy so a thrown decoy sounds like
	// a fleeing student instead of an electronic ping. InStepsPerSecond is the footstep rate (~1.6-2.2 is
	// a brisk walk/jog); InVolume is the master gain.
	void ConfigureDecoyFootsteps(float InStepsPerSecond, float InVolume);

protected:
	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float BaseFrequency;

	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float Volume;

	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float NoiseAmount;

	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float PulseSpeed;

	float Phase;
	float PulsePhase;
	int32 CachedSampleRate;
	uint32 NoiseSeed;

	// Organic footstep/breathing voice (see ConfigureDecoyFootsteps). bDecoyFootstepMode gates the whole
	// branch in OnGenerateAudio; the rest is the per-sample running state for the step envelope and breath bed.
	bool bDecoyFootstepMode;
	float DecoyStepInterval;
	float DecoyStepClock;
	float DecoyStepEnvelope;
	float DecoyThumpPhase;
	float DecoyBreathPhase;
	float DecoyBreathLowpass;
	int32 DecoyStepIndex;
};

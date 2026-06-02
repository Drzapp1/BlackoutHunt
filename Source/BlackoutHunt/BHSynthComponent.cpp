// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHSynthComponent.h"

UBHSynthComponent::UBHSynthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	BaseFrequency = 55.0f;
	Volume = 0.12f;
	NoiseAmount = 0.02f;
	PulseSpeed = 0.22f;
	Phase = 0.0f;
	PulsePhase = 0.0f;
	CachedSampleRate = 48000;
	NoiseSeed = 0x12345678;
	bDecoyFootstepMode = false;
	DecoyStepInterval = 0.55f;
	DecoyStepClock = 0.0f;
	DecoyStepEnvelope = 0.0f;
	DecoyThumpPhase = 0.0f;
	DecoyBreathPhase = 0.0f;
	DecoyBreathLowpass = 0.0f;
	DecoyStepIndex = 0;
	bAutoDestroy = false;
	bStopWhenOwnerDestroyed = true;
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.FalloffDistance = 1800.0f;
	AttenuationOverrides.AttenuationShapeExtents = FVector(450.0f);
	SetVolumeMultiplier(1.0f);
}

bool UBHSynthComponent::Init(int32& SampleRate)
{
	CachedSampleRate = FMath::Max(8000, SampleRate);
	return true;
}

int32 UBHSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	if (!OutAudio || NumSamples <= 0)
	{
		return 0;
	}

	const float TwoPi = 2.0f * PI;
	const float SafeSampleRate = static_cast<float>(FMath::Max(8000, CachedSampleRate));

	if (bDecoyFootstepMode)
	{
		// Each step fires a short, fast-decaying low thump (the heel hit) plus a noise transient (the scuff),
		// alternating pitch left/right foot. Underneath sits a slow filtered-noise breath bed that swells and
		// fades on a ~3.6s inhale/exhale cycle. Entirely procedural so it needs no audio assets to ship.
		const float StepDecayPerSample = FMath::Exp(-1.0f / (0.035f * SafeSampleRate));
		const float SafeStepInterval = FMath::Max(0.18f, DecoyStepInterval);
		const float BreathStep = TwoPi * 0.28f / SafeSampleRate; // ~3.6s breathing cycle
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			NoiseSeed = NoiseSeed * 1664525u + 1013904223u;
			const float Noise = (static_cast<float>((NoiseSeed >> 8) & 0xFFFFu) / 32767.5f) - 1.0f;

			DecoyStepClock += 1.0f / SafeSampleRate;
			if (DecoyStepClock >= SafeStepInterval)
			{
				DecoyStepClock -= SafeStepInterval;
				DecoyStepEnvelope = 1.0f;
				DecoyThumpPhase = 0.0f;
				++DecoyStepIndex;
			}

			const float ThumpFreq = (DecoyStepIndex & 1) ? 84.0f : 72.0f;
			DecoyThumpPhase += TwoPi * ThumpFreq / SafeSampleRate;
			if (DecoyThumpPhase > TwoPi)
			{
				DecoyThumpPhase -= TwoPi;
			}
			const float Thump = FMath::Sin(DecoyThumpPhase) * DecoyStepEnvelope;
			// Scuff lives only in the first instants of the step (envelope squared sharpens it to a transient).
			const float Scuff = Noise * DecoyStepEnvelope * DecoyStepEnvelope * 0.55f;
			DecoyStepEnvelope *= StepDecayPerSample;

			DecoyBreathPhase += BreathStep;
			if (DecoyBreathPhase > TwoPi)
			{
				DecoyBreathPhase -= TwoPi;
			}
			const float BreathLFO = 0.5f + 0.5f * FMath::Sin(DecoyBreathPhase);
			DecoyBreathLowpass += (Noise - DecoyBreathLowpass) * 0.045f; // one-pole lowpass for a breathy hiss
			const float Breath = DecoyBreathLowpass * BreathLFO * BreathLFO * 0.5f;

			OutAudio[SampleIndex] = FMath::Clamp((Thump * 0.95f + Scuff + Breath) * Volume, -1.0f, 1.0f);
		}
		return NumSamples;
	}

	const float PhaseStep = TwoPi * FMath::Max(1.0f, BaseFrequency) / SafeSampleRate;
	const float PulseStep = TwoPi * FMath::Max(0.01f, PulseSpeed) / SafeSampleRate;
	const bool bScreamMode = BaseFrequency >= 700.0f && NoiseAmount >= 0.5f;

	for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		NoiseSeed = NoiseSeed * 1664525u + 1013904223u;
		const float Noise = (static_cast<float>((NoiseSeed >> 8) & 0xFFFFu) / 32767.5f) - 1.0f;
		const float Pulse = 0.55f + 0.45f * (0.5f + 0.5f * FMath::Sin(PulsePhase));
		const float Tone = FMath::Sin(Phase) * 0.65f + FMath::Sin(Phase * 0.51f) * 0.25f;
		if (bScreamMode)
		{
			const float Vibrato = FMath::Sin(PulsePhase * 5.0f) * 2.1f + FMath::Sin(PulsePhase * 13.0f) * 0.9f;
			const float HarshTone =
				FMath::Sin(Phase + Vibrato) * 0.42f +
				FMath::Sin(Phase * 1.97f + Vibrato * 0.65f) * 0.28f +
				FMath::Sin(Phase * 2.71f) * 0.18f;
			const float Gate = 0.82f + 0.18f * FMath::Abs(FMath::Sin(PulsePhase * 2.3f));
			OutAudio[SampleIndex] = FMath::Clamp((HarshTone + Noise * NoiseAmount * 0.78f) * Volume * Gate, -1.0f, 1.0f);
		}
		else
		{
			OutAudio[SampleIndex] = FMath::Clamp((Tone * Pulse + Noise * NoiseAmount) * Volume, -1.0f, 1.0f);
		}

		Phase += PhaseStep;
		PulsePhase += PulseStep;
		if (Phase > TwoPi)
		{
			Phase -= TwoPi;
		}
		if (PulsePhase > TwoPi)
		{
			PulsePhase -= TwoPi;
		}
	}

	return NumSamples;
}

void UBHSynthComponent::Configure(float InBaseFrequency, float InVolume, float InNoiseAmount, float InPulseSpeed)
{
	BaseFrequency = InBaseFrequency;
	Volume = InVolume;
	NoiseAmount = InNoiseAmount;
	PulseSpeed = InPulseSpeed;
	bDecoyFootstepMode = false;
}

void UBHSynthComponent::ConfigureDecoyFootsteps(float InStepsPerSecond, float InVolume)
{
	bDecoyFootstepMode = true;
	DecoyStepInterval = 1.0f / FMath::Max(0.5f, InStepsPerSecond);
	Volume = FMath::Max(0.0f, InVolume);
	// Start the cadence ready to fire a step almost immediately so the lure reads the instant it lands.
	DecoyStepClock = DecoyStepInterval;
	DecoyStepEnvelope = 0.0f;
	DecoyThumpPhase = 0.0f;
	DecoyBreathPhase = 0.0f;
	DecoyBreathLowpass = 0.0f;
	DecoyStepIndex = 0;
}

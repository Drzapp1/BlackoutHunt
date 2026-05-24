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
			OutAudio[SampleIndex] = (Tone * Pulse + Noise * NoiseAmount) * Volume;
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
}

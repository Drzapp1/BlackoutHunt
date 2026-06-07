// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHTrainSwayCameraShake.h"

namespace
{
	// One oscillator term: Amplitude * sin(2*PI * Frequency * t + Phase).
	FORCEINLINE float BHSwaySine(float TimeSeconds, float Frequency, float Phase, float Amplitude)
	{
		return Amplitude * FMath::Sin(TimeSeconds * Frequency * 2.0f * PI + Phase);
	}
}

UBHTrainSwayCameraShakePattern::UBHTrainSwayCameraShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBHTrainSwayCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	// Plays for as long as the train is moving; ABHPlayerController stops it (with a blend-out) when it parks.
	OutInfo.Duration = FCameraShakeDuration::Infinite();
	OutInfo.BlendIn = 0.85f;
	OutInfo.BlendOut = 1.10f;
}

void UBHTrainSwayCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	if (!Params.bIsRestarting)
	{
		ElapsedSeconds = 0.0f;
	}
	State.Start(this, Params);
}

void UBHTrainSwayCameraShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	ElapsedSeconds += Params.DeltaTime;
	const float T = ElapsedSeconds;

	// "Rougher track at times": a slow, non-repeating envelope so the ride breathes instead of looping audibly.
	// Two incommensurate low frequencies keep it in roughly [0.52, 1.08].
	const float Envelope = 0.80f
		+ BHSwaySine(T, 0.050f, 0.0f, 0.18f)
		+ BHSwaySine(T, 0.123f, 2.0f, 0.10f);

	// Dominant side-to-side rock, gentle nod, slow yaw drift, plus a faint fast pitch "rumble" (degrees).
	FRotator Rot = FRotator::ZeroRotator;
	Rot.Roll = BHSwaySine(T, 0.90f, 0.0f, 1.15f) + BHSwaySine(T, 1.70f, 1.3f, 0.42f);
	Rot.Pitch = BHSwaySine(T, 1.45f, 0.7f, 0.40f) + BHSwaySine(T, 6.40f, 0.0f, 0.11f);
	Rot.Yaw = BHSwaySine(T, 0.65f, 2.1f, 0.27f);

	// Suspension bob + rail-clack texture, lateral sway, and a faint fore/aft surge (cm, camera-local space).
	FVector Loc = FVector::ZeroVector;
	Loc.Z = BHSwaySine(T, 2.00f, 0.0f, 0.85f) + BHSwaySine(T, 5.30f, 0.9f, 0.24f);
	Loc.Y = BHSwaySine(T, 0.80f, 1.7f, 0.55f);
	Loc.X = BHSwaySine(T, 1.10f, 0.0f, 0.20f);

	OutResult.Rotation = Rot * Envelope;
	OutResult.Location = Loc * Envelope;

	// Blend in/out weight; the base class then auto-scales by the live ShakeScale (interior vs. roof, comfort).
	const float BlendWeight = State.Update(Params.DeltaTime);
	OutResult.ApplyScale(BlendWeight);
}

bool UBHTrainSwayCameraShakePattern::IsFinishedImpl() const
{
	return !State.IsPlaying();
}

void UBHTrainSwayCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	State.Stop(Params.bImmediately);
}

void UBHTrainSwayCameraShakePattern::TeardownShakePatternImpl()
{
	State = FCameraShakeState();
}

UBHTrainSwayCameraShake::UBHTrainSwayCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Only ever one train-sway shake on the camera at a time; a second Play just restarts this one.
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UBHTrainSwayCameraShakePattern>(TEXT("RootShakePattern")));
}

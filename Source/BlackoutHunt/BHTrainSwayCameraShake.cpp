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
		// First jolt only after a long calm stretch; later ones are rescheduled as each jolt ends.
		NextJoltTime = FMath::FRandRange(15.0f, 30.0f);
		JoltStartTime = -1.0f;
		PendingQuickBumps = 0;
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

	// ---- Occasional JOLT: a brief, sharp lurch (rough rail joint / points crossing) every ~17-36s so the ride
	// reads as real track, not a perfectly steady hum. A damped impulse -- one big swing that rings down in ~1s --
	// authored at ShakeScale 1 like the sway. Added AFTER the envelope so the slow "breathing" dial doesn't soften
	// it; it still rides the shared ShakeScale + blend below, so interior/roof/reduced-camera-shake comfort scale
	// it for free and the sway-off toggle (which never starts this shake) suppresses it too.
	FRotator JoltRot = FRotator::ZeroRotator;
	FVector JoltLoc = FVector::ZeroVector;
	if (JoltStartTime < 0.0f && T >= NextJoltTime)
	{
		// Fire a fresh jolt: randomise length, lurch direction and strength (occasionally a notably bigger one).
		JoltStartTime = T;
		JoltDuration = FMath::FRandRange(0.70f, 1.00f);
		const bool bBigOne = FMath::FRand() < 0.12f;
		JoltStrength = bBigOne ? FMath::FRandRange(0.75f, 1.00f) : FMath::FRandRange(0.40f, 0.70f);
		JoltLateralSign = (FMath::FRand() < 0.5f) ? -1.0f : 1.0f;
	}
	if (JoltStartTime >= 0.0f)
	{
		const float Jt = T - JoltStartTime;
		if (Jt >= JoltDuration)
		{
			// Jolt over. Real track throws bumps in quick clusters (a switch/crossover) then long calm stretches,
			// so sometimes queue a fast follow-up bump; otherwise wait out a long, smooth gap.
			JoltStartTime = -1.0f;
			if (PendingQuickBumps > 0)
			{
				--PendingQuickBumps;
				NextJoltTime = T + FMath::FRandRange(0.45f, 0.90f);
			}
			else if (FMath::FRand() < 0.12f)
			{
				PendingQuickBumps = 1;
				NextJoltTime = T + FMath::FRandRange(0.45f, 0.90f);
			}
			else
			{
				NextJoltTime = T + FMath::FRandRange(40.0f, 80.0f);
			}
		}
		else
		{
			// Damped-impulse shape per axis: a dominant lateral lurch + an opposite roll rock, a sharp vertical
			// rail-joint thump, plus small fore/aft surge, pitch nod and yaw hunt -- all ringing down quickly.
			const float S = JoltStrength;
			JoltLoc.Y = JoltLateralSign * 2.8f * FMath::Exp(-Jt * 4.5f) * FMath::Sin(Jt * 1.40f * 2.0f * PI) * S;
			JoltLoc.Z = 3.2f * FMath::Exp(-Jt * 7.0f) * FMath::Sin(Jt * 3.00f * 2.0f * PI + 0.40f) * S;
			JoltLoc.X = 1.6f * FMath::Exp(-Jt * 5.0f) * FMath::Sin(Jt * 1.10f * 2.0f * PI + 0.20f) * S;
			JoltRot.Roll = -JoltLateralSign * 2.4f * FMath::Exp(-Jt * 4.0f) * FMath::Sin(Jt * 1.25f * 2.0f * PI) * S;
			JoltRot.Pitch = 0.9f * FMath::Exp(-Jt * 6.0f) * FMath::Sin(Jt * 2.60f * 2.0f * PI + 0.50f) * S;
			JoltRot.Yaw = JoltLateralSign * 0.75f * FMath::Exp(-Jt * 4.5f) * FMath::Sin(Jt * 1.00f * 2.0f * PI) * S;
		}
	}

	OutResult.Rotation = Rot * Envelope + JoltRot;
	OutResult.Location = Loc * Envelope + JoltLoc;

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

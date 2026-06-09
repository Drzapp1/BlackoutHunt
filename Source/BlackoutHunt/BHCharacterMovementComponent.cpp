// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHCharacterMovementComponent.h"

#include "BHCharacter.h"

// CS2/Source air-strafe tuning. These live in CalcVelocity, which runs identically on the predicting client and the
// authoritative server, so the values should match across machines; in shipping they would be locked/replicated, but
// for tuning they are console-exposed. See docs/ROADMAP_MOVEMENT_REVISION.md (WS1).
static TAutoConsoleVariable<int32> CVarBHAirStrafe(
	TEXT("bh.AirStrafe"),
	1,
	TEXT("1 (default) = CS2-style air-strafing (build speed by syncing mouse + strafe in the air); 0 = stock UE air control."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHAirAccelerate(
	TEXT("bh.AirAccelerate"),
	120.0f,
	TEXT("Air acceleration aggressiveness (Source 'airaccelerate'). Higher = easier to gain speed per strafe. (Default 120)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHAirSpeedCap(
	TEXT("bh.AirSpeedCap"),
	30.0f,
	TEXT("THE skill knob: per-tick cap (cm/s) on how much velocity-along-the-wish-direction can be added in the air. Low = harder, more CS2-like (you must aim across your velocity to gain). (Default 30)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHMaxAirSpeedMult(
	TEXT("bh.MaxAirSpeedMult"),
	1.6f,
	TEXT("Hard ceiling on horizontal air speed as a multiple of the role's current max ground speed -- 'faster than sprint, rewarding, but not broken'. Clamped to >= 1. (Default 1.6)"),
	ECVF_Default);

void UBHCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Only intercept the airborne case (and only when the feature is on). Grounded movement, swimming, etc. keep the
	// stock behaviour. PhysFalling zeroes Velocity.Z before calling us and restores+gravitates it after, so here we
	// only ever touch the horizontal (X/Y) plane and leave Z alone.
	// Also defer to the stock path when: animation root motion owns the displacement, or the character is in a transient
	// special move (roll/slide/dive drive position via AddActorWorldOffset and suppress input -- the air-strafe must not
	// fight that motion or stomp the exit velocity those moves re-inject). Input is already gated during a special move
	// so Acceleration is ~0, but this makes the intent explicit and frame-order-proof.
	const ABHCharacter* BHOwner = Cast<ABHCharacter>(GetCharacterOwner());
	if (DeltaTime <= 0.0f || !IsFalling() || CVarBHAirStrafe.GetValueOnGameThread() == 0
		|| HasAnimRootMotion() || (BHOwner && BHOwner->IsSpecialMoveActive()))
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}

	const FVector WishDir = Acceleration.GetSafeNormal2D();
	if (WishDir.IsNearlyZero())
	{
		// No directional input in the air: defer to the stock path. With the project's low FallingLateralFriction this
		// preserves existing horizontal momentum (so a clean jump keeps its speed), it just won't BUILD any.
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}

	// Source/CS2 AirAccelerate: project current velocity onto the wish-direction; only the component you are NOT yet
	// moving toward (up to the air-speed cap) can be added. Aiming straight ahead (aligned with velocity) yields no
	// gain; aiming across your velocity (mouse + strafe) yields up to the cap -- that asymmetry is the skill ceiling.
	const float MaxGroundSpeed = FMath::Max(1.0f, GetMaxSpeed());
	const float WishSpeed = MaxGroundSpeed;
	const float AirCap = FMath::Max(0.0f, CVarBHAirSpeedCap.GetValueOnGameThread());
	const float AirAccel = FMath::Max(0.0f, CVarBHAirAccelerate.GetValueOnGameThread());

	FVector Vel2D(Velocity.X, Velocity.Y, 0.0f);
	const float CurrentSpeedAlongWish = FVector::DotProduct(Vel2D, WishDir);
	const float AddSpeed = FMath::Min(WishSpeed, AirCap) - CurrentSpeedAlongWish;
	if (AddSpeed > 0.0f)
	{
		const float AccelSpeed = FMath::Min(AirAccel * WishSpeed * DeltaTime, AddSpeed);
		Vel2D += AccelSpeed * WishDir;
	}

	// Hard ceiling so a master can exceed sprint (rewarding) without it running away.
	const float MaxAirSpeed = MaxGroundSpeed * FMath::Max(1.0f, CVarBHMaxAirSpeedMult.GetValueOnGameThread());
	if (Vel2D.SizeSquared() > MaxAirSpeed * MaxAirSpeed)
	{
		Vel2D = Vel2D.GetSafeNormal() * MaxAirSpeed;
	}

	Velocity.X = Vel2D.X;
	Velocity.Y = Vel2D.Y;
	// Velocity.Z deliberately untouched -- PhysFalling restores it and applies gravity after this returns.
}

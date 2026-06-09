// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BHCharacterMovementComponent.generated.h"

// Custom movement component that adds CS2/Source-style AIR-STRAFING on top of the stock character movement: while
// airborne with directional input, the player builds speed by aiming the wish-direction across their velocity
// (mouse + strafe sync). A tight per-tick air-speed cap (bh.AirSpeedCap) is the skill knob -- you only gain speed
// when your velocity is NOT already aligned with the wish-direction -- and a hard ceiling (bh.MaxAirSpeedMult x
// sprint) keeps it rewarding but not broken. The logic lives in CalcVelocity (the predicted + server-authoritative
// movement path) and reads only the replicated Acceleration input, so it stays correct under client prediction.
UCLASS()
class BLACKOUTHUNT_API UBHCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
};

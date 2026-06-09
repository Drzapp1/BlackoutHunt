// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainSeat.generated.h"

// An INVISIBLE interaction volume placed on existing train seating (wall benches, lounge benches, stools). The
// bench/stool geometry is the visual; this just answers the look-and-tap interaction: press E to drop into the
// seated pose (teleport onto the seat, turn to face the aisle) and lock there until you press Jump to stand. The
// box is sized a touch larger than the seat so the view trace hits it before the seat's static block, and the
// seated facing is stored separately (the actor itself stays axis-aligned so the box matches the bench).
UCLASS()
class BLACKOUTHUNT_API ABHTrainSeat : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainSeat();

	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	// Spawn-time setup: the interaction box scale (matched to the seat it covers) and the yaw the sitter faces.
	void ConfigureSeat(const FVector& InBoxScale, float InFacingYaw);

protected:
	// Yaw (deg) the sitter's view is turned to when they sit (toward the aisle / table).
	UPROPERTY(EditAnywhere, Category = "Seat")
	float SeatFacingYaw = 0.0f;

	// Capsule-centre rise above the actor's floor-level placement when seated (~ standing capsule half-height, so
	// the seated pose -- camera + torso drop -- lands the avatar on the cushion).
	UPROPERTY(EditAnywhere, Category = "Seat")
	float SeatCapsuleRiseZ = 90.0f;
};

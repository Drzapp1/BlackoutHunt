// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainRoofHatch.generated.h"

// A hidden "maintenance hatch" tucked in a corner of the subway intermission car. Interacting with it lifts the
// passenger up onto the train roof (a cosmetic easter egg) and awards the "roof_rider" achievement. The
// intermission advances on its own timer, so a roof visit never affects boarding, scoring, or round flow.
UCLASS()
class BLACKOUTHUNT_API ABHTrainRoofHatch : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainRoofHatch();

	virtual void BeginPlay() override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;

	// Sets the world location the passenger is lifted to (a spot on the train roof). Call right after spawn.
	void ConfigureHatch(const FVector& InRoofTeleportLocation);

protected:
	UPROPERTY()
	FVector RoofTeleportLocation = FVector::ZeroVector;
};

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainRoofParkourGate.generated.h"

// The finish marker for the rooftop parkour course. It sits on the final (highest) platform that can only be
// reached by clearing the climb, so interacting with it means the course was completed. Doing so grants the
// owning player the cosmetic "roof_parkour" achievement (which also awards its XP) -- once per session. Purely
// cosmetic; nothing here touches scoring, capture/escape, or fairness.
UCLASS()
class BLACKOUTHUNT_API ABHTrainRoofParkourGate : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainRoofParkourGate();

	virtual void BeginPlay() override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
};

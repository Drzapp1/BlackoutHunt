// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainRoofBreaker.generated.h"

// A hidden electrical "breaker" tucked away on the train roof. Interacting with it toggles the PER-PLAYER roof
// service lights: because each passenger controls their own lighting (client-local lights, no shared world
// state), one player throwing the breaker brightens the roof only in their own view. The breaker is deliberately
// tucked out of the way and a bit hard to spot; pressing the N "node marker" key points the player to it.
UCLASS()
class BLACKOUTHUNT_API ABHTrainRoofBreaker : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainRoofBreaker();

	virtual void BeginPlay() override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
};

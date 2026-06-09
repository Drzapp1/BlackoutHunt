// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainDanceFloor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

// A light-up disco dance floor for the karaoke/social car: a grid of tiles that cycle colours over time
// (rainbow sweep / checker flash / sparkle). Tap it to change the pattern (replicated). Animation is purely
// time-driven and local, so no per-tile replication is needed.
UCLASS()
class BLACKOUTHUNT_API ABHTrainDanceFloor : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainDanceFloor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	void BuildTiles();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> FloorGlow;

	UPROPERTY(Replicated)
	uint8 Pattern = 0;   // 0 rainbow sweep, 1 checker flash, 2 sparkle
};

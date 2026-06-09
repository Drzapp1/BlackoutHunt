// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainSecretSwitch.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

// A tucked-away easter-egg switch. Find it and flip it to kick off a brief "party": a colour-cycling disco
// beacon here and a celebratory toast to everyone aboard. Has a cooldown so it can't be spammed.
UCLASS()
class BLACKOUTHUNT_API ABHTrainSecretSwitch : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainSecretSwitch();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	void ApplyVisuals();
	float GetServerTimeSeconds() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Lever;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Beacon;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> BeaconLight;

	UPROPERTY(Replicated)
	float PartyEndServerTime = 0.0f;

	// Server-only cooldown gate.
	float NextAllowedTime = 0.0f;
};

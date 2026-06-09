// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainAquarium.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

// A decorative aquarium for the relax cars: a lit glass tank with fish that drift back and forth (purely
// cosmetic, time-driven so every client sees roughly the same motion - no replication needed). Tapping the
// glass makes the fish dart for a moment (a tiny easter egg).
UCLASS()
class BLACKOUTHUNT_API ABHTrainAquarium : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainAquarium();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Stand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Water;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Glass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Fish;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> TankLight;

	// Replicated server-time of the last glass-tap, so the "fish dart" effect plays on every client off the
	// synchronized world clock (not just the listen-server host).
	UPROPERTY(Replicated)
	float StartleStartServerTime = -100.0f;
};

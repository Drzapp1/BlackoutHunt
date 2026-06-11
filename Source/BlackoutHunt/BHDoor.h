// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHDoor.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHDoor : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHDoor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

	void SetOpen(bool bNewOpen);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsOpen() const;

protected:
	UFUNCTION()
	void OnRep_Open();

	void ApplyDoorState();

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen;

	FRotator ClosedRotation;

	// OnRep_Open can fire before BeginPlay for a dynamically-replicated door; this gate defers the door
	// visual until BeginPlay has captured the true closed pose (see ABHDoor::BeginPlay / OnRep_Open).
	bool bClosedRotationCaptured = false;

	// Server time of the last accepted CLOSE (slam). Throttles slams in the style of the gate/power/
	// security anti-grief controls; opens are not throttled (see BeginInteract). Negative sentinel so
	// the first slam is always allowed.
	float LastCloseServerTime = -100.0f;
};

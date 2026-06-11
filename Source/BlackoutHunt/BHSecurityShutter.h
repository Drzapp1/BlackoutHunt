// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHSecurityShutter.generated.h"

class UStaticMeshComponent;

UCLASS()
class BLACKOUTHUNT_API ABHSecurityShutter : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHSecurityShutter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(int32 NewCircuitId);
	void SetOpen(bool bNewOpen);
	bool TrySetOpen(bool bNewOpen);
	int32 GetCircuitId() const;
	bool IsOpen() const;

	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	UFUNCTION()
	void OnRep_Open();

	FVector GetTargetMeshLocation() const;
	bool CanCloseWithoutBlockingPawn() const;
	void ApplyOpenState(bool bSnapMesh = false);
	void ApplyCollisionState();
	void ApplyStatusVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftRail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightRail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeaderPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WarningStripe;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StatusLight;

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen;

	UPROPERTY(Replicated)
	int32 CircuitId;

	UPROPERTY(EditDefaultsOnly, Category = "Security")
	float OpenLiftHeight;

	UPROPERTY(EditDefaultsOnly, Category = "Security")
	float OpenInterpSpeed;

	FVector ClosedMeshLocation;

	// Server time of the last accepted toggle. Throttles toggles so a griefing student can't thrash the
	// shutter or repeatedly blind a whole CCTV circuit for everyone. Negative sentinel = first allowed.
	float LastToggleServerTime = -100.0f;
};

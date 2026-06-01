#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHSlidingGate.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHSlidingGate : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHSlidingGate();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

	void SetOpen(bool bNewOpen);

protected:
	UFUNCTION()
	void OnRep_Open();

	void ApplyGateState();

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen;

	FVector ClosedMeshLocation;
	float OpenLiftHeight;

	// Server time of the last accepted toggle. Throttles raising/lowering so a griefing student can't
	// flap a gate (blocking a doorway / making noise) for everyone. Negative sentinel = first is free.
	float LastToggleServerTime = -100.0f;
};

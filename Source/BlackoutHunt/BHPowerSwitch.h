#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHPowerSwitch.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHPowerSwitch : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHPowerSwitch();

	void Configure(int32 NewCircuitId, const FText& NewLabel);

	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SwitchPlate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SwitchLever;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CircuitLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WarningLabel;

	UPROPERTY(EditAnywhere, Category = "Power")
	int32 CircuitId;

	UPROPERTY(EditAnywhere, Category = "Power")
	FText SwitchLabel;

	// Server time of the last accepted toggle. Throttles toggles so a griefing student can't strobe a
	// whole light circuit for all 32 players. Negative sentinel so the first toggle is always allowed.
	float LastToggleServerTime = -100.0f;
};

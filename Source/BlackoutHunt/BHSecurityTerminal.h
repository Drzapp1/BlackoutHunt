#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHSecurityTerminal.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHSecurityTerminal : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHSecurityTerminal();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Configure(int32 NewCircuitId, const FText& NewLabel);

	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	bool CircuitHasClosedShutter() const;
	void RefreshCircuitVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TerminalScreen;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> KeypadBackplate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> KeypadButtons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> AccessLight;

	UPROPERTY(EditAnywhere, Category = "Security")
	int32 CircuitId;

	UPROPERTY(EditAnywhere, Category = "Security")
	FText TerminalLabel;

	bool bHasCachedCircuitState;
	bool bCachedCircuitWillOpen;

	// Server time of the last accepted toggle. Throttles toggles so a griefing student can't thrash the
	// shutters or repeatedly blind a whole CCTV circuit for everyone. Negative sentinel = first allowed.
	float LastToggleServerTime = -100.0f;
};

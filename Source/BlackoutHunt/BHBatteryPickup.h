#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHBatteryPickup.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHBatteryPickup : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHBatteryPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	UFUNCTION()
	void OnRep_Consumed();

	void ApplyConsumedState();
	void ApplyBatteryVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PositiveCap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> NegativeCap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PositiveTerminal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LabelBand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChargeLight;

	UPROPERTY(ReplicatedUsing = OnRep_Consumed)
	bool bConsumed;

	UPROPERTY(EditDefaultsOnly, Category = "Battery")
	float RefillAmount;
};

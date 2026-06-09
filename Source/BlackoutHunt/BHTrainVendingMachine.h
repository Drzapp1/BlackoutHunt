// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainVendingMachine.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

// A snack/drink vending machine for the lounge cars. TAP to grab a random free snack: a can drops into the
// tray and you get a little toast. Pure comfort/flavour, cosmetic only.
UCLASS()
class BLACKOUTHUNT_API ABHTrainVendingMachine : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainVendingMachine();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

protected:
	float GetServerTimeSeconds() const;
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Glass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Products;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TrayItem;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(ReplicatedUsing = OnRep_Dispense)
	int32 LastItemIndex = -1;

	UPROPERTY(ReplicatedUsing = OnRep_Dispense)
	float DispenseServerTime = 0.0f;

	UFUNCTION()
	void OnRep_Dispense() {}

	// Local: index the tray can was last tinted to, so we only re-tint when the dispensed item changes.
	int32 CachedTintIndex = -1;
};

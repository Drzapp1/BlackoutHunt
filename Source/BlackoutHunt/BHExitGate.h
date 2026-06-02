// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHExitGate.generated.h"

class UPointLightComponent;

UCLASS()
class BLACKOUTHUNT_API ABHExitGate : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHExitGate();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void SetDirectorActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsDirectorActive() const;

protected:
	// Color-codes the gate state (inactive / locked / escape-ready) so survivors read it from a
	// distance instead of having to walk up and trigger the interaction prompt.
	void ApplyExitGateVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Exit")
	bool bDirectorActive;

	// 0 inactive, 1 locked, 2 escape-ready; -1 forces the first tint. Cached so the dynamic material
	// is only rebuilt on an actual state change while the status light keeps pulsing every frame.
	int32 LastVisualState = -1;
};

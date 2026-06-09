// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainStopCord.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

// A subway "stop request" pull-cord by the doors: tap E to toggle the lit STOP REQUESTED sign. The state is
// replicated so the whole carriage sees it light up (and who is asking to stop) -- a small, social train-flavour
// toy. The sign glows and gently pulses while requested.
UCLASS()
class BLACKOUTHUNT_API ABHTrainStopCord : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainStopCord();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	UFUNCTION()
	void OnRep_Requested();
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Cord;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Sign;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> SignLight;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SignMID;

	UPROPERTY(ReplicatedUsing = OnRep_Requested)
	bool bRequested = false;

	float PulseTime = 0.0f;
};

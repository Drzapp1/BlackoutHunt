// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainGhost.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

// An easter-egg "ghost passenger": every few minutes it silently materialises somewhere near its home spot,
// lingers a few seconds, then fades away. Server picks when/where (replicated); clients fade it in/out locally
// via scale + a pale emissive glow. Non-interactable - pure atmosphere.
UCLASS()
class BLACKOUTHUNT_API ABHTrainGhost : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainGhost();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;

protected:
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Robe;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Aura;

	// Plain replicated flag; Tick polls it every 0.05s to drive the fade, so no RepNotify is needed.
	UPROPERTY(Replicated)
	bool bShowing = false;

	FVector HomeLocation = FVector::ZeroVector;
	bool bHomeSet = false;
	float NextEventTime = 0.0f;
	float Alpha = 0.0f;   // local fade 0..1
};

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHCollectable.generated.h"

// A hidden relic the player can find and collect. Server-authoritative: on collect it hides itself (replicated)
// and asks the owning client to record the relic in its (client-local) account via ABHCharacter::ClientRecordCollectable.
// Always-on (not easter-egg-gated). Visuals are built from primitive meshes, so it needs no content assets.
UCLASS()
class BLACKOUTHUNT_API ABHCollectable : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHCollectable();

	// Stable per-site id (e.g. "facility_0"). Persisted in the account's found set, so it MUST be stable across
	// rounds/sessions (do not key it on the round seed). Set right after spawn. Server-only -- never read by clients.
	void Configure(FName InCollectableId);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;

protected:
	UFUNCTION()
	void OnRep_Collected();

	void ApplyCollectedState();
	void ApplyCollectableVisuals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Gem;

	// Stable id; server-side only (the find is recorded via a client RPC, so clients never need this replicated).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectable")
	FName CollectableId;

	UPROPERTY(ReplicatedUsing = OnRep_Collected)
	bool bCollected;
};

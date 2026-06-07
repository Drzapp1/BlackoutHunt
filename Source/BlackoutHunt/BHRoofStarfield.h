// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHRoofStarfield.generated.h"

class UInstancedStaticMeshComponent;

// Cosmetic stargazing dome for the train roof: a field of emissive "stars" high overhead plus a warm string of
// festoon "fairy lights" along the roof edge. Everything is built deterministically in BeginPlay from a fixed
// seed, so every client renders the identical sky with NO replicated state -- only the actor's existence needs
// to reach clients. Unlit/emissive, shadowless, non-colliding; pure atmosphere. Spawn one at world (0,0,~316).
UCLASS()
class BLACKOUTHUNT_API ABHRoofStarfield : public AActor
{
	GENERATED_BODY()

public:
	ABHRoofStarfield();

	virtual void BeginPlay() override;

protected:
	void BuildStars();
	void BuildFairyLights();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// One instanced-mesh component for the whole starfield (cheap: 1 draw, N instances, shared emissive material).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Stars;

	// One instanced-mesh component for the warm festoon "fairy light" beads along the roof edge.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> FairyLights;

	bool bBuilt = false;
};

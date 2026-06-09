// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTrainHandStrap.generated.h"

class USceneComponent;
class UStaticMeshComponent;

// A row of overhead hand-holds (subway "strap-hangers") that pendulum-sway as the carriage rides, so the train
// reads as actually moving. Purely cosmetic: each strap is a pivot at the rail with a hanging loop that gently
// swings on two incommensurate sines (a slow lateral rock + a faint fore/aft nod), staggered per strap. One actor
// manages a whole car's row; ConfigureRow sets the count / spread.
UCLASS()
class BLACKOUTHUNT_API ABHTrainHandStrap : public AActor
{
	GENERATED_BODY()

public:
	ABHTrainHandStrap();

	virtual void Tick(float DeltaSeconds) override;

	// Spawn-time: how many straps and the half-span (cm) they spread over along the actor's X.
	void ConfigureRow(int32 InCount, float InHalfSpanX);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<USceneComponent>> Pivots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Straps;

	float ElapsedSeconds = 0.0f;
};

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHNoiseDecoy.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UBHSynthComponent;

UCLASS()
class BLACKOUTHUNT_API ABHNoiseDecoy : public AActor
{
	GENERATED_BODY()

public:
	ABHNoiseDecoy();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SpeakerFace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Antenna;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SignalLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> NoiseRadius;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBHSynthComponent> Synth;

	// Server-only: fires a realistic survivor noise (footstep/breathing) into the perception system on a
	// repeating timer so the Teacher and bots chase the decoy as if a real student were moving here. The
	// emitted reason deliberately never says "decoy" -- that is the whole point of the rework.
	void EmitDecoyNoise();

	float AgeSeconds;

	FTimerHandle NoiseTimerHandle;
	int32 NoisePingIndex;
};

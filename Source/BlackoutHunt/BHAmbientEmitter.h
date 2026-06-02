// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHAmbientEmitter.generated.h"

class UBHSynthComponent;

UCLASS()
class BLACKOUTHUNT_API ABHAmbientEmitter : public AActor
{
	GENERATED_BODY()

public:
	ABHAmbientEmitter();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(float Frequency, float Volume, float Noise, float Pulse);

protected:
	UFUNCTION()
	void OnRep_AudioConfig();

	void ApplyAudioConfig();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBHSynthComponent> Synth;

	UPROPERTY(ReplicatedUsing = OnRep_AudioConfig)
	float ConfigFrequency;

	UPROPERTY(ReplicatedUsing = OnRep_AudioConfig)
	float ConfigVolume;

	UPROPERTY(ReplicatedUsing = OnRep_AudioConfig)
	float ConfigNoise;

	UPROPERTY(ReplicatedUsing = OnRep_AudioConfig)
	float ConfigPulse;
};

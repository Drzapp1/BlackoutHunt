#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "BHSynthComponent.generated.h"

UCLASS(ClassGroup = Audio, meta = (BlueprintSpawnableComponent))
class BLACKOUTHUNT_API UBHSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UBHSynthComponent(const FObjectInitializer& ObjectInitializer);

	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

	void Configure(float InBaseFrequency, float InVolume, float InNoiseAmount, float InPulseSpeed);

protected:
	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float BaseFrequency;

	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float Volume;

	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float NoiseAmount;

	UPROPERTY(EditAnywhere, Category = "Blackout Hunt Audio")
	float PulseSpeed;

	float Phase;
	float PulsePhase;
	int32 CachedSampleRate;
	uint32 NoiseSeed;
};

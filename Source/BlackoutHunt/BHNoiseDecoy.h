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

	float AgeSeconds;
};

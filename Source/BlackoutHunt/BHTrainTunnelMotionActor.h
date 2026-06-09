// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTrainTunnelMotionActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

UCLASS()
class BLACKOUTHUNT_API ABHTrainTunnelMotionActor : public AActor
{
	GENERATED_BODY()

public:
	ABHTrainTunnelMotionActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetMoving(bool bNewMoving);
	void ConfigureMotion(float NewLoopLength, float NewSpeed);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Far layer: horizontal light bars on the tunnel wall.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> LightStrips;

	// Near layer: vertical support pillars that sweep past FASTER than the light bars, so the windows read as a
	// real rushing tunnel (parallax sells the motion).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Pillars;

	// A couple of MOVABLE point lights that sweep along the window outside, blooming as they pass the panes and
	// fading at the ends -- so the carriage interior gets a moving wash of light, reading as lit things (lamps,
	// platforms, passing trains) going by outside rather than a static glow. Unshadowed + short radius for cost;
	// only lit while the train is moving.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UPointLightComponent>> PassingLights;

	// Cached dynamic materials (created once in BeginPlay, not per tick).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> StripMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PillarMaterials;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Train")
	bool bMoving;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float LoopLength;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	float MotionSpeed;

	float MotionOffset;
	float PillarOffset;
};

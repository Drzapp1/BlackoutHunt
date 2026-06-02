// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Components/ActorComponent.h"
#include "BHBlockActor.h"
#include "BHFootstepSurfaceComponent.generated.h"

UCLASS(ClassGroup = (BlackoutHunt), meta = (BlueprintSpawnableComponent))
class BLACKOUTHUNT_API UBHFootstepSurfaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBHFootstepSurfaceComponent();

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Footsteps")
	EBHFootstepSurface GetFootstepSurface() const;

	UFUNCTION(BlueprintCallable, Category = "Blackout Hunt|Footsteps")
	void SetFootstepSurface(EBHFootstepSurface NewSurface);

	static EBHFootstepSurface SurfaceForBlockMaterial(EBHBlockMaterial Material);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	EBHFootstepSurface FootstepSurface;
};

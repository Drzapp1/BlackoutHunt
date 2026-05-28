#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "GameFramework/Actor.h"
#include "BHFootstepSurfaceVolume.generated.h"

class UBoxComponent;
class UBHFootstepSurfaceComponent;

UCLASS()
class BLACKOUTHUNT_API ABHFootstepSurfaceVolume : public AActor
{
	GENERATED_BODY()

public:
	ABHFootstepSurfaceVolume();

	void ConfigureSurfaceVolume(EBHFootstepSurface Surface, const FVector& Extent);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Footsteps")
	EBHFootstepSurface GetFootstepSurface() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBHFootstepSurfaceComponent> SurfaceComponent;
};

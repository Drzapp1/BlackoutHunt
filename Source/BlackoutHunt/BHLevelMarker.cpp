#include "BHLevelMarker.h"

#include "Components/SceneComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BHLevelMarker)

ABHLevelMarker::ABHLevelMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	// Pure metadata marker: never relevant to clients and should not block gameplay traces.
	bReplicates = false;
	SetCanBeDamaged(false);
}

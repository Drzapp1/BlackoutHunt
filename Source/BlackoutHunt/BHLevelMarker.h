// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTypes.h"
#include "BHLevelMarker.generated.h"

// Drop a single ABHLevelMarker into an authored .umap (e.g. /Game/BlackoutHunt/Maps/Facility) to tell
// ABHGameMode that the level is hand-authored. When the game mode finds one of these at BeginPlay it
// discovers the gameplay actors already placed in the level instead of running the runtime block
// generator. Maps without a marker keep using the procedural generator exactly as before, so this is
// inert for the current /Engine/Maps/Entry runtime levels.
//
// See Docs/FACILITY_VERTICAL_SLICE.md ("Authored Map Pipeline") for the authoring workflow.

// One crawl-space gate baked into an authored map. The export commandlet records the transform + box
// extent of every ABHCrawlSpaceVolume the generator placed, so ABHGameMode::DiscoverAuthoredLevel can
// rebuild the gates if a later hand-edit of the .umap (replacing blockout with meshes) drops the volume
// actors. Without this, an authored map keeps the low crawl GEOMETRY (which only blocks standing pawns)
// but loses the role gate, letting a prone Teacher follow survivors through.
USTRUCT()
struct FBHAuthoredCrawlGate
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector Extent = FVector(320.0f, 125.0f, 155.0f);
};

UCLASS()
class BLACKOUTHUNT_API ABHLevelMarker : public AActor
{
	GENERATED_BODY()

public:
	ABHLevelMarker();

	// Logical Blackout Hunt level this authored map represents (Facility / Substation / Foggrounds).
	// Used for reporting/telemetry and to sanity-check the BHLevel travel option; it does not change
	// which actors are discovered.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Level")
	FString LevelName = TEXT("Facility");

	// Fog preset applied when an authored level is loaded without an explicit BHFogPreset travel option.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Level")
	EBHFogPreset DefaultFogPreset = EBHFogPreset::Heavy;

	// Stage index (0..2) this authored map represents when no BHStageIndex travel option is supplied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Level", meta = (ClampMin = "0", ClampMax = "2"))
	int32 StageIndex = 0;

	// When true the game mode still runs the runtime navigation rebuild after discovering placed actors.
	// Leave enabled until the authored map ships its own baked NavMeshBoundsVolume + RecastNavMesh.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blackout Hunt|Level")
	bool bRebuildRuntimeNavigation = true;

	// Crawl-space gates recorded at bake time (see FBHAuthoredCrawlGate). DiscoverAuthoredLevel rebuilds
	// these only when the loaded map has no ABHCrawlSpaceVolume of its own, so an intact bake is untouched.
	UPROPERTY(EditAnywhere, Category = "Blackout Hunt|Level")
	TArray<FBHAuthoredCrawlGate> CrawlGates;
};

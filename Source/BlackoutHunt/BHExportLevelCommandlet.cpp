// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHExportLevelCommandlet.h"

#if WITH_EDITOR
#include "BHCrawlSpaceVolume.h"
#include "BHGameMode.h"
#include "BHLevelMarker.h"
#include "Editor.h"
// UEditorLoadingAndSavingUtils and FEditorFileUtils both live in FileHelpers.h in UE 5.7 (there is no
// separate EditorLoadingAndSavingUtils.h header).
#include "FileHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Brush.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BHExportLevelCommandlet)

UBHExportLevelCommandlet::UBHExportLevelCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Seeds an authored BlackoutHunt level .umap (Facility/Substation/Foggrounds) from the runtime generator.");
	HelpUsage = TEXT("BlackoutHuntEditor-Cmd.exe BlackoutHunt.uproject -run=BHExportLevel -Level=Facility");
}

int32 UBHExportLevelCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogTemp, Error, TEXT("BHExportLevel requires an editor target."));
	return 1;
#else
	FString LevelName;
	if (!FParse::Value(*Params, TEXT("Level="), LevelName) || LevelName.IsEmpty())
	{
		LevelName = TEXT("Facility");
		UE_LOG(LogTemp, Warning, TEXT("[BHExportLevel] No -Level= supplied; defaulting to Facility."));
	}

	int32 StageIndex = 0;
	if (LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		LevelName = TEXT("Substation");
		StageIndex = 1;
	}
	else if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		LevelName = TEXT("Foggrounds");
		StageIndex = 2;
	}
	else if (LevelName.Equals(TEXT("Facility"), ESearchCase::IgnoreCase))
	{
		LevelName = TEXT("Facility");
		StageIndex = 0;
	}
	else if (LevelName.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase))
	{
		LevelName = TEXT("Tutorial");
		StageIndex = 0;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BHExportLevel] Invalid -Level=%s. Use Facility, Substation, Foggrounds, or Tutorial."), *LevelName);
		return 1;
	}

	if (!GEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHExportLevel] GEditor is null; this commandlet must run with an editor build."));
		return 1;
	}

	// Fresh blank editor world we can spawn into and save as a brand-new /Game map.
	UWorld* World = GEditor->NewMap();
	if (!World || !World->PersistentLevel)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHExportLevel] Failed to create a blank editor world."));
		return 1;
	}
	// GEditor->NewMap() already clears the level filename for us (it calls the editor's internal
	// ResetLevelFilenames() after emptying the map), so SaveMap below writes the new package rather
	// than overwriting any previously loaded on-disk map.

	// Spawn the game mode transient so it drives the build but is NOT serialized into the saved .umap
	// (the real game mode is supplied by the game mode override at runtime, not placed in the level).
	FActorSpawnParameters GameModeParams;
	GameModeParams.ObjectFlags |= RF_Transient;
	GameModeParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>(GameModeParams);
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHExportLevel] Failed to spawn ABHGameMode."));
		return 1;
	}

	GameMode->BuildLevelForExport(LevelName);

	// --- Uniform layout enlargement (authored bake only) ---
	// The runtime generator's Facility plays cramped. Scale the whole built layout outward about the origin
	// so rooms are roomier, while doors / stations / real-mesh props keep their real size (=> less cramped,
	// not just "zoomed in"). Uniform scaling preserves every relative position, so it is topology-safe: walls
	// stay connected, spawns stay inside their rooms, crawl gates stay in their openings. Cube blockout actors
	// (walls / floor / signs / console pixels) additionally get their XY footprint scaled, so all cube-built
	// geometry grows perfectly proportionally (no distortion). Heights (Z) are never scaled, so ceilings stay
	// put and rooms get wider/longer rather than taller. Done before the crawl-gate recording below so the
	// marker captures the scaled gate positions.
	// Uniform room-scaling is intentionally OFF: enlarging the rooms themselves killed the close-quarters
	// dread. "Bigger" is delivered instead by MORE rooms/areas in the generator (backrooms-style), with
	// rooms kept tight. Left here as a knob in case a gentle global scale is ever wanted.
	const float LayoutScale = 1.0f;
	if (!FMath::IsNearlyEqual(LayoutScale, 1.0f))
	{
		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ABrush* DefaultBrush = World->GetDefaultBrush();
		int32 ScaledActors = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || Actor == GameMode || Actor == DefaultBrush || !Actor->GetRootComponent()
				|| Actor->IsA(AWorldSettings::StaticClass()))
			{
				continue;
			}
			const FVector Loc = Actor->GetActorLocation();
			Actor->SetActorLocation(FVector(Loc.X * LayoutScale, Loc.Y * LayoutScale, Loc.Z));
			if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor))
			{
				if (UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent())
				{
					if (CubeMesh && MeshComp->GetStaticMesh() == CubeMesh)
					{
						const FVector ActorScale = Actor->GetActorScale3D();
						Actor->SetActorScale3D(FVector(ActorScale.X * LayoutScale, ActorScale.Y * LayoutScale, ActorScale.Z));
					}
				}
			}
			++ScaledActors;
		}
		UE_LOG(LogTemp, Display, TEXT("[BHExportLevel] Enlarged %s layout x%.2f about origin (%d actors scaled; heights and real-mesh prop sizes unchanged)."), *LevelName, LayoutScale, ScaledActors);
	}

	// One authored-level marker so ABHGameMode::DiscoverAuthoredLevel() treats the saved map as authored.
	FActorSpawnParameters MarkerParams;
	MarkerParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABHLevelMarker* Marker = World->SpawnActor<ABHLevelMarker>(FVector::ZeroVector, FRotator::ZeroRotator, MarkerParams);
	if (!Marker)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHExportLevel] Failed to spawn ABHLevelMarker."));
		return 1;
	}
	Marker->LevelName = LevelName;
	Marker->StageIndex = StageIndex;

	// Record every crawl-space gate the generator placed into the marker. The volume actors themselves are
	// also saved into the .umap, but replacing blockout with meshes in-editor can delete them; the marker
	// copy lets DiscoverAuthoredLevel rebuild the gates so a prone Teacher can never follow survivors
	// through. Coordinates are world-space because the generator already placed the volumes in world space.
	for (TActorIterator<ABHCrawlSpaceVolume> It(World); It; ++It)
	{
		ABHCrawlSpaceVolume* CrawlVolume = *It;
		if (!CrawlVolume)
		{
			continue;
		}
		FBHAuthoredCrawlGate Gate;
		Gate.Location = CrawlVolume->GetActorLocation();
		Gate.Rotation = CrawlVolume->GetActorRotation();
		Gate.Extent = CrawlVolume->GetConfiguredExtent();
		Marker->CrawlGates.Add(Gate);
	}

	UE_LOG(LogTemp, Display, TEXT("[BHExportLevel] Built %s: persistent level holds %d actors before save (%d crawl gates recorded on the marker)."),
		*LevelName, World->PersistentLevel->Actors.Num(), Marker->CrawlGates.Num());

	// Some BlackoutHunt actors (e.g. ABHBreakableGlassPane, ABHBlockActor) create a UMaterialInstanceDynamic
	// in their constructor, which parents the MID to the class-default object. SaveMap rejects that as an
	// "Illegal reference to private object". The runtime never saves these actors so it never trips, but the
	// authored-export bake does. Replace every dynamic-material override with its parent material so the
	// package serializes cleanly; the runtime re-creates the MIDs at BeginPlay, so visuals are unchanged.
	int32 StrippedMIDs = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (!Primitive)
			{
				continue;
			}
			const int32 NumMaterials = Primitive->GetNumMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
			{
				if (UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Primitive->GetMaterial(MaterialIndex)))
				{
					Primitive->SetMaterial(MaterialIndex, DynamicMaterial->Parent);
					++StrippedMIDs;
				}
			}
		}
	}
	if (StrippedMIDs > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[BHExportLevel] Stripped %d dynamic-material override(s) before save (runtime re-creates them at BeginPlay)."), StrippedMIDs);
	}

	if (UPackage* Package = World->GetOutermost())
	{
		Package->MarkPackageDirty();
	}

	const FString AssetPath = FString::Printf(TEXT("/Game/BlackoutHunt/Maps/%s"), *LevelName);
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, AssetPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[BHExportLevel] Failed to save %s."), *AssetPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[BHExportLevel] Saved authored level seed %s. Replace blocks with meshes, add baked lighting + NavMeshBoundsVolume, then set the marker's bRebuildRuntimeNavigation = false. Crawl-space gates are recorded on the marker, so the role gate survives even if the BHCrawlSpaceVolume actors are deleted while remeshing."), *AssetPath);
	return 0;
#endif
}

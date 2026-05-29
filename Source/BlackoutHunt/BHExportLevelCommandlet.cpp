#include "BHExportLevelCommandlet.h"

#if WITH_EDITOR
#include "BHGameMode.h"
#include "BHLevelMarker.h"
#include "Editor.h"
// UEditorLoadingAndSavingUtils and FEditorFileUtils both live in FileHelpers.h in UE 5.7 (there is no
// separate EditorLoadingAndSavingUtils.h header).
#include "FileHelpers.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BHExportLevel] Invalid -Level=%s. Use Facility, Substation, or Foggrounds."), *LevelName);
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

	UE_LOG(LogTemp, Display, TEXT("[BHExportLevel] Built %s: persistent level holds %d actors before save."),
		*LevelName, World->PersistentLevel->Actors.Num());

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

	UE_LOG(LogTemp, Display, TEXT("[BHExportLevel] Saved authored level seed %s. Replace blocks with meshes, add baked lighting + NavMeshBoundsVolume, then set the marker's bRebuildRuntimeNavigation = false."), *AssetPath);
	return 0;
#endif
}

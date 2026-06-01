#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationTestWorld.h"
#include "BHBreaker.h"
#include "BHCrawlSpaceVolume.h"
#include "BHExitGate.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHLevelMarker.h"
#include "BHObjectiveStation.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"

// Verifies the authored-level discovery contract: a placed ABHLevelMarker flips the game mode to discover
// the gameplay actors already in the level (instead of running the runtime generator), and a PlayerStart
// tagged "Hunter" becomes the teacher spawn while the rest become survivor spawns.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAuthoredLevelDiscoveryTest,
	"BlackoutHunt.Level.AuthoredDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAuthoredLevelDiscoveryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntAuthoredLevel"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (!GameMode)
	{
		return false;
	}

	// No marker yet: discovery declines and the runtime generator stays in charge.
	TestFalse(TEXT("Discovery returns false when no ABHLevelMarker is present."), GameMode->DebugDiscoverAuthoredLevelForTest());

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABHLevelMarker* Marker = World->SpawnActor<ABHLevelMarker>(FVector(10.0f, 20.0f, 30.0f), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Level marker spawns."), Marker);
	if (!Marker)
	{
		return false;
	}
	// Keep the test off the runtime navigation rebuild (no nav system in this lightweight world).
	Marker->bRebuildRuntimeNavigation = false;
	Marker->LevelName = TEXT("Facility");

	World->SpawnActor<ABHBreaker>(FVector(100.0f, 0.0f, 95.0f), FRotator::ZeroRotator, SpawnParams);
	World->SpawnActor<ABHBreaker>(FVector(200.0f, 0.0f, 95.0f), FRotator::ZeroRotator, SpawnParams);
	World->SpawnActor<ABHObjectiveStation>(FVector(300.0f, 0.0f, 95.0f), FRotator::ZeroRotator, SpawnParams);
	World->SpawnActor<ABHExitGate>(FVector(400.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams);

	World->SpawnActor<APlayerStart>(FVector(500.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams);
	World->SpawnActor<APlayerStart>(FVector(560.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams);

	const FVector HunterStartLocation(900.0f, 100.0f, 120.0f);
	APlayerStart* HunterStart = World->SpawnActor<APlayerStart>(HunterStartLocation, FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Hunter player start spawns."), HunterStart);
	if (HunterStart)
	{
		HunterStart->PlayerStartTag = FName(TEXT("Hunter"));
	}

	// With a marker present, discovery takes over and collects the placed actors.
	TestTrue(TEXT("Discovery returns true once a marker is present."), GameMode->DebugDiscoverAuthoredLevelForTest());
	TestEqual(TEXT("Both placed breakers are discovered."), GameMode->DebugGetBreakerCountForTest(), 2);
	TestEqual(TEXT("The placed objective station is discovered."), GameMode->DebugGetObjectiveStationCountForTest(), 1);
	TestEqual(TEXT("The placed exit gate is discovered."), GameMode->DebugGetExitGateCountForTest(), 1);
	TestEqual(TEXT("Untagged player starts become survivor spawns."), GameMode->DebugGetSurvivorSpawnCountForTest(), 2);
	TestEqual(TEXT("The Hunter-tagged player start becomes the hunter spawn."), GameMode->DebugGetHunterSpawnForTest(), HunterStartLocation);

	return true;
}

// Verifies authored discovery excludes the hidden teacher mirror-trap node from the counted objective
// stations. The runtime generator spawns the mirror node but deliberately keeps it OUT of ObjectiveStations
// (it is a scare relay, never completable); discovery must match, or an authored map counts an extra
// objective that can never complete -> the exit can never unlock (unwinnable round).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAuthoredLevelMirrorTrapTest,
	"BlackoutHunt.Level.AuthoredDiscoveryExcludesMirrorTrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAuthoredLevelMirrorTrapTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntMirrorTrap"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (!GameMode)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABHLevelMarker* Marker = World->SpawnActor<ABHLevelMarker>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Level marker spawns."), Marker);
	if (!Marker)
	{
		return false;
	}
	Marker->bRebuildRuntimeNavigation = false;
	Marker->LevelName = TEXT("Facility");

	// A winnable shell so discovery's required-element checks have something to find.
	World->SpawnActor<ABHBreaker>(FVector(100.0f, 0.0f, 95.0f), FRotator::ZeroRotator, SpawnParams);
	World->SpawnActor<ABHExitGate>(FVector(400.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams);

	// Two real objective stations plus one hidden mirror-trap node.
	World->SpawnActor<ABHObjectiveStation>(FVector(300.0f, 0.0f, 95.0f), FRotator::ZeroRotator, SpawnParams);
	World->SpawnActor<ABHObjectiveStation>(FVector(360.0f, 0.0f, 95.0f), FRotator::ZeroRotator, SpawnParams);
	if (ABHObjectiveStation* MirrorNode = World->SpawnActor<ABHObjectiveStation>(FVector(-500.0f, -380.0f, 95.0f), FRotator::ZeroRotator, SpawnParams))
	{
		MirrorNode->ConfigureTeacherMirrorTrapNode();
		TestTrue(TEXT("Mirror node reports IsTeacherMirrorTrapNode()."), MirrorNode->IsTeacherMirrorTrapNode());
	}

	TestTrue(TEXT("Discovery succeeds with a marker present."), GameMode->DebugDiscoverAuthoredLevelForTest());
	TestEqual(TEXT("Mirror-trap node is excluded; only the 2 real objective stations are counted."),
		GameMode->DebugGetObjectiveStationCountForTest(), 2);

	return true;
}

// Verifies crawl-space gates are rebuilt from the level marker when an authored map ships without any
// ABHCrawlSpaceVolume of its own (the volumes can be deleted while replacing blockout with meshes). Crawl
// runs are low GEOMETRY that only blocks standing pawns; the volume is what ejects a prone Teacher, so
// without this an authored map would let the Teacher follow survivors through.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAuthoredLevelCrawlGateRebuildTest,
	"BlackoutHunt.Level.AuthoredCrawlGateRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAuthoredLevelCrawlGateRebuildTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntAuthoredCrawl"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (!GameMode)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABHLevelMarker* Marker = World->SpawnActor<ABHLevelMarker>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Level marker spawns."), Marker);
	if (!Marker)
	{
		return false;
	}
	Marker->bRebuildRuntimeNavigation = false;
	Marker->LevelName = TEXT("Facility");

	// A winnable shell so discovery's required-element checks have something to find.
	World->SpawnActor<ABHBreaker>(FVector(100.0f, 0.0f, 95.0f), FRotator::ZeroRotator, SpawnParams);
	World->SpawnActor<ABHExitGate>(FVector(400.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams);

	// One baked crawl-gate spec, but NO ABHCrawlSpaceVolume placed (the remesh dropped it).
	FBHAuthoredCrawlGate Gate;
	Gate.Location = FVector(1500.0f, 250.0f, 82.0f);
	Gate.Rotation = FRotator(0.0f, 90.0f, 0.0f);
	Gate.Extent = FVector(325.0f, 118.0f, 118.0f);
	Marker->CrawlGates.Add(Gate);

	int32 CrawlVolumesBefore = 0;
	for (TActorIterator<ABHCrawlSpaceVolume> It(World); It; ++It)
	{
		++CrawlVolumesBefore;
	}
	TestEqual(TEXT("No crawl volume exists before discovery."), CrawlVolumesBefore, 0);

	TestTrue(TEXT("Discovery succeeds with a marker present."), GameMode->DebugDiscoverAuthoredLevelForTest());

	int32 CrawlVolumesAfter = 0;
	for (TActorIterator<ABHCrawlSpaceVolume> It(World); It; ++It)
	{
		++CrawlVolumesAfter;
	}
	TestEqual(TEXT("Discovery rebuilt the missing crawl gate from the marker."), CrawlVolumesAfter, 1);

	return true;
}

// Verifies the travel resolver is a safe no-op by default: with bUseAuthoredLevels off (the shipping
// default) every level resolves back to the stock runtime base map, so behavior is unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHAuthoredLevelTravelFallbackTest,
	"BlackoutHunt.Level.TravelRoutingFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHAuthoredLevelTravelFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntAuthoredLevelTravel"));
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (!GameMode)
	{
		return false;
	}

	// The resolver reads UBHGameSettings::bUseAuthoredLevels, which the shipping config now sets True.
	// Pin it off here so this test deterministically verifies the fallback contract (every level resolves
	// back to the runtime base map when authored levels are disabled) regardless of ambient project config,
	// then restore the previous value.
	UBHGameSettings* Settings = GetMutableDefault<UBHGameSettings>();
	const bool bPrevUseAuthored = Settings ? Settings->bUseAuthoredLevels : false;
	if (Settings)
	{
		Settings->bUseAuthoredLevels = false;
	}

	const FString EntryMap(TEXT("/Engine/Maps/Entry"));
	TestEqual(TEXT("Facility resolves to the runtime base map when authored levels are disabled."),
		GameMode->DebugResolveTravelMapForLevelForTest(TEXT("Facility")), EntryMap);
	TestEqual(TEXT("The train intermission always resolves to the runtime base map."),
		GameMode->DebugResolveTravelMapForLevelForTest(TEXT("TrainIntermission")), EntryMap);

	if (Settings)
	{
		Settings->bUseAuthoredLevels = bPrevUseAuthored;
	}

	return true;
}

#endif

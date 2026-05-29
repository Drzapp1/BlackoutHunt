#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationTestWorld.h"
#include "BHBreaker.h"
#include "BHExitGate.h"
#include "BHGameMode.h"
#include "BHLevelMarker.h"
#include "BHObjectiveStation.h"
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

	const FString EntryMap(TEXT("/Engine/Maps/Entry"));
	TestEqual(TEXT("Facility resolves to the runtime base map when authored levels are disabled."),
		GameMode->DebugResolveTravelMapForLevelForTest(TEXT("Facility")), EntryMap);
	TestEqual(TEXT("The train intermission always resolves to the runtime base map."),
		GameMode->DebugResolveTravelMapForLevelForTest(TEXT("TrainIntermission")), EntryMap);

	return true;
}

#endif

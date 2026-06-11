// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

// World-based prop-hunt tests (the pure helpers are pinned in BHPropHuntLibraryTests.cpp). Covers the P3 arena
// loader's testable seams: arena names as first-class level tokens, travel resolution to the pack packages, prop-hunt
// option propagation across travel, and the floor-trace spawn scatter against real collision geometry.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationTestWorld.h"
#include "BHGameMode.h"
#include "BHPropHuntLibrary.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

namespace
{
// A flat collision slab the scatter traces can hit: blocks every channel (the floor trace runs on ECC_Visibility,
// the clearance capsule on ECC_Pawn). Same idiom as the jumpscare spawn-resolver test's visibility blocker.
AActor* BHSpawnScatterSlab(UWorld* World, const FVector& Location, const FVector& Extent)
{
	AActor* Actor = World ? World->SpawnActor<AActor>(Location, FRotator::ZeroRotator) : nullptr;
	if (!Actor)
	{
		return nullptr;
	}

	UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("ScatterSlab"));
	Actor->SetRootComponent(Box);
	Actor->AddInstanceComponent(Box);
	Box->SetBoxExtent(Extent);
	Box->SetWorldLocation(Location);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponentWithWorld(World);
	return Actor;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHPropHuntArenaTravelTest,
	"BlackoutHunt.PropHunt.ArenaTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHPropHuntArenaTravelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntPropHuntTravel"));
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

	// --- Arena names are first-class level tokens; everything else normalizes as before. -----------------------------
	TestEqual(TEXT("Arena names pass through normalization with canonical casing."),
		ABHGameMode::DebugNormalizeBHLevelNameForTest(TEXT("containershouse")), FString(TEXT("ContainersHouse")));
	TestEqual(TEXT("RuinedCrypt passes through normalization."),
		ABHGameMode::DebugNormalizeBHLevelNameForTest(TEXT("ruinedcrypt")), FString(TEXT("RuinedCrypt")));
	TestEqual(TEXT("Fog still aliases to Foggrounds."),
		ABHGameMode::DebugNormalizeBHLevelNameForTest(TEXT("Fog")), FString(TEXT("Foggrounds")));
	TestEqual(TEXT("Unknown names still fall back to Facility."),
		ABHGameMode::DebugNormalizeBHLevelNameForTest(TEXT("NotAMap")), FString(TEXT("Facility")));

	// --- Travel resolution: an arena name resolves to a real pack package (authored bake preferred when present). ----
	int32 ArenaCount = 0;
	const BHPropHunt::FArenaSpec* Arenas = BHPropHunt::ArenaSpecs(ArenaCount);
	TestTrue(TEXT("The arena registry is non-empty."), ArenaCount > 0);
	const BHPropHunt::FArenaSpec* Containers = BHPropHunt::FindArenaSpec(TEXT("ContainersHouse"));
	TestNotNull(TEXT("ContainersHouse is a registered arena."), Containers);
	if (Containers)
	{
		const FString Resolved = GameMode->DebugResolveTravelMapForLevelForTest(TEXT("ContainersHouse"));
		TestTrue(TEXT("ContainersHouse resolves to its pack demo map (or the authored bake once it exists)."),
			Resolved.Equals(Containers->SourcePackage) || Resolved.Equals(Containers->AuthoredPackage));
		TestFalse(TEXT("ContainersHouse does not fall back to the runtime base map."),
			Resolved.Equals(TEXT("/Engine/Maps/Entry")));
	}

	// --- The prop-hunt flag rides every travel URL the mode builds (URL-option-only sessions must keep the mode). ----
	GameMode->DebugSetPropHuntModeForTest(true);
	const FString PropHuntOptions = GameMode->DebugBuildTravelOptionsForLevelForTest(TEXT("ContainersHouse"), 0);
	TestTrue(TEXT("Prop-hunt travel options carry ?BHPropHunt=1."), PropHuntOptions.Contains(TEXT("?BHPropHunt=1")));
	TestTrue(TEXT("Prop-hunt travel options carry the arena level token."), PropHuntOptions.Contains(TEXT("?BHLevel=ContainersHouse")));

	GameMode->DebugSetPropHuntModeForTest(false);
	const FString ClassroomOptions = GameMode->DebugBuildTravelOptionsForLevelForTest(TEXT("Facility"), 0);
	TestFalse(TEXT("Classroom travel options never mention prop hunt."), ClassroomOptions.Contains(TEXT("BHPropHunt")));

	// --- The arena dispatch is a no-op on non-arena worlds (this test world is not a registered package). ------------
	TestFalse(TEXT("TryBuildPropHuntArena declines a non-arena world."), GameMode->DebugTryBuildPropHuntArenaForTest());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHPropHuntArenaScatterTest,
	"BlackoutHunt.PropHunt.ArenaScatter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHPropHuntArenaScatterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntPropHuntScatter"), /*bCreatePhysicsScene*/ true);
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	// A 60x60 m floor slab, top surface at Z=+25.
	AActor* Floor = BHSpawnScatterSlab(World, FVector::ZeroVector, FVector(3000.0f, 3000.0f, 25.0f));
	TestNotNull(TEXT("Floor slab spawns."), Floor);
	const FBox FloorBounds(FVector(-3000.0f, -3000.0f, -25.0f), FVector(3000.0f, 3000.0f, 25.0f));

	const TArray<FVector> Points = ABHGameMode::DebugScatterPropHuntSpawnPointsForTest(World, FloorBounds, 10, 0);
	TestEqual(TEXT("The scatter finds the requested number of spawn points."), Points.Num(), 10);

	constexpr float ExpectedZ = 25.0f + 98.0f + 12.0f; // floor top + capsule half-height + hover margin
	for (const FVector& Point : Points)
	{
		TestTrue(TEXT("Every spawn point sits a capsule above the floor surface."), FMath::Abs(Point.Z - ExpectedZ) < 5.0f);
		TestTrue(TEXT("Every spawn point keeps the edge margin."),
			Point.X > -2800.0f && Point.X < 2800.0f && Point.Y > -2800.0f && Point.Y < 2800.0f);
	}
	for (int32 A = 0; A < Points.Num(); ++A)
	{
		for (int32 B = A + 1; B < Points.Num(); ++B)
		{
			TestTrue(TEXT("Spawn points keep their minimum spacing."),
				FVector::DistSquared2D(Points[A], Points[B]) >= FMath::Square(499.0f));
		}
	}

	// --- Roof handling: with a roof slab high above, points still land on the interior floor, never the roof. --------
	AActor* Roof = BHSpawnScatterSlab(World, FVector(0.0f, 0.0f, 600.0f), FVector(3000.0f, 3000.0f, 25.0f));
	TestNotNull(TEXT("Roof slab spawns."), Roof);
	const FBox RoofedBounds(FVector(-3000.0f, -3000.0f, -25.0f), FVector(3000.0f, 3000.0f, 625.0f));

	const TArray<FVector> InteriorPoints = ABHGameMode::DebugScatterPropHuntSpawnPointsForTest(World, RoofedBounds, 8, 0);
	TestEqual(TEXT("The scatter still finds points under a roof."), InteriorPoints.Num(), 8);
	for (const FVector& Point : InteriorPoints)
	{
		TestTrue(TEXT("Roofed-arena spawn points land on the interior floor, not the roof."),
			FMath::Abs(Point.Z - ExpectedZ) < 5.0f);
	}

	// Determinism: the same world + bounds + salt yields the same points (Halton, no RNG).
	const TArray<FVector> Repeat = ABHGameMode::DebugScatterPropHuntSpawnPointsForTest(World, RoofedBounds, 8, 0);
	TestEqual(TEXT("Scatter is deterministic for a fixed salt."), Repeat.Num(), InteriorPoints.Num());
	for (int32 Index = 0; Index < FMath::Min(Repeat.Num(), InteriorPoints.Num()); ++Index)
	{
		TestTrue(TEXT("Scatter points repeat exactly."), Repeat[Index].Equals(InteriorPoints[Index], 0.01f));
	}

	return true;
}

// The seek cadence clock (H2 regression). The taunt/pulse tightening lerps normalize by the seek length armed in
// BeginPropHuntHunt (bh.PropHuntSeekSeconds), NEVER the classroom HuntSeconds: dividing by HuntSeconds (900 default)
// with a 240 s seek started the curve at Elapsed = 660/900 = ~0.73 — forced taunts ran ~15s->10s instead of 30s->10s
// for the whole round. SeekElapsedFraction is the pure helper both live sites (TickPropHunt cadences and the
// RefreshPropHuntGameState HUD hint) now share.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHPropHuntSeekCadenceClockTest,
	"BlackoutHunt.PropHunt.SeekCadenceClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHPropHuntSeekCadenceClockTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	constexpr int32 SeekSeconds = 240;   // bh.PropHuntSeekSeconds default
	constexpr int32 ClassroomHunt = 900; // HuntSeconds classroom default — must play no part in the cadence
	constexpr float TauntBase = 30.0f;   // bh.PropHuntTauntBase default
	constexpr float TauntMin = 10.0f;    // bh.PropHuntTauntMin default

	// Seek start: the full clock remains -> 0.0 elapsed -> the loose Base interval.
	TestEqual(TEXT("Elapsed is 0.0 at seek start."), BHPropHunt::SeekElapsedFraction(SeekSeconds, SeekSeconds), 0.0f);
	TestEqual(TEXT("The taunt interval at seek start is the loose Base."),
		BHPropHunt::TauntIntervalSeconds(BHPropHunt::SeekElapsedFraction(SeekSeconds, SeekSeconds), TauntBase, TauntMin), TauntBase);

	// The regression shape: normalized by the CLASSROOM clock, the same instant read as ~73% spent and the interval
	// opened at ~15.3 s instead of 30 s. Pin both so a revert of the clock source fails loudly.
	const float WrongClockElapsed = static_cast<float>(ClassroomHunt - SeekSeconds) / static_cast<float>(ClassroomHunt);
	TestTrue(TEXT("(Regression) the classroom-clock fraction at seek start really was ~0.733."),
		FMath::IsNearlyEqual(WrongClockElapsed, 0.7333f, 0.001f));
	TestTrue(TEXT("(Regression) the classroom-clock interval at seek start was ~15.3 s, not Base."),
		BHPropHunt::TauntIntervalSeconds(WrongClockElapsed, TauntBase, TauntMin) < 16.0f);

	// Mid-seek and timeout.
	TestEqual(TEXT("Elapsed is 0.5 at half the seek."), BHPropHunt::SeekElapsedFraction(SeekSeconds, SeekSeconds / 2), 0.5f);
	TestEqual(TEXT("Elapsed is 1.0 at timeout."), BHPropHunt::SeekElapsedFraction(SeekSeconds, 0), 1.0f);
	TestEqual(TEXT("The taunt interval at timeout is the tight Min."),
		BHPropHunt::TauntIntervalSeconds(BHPropHunt::SeekElapsedFraction(SeekSeconds, 0), TauntBase, TauntMin), TauntMin);

	// Degenerate inputs: never divide by zero, always clamped to [0,1].
	TestEqual(TEXT("An unarmed (zero) seek clock reads as seek start."), BHPropHunt::SeekElapsedFraction(0, 100), 0.0f);
	TestEqual(TEXT("A negative seek clock reads as seek start."), BHPropHunt::SeekElapsedFraction(-5, 100), 0.0f);
	TestEqual(TEXT("Remaining time above the clock clamps to 0 (time added mid-seek)."),
		BHPropHunt::SeekElapsedFraction(SeekSeconds, SeekSeconds + 60), 0.0f);
	TestEqual(TEXT("Negative remaining time clamps to 1 (clock overrun)."), BHPropHunt::SeekElapsedFraction(SeekSeconds, -30), 1.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

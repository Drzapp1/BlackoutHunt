// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHPropHuntLibrary.h"
#include "BHTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHPropHuntLibraryTest,
	"BlackoutHunt.PropHunt.Library",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHPropHuntLibraryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// --- ResolvePropHuntRound: the whole win/loss table -----------------------------------------------------------
	// Hunt still going: props alive, a seeker alive, time left.
	TestEqual(TEXT("Props alive + seeker alive + time left -> Hunt continues."),
		BHPropHunt::ResolvePropHuntRound(/*AliveProps*/3, /*Escaped*/0, /*AliveSeekers*/1, /*bTimeExpired*/false),
		EBHRoundPhase::Hunt);

	// Seeker found every prop (none escaped) -> seeker (Hunter) wins.
	TestEqual(TEXT("No props left, none escaped -> HunterWin (seeker found all)."),
		BHPropHunt::ResolvePropHuntRound(0, 0, 1, false),
		EBHRoundPhase::HunterWin);

	// All props gone but at least one slipped out -> props win.
	TestEqual(TEXT("No props left but one escaped -> SurvivorsWin (props survived)."),
		BHPropHunt::ResolvePropHuntRound(0, 1, 1, false),
		EBHRoundPhase::SurvivorsWin);

	// The prop-hunt flip: time runs out with props still hidden -> props win (NOT a Hunter win).
	TestEqual(TEXT("Time expired with props still hidden -> SurvivorsWin (the prop-hunt timeout flip)."),
		BHPropHunt::ResolvePropHuntRound(2, 0, 1, true),
		EBHRoundPhase::SurvivorsWin);

	// No seeker remains -> props can never be found -> props win even with time on the clock.
	TestEqual(TEXT("No seeker left -> SurvivorsWin."),
		BHPropHunt::ResolvePropHuntRound(2, 0, 0, false),
		EBHRoundPhase::SurvivorsWin);

	// All-props-caught takes precedence over a simultaneously-expired timer (seeker earned the win on the last grab).
	TestEqual(TEXT("Last prop caught on the same tick the timer expires -> HunterWin."),
		BHPropHunt::ResolvePropHuntRound(0, 0, 1, true),
		EBHRoundPhase::HunterWin);

	// --- TauntIntervalSeconds: tightens as the round runs down ------------------------------------------------------
	const float EarlyInterval = BHPropHunt::TauntIntervalSeconds(0.0f, 30.0f, 10.0f);
	const float MidInterval = BHPropHunt::TauntIntervalSeconds(0.5f, 30.0f, 10.0f);
	const float LateInterval = BHPropHunt::TauntIntervalSeconds(1.0f, 30.0f, 10.0f);
	TestEqual(TEXT("Taunt interval at round start equals the base."), EarlyInterval, 30.0f);
	TestEqual(TEXT("Taunt interval at round end equals the minimum."), LateInterval, 10.0f);
	TestEqual(TEXT("Taunt interval at the half-way point is the midpoint."), MidInterval, 20.0f);
	TestTrue(TEXT("Taunt interval strictly tightens as the round elapses."), MidInterval < EarlyInterval && LateInterval < MidInterval);

	// Out-of-range fractions clamp; an inverted Min/Base never returns below the (clamped) minimum.
	TestEqual(TEXT("Negative elapsed fraction clamps to the base interval."),
		BHPropHunt::TauntIntervalSeconds(-1.0f, 30.0f, 10.0f), 30.0f);
	TestEqual(TEXT("Elapsed fraction above 1 clamps to the minimum interval."),
		BHPropHunt::TauntIntervalSeconds(5.0f, 30.0f, 10.0f), 10.0f);

	// --- SeekerMissSlowSeconds: grows with consecutive misses, capped ----------------------------------------------
	TestEqual(TEXT("Zero misses -> no self-slow."),
		BHPropHunt::SeekerMissSlowSeconds(0, 0.6f, 0.35f, 2.0f), 0.0f);
	TestEqual(TEXT("First miss -> the base slow."),
		BHPropHunt::SeekerMissSlowSeconds(1, 0.6f, 0.35f, 2.0f), 0.6f);
	TestEqual(TEXT("Second miss -> base + one step."),
		BHPropHunt::SeekerMissSlowSeconds(2, 0.6f, 0.35f, 2.0f), 0.95f);
	TestTrue(TEXT("Many misses clamp to the maximum self-slow."),
		FMath::IsNearlyEqual(BHPropHunt::SeekerMissSlowSeconds(20, 0.6f, 0.35f, 2.0f), 2.0f));

	// --- Fallback prop catalogue: always non-empty, salt-stable, in range ------------------------------------------
	const int32 FallbackCount = BHPropHunt::FallbackPropCount();
	TestTrue(TEXT("The fallback prop catalogue is non-empty."), FallbackCount > 0);
	for (int32 Salt = -5; Salt <= 5; ++Salt)
	{
		const BHPropHunt::FFallbackProp& Prop = BHPropHunt::FallbackPropForSalt(Salt);
		TestNotNull(TEXT("Every fallback prop has a mesh path."), Prop.MeshPath);
		TestTrue(TEXT("Every fallback prop has a positive scale."), Prop.Scale > 0.0f);
	}

	// --- Arena registry: rows well-formed, lookup case-insensitive, misses are null ---------------------------------
	int32 ArenaCount = 0;
	const BHPropHunt::FArenaSpec* Arenas = BHPropHunt::ArenaSpecs(ArenaCount);
	TestTrue(TEXT("The arena registry is non-empty."), ArenaCount > 0);
	for (int32 Index = 0; Index < ArenaCount; ++Index)
	{
		const BHPropHunt::FArenaSpec& Spec = Arenas[Index];
		TestTrue(TEXT("Every arena has a logical name."), Spec.LogicalName && FCString::Strlen(Spec.LogicalName) > 0);
		TestTrue(TEXT("Every arena's authored package is a /Game path."), Spec.AuthoredPackage && FString(Spec.AuthoredPackage).StartsWith(TEXT("/Game/")));
		TestTrue(TEXT("Every arena's source package is a /Game path."), Spec.SourcePackage && FString(Spec.SourcePackage).StartsWith(TEXT("/Game/")));
		TestTrue(TEXT("Every arena has a display name."), Spec.DisplayName && FCString::Strlen(Spec.DisplayName) > 0);
		// Arena names must not collide with the reserved level tokens (NormalizeBHLevelName resolves those first).
		const FString Logical(Spec.LogicalName);
		TestFalse(TEXT("Arena names must not shadow the built-in level names."),
			Logical.Equals(TEXT("Facility"), ESearchCase::IgnoreCase) || Logical.Equals(TEXT("Substation"), ESearchCase::IgnoreCase)
			|| Logical.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || Logical.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase)
			|| Logical.Equals(TEXT("TrainIntermission"), ESearchCase::IgnoreCase) || Logical.Equals(TEXT("Fog"), ESearchCase::IgnoreCase));
		for (int32 Other = Index + 1; Other < ArenaCount; ++Other)
		{
			TestFalse(TEXT("Arena logical names are unique."), Logical.Equals(Arenas[Other].LogicalName, ESearchCase::IgnoreCase));
		}
	}
	TestTrue(TEXT("Arena lookup is case-insensitive."), BHPropHunt::FindArenaSpec(TEXT("containershouse")) == &Arenas[0]);
	TestTrue(TEXT("Arena lookup trims whitespace."), BHPropHunt::FindArenaSpec(TEXT(" ContainersHouse ")) == &Arenas[0]);
	TestTrue(TEXT("Unknown arena tokens miss."), BHPropHunt::FindArenaSpec(TEXT("NotAnArena")) == nullptr);
	TestTrue(TEXT("The empty token misses."), BHPropHunt::FindArenaSpec(FString()) == nullptr);

	// --- Halton scatter: deterministic, in range, well spread --------------------------------------------------------
	TestEqual(TEXT("Halton base-2 position 1 is 1/2."), BHPropHunt::HaltonSequence(1, 2), 0.5f);
	TestEqual(TEXT("Halton base-2 position 2 is 1/4."), BHPropHunt::HaltonSequence(2, 2), 0.25f);
	TestEqual(TEXT("Halton base-3 position 1 is 1/3."), BHPropHunt::HaltonSequence(1, 3), 1.0f / 3.0f);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		const FVector2D UV = BHPropHunt::ArenaScatterUV(Index);
		TestTrue(TEXT("Scatter UVs stay inside the unit square."), UV.X >= 0.0f && UV.X < 1.0f && UV.Y >= 0.0f && UV.Y < 1.0f);
		TestTrue(TEXT("Scatter UVs are deterministic."), BHPropHunt::ArenaScatterUV(Index) == UV);
	}
	// Low-discrepancy: the first 16 candidates land in at least 10 distinct quadrant cells of a 4x4 grid.
	TSet<int32> Cells;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const FVector2D UV = BHPropHunt::ArenaScatterUV(Index);
		Cells.Add(FMath::Clamp(static_cast<int32>(UV.X * 4.0f), 0, 3) * 4 + FMath::Clamp(static_cast<int32>(UV.Y * 4.0f), 0, 3));
	}
	TestTrue(TEXT("The first 16 scatter candidates spread across the space (>=10 of 16 4x4 cells)."), Cells.Num() >= 10);

	// --- PickStartingSeekerIndex: fewest-seeks first, lowest score breaks ties, stable on full ties ------------------
	TestEqual(TEXT("Seeker rotation on empty input is INDEX_NONE."),
		BHPropHunt::PickStartingSeekerIndex(TArray<int32>(), TArray<int32>()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Seeker rotation rejects mismatched arrays."),
		BHPropHunt::PickStartingSeekerIndex({ 0, 1 }, { 5 }), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Seeker rotation picks the player who has seeked the fewest times."),
		BHPropHunt::PickStartingSeekerIndex({ 1, 0, 1 }, { 0, 500, 0 }), 1);
	TestEqual(TEXT("Equal seek counts: the trailing (lowest-score) player gets the turn."),
		BHPropHunt::PickStartingSeekerIndex({ 1, 1, 1 }, { 120, 40, 90 }), 1);
	TestEqual(TEXT("Full ties keep join order (the first candidate)."),
		BHPropHunt::PickStartingSeekerIndex({ 0, 0 }, { 0, 0 }), 0);

	// --- PickSeekerHoldIndex: farthest-from-centroid, safe on empty --------------------------------------------------
	TestEqual(TEXT("Seeker hold pick on an empty set is INDEX_NONE."), BHPropHunt::PickSeekerHoldIndex(TArray<FVector>()), static_cast<int32>(INDEX_NONE));
	TArray<FVector> Cluster = { FVector(0, 0, 0), FVector(100, 0, 0), FVector(0, 100, 0), FVector(5000, 5000, 0) };
	TestEqual(TEXT("Seeker hold pick takes the outlier farthest from the centroid."), BHPropHunt::PickSeekerHoldIndex(Cluster), 3);
	TArray<FVector> Single = { FVector(42, 42, 42) };
	TestEqual(TEXT("Seeker hold pick on a single point returns it."), BHPropHunt::PickSeekerHoldIndex(Single), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

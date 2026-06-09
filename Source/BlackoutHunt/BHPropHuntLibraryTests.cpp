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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

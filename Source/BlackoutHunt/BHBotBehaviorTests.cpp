// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHBotPolicySubsystem.h"
#include "BHTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotRemovalSelectionTest,
	"BlackoutHunt.Bots.RemovalSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotRemovalSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using BHBotBehavior::FBHBotRemovalCandidate;
	using BHBotBehavior::SelectBotRemovalIndex;

	auto MakeCandidate = [](bool bAlive, bool bHunter)
	{
		FBHBotRemovalCandidate Candidate;
		Candidate.bAlive = bAlive;
		Candidate.bHunter = bHunter;
		return Candidate;
	};

	// Empty roster -> nothing to trim.
	TestEqual(TEXT("Empty roster returns INDEX_NONE."),
		SelectBotRemovalIndex({}, true, 1), static_cast<int32>(INDEX_NONE));

	// Benched/captured bots go before anyone alive, even when an alive non-hunter is newer.
	{
		TArray<FBHBotRemovalCandidate> Roster;
		Roster.Add(MakeCandidate(false, false)); // index 0: captured survivor bot
		Roster.Add(MakeCandidate(true, false));  // index 1: alive survivor bot (newest)
		TestEqual(TEXT("A benched bot is trimmed before an alive one."),
			SelectBotRemovalIndex(Roster, true, 1), 0);
	}

	// Alive non-hunters go before the alive hunter.
	{
		TArray<FBHBotRemovalCandidate> Roster;
		Roster.Add(MakeCandidate(true, true));  // index 0: alive bot Teacher
		Roster.Add(MakeCandidate(true, false)); // index 1: alive survivor bot
		TestEqual(TEXT("An alive survivor bot is trimmed before the alive hunter bot."),
			SelectBotRemovalIndex(Roster, true, 1), 1);
	}

	// The lone alive hunter is NEVER trimmed in a live round - the trim defers instead.
	{
		TArray<FBHBotRemovalCandidate> Roster;
		Roster.Add(MakeCandidate(true, true)); // the only bot left is the round's lone hunter
		TestEqual(TEXT("Lone alive hunter in a live round defers the trim (INDEX_NONE)."),
			SelectBotRemovalIndex(Roster, true, 1), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("Same lone hunter is removable once the round is not live (Lobby/round end)."),
			SelectBotRemovalIndex(Roster, false, 1), 0);
	}

	// A hunter bot IS removable mid-round while another alive hunter (human or bot) remains.
	{
		TArray<FBHBotRemovalCandidate> Roster;
		Roster.Add(MakeCandidate(true, true)); // bot hunter; a second alive hunter exists in the match
		TestEqual(TEXT("Hunter bot is removable mid-round when a second alive hunter remains."),
			SelectBotRemovalIndex(Roster, true, 2), 0);
	}

	// Newest-first inside a tier (preserves the old pop-the-newest behaviour where it was safe).
	{
		TArray<FBHBotRemovalCandidate> Roster;
		Roster.Add(MakeCandidate(true, false)); // index 0: older alive survivor
		Roster.Add(MakeCandidate(true, false)); // index 1: newest alive survivor
		TestEqual(TEXT("Within a tier the newest bot is trimmed first."),
			SelectBotRemovalIndex(Roster, true, 1), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotAnswerRejectionCooldownTest,
	"BlackoutHunt.Bots.AnswerRejectionCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotAnswerRejectionCooldownTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using BHBotBehavior::AnswerRejectionCooldownSeconds;

	// A single refusal (correction hold / question swap) deserves one free local retry.
	TestEqual(TEXT("Zero rejections -> no cooldown."), AnswerRejectionCooldownSeconds(0), 0.0f);
	TestEqual(TEXT("One rejection -> no cooldown (one local retry allowed)."), AnswerRejectionCooldownSeconds(1), 0.0f);

	// Repeated refusals push the bot off the node so it spreads out instead of camping.
	const float SecondRefusal = AnswerRejectionCooldownSeconds(2);
	const float FourthRefusal = AnswerRejectionCooldownSeconds(4);
	TestTrue(TEXT("Two consecutive rejections trigger a cooldown."), SecondRefusal > 0.0f);
	TestTrue(TEXT("Structural rejection (4+) cools down much longer than a transient one."), FourthRefusal > SecondRefusal);
	TestEqual(TEXT("Cooldown saturates (no unbounded growth)."),
		AnswerRejectionCooldownSeconds(50), FourthRefusal);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotHonestSightScaleTest,
	"BlackoutHunt.Bots.HonestSightScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotHonestSightScaleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using BHBotBehavior::HonestSightRangeScale;

	constexpr float BlackoutScale = 0.35f;
	constexpr float HeavyScale = 0.62f;
	constexpr float ExtremeScale = 0.42f;

	// Clear conditions: nobody loses range.
	TestEqual(TEXT("Hunter in light fog vs lit target sees at full range."),
		HonestSightRangeScale(true, false, false, EBHFogPreset::Light, true, BlackoutScale, HeavyScale, ExtremeScale), 1.0f);
	TestEqual(TEXT("Survivor outside any blackout sees at full range."),
		HonestSightRangeScale(false, false, false, EBHFogPreset::Light, false, BlackoutScale, HeavyScale, ExtremeScale), 1.0f);

	// (a) A student bot inside the Teacher's blackout is blinded hard.
	TestEqual(TEXT("Student bot inside the blackout gets the blackout cut."),
		HonestSightRangeScale(false, true, false, EBHFogPreset::Light, false, BlackoutScale, HeavyScale, ExtremeScale), BlackoutScale);

	// Darkness hides both ways: a student bot OUTSIDE the blackout cannot see a target standing in it.
	TestEqual(TEXT("Student bot peering into the blackout from outside gets the same cut."),
		HonestSightRangeScale(false, false, true, EBHFogPreset::Light, false, BlackoutScale, HeavyScale, ExtremeScale), BlackoutScale);

	// The Teacher who cast the blackout is NOT blinded by it in either direction (the power keeps its asymmetry).
	TestEqual(TEXT("Hunter inside the blackout keeps full sight."),
		HonestSightRangeScale(true, true, false, EBHFogPreset::Light, true, BlackoutScale, HeavyScale, ExtremeScale), 1.0f);
	TestEqual(TEXT("Hunter sees INTO the blackout at full range."),
		HonestSightRangeScale(true, false, true, EBHFogPreset::Light, true, BlackoutScale, HeavyScale, ExtremeScale), 1.0f);

	// (b) A bot Teacher in heavy/extreme fog only spots LIGHTS-OFF targets at reduced range.
	TestEqual(TEXT("Hunter in heavy fog vs lights-off target gets the heavy cut."),
		HonestSightRangeScale(true, false, false, EBHFogPreset::Heavy, false, BlackoutScale, HeavyScale, ExtremeScale), HeavyScale);
	TestEqual(TEXT("Hunter in extreme fog vs lights-off target gets the extreme cut."),
		HonestSightRangeScale(true, false, false, EBHFogPreset::Extreme, false, BlackoutScale, HeavyScale, ExtremeScale), ExtremeScale);

	// A flashlight gives its holder away at full range even through extreme fog.
	TestEqual(TEXT("Hunter in extreme fog still sees a LIT flashlight at full range."),
		HonestSightRangeScale(true, false, false, EBHFogPreset::Extreme, true, BlackoutScale, HeavyScale, ExtremeScale), 1.0f);

	// Fog cuts are a hunter handicap only; the blackout is a student handicap only.
	TestEqual(TEXT("Survivor in heavy fog takes no fog cut (only the blackout blinds students)."),
		HonestSightRangeScale(false, false, false, EBHFogPreset::Heavy, false, BlackoutScale, HeavyScale, ExtremeScale), 1.0f);

	// Degenerate cvar values clamp instead of inverting or zeroing sight.
	TestEqual(TEXT("A zero/negative scale clamps to the 0.05 floor."),
		HonestSightRangeScale(false, true, false, EBHFogPreset::Light, false, -1.0f, HeavyScale, ExtremeScale), 0.05f);
	TestEqual(TEXT("A scale above 1 clamps back to 1 (cvars cannot grant super-sight)."),
		HonestSightRangeScale(true, false, false, EBHFogPreset::Heavy, false, BlackoutScale, 4.0f, ExtremeScale), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotSightingThreatPressureTest,
	"BlackoutHunt.Bots.SightingThreatPressure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotSightingThreatPressureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using BHBotBehavior::SightingThreatPressure;

	constexpr float Memory = 12.0f;
	constexpr float Range = 3220.0f;

	// A fresh sighting on top of the location is maximum pressure.
	TestEqual(TEXT("Fresh sighting at zero distance -> full pressure."),
		SightingThreatPressure(0.0f, Memory, 0.0f, Range), 1.0f);

	// Pressure falls with distance and is gone outside the range.
	const float NearPressure = SightingThreatPressure(0.0f, Memory, Range * 0.25f, Range);
	const float FarPressure = SightingThreatPressure(0.0f, Memory, Range * 0.75f, Range);
	TestTrue(TEXT("Pressure decreases with distance from the sighting."), FarPressure < NearPressure);
	TestEqual(TEXT("Beyond the pressure range a sighting contributes nothing."),
		SightingThreatPressure(0.0f, Memory, Range + 1.0f, Range), 0.0f);

	// Pressure fades with sighting age and expires with memory.
	const float FreshPressure = SightingThreatPressure(0.0f, Memory, 100.0f, Range);
	const float StalePressure = SightingThreatPressure(Memory * 0.9f, Memory, 100.0f, Range);
	TestTrue(TEXT("Older sightings contribute less than fresh ones."), StalePressure < FreshPressure);
	TestTrue(TEXT("A stale-but-remembered sighting still contributes something."), StalePressure > 0.0f);
	TestEqual(TEXT("A sighting older than memory contributes nothing."),
		SightingThreatPressure(Memory + 0.1f, Memory, 100.0f, Range), 0.0f);

	// Degenerate inputs are inert rather than NaN/negative.
	TestEqual(TEXT("Zero memory window yields zero pressure."),
		SightingThreatPressure(1.0f, 0.0f, 100.0f, Range), 0.0f);
	TestEqual(TEXT("Zero pressure range yields zero pressure."),
		SightingThreatPressure(1.0f, Memory, 100.0f, 0.0f), 0.0f);
	TestEqual(TEXT("A negative age clamps to fresh instead of over-weighting."),
		SightingThreatPressure(-5.0f, Memory, 0.0f, Range), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotDecoySuppressionTest,
	"BlackoutHunt.Bots.DecoySuppression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotDecoySuppressionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using BHBotBehavior::ShouldSuppressBotDecoyDrop;

	// Prop hunt: prop-side (survivor) bots must never beacon their own hiding spot.
	TestTrue(TEXT("Prop hunt suppresses survivor (prop) decoy drops."),
		ShouldSuppressBotDecoyDrop(true, EBHPlayerRole::Survivor));

	// Non-prop roles and classic mode keep their decoy play untouched.
	TestFalse(TEXT("Prop hunt does not suppress the seeker's role."),
		ShouldSuppressBotDecoyDrop(true, EBHPlayerRole::Hunter));
	TestFalse(TEXT("Prop hunt does not suppress hall monitors."),
		ShouldSuppressBotDecoyDrop(true, EBHPlayerRole::FakeHunter));
	TestFalse(TEXT("Classic mode never suppresses survivor decoys."),
		ShouldSuppressBotDecoyDrop(false, EBHPlayerRole::Survivor));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

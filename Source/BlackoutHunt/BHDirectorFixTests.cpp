// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHGameMode.h"
#include "BHRevisionQuestionBank.h"
#include "BHTypes.h"
#include "Misc/AutomationTest.h"

namespace
{
int32 BHTopicBitForStationType(EBHObjectiveStationType StationType)
{
	return FBHRevisionQuestionBank::TopicMaskBit(FBHRevisionQuestionBank::TopicForStationType(StationType));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHDirectorTopicMaskOverrideTest,
	"BlackoutHunt.GameMode.DirectorTopicMaskOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHDirectorTopicMaskOverrideTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const EBHObjectiveStationType AllTypes[4] = {
		EBHObjectiveStationType::Valve,
		EBHObjectiveStationType::Terminal,
		EBHObjectiveStationType::Antenna,
		EBHObjectiveStationType::Evidence
	};

	// --- All topics enabled: every station keeps its authored identity at any index. -------------------------------
	for (EBHObjectiveStationType NaturalType : AllTypes)
	{
		for (int32 StationIndex = 0; StationIndex < 7; ++StationIndex)
		{
			TestEqual(TEXT("Full mask leaves the natural station type untouched."),
				ABHGameMode::ResolveStationTypeForTopicMask(NaturalType, 0x0F, StationIndex),
				NaturalType);
		}
	}

	// --- Core invariant: under every non-degenerate mask, the resolved type's topic is ENABLED, and an
	// already-enabled natural type is never re-typed. This is exactly "masked-off topics are never asked
	// while every node still contributes to an enabled topic". --------------------------------------------------
	for (int32 TopicMask = 1; TopicMask <= 0x0F; ++TopicMask)
	{
		for (EBHObjectiveStationType NaturalType : AllTypes)
		{
			for (int32 StationIndex = 0; StationIndex < 12; ++StationIndex)
			{
				const EBHObjectiveStationType Resolved = ABHGameMode::ResolveStationTypeForTopicMask(NaturalType, TopicMask, StationIndex);
				TestTrue(TEXT("Resolved station type always maps to an ENABLED topic."),
					(TopicMask & BHTopicBitForStationType(Resolved)) != 0);
				if ((TopicMask & BHTopicBitForStationType(NaturalType)) != 0)
				{
					TestEqual(TEXT("A station whose natural topic is enabled is never re-typed."),
						Resolved,
						NaturalType);
				}
				// Stability: the pick is a pure function of (type, mask, index) — PrepareRoundDirector may
				// re-run in the same world (practice/test refresh) and must re-derive the same identity.
				TestEqual(TEXT("Override pick is stable for identical inputs."),
					ABHGameMode::ResolveStationTypeForTopicMask(NaturalType, TopicMask, StationIndex),
					Resolved);
			}
		}
	}

	// --- Round-robin spread: with Waves masked off, consecutive Antenna stations cycle through ALL three
	// enabled topics instead of piling onto one. -----------------------------------------------------------------
	{
		const int32 NoWavesMask = 0x0F & ~BHTopicBitForStationType(EBHObjectiveStationType::Antenna);
		TSet<EBHObjectiveStationType> SeenTypes;
		for (int32 StationIndex = 0; StationIndex < 3; ++StationIndex)
		{
			SeenTypes.Add(ABHGameMode::ResolveStationTypeForTopicMask(EBHObjectiveStationType::Antenna, NoWavesMask, StationIndex));
		}
		TestEqual(TEXT("Three consecutive masked-off stations spread across all three enabled topics."),
			SeenTypes.Num(),
			3);
		TestFalse(TEXT("The masked-off type itself never comes back out of the spread."),
			SeenTypes.Contains(EBHObjectiveStationType::Antenna));
		TestEqual(TEXT("The round-robin wraps after one full cycle of the enabled topics."),
			ABHGameMode::ResolveStationTypeForTopicMask(EBHObjectiveStationType::Antenna, NoWavesMask, 3),
			ABHGameMode::ResolveStationTypeForTopicMask(EBHObjectiveStationType::Antenna, NoWavesMask, 0));
	}

	// --- Single-topic mask: every station funnels into that one enabled topic. -------------------------------------
	{
		const int32 ElectricityOnlyMask = BHTopicBitForStationType(EBHObjectiveStationType::Terminal);
		for (EBHObjectiveStationType NaturalType : AllTypes)
		{
			TestEqual(TEXT("A single enabled topic absorbs every station."),
				ABHGameMode::ResolveStationTypeForTopicMask(NaturalType, ElectricityOnlyMask, 5),
				EBHObjectiveStationType::Terminal);
		}
	}

	// --- Degenerate / out-of-range masks: never invent a topic, never read bits above the four real ones. ----------
	for (EBHObjectiveStationType NaturalType : AllTypes)
	{
		TestEqual(TEXT("An empty mask (nothing enabled) keeps the authored type."),
			ABHGameMode::ResolveStationTypeForTopicMask(NaturalType, 0, 2),
			NaturalType);
		TestEqual(TEXT("Bits above the four topic bits are ignored (mask is clamped to 0x0F)."),
			ABHGameMode::ResolveStationTypeForTopicMask(NaturalType, 0xF0 | 0x0F, 4),
			ABHGameMode::ResolveStationTypeForTopicMask(NaturalType, 0x0F, 4));
	}
	TestEqual(TEXT("A negative station index degrades to the first enabled pick instead of indexing out of range."),
		ABHGameMode::ResolveStationTypeForTopicMask(EBHObjectiveStationType::Antenna, BHTopicBitForStationType(EBHObjectiveStationType::Valve) | BHTopicBitForStationType(EBHObjectiveStationType::Evidence), -3),
		EBHObjectiveStationType::Valve);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

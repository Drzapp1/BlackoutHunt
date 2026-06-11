// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHHUD.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHHudDiagramLabelMappingTest,
	"BlackoutHunt.Hud.DiagramLabelMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHHudDiagramLabelMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Single-band identify question: each band LEADS exactly one choice, so every band click maps.
	const TArray<FString> IdentifyChoices = {
		TEXT("Radio waves"),
		TEXT("Infrared"),
		TEXT("Ultraviolet"),
		TEXT("Gamma rays")
	};
	TestEqual(TEXT("Identify question maps a unique band label to its choice."),
		BHMapDiagramLabelToChoice(TEXT("Infrared"), IdentifyChoices), 1);
	TestEqual(TEXT("Band label matching is case-insensitive."),
		BHMapDiagramLabelToChoice(TEXT("infrared"), IdentifyChoices), 1);
	TestEqual(TEXT("Band label matches as the leading token of the displayed choice text."),
		BHMapDiagramLabelToChoice(TEXT("Radio"), IdentifyChoices), 0);
	TestEqual(TEXT("A band absent from every choice maps to nothing."),
		BHMapDiagramLabelToChoice(TEXT("Microwave"), IdentifyChoices), static_cast<int32>(INDEX_NONE));

	// Bank row 627 shape (TF on an EMSpectrum diagram): the band names appear exactly once each — inside
	// WRONG distractors. The bare True/False in the choice set must disable diagram answers entirely;
	// without that guard, a click on the bracketed radio band would submit "Only radio waves".
	const TArray<FString> TrueFalseBankChoices = {
		TEXT("True"), TEXT("False"), TEXT("Only radio waves"), TEXT("Only visible light in glass")
	};
	TestEqual(TEXT("TF row: the radio band must not submit the 'Only radio waves' distractor."),
		BHMapDiagramLabelToChoice(TEXT("radio"), TrueFalseBankChoices), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("TF row: the visible band must not submit a distractor either."),
		BHMapDiagramLabelToChoice(TEXT("visible"), TrueFalseBankChoices), static_cast<int32>(INDEX_NONE));
	const TArray<FString> TrueFalseChoices = { TEXT("True"), TEXT("False") };
	TestEqual(TEXT("Plain true/false choices never take a band click."),
		BHMapDiagramLabelToChoice(TEXT("X-ray"), TrueFalseChoices), static_cast<int32>(INDEX_NONE));

	// Bank row 645 shape (EM safety): every choice LEADS with "Microwaves..." (ambiguous), and "gamma"
	// appears only mid-sentence in a wrong distractor (not a leading token) — neither may map.
	const TArray<FString> EmSafetyChoices = {
		TEXT("Microwaves can cause heating, so power levels and exposure are controlled"),
		TEXT("Microwaves are ionising like gamma rays"),
		TEXT("Microwaves are sound waves"),
		TEXT("Microwaves cannot transfer energy")
	};
	TestEqual(TEXT("A band leading EVERY choice is ambiguous and maps to nothing."),
		BHMapDiagramLabelToChoice(TEXT("microwave"), EmSafetyChoices), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A band mentioned mid-sentence in a distractor must not become click-submittable."),
		BHMapDiagramLabelToChoice(TEXT("gamma"), EmSafetyChoices), static_cast<int32>(INDEX_NONE));

	// "Which statement..." rows: no choice leads with a band name, so the diagram is illustration only.
	const TArray<FString> StatementChoices = {
		TEXT("They have very high frequency and are ionising"),
		TEXT("They are the longest-wavelength EM waves"),
		TEXT("They are sound waves in air"),
		TEXT("They are safer than radio waves at any dose")
	};
	TestEqual(TEXT("Statement rows never take a band click (bands appear mid-sentence only)."),
		BHMapDiagramLabelToChoice(TEXT("gamma"), StatementChoices), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Statement rows: a mid-sentence 'radio waves' mention does not map."),
		BHMapDiagramLabelToChoice(TEXT("radio"), StatementChoices), static_cast<int32>(INDEX_NONE));

	// Degenerate inputs stay inert.
	TestEqual(TEXT("Empty label maps to nothing."),
		BHMapDiagramLabelToChoice(FString(), IdentifyChoices), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Empty choice list maps to nothing."),
		BHMapDiagramLabelToChoice(TEXT("Infrared"), TArray<FString>()), static_cast<int32>(INDEX_NONE));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

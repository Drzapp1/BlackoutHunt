#include "BHGameSettings.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHJumpscareVariantDefaultsTest,
	"BlackoutHunt.Horror.JumpscareVariantDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHJumpscareVariantDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	TestNotNull(TEXT("Game settings exist."), Settings);
	if (!Settings)
	{
		return false;
	}

	TestTrue(TEXT("Default jumpscare pool keeps the existing SCP fallback and three Fab slots."), Settings->JumpscareVariants.Num() >= 4);

	bool bFoundScpFallback = false;
	bool bFoundFabSlot = false;
	for (const FBHJumpscareVariant& Variant : Settings->JumpscareVariants)
	{
		if (Variant.VariantId == TEXT("SCP096"))
		{
			bFoundScpFallback = true;
			TestFalse(TEXT("SCP fallback has a skeletal mesh path."), Variant.SkeletalMesh.IsNull());
		}
		if (Variant.VariantId.ToString().StartsWith(TEXT("FabMonster"), ESearchCase::IgnoreCase))
		{
			bFoundFabSlot = true;
			TestFalse(TEXT("Fab slot has a visual actor or mesh path."), Variant.VisualActorClass.IsNull() && Variant.SkeletalMesh.IsNull() && Variant.StaticMesh.IsNull());
			TestTrue(TEXT("Fab close-up visual is framed below eye height to keep legs out of view."), Variant.CloseVisualOffset.Z < -90.0f);
			TestTrue(TEXT("Fab close-up visual is scaled for face/upper-torso impact."), Variant.CloseVisualScale.GetMin() > 1.0f);
		}
	}

	TestTrue(TEXT("SCP fallback variant is configured."), bFoundScpFallback);
	TestTrue(TEXT("At least one Fab jumpscare variant slot is configured."), bFoundFabSlot);
	TestEqual(TEXT("Full horror is the default revision scare intensity."), Settings->RevisionScareIntensity, 3);
	return true;
}

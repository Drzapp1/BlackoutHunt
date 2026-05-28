#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationTestWorld.h"
#include "BHGameSettings.h"
#include "BHJumpscareMonster.h"
#include "BHJumpscareVariantLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

namespace
{
bool BHTestSoftPathPackageExists(const FSoftObjectPath& ObjectPath)
{
	const FString PackageName = ObjectPath.GetLongPackageName();
	return !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
}
}

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
	const TArray<FBHJumpscareVariant> ResolvedVariants = GetResolvedJumpscareVariants();
	TestTrue(TEXT("Resolved jumpscare pool includes all configured defaults."), ResolvedVariants.Num() >= Settings->JumpscareVariants.Num());

	bool bFoundScpFallback = false;
	bool bFoundFabSlot = false;
	for (const FBHJumpscareVariant& Variant : Settings->JumpscareVariants)
	{
		if (!Variant.VariantId.IsNone())
		{
			FBHJumpscareVariant ResolvedVariant;
			TestTrue(TEXT("Configured jumpscare variant resolves through the shared library."), FindResolvedJumpscareVariantById(Variant.VariantId, ResolvedVariant));
		}
		if (Variant.VariantId == TEXT("SCP096"))
		{
			bFoundScpFallback = true;
			TestFalse(TEXT("SCP fallback has a skeletal mesh path."), Variant.SkeletalMesh.IsNull());
		}
		if (Variant.VariantId.ToString().StartsWith(TEXT("FabMonster"), ESearchCase::IgnoreCase))
		{
			bFoundFabSlot = true;
			TestFalse(TEXT("Fab slot has a visual actor or mesh path."), Variant.VisualActorClass.IsNull() && Variant.SkeletalMesh.IsNull() && Variant.StaticMesh.IsNull());
			TestTrue(TEXT("Fab skeletal mesh is restored under the BlackoutHunt jumpscare path."), Variant.SkeletalMesh.ToSoftObjectPath().ToString().StartsWith(TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/")));
			TestTrue(TEXT("Fab skeletal mesh package exists."), BHTestSoftPathPackageExists(Variant.SkeletalMesh.ToSoftObjectPath()));
			TestTrue(TEXT("Fab material package exists."), BHTestSoftPathPackageExists(Variant.Material.ToSoftObjectPath()));
			TestTrue(TEXT("Fab audio package exists."), BHTestSoftPathPackageExists(Variant.LaunchSound.ToSoftObjectPath()));
			TestTrue(TEXT("Fab close-up visual is framed below eye height to keep legs out of view."), Variant.CloseVisualOffset.Z < -90.0f);
			TestTrue(TEXT("Fab close-up visual is scaled for face/upper-torso impact."), Variant.CloseVisualScale.GetMin() > 1.0f);
		}
	}

	TestTrue(TEXT("SCP fallback variant is configured."), bFoundScpFallback);
	TestTrue(TEXT("At least one Fab jumpscare variant slot is configured."), bFoundFabSlot);
	TestEqual(TEXT("Full horror is the default revision scare intensity."), Settings->RevisionScareIntensity, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHWhisperJumpscareDiscoveryTest,
	"BlackoutHunt.Horror.WhisperJumpscareDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHWhisperJumpscareDiscoveryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	TestNotNull(TEXT("Game settings exist."), Settings);
	if (!Settings)
	{
		return false;
	}

	const TArray<FBHJumpscareVariant> WhisperVariants = GetResolvedWhisperJumpscareVariants();
	const TArray<FBHJumpscareVariant> ResolvedVariants = GetResolvedJumpscareVariants();
	if (WhisperVariants.IsEmpty())
	{
		TestEqual(TEXT("Missing Whisper content root leaves resolved variants equal to configured defaults."), ResolvedVariants.Num(), Settings->JumpscareVariants.Num());
		FBHJumpscareVariant MissingWhisper;
		TestFalse(TEXT("Missing Whisper content does not synthesize Whisper01."), FindResolvedJumpscareVariantById(FName(TEXT("Whisper01")), MissingWhisper));
		return true;
	}

	TestTrue(TEXT("Restored Whisper root discovers at least one variant."), WhisperVariants.Num() > 0);
	FString PreviousVisualPath;
	for (int32 Index = 0; Index < WhisperVariants.Num(); ++Index)
	{
		const FBHJumpscareVariant& Variant = WhisperVariants[Index];
		const FName ExpectedId(*FString::Printf(TEXT("Whisper%02d"), Index + 1));
		const FString VisualPath = !Variant.SkeletalMesh.IsNull()
			? Variant.SkeletalMesh.ToSoftObjectPath().ToString()
			: Variant.StaticMesh.ToSoftObjectPath().ToString();
		TestEqual(TEXT("Whisper variant IDs are stable and sorted."), Variant.VariantId, ExpectedId);
		TestTrue(TEXT("Whisper helper recognizes discovered Whisper variants."), IsWhisperJumpscareVariant(Variant));
		TestTrue(TEXT("Whisper display name is useful in test menus."), Variant.DisplayName.Contains(TEXT("Whisper"), ESearchCase::IgnoreCase));
		TestTrue(TEXT("Whisper variants are sorted by visual asset path."), PreviousVisualPath.IsEmpty() || VisualPath >= PreviousVisualPath);
		PreviousVisualPath = VisualPath;
		TestTrue(TEXT("Whisper variant uses restored project content."), VisualPath.StartsWith(TEXT("/Game/BlackoutHunt/Art/Jumpscares/Whisper/")));
		TestFalse(TEXT("Whisper variant has a visual asset."), Variant.SkeletalMesh.IsNull() && Variant.StaticMesh.IsNull());
		if (!Variant.SkeletalMesh.IsNull())
		{
			TestTrue(TEXT("Whisper skeletal mesh package exists."), BHTestSoftPathPackageExists(Variant.SkeletalMesh.ToSoftObjectPath()));
		}
		if (!Variant.StaticMesh.IsNull())
		{
			TestTrue(TEXT("Whisper static mesh package exists."), BHTestSoftPathPackageExists(Variant.StaticMesh.ToSoftObjectPath()));
		}
		if (!Variant.Material.IsNull())
		{
			TestTrue(TEXT("Whisper material package exists when paired."), BHTestSoftPathPackageExists(Variant.Material.ToSoftObjectPath()));
		}
		if (!Variant.RunAnimation.IsNull())
		{
			TestTrue(TEXT("Whisper animation package exists when paired."), BHTestSoftPathPackageExists(Variant.RunAnimation.ToSoftObjectPath()));
		}
		TestFalse(TEXT("Whisper variant has imported audio or fallback audio."), Variant.LaunchSound.IsNull());
		TestTrue(TEXT("Whisper audio package exists."), BHTestSoftPathPackageExists(Variant.LaunchSound.ToSoftObjectPath()));

		FBHJumpscareVariant FoundVariant;
		TestTrue(TEXT("Resolved variant lookup can find Whisper variants."), FindResolvedJumpscareVariantById(Variant.VariantId, FoundVariant));
		TestEqual(TEXT("Resolved Whisper lookup returns the same stable ID."), FoundVariant.VariantId, Variant.VariantId);
	}

	TestTrue(TEXT("Resolved pool includes configured defaults plus discovered Whisper variants."), ResolvedVariants.Num() >= Settings->JumpscareVariants.Num() + WhisperVariants.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHJumpscareFallbackVisualSizingTest,
	"BlackoutHunt.Horror.FallbackVisualSizing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHJumpscareFallbackVisualSizingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHJumpscareVariant ScpVariant;
	TestTrue(TEXT("SCP096 fallback variant resolves."), FindResolvedJumpscareVariantById(FName(TEXT("SCP096")), ScpVariant));

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntJumpscareFallbackVisual"), true);
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHJumpscareMonster* Monster = World->SpawnActor<ABHJumpscareMonster>(FVector::ZeroVector, FRotator::ZeroRotator);
	TestNotNull(TEXT("Fallback jumpscare monster spawns."), Monster);
	if (!Monster)
	{
		return false;
	}

	Monster->ConfigureCloseupPresentation(ScpVariant, 1.0f, true);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Monster->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	int32 VisiblePrimitiveCount = 0;
	FBox VisibleBounds(ForceInit);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->IsVisible())
		{
			++VisiblePrimitiveCount;
			VisibleBounds += PrimitiveComponent->Bounds.GetBox();
		}
	}

	TestTrue(TEXT("SCP096 fallback has visible procedural primitives when imported assets are missing."), VisiblePrimitiveCount > 0);
	if (VisibleBounds.IsValid)
	{
		TestTrue(TEXT("Close fallback visual is camera-sized instead of full-height."), VisibleBounds.GetSize().Z <= 190.0f);
	}

	return true;
}

#endif

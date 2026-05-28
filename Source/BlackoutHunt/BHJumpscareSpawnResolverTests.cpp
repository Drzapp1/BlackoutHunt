#if WITH_DEV_AUTOMATION_TESTS

#include "BHAutomationTestWorld.h"
#include "BHGameMode.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

namespace
{
AActor* BHSpawnVisibilityBlocker(UWorld* World, const FVector& Location, const FVector& Extent)
{
	AActor* Actor = World ? World->SpawnActor<AActor>(Location, FRotator::ZeroRotator) : nullptr;
	if (!Actor)
	{
		return nullptr;
	}

	UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("VisibilityBlocker"));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHJumpscareSpawnResolverTest,
	"BlackoutHunt.Horror.JumpscareSpawnResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHJumpscareSpawnResolverTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FBHScopedAutomationWorld TestWorld(TEXT("BlackoutHuntJumpscareSpawn"), true);
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

	const FVector ViewLocation(0.0f, 0.0f, 100.0f);
	const FVector ViewForward(1.0f, 0.0f, 0.0f);
	const FVector TargetLocation(0.0f, 0.0f, 0.0f);
	float Score = -1.0f;

	TestTrue(TEXT("Forward unobstructed candidate is accepted."),
		GameMode->DebugValidateJumpscareSpawnCandidate(ViewLocation, ViewForward, TargetLocation, FVector(1200.0f, 0.0f, 4.0f), 150.0f, 28.0f, &Score));
	TestTrue(TEXT("Accepted candidate has a positive score."), Score > 0.0f);

	TestFalse(TEXT("Behind-camera candidate is rejected."),
		GameMode->DebugValidateJumpscareSpawnCandidate(ViewLocation, ViewForward, TargetLocation, FVector(-1200.0f, 0.0f, 4.0f), 150.0f, 28.0f));

	AActor* Occluder = BHSpawnVisibilityBlocker(World, FVector(600.0f, 0.0f, 150.0f), FVector(30.0f, 300.0f, 260.0f));
	TestNotNull(TEXT("Occluder spawns."), Occluder);
	TestFalse(TEXT("Wall-occluded candidate is rejected."),
		GameMode->DebugValidateJumpscareSpawnCandidate(ViewLocation, ViewForward, TargetLocation, FVector(1200.0f, 0.0f, 4.0f), 150.0f, 28.0f));
	if (Occluder)
	{
		Occluder->Destroy();
	}

	AActor* SpawnBlocker = BHSpawnVisibilityBlocker(World, FVector(1200.0f, 220.0f, 146.0f), FVector(95.0f, 95.0f, 170.0f));
	TestNotNull(TEXT("Spawn blocker spawns."), SpawnBlocker);
	TestFalse(TEXT("Blocked spawn capsule candidate is rejected."),
		GameMode->DebugValidateJumpscareSpawnCandidate(ViewLocation, ViewForward, TargetLocation, FVector(1200.0f, 220.0f, 4.0f), 150.0f, 28.0f));
	if (SpawnBlocker)
	{
		SpawnBlocker->Destroy();
	}

	TestFalse(TEXT("Resolver failure condition is available for close-overlay fallback."),
		GameMode->DebugValidateJumpscareSpawnCandidate(ViewLocation, ViewForward, TargetLocation, FVector(0.0f, -1400.0f, 4.0f), 150.0f, 28.0f));

	return true;
}

#endif

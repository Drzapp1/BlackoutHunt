#if WITH_DEV_AUTOMATION_TESTS

#include "BHFlickerLight.h"
#include "BHGameMode.h"
#include "BHSecurityShutter.h"
#include "BHSecurityTerminal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"

namespace
{
UWorld* BHCreateProofTestWorld()
{
	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString PackageName = FString::Printf(TEXT("/Temp/BlackoutHuntProof_%s"), *UniqueSuffix);
	UPackage* Package = CreatePackage(*PackageName);
	const FName WorldName(*FString::Printf(TEXT("BlackoutHuntProof_%s"), *UniqueSuffix.Left(12)));

	UWorld::InitializationValues InitValues;
	InitValues
		.AllowAudioPlayback(false)
		.RequiresHitProxies(false)
		.CreatePhysicsScene(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.ShouldSimulatePhysics(false)
		.SetTransactional(false);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, Package, false, ERHIFeatureLevel::Num, &InitValues);
	if (World)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
	}
	return World;
}

void BHDestroyProofTestWorld(UWorld*& World)
{
	if (World)
	{
		if (World->HasBegunPlay())
		{
			World->BeginTearingDown();
			World->EndPlay(EEndPlayReason::Quit);
		}
		UPackage* Package = World->GetOutermost();
		if (GEngine)
		{
			GEngine->DestroyWorldContext(World);
		}
		World->DestroyWorld(false);
		World->MarkAsGarbage();
		if (Package)
		{
			Package->MarkAsGarbage();
		}
		World = nullptr;
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHFlickerBurstRestoreTest,
	"BlackoutHunt.Horror.FlickerBurstRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHFlickerBurstRestoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = BHCreateProofTestWorld();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHFlickerLight* Light = World->SpawnActor<ABHFlickerLight>();
	TestNotNull(TEXT("Flicker light spawns."), Light);
	if (Light)
	{
		TestTrue(TEXT("Flicker light starts powered."), Light->IsPowered());
		Light->TriggerFlickerBurst(0.1f, 1.8f, FLinearColor::Red, 18.0f, true);
		TestTrue(TEXT("Temporary flicker burst starts."), Light->IsFlickerBurstActive());
		for (int32 TickIndex = 0; TickIndex < 8 && Light->IsFlickerBurstActive(); ++TickIndex)
		{
			World->TimeSeconds += 0.05;
			World->UnpausedTimeSeconds += 0.05;
			World->RealTimeSeconds += 0.05;
			World->AudioTimeSeconds += 0.05;
			World->DeltaTimeSeconds = 0.05f;
			World->DeltaRealTimeSeconds = 0.05f;
			World->GetTimerManager().Tick(0.05f);
			Light->Tick(0.05f);
		}
		TestFalse(TEXT("Temporary flicker burst restores after its timer."), Light->IsFlickerBurstActive());
		TestTrue(TEXT("Temporary flicker burst does not permanently cut power."), Light->IsPowered());
	}

	BHDestroyProofTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHBotMemoryNoiseStimulusTest,
	"BlackoutHunt.AI.BotMemoryNoiseStimulus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHBotMemoryNoiseStimulusTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = BHCreateProofTestWorld();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (GameMode)
	{
		const FVector NoiseLocation(125.0f, -350.0f, 98.0f);
		GameMode->ReportBotStimulus(EBHBotStimulusType::Noise, NoiseLocation, nullptr, nullptr, TEXT("automation footstep"), 1.0f);

		FBHBotMemory Memory;
		TestTrue(TEXT("Bot memory snapshot reports recent stimuli."), GameMode->GetBotWorldMemorySnapshot(Memory, 2.0f));
		TestEqual(TEXT("Bot memory records the latest noise location."), Memory.LastHeardLocation, NoiseLocation);
		TestTrue(TEXT("Bot memory stores the recent noise stimulus."), Memory.RecentStimuli.Num() == 1 && Memory.RecentStimuli[0].Type == EBHBotStimulusType::Noise);
	}

	BHDestroyProofTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHHostAuthorityRejectsNullRequesterTest,
	"BlackoutHunt.Network.HostAuthorityRejectsNullRequester",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHHostAuthorityRejectsNullRequesterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = BHCreateProofTestWorld();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);
	if (GameMode)
	{
		TestFalse(TEXT("Host-only actions reject missing or non-authoritative requesters."), GameMode->RequireHostAdmin(nullptr, TEXT("automation authority test")));
	}

	BHDestroyProofTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHSecurityShutterCircuitTest,
	"BlackoutHunt.Security.ShutterCircuitMovesAndBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHSecurityShutterCircuitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = BHCreateProofTestWorld();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
	{
		return false;
	}

	ABHGameMode* GameMode = World->SpawnActor<ABHGameMode>();
	TestNotNull(TEXT("Game mode spawns."), GameMode);

	ABHSecurityShutter* ShutterA = World->SpawnActor<ABHSecurityShutter>();
	ABHSecurityShutter* ShutterB = World->SpawnActor<ABHSecurityShutter>(FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ABHSecurityTerminal* Terminal = World->SpawnActor<ABHSecurityTerminal>(FVector(600.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("First shutter spawns."), ShutterA);
	TestNotNull(TEXT("Second shutter spawns."), ShutterB);
	TestNotNull(TEXT("Security terminal spawns."), Terminal);
	if (GameMode && ShutterA && ShutterB)
	{
		ShutterA->Configure(77);
		ShutterB->Configure(77);
		if (Terminal)
		{
			Terminal->Configure(77, FText::FromString(TEXT("Toggle Test Shutters")));
		}

		UStaticMeshComponent* ShutterMesh = ShutterA->FindComponentByClass<UStaticMeshComponent>();
		TestNotNull(TEXT("Shutter has a mesh."), ShutterMesh);
		const FVector ClosedMeshLocation = ShutterMesh ? ShutterMesh->GetRelativeLocation() : FVector::ZeroVector;
		auto FinishShutterAnimation = [ShutterA, ShutterB]()
		{
			ShutterA->Tick(0.25f);
			ShutterB->Tick(0.25f);
		};

		if (Terminal)
		{
			TestEqual(TEXT("Terminal prompt starts with the opening action."), Terminal->GetInteractionLabel_Implementation(nullptr).ToString(), FString(TEXT("Open Test Shutters")));
		}

		GameMode->ToggleSecurityCircuit(77);
		FinishShutterAnimation();
		TestTrue(TEXT("Circuit toggle opens first shutter."), ShutterA->IsOpen());
		TestTrue(TEXT("Circuit toggle opens second shutter."), ShutterB->IsOpen());
		if (Terminal)
		{
			TestEqual(TEXT("Terminal prompt changes to the closing action."), Terminal->GetInteractionLabel_Implementation(nullptr).ToString(), FString(TEXT("Close Test Shutters")));
		}
		if (ShutterMesh)
		{
			TestTrue(TEXT("Open shutter visibly lifts its mesh."), ShutterMesh->GetRelativeLocation().Z > ClosedMeshLocation.Z + 1.0f);
			TestEqual(TEXT("Open shutter stops blocking pawns."), ShutterMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
		}

		GameMode->ToggleSecurityCircuit(77);
		FinishShutterAnimation();
		TestFalse(TEXT("Second circuit toggle closes first shutter."), ShutterA->IsOpen());
		TestFalse(TEXT("Second circuit toggle closes second shutter."), ShutterB->IsOpen());
		if (ShutterMesh)
		{
			TestEqual(TEXT("Closed shutter returns to its original mesh location."), ShutterMesh->GetRelativeLocation(), ClosedMeshLocation);
			TestEqual(TEXT("Closed shutter blocks pawns again."), ShutterMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
		}
	}

	BHDestroyProofTestWorld(World);
	return true;
}

#endif

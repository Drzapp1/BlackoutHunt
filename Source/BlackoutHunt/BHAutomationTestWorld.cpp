// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHAutomationTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/Guid.h"
#include "TimerManager.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

FBHScopedAutomationWorld::FBHScopedAutomationWorld(const TCHAR* InBaseName, bool bCreatePhysicsScene, bool bCreateNavigation, bool bCreateAISystem)
{
	if (!GEngine)
	{
		return;
	}

	const FString BaseName = InBaseName && *InBaseName ? InBaseName : TEXT("BlackoutHuntAutomation");
	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString PackageName = FString::Printf(TEXT("/Temp/%s_%s"), *BaseName, *UniqueSuffix);
	Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return;
	}

	const FName WorldName(*FString::Printf(TEXT("%s_%s"), *BaseName, *UniqueSuffix.Left(12)));
	UWorld::InitializationValues InitValues;
	InitValues
		.AllowAudioPlayback(false)
		.RequiresHitProxies(false)
		.CreatePhysicsScene(bCreatePhysicsScene)
		.CreateNavigation(bCreateNavigation)
		.CreateAISystem(bCreateAISystem)
		.ShouldSimulatePhysics(false)
		.SetTransactional(false);

	World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, Package, false, ERHIFeatureLevel::Num, &InitValues);
	if (!World)
	{
		Package->MarkAsGarbage();
		Package = nullptr;
		return;
	}

	// Anchor the world (and its package) against GC for the harness lifetime. Today only the world
	// context's reference keeps it alive across an in-test CollectGarbage(); root it explicitly so a
	// test that collects garbage can't pull the world out from under itself.
	World->AddToRoot();
	if (Package)
	{
		Package->AddToRoot();
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
}

FBHScopedAutomationWorld::~FBHScopedAutomationWorld()
{
	UWorld* WorldToDestroy = World;
	UPackage* PackageToDestroy = Package;
	World = nullptr;
	Package = nullptr;

	if (!WorldToDestroy)
	{
		return;
	}

	WorldToDestroy->RemoveFromRoot();
	if (PackageToDestroy)
	{
		PackageToDestroy->RemoveFromRoot();
	}

	WorldToDestroy->GetTimerManager().ClearAllTimersForObject(WorldToDestroy);
	if (WorldToDestroy->HasBegunPlay())
	{
		WorldToDestroy->BeginTearingDown();
		WorldToDestroy->EndPlay(EEndPlayReason::Quit);
	}

	if (GEngine)
	{
		GEngine->DestroyWorldContext(WorldToDestroy);
	}

	WorldToDestroy->DestroyWorld(false);
	WorldToDestroy->MarkAsGarbage();
	if (PackageToDestroy)
	{
		PackageToDestroy->MarkAsGarbage();
	}

	CollectGarbage(RF_NoFlags, true);
}

#endif

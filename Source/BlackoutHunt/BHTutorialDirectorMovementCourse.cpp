// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

// Defines ABHTutorialDirector::BuildMovementCourse out-of-line (mirroring BHGameModeEasterEgg.cpp, which splits
// ABHGameMode::BuildEasterEggRoom out of the large BHGameMode.cpp). It spawns the Movement tutorial's small practice
// course -- a waist-high cover wall + low overhang to roll/slide under, a drop ledge to drop-roll off, and a
// stationary silhouette "chase dummy" framed in a doorway -- so the advanced-movement lesson has real targets to
// practise against. The director cannot call ABHGameMode::SpawnBlock (a game-mode method), so it spawns ABHBlockActor
// actors directly (SpawnActor + SetActorScale3D / SetVisualTint / SetBlockMaterial / SetBlockCollisionEnabled).
// Props sit in the clear western practice bay (X ~700..1300): clear of the lone Movement spawn at X=-200 and of the
// first partition wall at X=1600, in the open space the player stands in for the early move steps.

#include "BHTutorialDirector.h"

#include "BHBlockActor.h"
#include "BHLocker.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	// File-local mirrors of the block-Z helpers used across the level builders. A block's default mesh is a 100 cm
	// cube, so ScaleZ 1.0 == 100 cm tall and the Z passed to SpawnActor is the block CENTRE. These convert a desired
	// bottom/top world Z into that centre. Distinct names so they can't collide in a unity build.
	float CourseCenterZForBottom(float BottomZ, float ScaleZ) { return BottomZ + ScaleZ * 50.0f; }
	float CourseCenterZForTop(float TopZ, float ScaleZ) { return TopZ - ScaleZ * 50.0f; }
}

void ABHTutorialDirector::BuildMovementCourse()
{
	UWorld* World = GetWorld();
	if (!World || bMovementCourseBuilt)
	{
		return;
	}
	bMovementCourseBuilt = true;

	// Spawn one ABHBlockActor (mirrors how the game mode spawns dynamic blocks). Scale is in 100 cm-cube units.
	auto SpawnProp = [World](const FVector& Center, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rot, bool bCollides, EBHBlockMaterial Material)
	{
		if (ABHBlockActor* Block = World->SpawnActor<ABHBlockActor>(Center, Rot))
		{
			Block->SetActorScale3D(Scale);
			Block->SetVisualTint(Tint);
			Block->SetBlockMaterial(Material);
			Block->SetBlockCollisionEnabled(bCollides);
		}
	};

	const FLinearColor CoverTint(0.30f, 0.33f, 0.20f, 1.0f);   // olive cover
	const FLinearColor LedgeTint(0.22f, 0.24f, 0.27f, 1.0f);   // grey platform
	const FLinearColor DummyTint(0.45f, 0.10f, 0.10f, 1.0f);   // red silhouette so it reads as a target
	const FLinearColor FrameTint(0.18f, 0.20f, 0.23f, 1.0f);   // doorway frame

	// --- (a1) Waist-high COVER WALL + low OVERHANG (roll / slide-under target), to the north of centre so it does
	// not block the open running lane along Y=0. Cover wall ~110 cm tall; the overhang roof is a thin slab at ~150 cm
	// leaving a low gap a standing pawn must roll/slide under.
	const FVector CoverWall(900.0f, 450.0f, 0.0f);
	SpawnProp(FVector(CoverWall.X, CoverWall.Y, CourseCenterZForBottom(0.0f, 1.1f)), FVector(0.3f, 4.0f, 1.1f), CoverTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnProp(FVector(CoverWall.X + 130.0f, CoverWall.Y, CourseCenterZForBottom(150.0f, 0.12f)), FVector(2.6f, 4.0f, 0.12f), FrameTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	CoverWallLoc = CoverWall + FVector(0.0f, 0.0f, 90.0f);
	OverhangLoc = FVector(CoverWall.X + 130.0f, CoverWall.Y, 150.0f);

	// --- (a2) DROP LEDGE (drop-roll target): a ~135 cm platform to the south, with a three-step stair on its west
	// face whose rises are each ~44 cm so they are WALKABLE at the default character MaxStepHeight (45 cm) -- the
	// player walks up, then drop-rolls off the east edge back onto the main floor. (Earlier 90/80 cm rises were not
	// walkable and had to be jumped, reading as broken geometry.)
	const FVector Ledge(900.0f, -550.0f, 0.0f);
	const float LedgeTop = 134.0f;
	SpawnProp(FVector(Ledge.X, Ledge.Y, CourseCenterZForTop(LedgeTop, 0.25f)), FVector(4.5f, 4.5f, 0.25f), LedgeTint, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
	SpawnProp(FVector(Ledge.X - 340.0f, Ledge.Y, CourseCenterZForTop(44.0f, 0.44f)), FVector(0.8f, 4.5f, 0.44f), LedgeTint, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);   // step top 44
	SpawnProp(FVector(Ledge.X - 260.0f, Ledge.Y, CourseCenterZForTop(88.0f, 0.88f)), FVector(0.8f, 4.5f, 0.88f), LedgeTint, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);   // step top 88
	SpawnProp(FVector(Ledge.X - 180.0f, Ledge.Y, CourseCenterZForTop(132.0f, 1.32f)), FVector(0.8f, 4.5f, 1.32f), LedgeTint, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate); // step top 132 (~ledge)
	DropLedgeLoc = FVector(Ledge.X + 150.0f, Ledge.Y, LedgeTop + 60.0f); // the east lip you drop off

	// --- (a3) Stationary CHASE DUMMY in a DOORWAY (dive / flow-chain target): a doorway frame astride the running
	// lane with a tall thin red silhouette "dummy" standing in the gap, so diving/rolling past it reads as juking a
	// Teacher in a chokepoint. The dummy does NOT collide (you pass through it -- a visual target, not a wall).
	const FVector Door(1300.0f, 0.0f, 0.0f);
	SpawnProp(FVector(Door.X, Door.Y - 230.0f, CourseCenterZForBottom(0.0f, 2.8f)), FVector(0.3f, 1.0f, 2.8f), FrameTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnProp(FVector(Door.X, Door.Y + 230.0f, CourseCenterZForBottom(0.0f, 2.8f)), FVector(0.3f, 1.0f, 2.8f), FrameTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnProp(FVector(Door.X, Door.Y, CourseCenterZForBottom(220.0f, 0.3f)), FVector(0.3f, 3.6f, 0.3f), FrameTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete); // lintel
	SpawnProp(FVector(Door.X, Door.Y, CourseCenterZForBottom(0.0f, 1.85f)), FVector(0.55f, 0.55f, 1.85f), DummyTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal); // the dummy (non-colliding)
	ChaseDummyLoc = FVector(Door.X, Door.Y, 120.0f);

	// --- (a4) A practice LOCKER in the bay (north-east corner) for the LockerRoll step, so the marker points at a
	// nearby locker instead of yanking the player two rooms east to the nearest map locker. Spawned the same way the
	// game mode spawns tutorial lockers (Z=95). Its location is cached so PublishStepBeat's LockerRoll beat uses it.
	if (ABHLocker* Locker = World->SpawnActor<ABHLocker>(FVector(1150.0f, 600.0f, 95.0f), FRotator(0.0f, 180.0f, 0.0f)))
	{
		PracticeLockerLoc = Locker->GetActorLocation();
	}

	// A floating "DUMMY" plaque over the silhouette so it reads as a practice target, not a real Teacher
	// (lightweight UTextRenderComponent on a bare actor -- same approach as the easter-egg plaques).
	if (AActor* A = World->SpawnActor<AActor>(FVector(Door.X, Door.Y, 250.0f), FRotator(0.0f, 180.0f, 0.0f)))
	{
		if (UTextRenderComponent* T = NewObject<UTextRenderComponent>(A))
		{
			A->SetRootComponent(T);
			T->RegisterComponent();
			T->SetText(FText::FromString(TEXT("DUMMY")));
			T->SetTextRenderColor(FColor(255, 90, 90, 255));
			T->SetWorldSize(28.0f);
			T->SetHorizontalAlignment(EHTA_Center);
			T->SetVerticalAlignment(EVRTA_TextCenter);
		}
	}
}

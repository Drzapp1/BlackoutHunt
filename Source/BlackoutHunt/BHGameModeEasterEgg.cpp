// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

// Defines ABHGameMode::BuildEasterEggRoom out-of-line (mirroring BHGameModeBotServices.cpp etc.) so the hidden
// easter-egg HUB can be authored without touching the large BHGameMode.cpp. Reached via EBHTutorialPhase::EasterEgg
// (Escape at the main menu): a clear central gallery ringed by themed sector zones + skill rooms + a secret egg map.

#include "BHGameMode.h"

#include "BHAccountSubsystem.h"
#include "BHBlockActor.h"
#include "BHFlickerLight.h"
#include "BHTypes.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	// File-local mirrors of the block-placement helpers in BHGameMode.cpp (those are translation-unit-local
	// there, so this out-of-line builder needs its own copies). Added to unblock the shared build.
	float CenterZForBlockBottom(float BottomZ, float ScaleZ) { return BottomZ + ScaleZ * 50.0f; }
	float CenterZForBlockTop(float TopZ, float ScaleZ) { return TopZ - ScaleZ * 50.0f; }
}

void ABHGameMode::BuildEasterEggRoom()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// One big CLEAR central gallery at the origin (the main area is kept open), ringed by themed sector ZONES marked
	// with glowing label plaques; doorways in the N/S walls branch to skill rooms (Movement range / Teacher evasion)
	// and a gap in the east wall leads to the SECRET room that maps the hidden eggs. Player spawns dead centre.
	const FLinearColor FloorTint(0.10f, 0.11f, 0.13f, 1.0f);
	const FLinearColor CeilTint(0.05f, 0.06f, 0.07f, 1.0f);
	const FLinearColor WallTint(0.20f, 0.23f, 0.30f, 1.0f);

	const float HX = 1200.0f; // gallery half-extent X
	const float HY = 900.0f;  // gallery half-extent Y
	const float WallH = 3.6f; // 360 cm
	const float DoorHalf = 170.0f;

	// Gallery floor + ceiling.
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.25f)), FVector(HX * 2.0f / 100.0f, HY * 2.0f / 100.0f, 0.25f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(360.0f, 0.15f)), FVector(HX * 2.0f / 100.0f, HY * 2.0f / 100.0f, 0.15f), CeilTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	// West wall solid; east wall split around a doorway to the secret room.
	SpawnBlock(FVector(-HX, 0.0f, CenterZForBlockBottom(0.0f, WallH)), FVector(0.3f, HY * 2.0f / 100.0f, WallH), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	const float EastSegLen = (HY - DoorHalf) / 100.0f;
	SpawnBlock(FVector(HX, (HY + DoorHalf) * 0.5f, CenterZForBlockBottom(0.0f, WallH)), FVector(0.3f, EastSegLen, WallH), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(HX, -(HY + DoorHalf) * 0.5f, CenterZForBlockBottom(0.0f, WallH)), FVector(0.3f, EastSegLen, WallH), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);

	// North + south walls, each split around a central doorway to a skill room.
	const float NSSegLen = (HX - DoorHalf) / 100.0f;
	for (float Sy : { HY, -HY })
	{
		SpawnBlock(FVector((HX + DoorHalf) * 0.5f, Sy, CenterZForBlockBottom(0.0f, WallH)), FVector(NSSegLen, 0.3f, WallH), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
		SpawnBlock(FVector(-(HX + DoorHalf) * 0.5f, Sy, CenterZForBlockBottom(0.0f, WallH)), FVector(NSSegLen, 0.3f, WallH), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	}

	// Skill rooms beyond the N/S doorways + the secret room beyond the east doorway (enclosed boxes, open toward the hub).
	auto BuildSideRoom = [&](FVector Center, float RoomHX, float RoomHY, FLinearColor Tint, EBHBlockMaterial Mat, int32 OpenSide)
	{
		SpawnBlock(FVector(Center.X, Center.Y, CenterZForBlockTop(0.0f, 0.25f)), FVector(RoomHX * 2.0f / 100.0f, RoomHY * 2.0f / 100.0f, 0.25f), Tint * 0.5f, FRotator::ZeroRotator, true, Mat);
		SpawnBlock(FVector(Center.X, Center.Y, CenterZForBlockBottom(340.0f, 0.15f)), FVector(RoomHX * 2.0f / 100.0f, RoomHY * 2.0f / 100.0f, 0.15f), CeilTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
		if (OpenSide != 0) { SpawnBlock(FVector(Center.X - RoomHX, Center.Y, CenterZForBlockBottom(0.0f, 3.4f)), FVector(0.3f, RoomHY * 2.0f / 100.0f, 3.4f), WallTint, FRotator::ZeroRotator, true, Mat); }
		if (OpenSide != 1) { SpawnBlock(FVector(Center.X + RoomHX, Center.Y, CenterZForBlockBottom(0.0f, 3.4f)), FVector(0.3f, RoomHY * 2.0f / 100.0f, 3.4f), WallTint, FRotator::ZeroRotator, true, Mat); }
		if (OpenSide != 2) { SpawnBlock(FVector(Center.X, Center.Y - RoomHY, CenterZForBlockBottom(0.0f, 3.4f)), FVector(RoomHX * 2.0f / 100.0f, 0.3f, 3.4f), WallTint, FRotator::ZeroRotator, true, Mat); }
		if (OpenSide != 3) { SpawnBlock(FVector(Center.X, Center.Y + RoomHY, CenterZForBlockBottom(0.0f, 3.4f)), FVector(RoomHX * 2.0f / 100.0f, 0.3f, 3.4f), WallTint, FRotator::ZeroRotator, true, Mat); }
		if (ABHFlickerLight* L = World->SpawnActor<ABHFlickerLight>(FVector(Center.X, Center.Y, 300.0f), FRotator::ZeroRotator)) { (void)L; }
	};
	BuildSideRoom(FVector(0.0f, HY + 700.0f, 0.0f), 700.0f, 600.0f, FLinearColor(0.16f, 0.30f, 0.34f, 1.0f), EBHBlockMaterial::ConcreteWA, 2); // Movement range (opens south)
	BuildSideRoom(FVector(0.0f, -(HY + 700.0f), 0.0f), 700.0f, 600.0f, FLinearColor(0.34f, 0.16f, 0.16f, 1.0f), EBHBlockMaterial::ConcreteWA, 3); // Teacher evasion (opens north)
	BuildSideRoom(FVector(HX + 650.0f, 0.0f, 0.0f), 650.0f, 550.0f, FLinearColor(0.30f, 0.26f, 0.10f, 1.0f), EBHBlockMaterial::Concrete, 0); // Secret room (opens west)

	for (float Lx : { -700.0f, 700.0f })
	{
		if (ABHFlickerLight* L = World->SpawnActor<ABHFlickerLight>(FVector(Lx, 0.0f, 320.0f), FRotator::ZeroRotator)) { (void)L; }
	}

	// Glowing label/stat plaques (UTextRenderComponent on lightweight actors). Stats are read from the local account
	// subsystem (this runs on the listen-server host, where the subsystem lives).
	auto SpawnPlaque = [&](FVector Loc, FRotator Rot, const FString& Text, FLinearColor Color, float Size)
	{
		AActor* A = World->SpawnActor<AActor>(Loc, Rot);
		if (!A) { return; }
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(A);
		if (!T) { return; }
		A->SetRootComponent(T);
		T->RegisterComponent();
		T->SetText(FText::FromString(Text));
		T->SetTextRenderColor(Color.ToFColor(true));
		T->SetWorldSize(Size);
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
	};

	const FLinearColor LabelGold(1.0f, 0.82f, 0.32f, 1.0f);
	const FLinearColor LabelCyan(0.5f, 0.85f, 1.0f, 1.0f);
	SpawnPlaque(FVector(0.0f, HY - 30.0f, 240.0f), FRotator(0.0f, -90.0f, 0.0f), TEXT("MOVEMENT RANGE  >"), LabelCyan, 36.0f);
	SpawnPlaque(FVector(0.0f, -HY + 30.0f, 240.0f), FRotator(0.0f, 90.0f, 0.0f), TEXT("<  TEACHER EVASION"), LabelCyan, 36.0f);
	SpawnPlaque(FVector(HX - 30.0f, 0.0f, 240.0f), FRotator(0.0f, 180.0f, 0.0f), TEXT("?  ?  ?"), FLinearColor(0.5f, 0.2f, 0.5f, 1.0f), 30.0f);
	SpawnPlaque(FVector(-HX + 30.0f, 0.0f, 250.0f), FRotator(0.0f, 0.0f, 0.0f), TEXT("BLACKOUT HUNT  --  the faculty sees you."), LabelGold, 34.0f);
	SpawnPlaque(FVector(-HX + 40.0f, 350.0f, 200.0f), FRotator(0.0f, 0.0f, 0.0f), TEXT("TROPHY HALL"), LabelGold, 40.0f);

	FString StatsText = TEXT("YOUR RECORD");
	int32 Earned = 0, Total = 0;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UBHAccountSubsystem* Account = GI->GetSubsystem<UBHAccountSubsystem>())
		{
			const FBHAccountProgress& P = Account->GetProgress();
			Account->GetAchievementCounts(Earned, Total);
			int32 CorrectSum = 0, AnswerSum = 0;
			for (int32 c : P.TopicCorrectCounts) { CorrectSum += c; }
			for (int32 a : P.TopicAnswerCounts) { AnswerSum += a; }
			const int32 MasteryPct = AnswerSum > 0 ? FMath::RoundToInt(100.0f * CorrectSum / AnswerSum) : 0;
			StatsText = FString::Printf(TEXT("YOUR RECORD\nRounds: %d\nSurvivor wins: %d   Hunter wins: %d\nEscapes: %d\nBest streak: %d\nMastery: %d%%\nAchievements: %d / %d\nXP: %d"),
				P.RoundsPlayed, P.SurvivorWins, P.HunterWins, P.Escapes, P.BestWinStreak, MasteryPct, Earned, Total, P.XP);
		}
	}
	SpawnPlaque(FVector(-HX + 40.0f, -350.0f, 170.0f), FRotator(0.0f, 0.0f, 0.0f), StatsText, LabelGold, 24.0f);

	SpawnPlaque(FVector(HX + 650.0f, 0.0f, 210.0f), FRotator(0.0f, 180.0f, 0.0f),
		TEXT("HIDDEN EGGS\n- Konami code on the menu (Up Up Down Down L R L R B A)\n- Train roof hatch (ride the intermission carriage)\n- The locker that exits onto the roof\n- Press Esc at the menu... you already found this one."),
		FLinearColor(0.7f, 0.5f, 0.9f, 1.0f), 20.0f);

	SurvivorSpawns = { FVector(0.0f, 0.0f, 120.0f) };
	HunterSpawn = FVector(0.0f, -(HY + 700.0f), 120.0f); // parked in the teacher-evasion room for that drill
}

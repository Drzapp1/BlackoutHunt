// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

// Defines ABHGameMode::BuildEasterEggRoom out-of-line (mirroring BHGameModeBotServices.cpp etc.) so the hidden
// easter-egg HUB can be authored without touching the large BHGameMode.cpp. Reached via EBHTutorialPhase::EasterEgg
// (Escape at the main menu).
//
// THE VOID -- a secret neon playground, deliberately NOTHING like the grey concrete rooms elsewhere. The player
// materialises inside a glowing henge at the centre of a dark void hub; four corner pockets each have their own
// theme + real, playable props (a personalised TROPHY shrine whose pillars grow with your record, an after-hours
// dance club, a cosy lounge, an aquarium deep); branch chambers off the hub hold an arcade, two training sims, and
// a secret-inside-the-secret switch room. All lit by coloured point lights instead of the game's usual flicker dread.

#include "BHGameMode.h"

#include "BHAccountSubsystem.h"
#include "BHBlockActor.h"
#include "BHFlickerLight.h"
#include "BHTrainAquarium.h"
#include "BHTrainCat.h"
#include "BHTrainChessTable.h"
#include "BHTrainDanceFloor.h"
#include "BHTrainDartboard.h"
#include "BHTrainFireplace.h"
#include "BHTrainGhost.h"
#include "BHTrainJukebox.h"
#include "BHTrainSecretSwitch.h"
#include "BHTrainSlotMachine.h"
#include "BHTrainTicTacToeTable.h"
#include "BHTrainToyTrain.h"
#include "BHTrainVendingMachine.h"
#include "BHTypes.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/PointLight.h"
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

	// ---- Palette -------------------------------------------------------------------------------------------
	const FLinearColor VoidFloor(0.020f, 0.025f, 0.035f, 1.0f);
	const FLinearColor CeilTint(0.010f, 0.012f, 0.020f, 1.0f);
	const FLinearColor WallTint(0.045f, 0.055f, 0.085f, 1.0f);
	const FLinearColor Cyan(0.15f, 0.95f, 1.0f, 1.0f);
	const FLinearColor Magenta(1.0f, 0.15f, 0.70f, 1.0f);
	const FLinearColor Lime(0.50f, 1.0f, 0.25f, 1.0f);
	const FLinearColor Gold(1.0f, 0.82f, 0.32f, 1.0f);
	const FLinearColor Purple(0.65f, 0.35f, 1.0f, 1.0f);
	const FLinearColor Orange(1.0f, 0.55f, 0.12f, 1.0f);
	const FLinearColor NeonRed(1.0f, 0.18f, 0.18f, 1.0f);

	// ---- Helpers -------------------------------------------------------------------------------------------
	// Floor / ceiling slab. Scale is in metres (100 cm). Floors overlap the hub at each doorway so there is
	// never a gap to fall through.
	auto Floor = [&](float Cx, float Cy, float Sx, float Sy, EBHBlockMaterial Mat)
	{
		SpawnBlock(FVector(Cx, Cy, CenterZForBlockTop(0.0f, 0.3f)), FVector(Sx, Sy, 0.3f), VoidFloor, FRotator::ZeroRotator, true, Mat);
	};
	auto Ceil = [&](float Cx, float Cy, float Sx, float Sy)
	{
		SpawnBlock(FVector(Cx, Cy, CenterZForBlockBottom(480.0f, 0.2f)), FVector(Sx, Sy, 0.2f), CeilTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted);
	};
	// A 4.8 m tall solid wall segment. Sx/Sy in metres; one of them is 0.3 (the 30 cm thickness).
	auto WallSeg = [&](float Cx, float Cy, float Sx, float Sy)
	{
		SpawnBlock(FVector(Cx, Cy, CenterZForBlockBottom(0.0f, 4.8f)), FVector(Sx, Sy, 4.8f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted);
	};
	// A bright, NON-colliding decorative block (glow strips, pillar caps, signage).
	auto Glow = [&](FVector Loc, FVector Scale, FLinearColor Tint)
	{
		SpawnBlock(Loc, Scale, Tint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
	};
	// Steady coloured point light (no flicker -- this is a reward space, not a horror corridor).
	auto AccentLight = [&](FVector Loc, FLinearColor Color, float Intensity, float Radius)
	{
		if (APointLight* L = World->SpawnActor<APointLight>(Loc, FRotator::ZeroRotator))
		{
			L->SetMobility(EComponentMobility::Movable);
			if (UPointLightComponent* PLC = Cast<UPointLightComponent>(L->GetLightComponent()))
			{
				PLC->SetLightColor(Color);
				PLC->SetIntensity(Intensity);
				PLC->SetAttenuationRadius(Radius);
				PLC->SetCastShadows(false);
			}
		}
	};
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

	// ---- Shell: one continuous floor + ceiling per region (branch floors overlap the hub), perimeter walls
	//      with doorway gaps toward each branch. ---------------------------------------------------------------
	Floor(0.0f, 0.0f, 30.0f, 24.0f, EBHBlockMaterial::Tinted);     // hub  X[-1500,1500] Y[-1200,1200]
	Floor(0.0f, 1600.0f, 14.0f, 10.0f, EBHBlockMaterial::Tinted);  // N chamber (overlaps hub to Y1100)
	Floor(0.0f, -1600.0f, 14.0f, 10.0f, EBHBlockMaterial::Tinted); // S chamber
	Floor(1950.0f, 0.0f, 11.0f, 14.0f, EBHBlockMaterial::Tinted);  // E arcade (overlaps to X1400)
	Floor(-1700.0f, 0.0f, 6.0f, 9.0f, EBHBlockMaterial::Tinted);   // W secret alcove (overlaps to X-1400)

	Ceil(0.0f, 0.0f, 30.0f, 24.0f);
	Ceil(0.0f, 1600.0f, 14.0f, 10.0f);
	Ceil(0.0f, -1600.0f, 14.0f, 10.0f);
	Ceil(1950.0f, 0.0f, 11.0f, 14.0f);
	Ceil(-1700.0f, 0.0f, 6.0f, 9.0f);

	// Hub perimeter (doorway gaps: N/S at X[-700,700], E at Y[-700,700], W at Y[-450,450]).
	WallSeg(-1500.0f, -825.0f, 0.3f, 7.5f);  // west, south of alcove
	WallSeg(-1500.0f, 825.0f, 0.3f, 7.5f);   // west, north of alcove
	WallSeg(1500.0f, -950.0f, 0.3f, 5.0f);   // east, south of arcade door
	WallSeg(1500.0f, 950.0f, 0.3f, 5.0f);    // east, north of arcade door
	WallSeg(-1100.0f, 1200.0f, 8.0f, 0.3f);  // north, west of movement door
	WallSeg(1100.0f, 1200.0f, 8.0f, 0.3f);   // north, east of movement door
	WallSeg(-1100.0f, -1200.0f, 8.0f, 0.3f); // south, west of evasion door
	WallSeg(1100.0f, -1200.0f, 8.0f, 0.3f);  // south, east of evasion door

	// Branch chamber outer walls (open side faces the hub).
	WallSeg(0.0f, 2100.0f, 14.0f, 0.3f);   // N chamber back
	WallSeg(700.0f, 1600.0f, 0.3f, 10.0f); // N chamber east
	WallSeg(-700.0f, 1600.0f, 0.3f, 10.0f);// N chamber west
	WallSeg(0.0f, -2100.0f, 14.0f, 0.3f);  // S chamber back
	WallSeg(700.0f, -1600.0f, 0.3f, 10.0f);// S chamber east
	WallSeg(-700.0f, -1600.0f, 0.3f, 10.0f);// S chamber west
	WallSeg(2500.0f, 0.0f, 0.3f, 14.0f);   // E arcade back
	WallSeg(1950.0f, 700.0f, 11.0f, 0.3f); // E arcade north
	WallSeg(1950.0f, -700.0f, 11.0f, 0.3f);// E arcade south
	WallSeg(-2000.0f, 0.0f, 0.3f, 9.0f);   // W alcove back
	WallSeg(-1700.0f, 450.0f, 6.0f, 0.3f); // W alcove north
	WallSeg(-1700.0f, -450.0f, 6.0f, 0.3f);// W alcove south

	// ---- Hub atmosphere: a glowing floor grid + coloured ambient fill from the ceiling. --------------------
	const FLinearColor GridGlow(0.10f, 0.55f, 0.70f, 1.0f);
	for (float Gy : { -600.0f, 0.0f, 600.0f })
	{
		Glow(FVector(0.0f, Gy, CenterZForBlockTop(3.0f, 0.02f)), FVector(30.0f, 0.06f, 0.02f), GridGlow);
	}
	for (float Gx : { -700.0f, 0.0f, 700.0f })
	{
		Glow(FVector(Gx, 0.0f, CenterZForBlockTop(3.0f, 0.02f)), FVector(0.06f, 24.0f, 0.02f), GridGlow);
	}
	AccentLight(FVector(0.0f, 0.0f, 455.0f), Cyan * 0.7f, 28.0f, 1800.0f);
	AccentLight(FVector(-650.0f, -420.0f, 440.0f), Purple, 22.0f, 1400.0f);
	AccentLight(FVector(650.0f, 420.0f, 440.0f), Magenta, 22.0f, 1400.0f);
	AccentLight(FVector(0.0f, 0.0f, 220.0f), FLinearColor(0.28f, 0.36f, 0.55f, 1.0f), 16.0f, 1700.0f);

	// ---- Centre: the player materialises inside a neon henge. ----------------------------------------------
	for (int32 i = 0; i < 8; ++i)
	{
		const float Ang = static_cast<float>(i) * (PI / 4.0f);
		const float Px = FMath::Cos(Ang) * 380.0f;
		const float Py = FMath::Sin(Ang) * 380.0f;
		const FLinearColor PostTint = (i % 2 == 0) ? Cyan : Purple;
		SpawnBlock(FVector(Px, Py, CenterZForBlockBottom(0.0f, 2.6f)), FVector(0.18f, 0.18f, 2.6f), FLinearColor(0.82f, 0.84f, 0.9f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Marble);
		Glow(FVector(Px, Py, CenterZForBlockBottom(260.0f, 0.16f)), FVector(0.34f, 0.34f, 0.16f), PostTint);
		AccentLight(FVector(Px, Py, 300.0f), PostTint, 14.0f, 420.0f);
	}
	AccentLight(FVector(0.0f, 0.0f, 90.0f), Cyan, 30.0f, 650.0f);
	SpawnPlaque(FVector(0.0f, 440.0f, 250.0f), FRotator(0.0f, -90.0f, 0.0f), TEXT("+  THE VOID  +\nyou pressed ESC.  welcome to the secret."), Cyan, 30.0f);
	SpawnPlaque(FVector(0.0f, -440.0f, 250.0f), FRotator(0.0f, 90.0f, 0.0f), TEXT("a reward room.  go play."), Purple, 22.0f);

	// ---- NW corner: TROPHY shrine. Pillars grow with your record (a personal bar chart in glowing marble). -
	const float TY = 1130.0f;
	const FLinearColor TrophyTints[7] = { Cyan, Lime, Orange, Magenta, Gold, Purple, FLinearColor(0.3f, 0.8f, 1.0f, 1.0f) };
	auto Trophy = [&](int32 Idx, float Frac, const FString& Label, const FString& Value)
	{
		const float X = -1400.0f + static_cast<float>(Idx) * 107.0f;
		const FLinearColor Neon = TrophyTints[FMath::Clamp(Idx, 0, 6)];
		const float Frac01 = FMath::Clamp(Frac, 0.0f, 1.0f);
		const float PillarH = 70.0f + 300.0f * Frac01;
		const float ScaleZ = PillarH / 100.0f;
		SpawnBlock(FVector(X, TY, CenterZForBlockBottom(0.0f, ScaleZ)), FVector(0.30f, 0.30f, ScaleZ), FLinearColor(0.82f, 0.84f, 0.9f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Marble);
		Glow(FVector(X, TY, CenterZForBlockBottom(PillarH, 0.16f)), FVector(0.40f, 0.40f, 0.16f), Neon);
		AccentLight(FVector(X, TY - 35.0f, PillarH + 25.0f), Neon, 22.0f, 320.0f);
		SpawnPlaque(FVector(X, TY - 48.0f, PillarH + 38.0f), FRotator(0.0f, -90.0f, 0.0f), Label, Neon, 13.0f);
		SpawnPlaque(FVector(X, TY - 48.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f), Value, FLinearColor::White, 24.0f);
	};
	SpawnPlaque(FVector(-1080.0f, 1185.0f, 360.0f), FRotator(0.0f, -90.0f, 0.0f), TEXT("*  YOUR TROPHIES  *"), Gold, 38.0f);
	AccentLight(FVector(-1080.0f, 900.0f, 360.0f), Gold, 26.0f, 1100.0f);

	FString RecordText = TEXT("YOUR RECORD");
	{
		int32 Rounds = 0, SurvWins = 0, HuntWins = 0, Escapes = 0, Streak = 0, MasteryPct = 0, XP = 0, Earned = 0, Total = 0;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UBHAccountSubsystem* Account = GI->GetSubsystem<UBHAccountSubsystem>())
			{
				const FBHAccountProgress& P = Account->GetProgress();
				Account->GetAchievementCounts(Earned, Total);
				int32 CorrectSum = 0, AnswerSum = 0;
				for (int32 c : P.TopicCorrectCounts) { CorrectSum += c; }
				for (int32 a : P.TopicAnswerCounts) { AnswerSum += a; }
				MasteryPct = AnswerSum > 0 ? FMath::RoundToInt(100.0f * CorrectSum / AnswerSum) : 0;
				Rounds = P.RoundsPlayed; SurvWins = P.SurvivorWins; HuntWins = P.HunterWins;
				Escapes = P.Escapes; Streak = P.BestWinStreak; XP = P.XP;
			}
		}
		Trophy(0, Rounds / 100.0f, TEXT("ROUNDS"), FString::FromInt(Rounds));
		Trophy(1, SurvWins / 50.0f, TEXT("SURVIVOR"), FString::FromInt(SurvWins));
		Trophy(2, HuntWins / 50.0f, TEXT("HUNTER"), FString::FromInt(HuntWins));
		Trophy(3, Escapes / 30.0f, TEXT("ESCAPES"), FString::FromInt(Escapes));
		Trophy(4, Streak / 15.0f, TEXT("STREAK"), FString::FromInt(Streak));
		Trophy(5, MasteryPct / 100.0f, TEXT("MASTERY"), FString::Printf(TEXT("%d%%"), MasteryPct));
		Trophy(6, XP / 5000.0f, TEXT("XP"), FString::FromInt(XP));
		RecordText = FString::Printf(TEXT("YOUR RECORD\nRounds: %d\nSurvivor wins: %d\nHunter wins: %d\nEscapes: %d\nBest streak: %d\nMastery: %d%%\nAchievements: %d / %d\nXP: %d"),
			Rounds, SurvWins, HuntWins, Escapes, Streak, MasteryPct, Earned, Total, XP);
	}
	SpawnPlaque(FVector(-1480.0f, 650.0f, 175.0f), FRotator(0.0f, 0.0f, 0.0f), RecordText, Gold, 22.0f);

	// ---- NE corner: after-hours dance club. ---------------------------------------------------------------
	World->SpawnActor<ABHTrainDanceFloor>(FVector(1100.0f, 950.0f, 0.0f), FRotator::ZeroRotator);
	World->SpawnActor<ABHTrainJukebox>(FVector(1380.0f, 760.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	AccentLight(FVector(900.0f, 950.0f, 260.0f), Magenta, 70.0f, 600.0f);
	AccentLight(FVector(1300.0f, 950.0f, 260.0f), Cyan, 70.0f, 600.0f);
	Glow(FVector(1100.0f, 1180.0f, 230.0f), FVector(8.0f, 0.1f, 0.4f), Magenta); // a glowing strip over the floor
	SpawnPlaque(FVector(1100.0f, 1185.0f, 305.0f), FRotator(0.0f, -90.0f, 0.0f), TEXT("<>  AFTER-HOURS  <>"), Magenta, 34.0f);

	// ---- SW corner: cosy lounge (warm contrast to all the neon). ------------------------------------------
	SpawnBlock(FVector(-1100.0f, -825.0f, CenterZForBlockTop(0.6f, 0.05f)), FVector(7.0f, 6.0f, 0.05f), FLinearColor(0.45f, 0.10f, 0.10f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Carpet);
	World->SpawnActor<ABHTrainFireplace>(FVector(-1430.0f, -825.0f, 0.0f), FRotator::ZeroRotator);
	World->SpawnActor<ABHTrainCat>(FVector(-1080.0f, -700.0f, 6.0f), FRotator::ZeroRotator);
	World->SpawnActor<ABHTrainChessTable>(FVector(-950.0f, -1050.0f, 0.0f), FRotator::ZeroRotator);
	World->SpawnActor<ABHTrainTicTacToeTable>(FVector(-950.0f, -560.0f, 0.0f), FRotator::ZeroRotator);
	AccentLight(FVector(-1150.0f, -825.0f, 250.0f), FLinearColor(1.0f, 0.72f, 0.42f, 1.0f), 70.0f, 750.0f);
	SpawnPlaque(FVector(-1100.0f, -1185.0f, 300.0f), FRotator(0.0f, 90.0f, 0.0f), TEXT("~  THE LOUNGE  ~"), FLinearColor(1.0f, 0.78f, 0.5f, 1.0f), 32.0f);

	// ---- SE corner: the deep (aquarium + whimsy). ---------------------------------------------------------
	World->SpawnActor<ABHTrainAquarium>(FVector(1400.0f, -825.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	World->SpawnActor<ABHTrainToyTrain>(FVector(1050.0f, -640.0f, 0.0f), FRotator::ZeroRotator);
	World->SpawnActor<ABHTrainGhost>(FVector(1120.0f, -1040.0f, 6.0f), FRotator::ZeroRotator); // rare visitor, easter egg
	AccentLight(FVector(1150.0f, -825.0f, 250.0f), FLinearColor(0.45f, 0.75f, 1.0f, 1.0f), 65.0f, 750.0f);
	SpawnPlaque(FVector(1100.0f, -1185.0f, 300.0f), FRotator(0.0f, 90.0f, 0.0f), TEXT("=  THE DEEP  ="), Cyan, 32.0f);

	// ---- N chamber: MOVEMENT SIM -- glowing platforms to dash and jump. -----------------------------------
	struct FPlat { float X, Y, Top; };
	const FPlat Plats[4] = { { -400.0f, 1450.0f, 55.0f }, { 80.0f, 1650.0f, 130.0f }, { 420.0f, 1820.0f, 205.0f }, { -120.0f, 1980.0f, 285.0f } };
	for (const FPlat& Pl : Plats)
	{
		SpawnBlock(FVector(Pl.X, Pl.Y, CenterZForBlockTop(Pl.Top, 0.2f)), FVector(2.2f, 2.2f, 0.2f), FLinearColor(0.10f, 0.16f, 0.22f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Marble);
		Glow(FVector(Pl.X, Pl.Y, CenterZForBlockTop(Pl.Top + 2.0f, 0.02f)), FVector(2.3f, 0.08f, 0.02f), Cyan);
		Glow(FVector(Pl.X, Pl.Y, CenterZForBlockTop(Pl.Top + 2.0f, 0.02f)), FVector(0.08f, 2.3f, 0.02f), Cyan);
		AccentLight(FVector(Pl.X, Pl.Y, Pl.Top + 120.0f), Cyan, 20.0f, 500.0f);
	}
	SpawnPlaque(FVector(0.0f, 1255.0f, 250.0f), FRotator(0.0f, -90.0f, 0.0f), TEXT("^  MOVEMENT SIM  ^\ndash + jump the platforms"), Cyan, 26.0f);

	// ---- S chamber: EVASION SIM -- red cover to weave through while the teacher chases. -------------------
	// Note: the HunterSpawn lane (0, -1700) is kept clear -- the centre cover sits at Y -1600, not on the spawn.
	const FVector Covers[5] = { FVector(-360.0f, -1480.0f, 0.0f), FVector(360.0f, -1480.0f, 0.0f), FVector(0.0f, -1600.0f, 0.0f), FVector(-360.0f, -1940.0f, 0.0f), FVector(360.0f, -1940.0f, 0.0f) };
	for (const FVector& C : Covers)
	{
		SpawnBlock(FVector(C.X, C.Y, CenterZForBlockBottom(0.0f, 2.0f)), FVector(0.6f, 0.6f, 2.0f), FLinearColor(0.14f, 0.05f, 0.05f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted);
		Glow(FVector(C.X, C.Y, CenterZForBlockBottom(200.0f, 0.1f)), FVector(0.7f, 0.7f, 0.1f), NeonRed);
	}
	AccentLight(FVector(0.0f, -1700.0f, 360.0f), NeonRed, 30.0f, 1300.0f);
	if (ABHFlickerLight* FL = World->SpawnActor<ABHFlickerLight>(FVector(0.0f, -1900.0f, 380.0f), FRotator::ZeroRotator))
	{
		FL->Configure(0, NeonRed, 900.0f, 1100.0f);
	}
	SpawnPlaque(FVector(0.0f, -1255.0f, 250.0f), FRotator(0.0f, 90.0f, 0.0f), TEXT("v  EVASION SIM  v\nlose the teacher"), NeonRed, 26.0f);

	// ---- E chamber: arcade. -------------------------------------------------------------------------------
	World->SpawnActor<ABHTrainSlotMachine>(FVector(1780.0f, -320.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f));
	World->SpawnActor<ABHTrainSlotMachine>(FVector(1780.0f, 320.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f));
	World->SpawnActor<ABHTrainDartboard>(FVector(2440.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	World->SpawnActor<ABHTrainVendingMachine>(FVector(2360.0f, -560.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	AccentLight(FVector(2000.0f, -300.0f, 280.0f), Magenta, 55.0f, 650.0f);
	AccentLight(FVector(2000.0f, 300.0f, 280.0f), Cyan, 55.0f, 650.0f);
	AccentLight(FVector(2300.0f, 0.0f, 280.0f), Lime, 50.0f, 600.0f);
	SpawnPlaque(FVector(1545.0f, 0.0f, 270.0f), FRotator(0.0f, 180.0f, 0.0f), TEXT("[  ARCADE  ]"), Lime, 32.0f);

	// ---- W alcove: a secret inside the secret. ------------------------------------------------------------
	World->SpawnActor<ABHTrainSecretSwitch>(FVector(-1900.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	AccentLight(FVector(-1750.0f, 0.0f, 260.0f), Purple, 28.0f, 600.0f);
	SpawnPlaque(FVector(-1455.0f, 0.0f, 250.0f), FRotator(0.0f, 0.0f, 0.0f), TEXT("?   ?   ?"), Purple, 30.0f);
	SpawnPlaque(FVector(-1980.0f, 0.0f, 210.0f), FRotator(0.0f, 0.0f, 0.0f),
		TEXT("HIDDEN EGGS\n- Konami code on the menu (Up Up Down Down L R L R B A)\n- Train roof hatch (ride the intermission carriage)\n- The locker that exits onto the roof\n- Press Esc at the menu... you already found this one."),
		FLinearColor(0.75f, 0.55f, 0.95f, 1.0f), 18.0f);
	SpawnPlaque(FVector(-1700.0f, 330.0f, 165.0f), FRotator(0.0f, -90.0f, 0.0f), TEXT("a secret, inside a secret,\ninside a secret..."), FLinearColor(0.6f, 0.5f, 0.8f, 1.0f), 15.0f);

	// Spawns: survivor in the henge, the "teacher" parked in the evasion sim for that drill.
	SurvivorSpawns = { FVector(0.0f, 0.0f, 120.0f) };
	HunterSpawn = FVector(0.0f, -1700.0f, 120.0f);
}

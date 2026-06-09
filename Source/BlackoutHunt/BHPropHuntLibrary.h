// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

// -------------------------------------------------------------------------------------------------------------------
// Prop Hunt (opt-in, reversible) pure logic. Parameter-only inline helpers -- no world, no CVar reads, no actor state --
// so the headless automation tests can pin the curves exactly (mirrors the BHCompute* helpers in BHTypes.h). The
// ABHGameMode prop-hunt partial (BHGameModePropHunt.cpp) reads the bh.PropHunt* CVars and calls these.
//
// Prop Hunt reuses the existing roles and round flow: the SEEKER is the Hunter, the PROPS are the Survivors. "All props
// found" is the normal CountAliveSurvivors()<=0 HunterWin; the only flipped rule is that running the Hunt timer out is a
// PROPS win (they survived), not a Hunter win. ResolvePropHuntRound encodes the whole resolution so it is unit-testable.
// -------------------------------------------------------------------------------------------------------------------
namespace BHPropHunt
{
	// The round result for the current live-Hunt state, or EBHRoundPhase::Hunt while it is still going.
	//  * No props left alive: if any prop got out (escaped) the props win, otherwise the seeker found everyone (HunterWin).
	//  * No seeker left alive (all seekers disconnected): the props can never be found, so they win.
	//  * Hunt timer expired with props still hidden: the props survived -> SurvivorsWin (the prop-hunt flip).
	//  * Otherwise the hunt continues.
	inline EBHRoundPhase ResolvePropHuntRound(int32 AliveProps, int32 EscapedProps, int32 AliveSeekers, bool bTimeExpired)
	{
		if (AliveProps <= 0)
		{
			return EscapedProps > 0 ? EBHRoundPhase::SurvivorsWin : EBHRoundPhase::HunterWin;
		}
		if (AliveSeekers <= 0)
		{
			return EBHRoundPhase::SurvivorsWin;
		}
		if (bTimeExpired)
		{
			return EBHRoundPhase::SurvivorsWin;
		}
		return EBHRoundPhase::Hunt;
	}

	// Seconds between forced "taunts" (every hidden prop emits a noise so the seeker has a fair chance). The cadence
	// tightens as the round runs down -- early game gives props room to settle, late game flushes the last stragglers.
	// ElapsedFraction is (HuntSeconds - RemainingTime)/HuntSeconds, clamped to [0,1]; the interval lerps Base -> Min.
	inline float TauntIntervalSeconds(float ElapsedFraction, float BaseInterval, float MinInterval)
	{
		const float SafeBase = FMath::Max(1.0f, BaseInterval);
		const float SafeMin = FMath::Clamp(MinInterval, 1.0f, SafeBase);
		const float Alpha = FMath::Clamp(ElapsedFraction, 0.0f, 1.0f);
		return FMath::Lerp(SafeBase, SafeMin, Alpha);
	}

	// A seeker who swings at empty air / the wrong prop gets a short self-slow that grows with consecutive misses, so
	// frantic flailing is punished but a single honest miss barely stings. Climbs Base + (misses-1)*PerExtraMiss, capped.
	// With (0.6, 0.35, 2.0): 1->0.60, 2->0.95, 3->1.30, 4->1.65, 5+->2.00.
	inline float SeekerMissSlowSeconds(int32 ConsecutiveMisses, float BaseSeconds, float PerExtraMiss, float MaxSeconds)
	{
		if (ConsecutiveMisses <= 0)
		{
			return 0.0f;
		}
		const float Raw = FMath::Max(0.0f, BaseSeconds) + static_cast<float>(ConsecutiveMisses - 1) * FMath::Max(0.0f, PerExtraMiss);
		return FMath::Clamp(Raw, 0.0f, FMath::Max(0.0f, MaxSeconds));
	}

	// A small, always-available fallback prop catalogue (engine basic shapes, so they exist in every cook). Used only
	// when a prop presses disguise while NOT looking at any real world prop -- they become a random one of these instead
	// of nothing. The primary disguise path copies the mesh/material of the actor the player is actually looking at.
	struct FFallbackProp
	{
		const TCHAR* MeshPath;     // engine static mesh object path
		const TCHAR* MaterialPath; // optional override material ("" = keep the mesh's own / runtime fallback)
		float Scale;               // uniform relative scale applied to the disguise mesh
		const TCHAR* Label;        // short HUD label
	};

	inline const FFallbackProp* FallbackProps(int32& OutCount)
	{
		static const FFallbackProp Props[] = {
			{ TEXT("/Engine/BasicShapes/Cube.Cube"),         TEXT(""), 0.85f, TEXT("Crate") },
			{ TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT(""), 0.85f, TEXT("Barrel") },
			{ TEXT("/Engine/BasicShapes/Cone.Cone"),         TEXT(""), 0.85f, TEXT("Cone") },
			{ TEXT("/Engine/BasicShapes/Sphere.Sphere"),     TEXT(""), 0.80f, TEXT("Ball") },
		};
		OutCount = UE_ARRAY_COUNT(Props);
		return Props;
	}

	inline int32 FallbackPropCount()
	{
		int32 Count = 0;
		FallbackProps(Count);
		return Count;
	}

	// Pick a fallback prop by an arbitrary salt (e.g. a per-player random seed). Always returns a valid entry.
	inline const FFallbackProp& FallbackPropForSalt(int32 Salt)
	{
		int32 Count = 0;
		const FFallbackProp* Props = FallbackProps(Count);
		const int32 Index = ((Salt % Count) + Count) % Count;
		return Props[Index];
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Arena registry (P3). Prop-hunt arenas are the prop-rich imported pack maps, wired by NAME: the logical name is a
	// first-class BHLevel token (NormalizeBHLevelName passes it through), travel resolves it to a real package, and
	// BuildRuntimeFacility dispatches BuildPropHuntArena when the loaded map IS one of these packages. Pure data here --
	// package existence / preference is resolved by BHResolvePropHuntArenaPackage in BHGameModePropHunt.cpp, which
	// prefers the authored bake (the Tools/Setup-PropHuntArena.py output, hand-placed spawns) over the raw pack demo.
	// Adding an arena = adding a row (+ cook-list entries when it should ship in packaged builds).
	struct FArenaSpec
	{
		const TCHAR* LogicalName;     // canonical ?BHLevel=/?BHArena= token, e.g. "ContainersHouse"
		const TCHAR* AuthoredPackage; // preferred authored arena map (editor-script output; may not exist yet)
		const TCHAR* SourcePackage;   // raw imported pack demo map (in the tree today; runtime scatter handles spawns)
		const TCHAR* DisplayName;     // host UI / HUD label
	};

	inline const FArenaSpec* ArenaSpecs(int32& OutCount)
	{
		static const FArenaSpec Arenas[] = {
			{ TEXT("ContainersHouse"), TEXT("/Game/BlackoutHunt/Maps/Arena_ContainersHouse"),
			  TEXT("/Game/ContainersHouseCH/Maps/Map_ContainersHouse_Demo"), TEXT("Container Warehouse") },
			{ TEXT("RuinedCrypt"), TEXT("/Game/BlackoutHunt/Maps/Arena_RuinedCrypt"),
			  TEXT("/Game/RuinedCrypt/Demo/Maps/RuinedCrypt_01/RuinedCrypt_01_P"), TEXT("Ruined Crypt") },
		};
		OutCount = UE_ARRAY_COUNT(Arenas);
		return Arenas;
	}

	// Case-insensitive logical-name lookup; nullptr when the token is not a registered arena.
	inline const FArenaSpec* FindArenaSpec(const FString& Token)
	{
		const FString Trimmed = Token.TrimStartAndEnd();
		int32 Count = 0;
		const FArenaSpec* Arenas = ArenaSpecs(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Trimmed.Equals(Arenas[Index].LogicalName, ESearchCase::IgnoreCase))
			{
				return &Arenas[Index];
			}
		}
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Spawn scatter (P3). Pack demo maps ship few/no PlayerStarts, so the arena builder samples candidate XY points
	// over the level bounds and floor-traces each one. The candidate pattern is a deterministic low-discrepancy Halton
	// sequence (bases 2/3): evenly spread, no clumping, reproducible for tests -- no RNG state involved.
	inline float HaltonSequence(int32 Index, int32 Base)
	{
		float Result = 0.0f;
		float Fraction = 1.0f / static_cast<float>(Base);
		int32 Remaining = FMath::Max(0, Index);
		while (Remaining > 0)
		{
			Result += Fraction * static_cast<float>(Remaining % Base);
			Remaining /= Base;
			Fraction /= static_cast<float>(Base);
		}
		return Result;
	}

	// The Index-th candidate point in the unit square (Salt offsets the sequence so different rounds can vary).
	// Index 0 maps to sequence position 1 (Halton(0) is degenerate at the corner).
	inline FVector2D ArenaScatterUV(int32 Index, int32 Salt = 0)
	{
		const int32 Position = 1 + FMath::Max(0, Index) + FMath::Max(0, Salt);
		return FVector2D(HaltonSequence(Position, 2), HaltonSequence(Position, 3));
	}

	// Rotating starting seeker (P6): everyone seeks before anyone seeks twice. Pick the candidate who has STARTED
	// as seeker the fewest times; break ties by LOWEST match score (trailing players get the catch-points turn),
	// then by index (stable join order). The arrays are parallel per-candidate; INDEX_NONE on empty/mismatched input.
	inline int32 PickStartingSeekerIndex(const TArray<int32>& TimesSeeker, const TArray<int32>& Scores)
	{
		if (TimesSeeker.Num() == 0 || TimesSeeker.Num() != Scores.Num())
		{
			return INDEX_NONE;
		}
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < TimesSeeker.Num(); ++Index)
		{
			if (TimesSeeker[Index] < TimesSeeker[BestIndex]
				|| (TimesSeeker[Index] == TimesSeeker[BestIndex] && Scores[Index] < Scores[BestIndex]))
			{
				BestIndex = Index;
			}
		}
		return BestIndex;
	}

	// The seeker's hide-phase holding spot: the spawn point farthest from the cloud's centroid, so the seeker starts
	// at the edge of the play space rather than in the middle of the hiding props. INDEX_NONE on an empty set.
	inline int32 PickSeekerHoldIndex(const TArray<FVector>& Points)
	{
		if (Points.Num() == 0)
		{
			return INDEX_NONE;
		}
		FVector Centroid = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Centroid += Point;
		}
		Centroid /= static_cast<float>(Points.Num());

		int32 BestIndex = 0;
		float BestDistSq = -1.0f;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const float DistSq = FVector::DistSquared2D(Points[Index], Centroid);
			if (DistSq > BestDistSq)
			{
				BestDistSq = DistSq;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}
}

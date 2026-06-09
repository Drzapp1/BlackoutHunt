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
}

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#if WITH_DEV_AUTOMATION_TESTS

#include "BHCharacter.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHCharacterMoveHelpersTest,
	"BlackoutHunt.Movement.CharacterMoveHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHCharacterMoveHelpersTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// --- ResolveSpecialMoveEndState: the FinishSpecialMoveAuthority end-state + headroom matrix ---------------------
	// (bEndsProneRequested, bEarlyBrake, bLowCapsuleApplied, bCanStandNow)
	{
		// A clean slide released late with headroom ends standing.
		const FBHSpecialMoveEndState Stand = ABHCharacter::ResolveSpecialMoveEndState(false, false, true, true);
		TestFalse(TEXT("Slide end with headroom: not prone."), Stand.bEndProne);
		TestFalse(TEXT("Slide end with headroom: not crouched."), Stand.bEndCrouched);

		// Hold-prone through the slide is honored.
		const FBHSpecialMoveEndState HoldProne = ABHCharacter::ResolveSpecialMoveEndState(true, false, true, true);
		TestTrue(TEXT("Requested prone end is honored."), HoldProne.bEndProne);
		TestFalse(TEXT("Requested prone end never also crouches."), HoldProne.bEndCrouched);

		// The headroom gate: a slide finishing under low geometry must NOT end standing -- it settles prone (the
		// height the move itself validated) instead of wedging the unswept full-height capsule restore.
		const FBHSpecialMoveEndState Wedged = ABHCharacter::ResolveSpecialMoveEndState(false, false, true, false);
		TestTrue(TEXT("No headroom at slide end forces prone."), Wedged.bEndProne);
		TestFalse(TEXT("No headroom at slide end never crouches."), Wedged.bEndCrouched);

		// Silent slide-stop with headroom settles into the quiet crouch.
		const FBHSpecialMoveEndState Brake = ABHCharacter::ResolveSpecialMoveEndState(false, true, true, true);
		TestFalse(TEXT("Early brake with headroom: not prone."), Brake.bEndProne);
		TestTrue(TEXT("Early brake with headroom settles crouched."), Brake.bEndCrouched);

		// The early-brake crouch TRANSITS the full-height restore, so blocked headroom must beat it to prone.
		const FBHSpecialMoveEndState BrakeWedged = ABHCharacter::ResolveSpecialMoveEndState(false, true, true, false);
		TestTrue(TEXT("Early brake without headroom forces prone."), BrakeWedged.bEndProne);
		TestFalse(TEXT("Early brake without headroom must NOT crouch (full-height transit)."), BrakeWedged.bEndCrouched);

		// Prone request wins over the early brake (the two stops are mutually exclusive; prone is the lower pose).
		const FBHSpecialMoveEndState ProneOverBrake = ABHCharacter::ResolveSpecialMoveEndState(true, true, true, true);
		TestTrue(TEXT("Prone request beats the early-brake crouch."), ProneOverBrake.bEndProne);
		TestFalse(TEXT("Prone request suppresses the crouch flag."), ProneOverBrake.bEndCrouched);

		// A full-height move (roll: low capsule never applied) is never forced prone -- the capsule never shrank, so
		// there is no restore to validate, whatever the stand probe would say.
		const FBHSpecialMoveEndState RollEnd = ABHCharacter::ResolveSpecialMoveEndState(false, false, false, false);
		TestFalse(TEXT("Full-height move end is never forced prone."), RollEnd.bEndProne);
		TestFalse(TEXT("Full-height move end is not crouched."), RollEnd.bEndCrouched);
	}

	// --- DecodeRemoteViewPitchDegrees: APawn::GetRemoteViewPitch short -> normalized degrees ------------------------
	{
		TestEqual(TEXT("Pitch short 0 decodes to 0 deg (level)."), ABHCharacter::DecodeRemoteViewPitchDegrees(0), 0.0f, 0.01f);
		// SetRemoteViewPitch packs via FRotator::CompressAxisToShort (pitch * 65536/360), so 16384 is 90deg looking UP.
		TestEqual(TEXT("Pitch short 16384 decodes to +90 deg (looking up)."),
			ABHCharacter::DecodeRemoteViewPitchDegrees(16384), 90.0f, 0.01f);
		// Raw values past 180deg must wrap NEGATIVE: 49152 is raw 270deg = looking DOWN -90, not +270 -- the
		// whole point of normalizing before feeding a beam rotation.
		TestEqual(TEXT("Pitch short 49152 decodes to -90 deg (looking down)."),
			ABHCharacter::DecodeRemoteViewPitchDegrees(49152), -90.0f, 0.01f);
		TestEqual(TEXT("Pitch short 65535 decodes back to ~0 deg (full wrap)."),
			ABHCharacter::DecodeRemoteViewPitchDegrees(65535), 0.0f, 0.01f);

		// Every possible packed value lands in the normalized band a rotator expects.
		for (int32 Value = 0; Value <= 65535; ++Value)
		{
			const float Decoded = ABHCharacter::DecodeRemoteViewPitchDegrees(static_cast<uint16>(Value));
			if (Decoded < -180.0f || Decoded > 180.0f)
			{
				AddError(FString::Printf(TEXT("Pitch short %d decoded outside [-180,180]: %f"), Value, Decoded));
				break;
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

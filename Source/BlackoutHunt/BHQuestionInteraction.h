#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

// Client-only types shared between the HUD (which lays out and draws the interactive question
// panel) and the player character (which reads the mouse and acts on what was clicked). These are
// NOT replicated and never carry answer data -- a hit region only describes a clickable rectangle
// and what it means on screen. The HUD rebuilds the region list every frame while it draws the
// panel, so the geometry always matches exactly what the player sees at the current HudScale.
//
// Kept out of BHTypes.h on purpose: this is pure presentation/input glue, so only BHHUD and
// BHCharacter pull it in (BHTypes.h is included almost everywhere and is expensive to touch).

enum class EBHQuestionRegionKind : uint8
{
	None,
	// A multiple-choice / true-false / graph answer row. IndexA = choice index -> SubmitAnswer(IndexA).
	Choice,
	// A labelled element of the diagram that maps onto one of the choices (e.g. an EM-spectrum band).
	// IndexA = the choice index it resolves to -> SubmitAnswer(IndexA). Behaves like Choice once mapped.
	DiagramChoice,
	// A draggable arrangement piece (matching value / ordering item). IndexA = piece index.
	Piece,
	// A drop target for arrangement pieces. IndexA = slot index.
	Slot,
	// Confirm the current arrangement -> ServerSubmitArrangement.
	Submit,
	// On-screen numeric keypad for Calculation questions (mouse parity with typed entry).
	KeypadDigit,      // IndexA = 0..9
	KeypadDecimal,
	KeypadMinus,
	KeypadBackspace,
	KeypadEnter
};

// One clickable rectangle on the question panel, in viewport pixel space (matches Canvas->ClipX/Y
// and APlayerController::GetMousePosition, so hit-testing is a direct point-in-rect test).
struct FBHQuestionHitRegion
{
	FBox2D Rect = FBox2D(ForceInit);
	EBHQuestionRegionKind Kind = EBHQuestionRegionKind::None;
	int32 IndexA = INDEX_NONE;

	FBHQuestionHitRegion() = default;
	FBHQuestionHitRegion(const FBox2D& InRect, EBHQuestionRegionKind InKind, int32 InIndexA = INDEX_NONE)
		: Rect(InRect), Kind(InKind), IndexA(InIndexA)
	{
	}

	bool Contains(const FVector2D& Point) const
	{
		return Rect.bIsValid && Point.X >= Rect.Min.X && Point.X <= Rect.Max.X && Point.Y >= Rect.Min.Y && Point.Y <= Rect.Max.Y;
	}
};

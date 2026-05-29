#pragma once

#include "CoreMinimal.h"

// Centralized HUD theme for the hand-drawn "scanner/terminal" HUD (see BHHUD.cpp).
//
// This is intentionally plain C++ (no UCLASS/UENUM/asset references): a header-only
// set of semantic colour tokens plus pure factory/resolver functions returning
// FBHHudPalette by value. Keeping it reflection-free means it is cook-safe and has no
// runtime/asset cost beyond a few FLinearColor lerps per frame.
//
// The HUD caches one resolved FBHHudPalette per frame (see ABHHUD::RefreshHudPreferences)
// and queries its tokens instead of hardcoding inline FLinearColor literals. Three base
// variants exist -- Standard, High-Contrast, Colorblind-safe -- and a per-map accent tint
// is lerped into the accent tokens so each map keeps its own character without losing the
// shared aesthetic.

namespace BHHudTheme
{
	// Player-selectable base palette. Stored in config as an int, so no UENUM is needed.
	enum class EPaletteMode : uint8
	{
		Standard = 0,
		Colorblind = 1
	};

	// Semantic colour tokens. Only base colours are tokens; structural alpha math
	// (e.g. FillColor.A * 0.42f for drop shadows) stays as math on these in the HUD code.
	struct FBHHudPalette
	{
		// Panel structure
		FLinearColor PanelFill;       // default background fill of framed panels
		FLinearColor PanelHighlight;  // 1px top highlight line on panels
		FLinearColor PanelShadow;     // drop-shadow / inner-shade base (usually black)

		// Accents (lerped toward the active map's tint)
		FLinearColor AccentPrimary;
		FLinearColor AccentSecondary;

		// Text
		FLinearColor TextMain;
		FLinearColor TextMuted;
		FLinearColor TextDim;

		// Status semantics
		FLinearColor WarnHot;  // imminent danger / warning pulses
		FLinearColor Good;     // success / unlocked / safe
		FLinearColor Bad;      // failure / locked / hostile

		// Key-cap boxes
		FLinearColor KeyBoxLit;
		FLinearColor KeyBoxDim;
	};

	// Standard palette -- seeded with the HUD's historical inline literal values so that
	// migrating literals to tokens produces ZERO visual diff in standard mode.
	inline FBHHudPalette MakeStandardPalette()
	{
		FBHHudPalette P;
		P.PanelFill      = FLinearColor(0.020f, 0.020f, 0.018f, 0.84f);
		P.PanelHighlight = FLinearColor(0.84f, 0.96f, 0.93f, 1.0f);
		P.PanelShadow    = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		P.AccentPrimary  = FLinearColor(0.40f, 0.90f, 0.82f, 1.0f);
		P.AccentSecondary= FLinearColor(0.30f, 0.92f, 0.82f, 1.0f);
		P.TextMain       = FLinearColor(0.88f, 0.95f, 0.92f, 1.0f);
		P.TextMuted      = FLinearColor(0.58f, 0.66f, 0.66f, 0.92f);
		P.TextDim        = FLinearColor(0.55f, 0.52f, 0.47f, 0.76f);
		P.WarnHot        = FLinearColor(1.0f, 0.42f, 0.30f, 0.98f);
		P.Good           = FLinearColor(0.73f, 0.96f, 0.64f, 0.95f);
		P.Bad            = FLinearColor(0.96f, 0.24f, 0.16f, 0.95f);
		P.KeyBoxLit      = FLinearColor(0.88f, 1.0f, 0.96f, 1.0f);
		P.KeyBoxDim      = FLinearColor(0.50f, 0.58f, 0.58f, 0.88f);
		return P;
	}

	// High-contrast variant: brighter text, fully opaque near-black panels, hotter accents.
	// Used for accessibility; wins on text/border tokens during resolution.
	inline FBHHudPalette MakeHighContrastPalette()
	{
		FBHHudPalette P = MakeStandardPalette();
		P.PanelFill      = FLinearColor(0.0f, 0.0f, 0.0f, 0.92f);
		P.PanelHighlight = FLinearColor(0.90f, 1.0f, 0.96f, 1.0f);
		P.AccentPrimary  = FLinearColor(0.46f, 1.0f, 0.90f, 1.0f);
		P.AccentSecondary= FLinearColor(0.40f, 1.0f, 0.88f, 1.0f);
		P.TextMain       = FLinearColor(0.92f, 0.98f, 0.90f, 1.0f);
		P.TextMuted      = FLinearColor(0.78f, 0.92f, 0.88f, 0.98f);
		P.TextDim        = FLinearColor(0.70f, 0.84f, 0.82f, 0.94f);
		P.WarnHot        = FLinearColor(1.0f, 0.50f, 0.34f, 1.0f);
		P.Good           = FLinearColor(0.62f, 1.0f, 0.58f, 1.0f);
		P.Bad            = FLinearColor(1.0f, 0.30f, 0.20f, 1.0f);
		P.KeyBoxLit      = FLinearColor(0.94f, 1.0f, 0.98f, 1.0f);
		return P;
	}

	// Colorblind-safe variant: red->amber, green->blue-teal so Good/Bad/WarnHot stay
	// distinguishable for deuteranopia/protanopia. Text/panel stay neutral.
	inline FBHHudPalette MakeColorblindPalette()
	{
		FBHHudPalette P = MakeStandardPalette();
		// Amber/orange for danger instead of red.
		P.WarnHot        = FLinearColor(1.0f, 0.62f, 0.10f, 0.98f);
		P.Bad            = FLinearColor(1.0f, 0.55f, 0.0f, 0.95f);
		// Blue-teal for safe/good instead of green.
		P.Good           = FLinearColor(0.30f, 0.74f, 0.98f, 0.95f);
		// Push accents toward blue so they read distinctly from the amber danger cues.
		P.AccentPrimary  = FLinearColor(0.32f, 0.74f, 0.98f, 1.0f);
		P.AccentSecondary= FLinearColor(0.28f, 0.68f, 0.96f, 1.0f);
		return P;
	}

	// Lerp only the RGB of a token toward a tint by Amount, preserving the token's alpha.
	inline FLinearColor TintAccent(const FLinearColor& Token, const FLinearColor& Tint, float Amount)
	{
		const float A = FMath::Clamp(Amount, 0.0f, 1.0f);
		return FLinearColor(
			FMath::Lerp(Token.R, Tint.R, A),
			FMath::Lerp(Token.G, Tint.G, A),
			FMath::Lerp(Token.B, Tint.B, A),
			Token.A);
	}

	// Resolve the active palette for this frame.
	//   - Base is Standard or Colorblind (Colorblind keeps its danger/safe semantics).
	//   - High-Contrast overrides text/panel/border tokens (accessibility wins there).
	//   - The map accent tint is lerped into the accent tokens only, so the shared
	//     aesthetic survives while each map keeps its own colour.
	inline FBHHudPalette ResolveActivePalette(EPaletteMode Mode, const FLinearColor& MapAccentTint, bool bHighContrast, float MapAccentAmount = 0.6f)
	{
		FBHHudPalette P = (Mode == EPaletteMode::Colorblind) ? MakeColorblindPalette() : MakeStandardPalette();

		if (bHighContrast)
		{
			const FBHHudPalette HC = MakeHighContrastPalette();
			// High-contrast wins on text / panel / border tokens for readability, but we
			// keep the colorblind-safe danger/safe semantics if that mode is active.
			P.PanelFill      = HC.PanelFill;
			P.PanelHighlight = HC.PanelHighlight;
			P.TextMain       = HC.TextMain;
			P.TextMuted      = HC.TextMuted;
			P.TextDim        = HC.TextDim;
			P.KeyBoxLit      = HC.KeyBoxLit;
			P.KeyBoxDim      = HC.KeyBoxDim;
			if (Mode != EPaletteMode::Colorblind)
			{
				P.WarnHot = HC.WarnHot;
				P.Good    = HC.Good;
				P.Bad     = HC.Bad;
			}
		}

		P.AccentPrimary   = TintAccent(P.AccentPrimary, MapAccentTint, MapAccentAmount);
		P.AccentSecondary = TintAccent(P.AccentSecondary, MapAccentTint, MapAccentAmount);
		return P;
	}

	// Per-map accent tint by (PIE-prefix-stripped) map name. Defaults to the historical
	// teal so unrecognised maps look unchanged.
	inline FLinearColor MapAccentTintForLevel(const FString& LevelName)
	{
		const FLinearColor DefaultTint(0.40f, 0.90f, 0.82f, 1.0f); // teal
		if (LevelName.Contains(TEXT("Facility")))
		{
			return FLinearColor(0.40f, 0.90f, 0.82f, 1.0f); // clean teal
		}
		if (LevelName.Contains(TEXT("Substation")))
		{
			return FLinearColor(0.46f, 0.80f, 1.0f, 1.0f); // electric blue
		}
		if (LevelName.Contains(TEXT("Foggrounds")))
		{
			return FLinearColor(0.62f, 0.86f, 0.74f, 1.0f); // pale fog green
		}
		if (LevelName.Contains(TEXT("Train")) || LevelName.Contains(TEXT("Intermission")))
		{
			return FLinearColor(0.30f, 0.92f, 0.82f, 1.0f); // subway teal-cyan
		}
		return DefaultTint;
	}
}

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHMenuTheme.h"

namespace
{
	// Global active-theme index. UI-only preference; defaults to "Dread" (index 5), the atmospheric-horror
	// look. The account/theme-picker flow may override this on menu open.
	int32 GActiveMenuThemeIndex = 5;

	// Global theme STRENGTH (0 = faint/near-neutral, 1 = full theme colour). UI-only preference. 1 = today's look.
	float GActiveMenuThemeStrength = 1.0f;

	const TArray<FBHMenuTheme>& BHAllMenuThemes()
	{
		// Order is the persisted key -- APPEND ONLY. Each row: Name, Background, Panel, PanelBorder, Header,
		// TextPrimary, TextDim, Accent, AccentBright, ButtonIdle, ButtonHover, Danger.
		static const TArray<FBHMenuTheme> Themes = {
			{ TEXT("Classic"),
				FLinearColor(0.020f, 0.025f, 0.030f, 1.0f), FLinearColor(0.050f, 0.060f, 0.070f, 1.0f), FLinearColor(0.180f, 0.220f, 0.260f, 1.0f),
				FLinearColor(0.780f, 0.840f, 0.880f, 1.0f), FLinearColor(0.860f, 0.900f, 0.920f, 1.0f), FLinearColor(0.550f, 0.620f, 0.660f, 1.0f),
				FLinearColor(0.300f, 0.620f, 0.780f, 1.0f), FLinearColor(0.460f, 0.800f, 0.950f, 1.0f),
				FLinearColor(0.100f, 0.130f, 0.160f, 1.0f), FLinearColor(0.180f, 0.240f, 0.300f, 1.0f), FLinearColor(0.920f, 0.300f, 0.220f, 1.0f) },

			{ TEXT("Midnight"),
				FLinearColor(0.015f, 0.020f, 0.045f, 1.0f), FLinearColor(0.040f, 0.050f, 0.100f, 1.0f), FLinearColor(0.200f, 0.240f, 0.420f, 1.0f),
				FLinearColor(0.800f, 0.840f, 0.950f, 1.0f), FLinearColor(0.880f, 0.900f, 0.980f, 1.0f), FLinearColor(0.550f, 0.600f, 0.780f, 1.0f),
				FLinearColor(0.420f, 0.460f, 0.920f, 1.0f), FLinearColor(0.580f, 0.640f, 1.000f, 1.0f),
				FLinearColor(0.080f, 0.100f, 0.200f, 1.0f), FLinearColor(0.160f, 0.200f, 0.380f, 1.0f), FLinearColor(0.950f, 0.350f, 0.450f, 1.0f) },

			{ TEXT("Crimson"),
				FLinearColor(0.040f, 0.015f, 0.018f, 1.0f), FLinearColor(0.090f, 0.040f, 0.045f, 1.0f), FLinearColor(0.420f, 0.160f, 0.180f, 1.0f),
				FLinearColor(0.940f, 0.840f, 0.840f, 1.0f), FLinearColor(0.960f, 0.900f, 0.900f, 1.0f), FLinearColor(0.700f, 0.550f, 0.560f, 1.0f),
				FLinearColor(0.820f, 0.220f, 0.240f, 1.0f), FLinearColor(0.980f, 0.380f, 0.400f, 1.0f),
				FLinearColor(0.160f, 0.080f, 0.090f, 1.0f), FLinearColor(0.300f, 0.120f, 0.140f, 1.0f), FLinearColor(0.980f, 0.620f, 0.220f, 1.0f) },

			{ TEXT("Emerald"),
				FLinearColor(0.015f, 0.030f, 0.022f, 1.0f), FLinearColor(0.040f, 0.080f, 0.055f, 1.0f), FLinearColor(0.180f, 0.360f, 0.260f, 1.0f),
				FLinearColor(0.820f, 0.920f, 0.840f, 1.0f), FLinearColor(0.900f, 0.960f, 0.900f, 1.0f), FLinearColor(0.560f, 0.720f, 0.600f, 1.0f),
				FLinearColor(0.240f, 0.740f, 0.450f, 1.0f), FLinearColor(0.400f, 0.920f, 0.600f, 1.0f),
				FLinearColor(0.070f, 0.140f, 0.100f, 1.0f), FLinearColor(0.130f, 0.260f, 0.180f, 1.0f), FLinearColor(0.950f, 0.550f, 0.250f, 1.0f) },

			{ TEXT("Arcade"),
				FLinearColor(0.030f, 0.020f, 0.040f, 1.0f), FLinearColor(0.070f, 0.050f, 0.100f, 1.0f), FLinearColor(0.400f, 0.200f, 0.480f, 1.0f),
				FLinearColor(0.920f, 0.860f, 0.960f, 1.0f), FLinearColor(0.960f, 0.920f, 1.000f, 1.0f), FLinearColor(0.660f, 0.580f, 0.740f, 1.0f),
				FLinearColor(0.950f, 0.250f, 0.700f, 1.0f), FLinearColor(1.000f, 0.420f, 0.850f, 1.0f),
				FLinearColor(0.120f, 0.080f, 0.160f, 1.0f), FLinearColor(0.240f, 0.140f, 0.300f, 1.0f), FLinearColor(0.300f, 0.900f, 0.920f, 1.0f) },

			// "Dread" -- atmospheric horror: near-black, oppressively low contrast, a single restrained
			// dried-blood accent. The default menu look (see GActiveMenuThemeIndex).
			{ TEXT("Dread"),
				FLinearColor(0.009f, 0.010f, 0.011f, 1.0f), FLinearColor(0.026f, 0.027f, 0.029f, 1.0f), FLinearColor(0.130f, 0.115f, 0.100f, 1.0f),
				FLinearColor(0.640f, 0.615f, 0.575f, 1.0f), FLinearColor(0.730f, 0.705f, 0.665f, 1.0f), FLinearColor(0.430f, 0.410f, 0.385f, 1.0f),
				FLinearColor(0.500f, 0.150f, 0.130f, 1.0f), FLinearColor(0.820f, 0.260f, 0.200f, 1.0f),
				FLinearColor(0.040f, 0.037f, 0.035f, 1.0f), FLinearColor(0.150f, 0.090f, 0.080f, 1.0f), FLinearColor(0.900f, 0.170f, 0.120f, 1.0f) },
		};
		return Themes;
	}

	// Per-theme unlock gate. SAME index order as BHAllMenuThemes (append-only). A non-empty RequiredAchievement means
	// the theme is achievement-gated and XP is ignored; otherwise the XP threshold applies. Classic (0) is free.
	struct FBHMenuThemeGate { int32 RequiredXP = 0; const TCHAR* RequiredAchievement = nullptr; };
	const FBHMenuThemeGate& BHMenuThemeGateAt(int32 Index)
	{
		static const FBHMenuThemeGate Gates[] = {
			{ 0,   nullptr },               // 0 Classic  -- free (today's look)
			{ 500, nullptr },               // 1 Midnight -- XP gate
			{ 0,   TEXT("first_blood") },   // 2 Crimson  -- achievement: capture your first survivor
			{ 0,   TEXT("flow_master") },   // 3 Emerald  -- achievement: full three-link flow chain
			{ 0,   TEXT("codebreaker") },   // 4 Arcade   -- achievement: the Konami easter egg
			{ 0,   nullptr },               // 5 Dread    -- free (the default atmospheric-horror look)
		};
		return Gates[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(Gates)) - 1)];
	}
}

int32 BHMenuThemeCount()
{
	return BHAllMenuThemes().Num();
}

const FBHMenuTheme& BHMenuThemeAt(int32 Index)
{
	const TArray<FBHMenuTheme>& Themes = BHAllMenuThemes();
	return Themes[FMath::Clamp(Index, 0, Themes.Num() - 1)];
}

const TCHAR* BHMenuThemeName(int32 Index)
{
	return BHMenuThemeAt(Index).Name;
}

int32 BHMenuThemeRequiredXP(int32 Index)
{
	return FMath::Max(0, BHMenuThemeGateAt(Index).RequiredXP);
}

const TCHAR* BHMenuThemeRequiredAchievement(int32 Index)
{
	return BHMenuThemeGateAt(Index).RequiredAchievement;
}

bool BHMenuThemeIsUnlocked(int32 Index, int32 XP, const TArray<FName>* UnlockedAchievements)
{
	const TCHAR* Ach = BHMenuThemeRequiredAchievement(Index);
	if (Ach && Ach[0] != TEXT('\0'))
	{
		return UnlockedAchievements && UnlockedAchievements->Contains(FName(Ach));
	}
	return FMath::Max(0, XP) >= BHMenuThemeRequiredXP(Index);
}

const FBHMenuTheme& BHActiveMenuTheme()
{
	return BHMenuThemeAt(GActiveMenuThemeIndex);
}

void BHSetActiveMenuThemeIndex(int32 Index)
{
	GActiveMenuThemeIndex = FMath::Clamp(Index, 0, BHAllMenuThemes().Num() - 1);
}

int32 BHActiveMenuThemeIndex()
{
	return GActiveMenuThemeIndex;
}

float BHActiveMenuThemeStrength()
{
	return GActiveMenuThemeStrength;
}

void BHSetActiveMenuThemeStrength(float Strength)
{
	GActiveMenuThemeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
}

FLinearColor BHResolveActiveThemeColor(FLinearColor FBHMenuTheme::* Role)
{
	// Blend the active theme's role colour toward the near-neutral Classic (index 0) baseline by strength, so a
	// faint theme fades to the original look and a strong theme shows the full colour. Alpha is taken from the
	// theme colour unchanged (we blend hue/value only). This is the single source of truth for themed colours.
	const FLinearColor ThemeColor = BHActiveMenuTheme().*Role;
	const FLinearColor Neutral = BHMenuThemeAt(0).*Role;
	FLinearColor Out = FMath::Lerp(Neutral, ThemeColor, GActiveMenuThemeStrength);
	Out.A = ThemeColor.A;
	return Out;
}

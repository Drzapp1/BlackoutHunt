// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHMenuTheme.h"

namespace
{
	// Global active-theme index. UI-only preference; defaults to Classic (the current look).
	int32 GActiveMenuThemeIndex = 0;

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
		};
		return Themes;
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

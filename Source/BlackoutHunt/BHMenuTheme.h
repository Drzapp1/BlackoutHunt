// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

// A menu colour theme. The menu reads colours by ROLE (not literal value) so a whole look can be swapped at
// once. Cosmetic-only: themes never touch gameplay, scoring, or replication -- they are a local UI preference.
struct FBHMenuTheme
{
	const TCHAR* Name = TEXT("Classic");
	FLinearColor Background;    // outermost menu backdrop
	FLinearColor Panel;         // card / section background
	FLinearColor PanelBorder;   // card borders / dividers
	FLinearColor Header;        // section header text
	FLinearColor TextPrimary;   // body text
	FLinearColor TextDim;       // secondary / hint text
	FLinearColor Accent;        // primary accent: selected tab, highlights, swatches
	FLinearColor AccentBright;  // brighter accent for emphasis
	FLinearColor ButtonIdle;    // default button background
	FLinearColor ButtonHover;   // button hover background
	FLinearColor Danger;        // warnings / threat
};

// Theme registry. Append-only (the index is persisted), index 0 is "Classic" (today's look).
BLACKOUTHUNT_API int32 BHMenuThemeCount();
BLACKOUTHUNT_API const FBHMenuTheme& BHMenuThemeAt(int32 Index);   // clamped to a valid index
BLACKOUTHUNT_API const TCHAR* BHMenuThemeName(int32 Index);

// The active theme is a global UI preference (not gameplay state). The menu sets it from the account selection
// on open, and the theme picker updates it live. Colours read through BHActiveMenuTheme() so swapping is instant.
BLACKOUTHUNT_API const FBHMenuTheme& BHActiveMenuTheme();
BLACKOUTHUNT_API void BHSetActiveMenuThemeIndex(int32 Index);
BLACKOUTHUNT_API int32 BHActiveMenuThemeIndex();

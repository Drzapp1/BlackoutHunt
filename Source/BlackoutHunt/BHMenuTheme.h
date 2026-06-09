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

// Theme unlock gating -- same model as the avatar cosmetics (an XP threshold OR an achievement id per theme index).
// Index 0 (Classic) is always free. UnlockedAchievements may be null (treated as none earned). An achievement gate
// ignores XP; otherwise the XP threshold applies.
BLACKOUTHUNT_API int32 BHMenuThemeRequiredXP(int32 Index);
BLACKOUTHUNT_API const TCHAR* BHMenuThemeRequiredAchievement(int32 Index);   // empty/nullptr = no achievement gate
BLACKOUTHUNT_API bool BHMenuThemeIsUnlocked(int32 Index, int32 XP, const TArray<FName>* UnlockedAchievements = nullptr);

// The active theme is a global UI preference (not gameplay state). The menu sets it from the account selection
// on open, and the theme picker updates it live. Colours read through BHActiveMenuTheme() so swapping is instant.
BLACKOUTHUNT_API const FBHMenuTheme& BHActiveMenuTheme();
BLACKOUTHUNT_API void BHSetActiveMenuThemeIndex(int32 Index);
BLACKOUTHUNT_API int32 BHActiveMenuThemeIndex();

// UI-only theme STRENGTH (0 = faint/near-neutral, 1 = full theme colour). Global preference like the active index.
// Every themed colour is blended Lerp(Classic[role], ActiveTheme[role], Strength) so a faint theme fades toward the
// original near-neutral look. Default 1.0 = full strength.
BLACKOUTHUNT_API float BHActiveMenuThemeStrength();
BLACKOUTHUNT_API void BHSetActiveMenuThemeStrength(float Strength);   // clamped to [0,1]

// UI-only per-section CONTRAST (0 = very faint / near-monotone, 1 = very high per-section identity). Global
// preference like the theme strength. The in-menu Play "section cards" blend every per-section colour
// Lerp(neutral-theme-baseline, bold-derived-identity, Contrast), so this dial scales how strongly each
// section's hue/value reads apart. Default 1.0 = full identity.
BLACKOUTHUNT_API float BHMenuSectionContrast();
BLACKOUTHUNT_API void BHSetMenuSectionContrast(float Contrast);   // clamped to [0,1]

// Resolved colour for a role under the active theme AND strength. Single source of truth -- the menu helper
// BHThemeColorAttr and the option-colour getters all route through this so the strength dial affects everything.
BLACKOUTHUNT_API FLinearColor BHResolveActiveThemeColor(FLinearColor FBHMenuTheme::* Role);

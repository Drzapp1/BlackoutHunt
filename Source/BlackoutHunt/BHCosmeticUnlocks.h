// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

enum class EBHCosmeticCategory : uint8
{
	Outfit,
	ShirtColor,
	Headwear,
	Gear,
	// Achievement-flair shown on the nameplate (not on the avatar mesh): an earned Title under your name and a
	// small earned Emblem badge beside it. Index 0 is always "none".
	Title,
	Emblem
};

BLACKOUTHUNT_API int32 BHCosmeticMaxIndex(EBHCosmeticCategory Category);
BLACKOUTHUNT_API int32 BHCosmeticClampIndex(EBHCosmeticCategory Category, int32 Index);
BLACKOUTHUNT_API int32 BHCosmeticRequiredXP(EBHCosmeticCategory Category, int32 Index);
// An item can be gated by an ACHIEVEMENT instead of XP (returns the achievement id, or nullptr/empty for the
// usual XP gate). Achievement-gated items are the hidden "prestige" tints; pass the player's unlocked set to
// the unlock checks below so they resolve. The optional UnlockedAchievements is null-safe (treated as locked).
BLACKOUTHUNT_API const TCHAR* BHCosmeticRequiredAchievement(EBHCosmeticCategory Category, int32 Index);
BLACKOUTHUNT_API bool BHCosmeticIsUnlocked(EBHCosmeticCategory Category, int32 Index, int32 XP, const TArray<FName>* UnlockedAchievements = nullptr);
BLACKOUTHUNT_API int32 BHCosmeticClampUnlockedIndex(EBHCosmeticCategory Category, int32 Index, int32 XP, const TArray<FName>* UnlockedAchievements = nullptr);
BLACKOUTHUNT_API int32 BHCosmeticNextUnlockedIndex(EBHCosmeticCategory Category, int32 CurrentIndex, int32 XP, const TArray<FName>* UnlockedAchievements = nullptr);
BLACKOUTHUNT_API const TCHAR* BHCosmeticCategoryName(EBHCosmeticCategory Category);
BLACKOUTHUNT_API const TCHAR* BHCosmeticItemName(EBHCosmeticCategory Category, int32 Index);

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHCosmeticUnlocks.h"

namespace
{
struct FBHCosmeticUnlockDefinition
{
	const TCHAR* Name = TEXT("Unknown");
	int32 RequiredXP = 0;
	// When set, this item is gated by an ACHIEVEMENT (see BHAccountSubsystem) instead of XP -- the hidden
	// prestige tints. nullptr = the usual XP gate.
	const TCHAR* RequiredAchievement = nullptr;
};

const FBHCosmeticUnlockDefinition* BHCosmeticDefinitions(EBHCosmeticCategory Category, int32& OutCount)
{
	static const FBHCosmeticUnlockDefinition OutfitDefinitions[] = {
		{ TEXT("Casual"), 0 },
		{ TEXT("Worker"), 0 },
		{ TEXT("Adventurer"), 100 },
		{ TEXT("Farmer"), 250 },
		{ TEXT("Beach"), 450 },
		{ TEXT("Punk"), 700 },
		{ TEXT("Suit"), 950 },
		{ TEXT("Spacesuit"), 1250 }
	};
	static const FBHCosmeticUnlockDefinition ShirtColorDefinitions[] = {
		{ TEXT("Blue"), 0 },
		{ TEXT("Orange"), 0 },
		{ TEXT("Green"), 0 },
		{ TEXT("Red"), 0 },
		{ TEXT("Purple"), 0 },
		{ TEXT("Gold"), 0 },
		{ TEXT("Teal"), 0 },
		{ TEXT("White"), 0 },
		// Hidden "prestige" tints -- unlocked by ACHIEVEMENTS, not XP (see Docs/EASTER_EGGS.md). Their exact
		// colour shows on nameplates / roster / blips; on the 8-material Quaternius body it maps to the nearest
		// base material until matching body materials are authored. This index order MUST match the palette
		// tables in BHGameMode/BHCharacter/BHPlayerController (entries 8..11).
		{ TEXT("Chalk"), 0, TEXT("honorary_faculty") },
		{ TEXT("Arcade"), 0, TEXT("codebreaker") },
		{ TEXT("Exit Sign"), 0, TEXT("escape_artist") },
		{ TEXT("Afterimage"), 0, TEXT("perfect_chain") }
	};
	static const FBHCosmeticUnlockDefinition HeadwearDefinitions[] = {
		{ TEXT("None"), 0 },
		{ TEXT("Cap"), 150 },
		{ TEXT("Glasses"), 300 },
		{ TEXT("Beanie"), 500 },
		{ TEXT("Visor"), 750 }
	};
	static const FBHCosmeticUnlockDefinition GearDefinitions[] = {
		{ TEXT("None"), 0 }
	};

	switch (Category)
	{
	case EBHCosmeticCategory::Outfit:
		OutCount = UE_ARRAY_COUNT(OutfitDefinitions);
		return OutfitDefinitions;
	case EBHCosmeticCategory::ShirtColor:
		OutCount = UE_ARRAY_COUNT(ShirtColorDefinitions);
		return ShirtColorDefinitions;
	case EBHCosmeticCategory::Headwear:
		OutCount = UE_ARRAY_COUNT(HeadwearDefinitions);
		return HeadwearDefinitions;
	case EBHCosmeticCategory::Gear:
		OutCount = UE_ARRAY_COUNT(GearDefinitions);
		return GearDefinitions;
	default:
		OutCount = 0;
		return nullptr;
	}
}
}

int32 BHCosmeticMaxIndex(EBHCosmeticCategory Category)
{
	int32 Count = 0;
	BHCosmeticDefinitions(Category, Count);
	return FMath::Max(0, Count - 1);
}

int32 BHCosmeticClampIndex(EBHCosmeticCategory Category, int32 Index)
{
	return FMath::Clamp(Index, 0, BHCosmeticMaxIndex(Category));
}

int32 BHCosmeticClampUnlockedIndex(EBHCosmeticCategory Category, int32 Index, int32 XP, const TArray<FName>* UnlockedAchievements)
{
	const int32 ClampedIndex = BHCosmeticClampIndex(Category, Index);
	return BHCosmeticIsUnlocked(Category, ClampedIndex, XP, UnlockedAchievements) ? ClampedIndex : 0;
}

const TCHAR* BHCosmeticRequiredAchievement(EBHCosmeticCategory Category, int32 Index)
{
	int32 Count = 0;
	const FBHCosmeticUnlockDefinition* Definitions = BHCosmeticDefinitions(Category, Count);
	if (!Definitions || Count <= 0)
	{
		return nullptr;
	}
	const int32 ClampedIndex = FMath::Clamp(Index, 0, Count - 1);
	return Definitions[ClampedIndex].RequiredAchievement;
}

int32 BHCosmeticRequiredXP(EBHCosmeticCategory Category, int32 Index)
{
	int32 Count = 0;
	const FBHCosmeticUnlockDefinition* Definitions = BHCosmeticDefinitions(Category, Count);
	const int32 ClampedIndex = FMath::Clamp(Index, 0, FMath::Max(0, Count - 1));
	return Definitions && Count > 0 ? FMath::Max(0, Definitions[ClampedIndex].RequiredXP) : 0;
}

bool BHCosmeticIsUnlocked(EBHCosmeticCategory Category, int32 Index, int32 XP, const TArray<FName>* UnlockedAchievements)
{
	const TCHAR* RequiredAchievement = BHCosmeticRequiredAchievement(Category, Index);
	if (RequiredAchievement && RequiredAchievement[0] != TEXT('\0'))
	{
		// Achievement-gated (hidden prestige tints): XP is ignored; the player must have earned the achievement.
		return UnlockedAchievements && UnlockedAchievements->Contains(FName(RequiredAchievement));
	}
	const int32 ClampedXP = FMath::Max(0, XP);
	return ClampedXP >= BHCosmeticRequiredXP(Category, Index);
}

int32 BHCosmeticNextUnlockedIndex(EBHCosmeticCategory Category, int32 CurrentIndex, int32 XP, const TArray<FName>* UnlockedAchievements)
{
	const int32 MaxIndex = BHCosmeticMaxIndex(Category);
	const int32 StartIndex = BHCosmeticClampIndex(Category, CurrentIndex);
	for (int32 Offset = 1; Offset <= MaxIndex + 1; ++Offset)
	{
		const int32 CandidateIndex = (StartIndex + Offset) % (MaxIndex + 1);
		if (BHCosmeticIsUnlocked(Category, CandidateIndex, XP, UnlockedAchievements))
		{
			return CandidateIndex;
		}
	}

	return 0;
}

const TCHAR* BHCosmeticCategoryName(EBHCosmeticCategory Category)
{
	switch (Category)
	{
	case EBHCosmeticCategory::Outfit:
		return TEXT("Outfit");
	case EBHCosmeticCategory::ShirtColor:
		return TEXT("Shirt");
	case EBHCosmeticCategory::Headwear:
		return TEXT("Headwear");
	case EBHCosmeticCategory::Gear:
		return TEXT("Gear");
	default:
		return TEXT("Cosmetic");
	}
}

const TCHAR* BHCosmeticItemName(EBHCosmeticCategory Category, int32 Index)
{
	int32 Count = 0;
	const FBHCosmeticUnlockDefinition* Definitions = BHCosmeticDefinitions(Category, Count);
	if (!Definitions || Count <= 0)
	{
		return TEXT("Unknown");
	}

	const int32 ClampedIndex = FMath::Clamp(Index, 0, Count - 1);
	return Definitions[ClampedIndex].Name;
}

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHCosmeticUnlocks.h"

const TArray<FName>& BHColorableMaterialNames()
{
	// APPEND-ONLY (see header). From the Quaternius skin material-slot dump; excludes the face/skin/hair slots
	// (Skin/Skin_Darker/Hair/Hair_White/Eye/Eyebrows/Moustache) handled by BHShouldPreserveQuaterniusMaterial.
	static const TArray<FName> Names = {
		FName(TEXT("Red_Dark")), FName(TEXT("White")), FName(TEXT("LightBrown")), FName(TEXT("LightBlue")),
		FName(TEXT("Purple")), FName(TEXT("Grey")), FName(TEXT("Black")), FName(TEXT("Brown")),
		FName(TEXT("Brown2")), FName(TEXT("Worker_Yellow")), FName(TEXT("Worker_Vest")), FName(TEXT("Green")),
		FName(TEXT("LightGreen")), FName(TEXT("Gold")), FName(TEXT("Beige")), FName(TEXT("Red")),
		FName(TEXT("Earrings")), FName(TEXT("Suit")), FName(TEXT("Tie")), FName(TEXT("SciFi_Light_Accent")),
		FName(TEXT("SciFi_Light")), FName(TEXT("SciFi_Main")), FName(TEXT("SciFi_MainDark")), FName(TEXT("Swat")),
		FName(TEXT("Swat_Black")), FName(TEXT("Visor")), FName(TEXT("Metal")), FName(TEXT("DarkBrown")),
		FName(TEXT("Metal_Dark")), FName(TEXT("Blue"))
	};
	return Names;
}

int32 BHColorableMaterialCount()
{
	return BHColorableMaterialNames().Num();
}

int32 BHColorableMaterialIndex(FName MaterialSlotName)
{
	return BHColorableMaterialNames().IndexOfByKey(MaterialSlotName);
}

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
		{ TEXT("Adventurer"), 100 },          // the one early XP carrot kept for brand-new players
		// Most outfits now gate behind ACHIEVEMENTS rather than raw XP, so the bulk of the wardrobe rewards play
		// goals (see Docs/EASTER_EGGS.md). Farmer/Beach/Punk were XP unlocks (250/450/700) before the 2026 pass.
		{ TEXT("Farmer"), 0, TEXT("centurion") },
		{ TEXT("Beach"), 0, TEXT("houdini") },
		{ TEXT("Punk"), 0, TEXT("unstoppable") },
		// Prestige outfits -- unlocked by ACHIEVEMENTS instead of XP (see Docs/EASTER_EGGS.md).
		{ TEXT("Suit"), 0, TEXT("survivor") },
		{ TEXT("Spacesuit"), 0, TEXT("perfect_chain") }
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
		// tables in BHGameMode/BHCharacter/BHPlayerController (entries 8..17).
		{ TEXT("Chalk"), 0, TEXT("honorary_faculty") },
		{ TEXT("Arcade"), 0, TEXT("codebreaker") },
		{ TEXT("Exit Sign"), 0, TEXT("escape_artist") },
		{ TEXT("Afterimage"), 0, TEXT("perfect_chain") },
		{ TEXT("Veteran"), 0, TEXT("veteran") },
		{ TEXT("Faculty"), 0, TEXT("top_of_the_class") },
		{ TEXT("Slipstream"), 0, TEXT("flow_master") },
		{ TEXT("Detention"), 0, TEXT("first_blood") },
		{ TEXT("Apex"), 0, TEXT("flawless_hunt") },
		{ TEXT("Commuter"), 0, TEXT("tourist") },
		// Free neutral. Recolour-only black -- maps to the dedicated Black Quaternius body material (index 18).
		{ TEXT("Black"), 0 }
	};
	static const FBHCosmeticUnlockDefinition HeadwearDefinitions[] = {
		// Headwear was REMOVED (2026): the procedural hats + face accessories (Cap/Glasses/Beanie/Visor/Top Hat/
		// Crown/Halo/Graduation Cap) stacked on the Quaternius skins' baked-in headwear and read wrong, so the whole
		// category is now just "None". The picker row is hidden in the menu and nothing renders. The achievements
		// that used to grant hats (Roof Rider / On a Roll / Completionist / Graduate) now reward titles/emblems.
		{ TEXT("None"), 0 }
	};
	static const FBHCosmeticUnlockDefinition GearDefinitions[] = {
		{ TEXT("None"), 0 }
	};
	// Nameplate flair (achievement-gated; index 0 = none). Shown under the player's name / beside it, not on
	// the avatar mesh. See Docs/EASTER_EGGS.md.
	static const FBHCosmeticUnlockDefinition TitleDefinitions[] = {
		{ TEXT("No Title"), 0 },
		{ TEXT("Honors Student"), 0, TEXT("escape_artist") },
		{ TEXT("Last One Standing"), 0, TEXT("survivor") },
		{ TEXT("The Untouchable"), 0, TEXT("flawless_hunt") },
		{ TEXT("Speedrunner"), 0, TEXT("flow_master") },
		{ TEXT("Graduate"), 0, TEXT("graduate") },
		{ TEXT("Roof Rider"), 0, TEXT("roof_rider") },
		{ TEXT("Completionist"), 0, TEXT("completionist") },
		{ TEXT("Honor Roll"), 0, TEXT("honor_roll") },
		{ TEXT("Polymath"), 0, TEXT("polymath") },
		// 2026 expansion titles (each gated on a new achievement; pure nameplate text -- no art).
		{ TEXT("Survivalist"), 0, TEXT("survivalist") },
		{ TEXT("Headmaster"), 0, TEXT("headmaster") },
		{ TEXT("Honor Society"), 0, TEXT("honor_society") },
		{ TEXT("Dean's List"), 0, TEXT("deans_list") },
		{ TEXT("Maestro"), 0, TEXT("momentum_maestro") },
		{ TEXT("Valedictorian"), 0, TEXT("valedictorian") },
		{ TEXT("Bookworm"), 0, TEXT("bookworm") },
		{ TEXT("Ghost"), 0, TEXT("ghost_in_walls") },
		{ TEXT("Saved by the Bell"), 0, TEXT("saved_by_bell") },
		{ TEXT("Night Owl"), 0, TEXT("night_owl") },
		{ TEXT("Hitchhiker"), 0, TEXT("dont_panic") },
		{ TEXT("Honor Graduate"), 0, TEXT("honor_graduate") },
		{ TEXT("Perfectionist"), 0, TEXT("perfectionist") },
		{ TEXT("Treasure Hunter"), 0, TEXT("relic_collector") },
		{ TEXT("Faculty Lounge"), 0, TEXT("roll_call") },   // VIP egg: the real teacher
		{ TEXT("Treehugger"), 0, TEXT("stuck_in_tree") }
	};
	static const FBHCosmeticUnlockDefinition EmblemDefinitions[] = {
		{ TEXT("No Emblem"), 0 },
		{ TEXT("Chalk Star"), 0, TEXT("honorary_faculty") },
		{ TEXT("Exit Sign"), 0, TEXT("escape_artist") },
		{ TEXT("Crown"), 0, TEXT("on_a_roll") },
		{ TEXT("Halo"), 0, TEXT("completionist") },
		{ TEXT("Ember"), 0, TEXT("flawless_hunt") },
		// 2026 expansion emblems (index order MUST match the colour cases in BHHUD.cpp BHNameplateEmblemColor).
		{ TEXT("Pop Quiz"), 0, TEXT("pop_quiz") },            // 6
		{ TEXT("Falling Apple"), 0, TEXT("gravity_code") },   // 7
		{ TEXT("Static"), 0, TEXT("did_you_see_that") },      // 8
		{ TEXT("Phoenix"), 0, TEXT("comeback_kid") },         // 9
		{ TEXT("Hall Pass"), 0, TEXT("truant_officer") },     // 10
		{ TEXT("Atom"), 0, TEXT("subject_expert") },          // 11
		{ TEXT("Gold Star"), 0, TEXT("roll_call") }           // 12 (VIP egg: the real teacher)
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
	case EBHCosmeticCategory::Title:
		OutCount = UE_ARRAY_COUNT(TitleDefinitions);
		return TitleDefinitions;
	case EBHCosmeticCategory::Emblem:
		OutCount = UE_ARRAY_COUNT(EmblemDefinitions);
		return EmblemDefinitions;
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
	case EBHCosmeticCategory::Title:
		return TEXT("Title");
	case EBHCosmeticCategory::Emblem:
		return TEXT("Emblem");
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

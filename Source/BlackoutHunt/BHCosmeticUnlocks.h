#pragma once

#include "CoreMinimal.h"

enum class EBHCosmeticCategory : uint8
{
	Outfit,
	ShirtColor,
	Headwear,
	Gear
};

BLACKOUTHUNT_API int32 BHCosmeticMaxIndex(EBHCosmeticCategory Category);
BLACKOUTHUNT_API int32 BHCosmeticClampIndex(EBHCosmeticCategory Category, int32 Index);
BLACKOUTHUNT_API int32 BHCosmeticClampUnlockedIndex(EBHCosmeticCategory Category, int32 Index, int32 XP);
BLACKOUTHUNT_API int32 BHCosmeticRequiredXP(EBHCosmeticCategory Category, int32 Index);
BLACKOUTHUNT_API bool BHCosmeticIsUnlocked(EBHCosmeticCategory Category, int32 Index, int32 XP);
BLACKOUTHUNT_API int32 BHCosmeticNextUnlockedIndex(EBHCosmeticCategory Category, int32 CurrentIndex, int32 XP);
BLACKOUTHUNT_API const TCHAR* BHCosmeticCategoryName(EBHCosmeticCategory Category);
BLACKOUTHUNT_API const TCHAR* BHCosmeticItemName(EBHCosmeticCategory Category, int32 Index);

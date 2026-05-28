#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

BLACKOUTHUNT_API TArray<FBHJumpscareVariant> GetResolvedJumpscareVariants();
BLACKOUTHUNT_API bool FindResolvedJumpscareVariantById(FName VariantId, FBHJumpscareVariant& OutVariant);
BLACKOUTHUNT_API TArray<FBHJumpscareVariant> GetResolvedWhisperJumpscareVariants();
BLACKOUTHUNT_API bool IsWhisperJumpscareVariant(const FBHJumpscareVariant& Variant);

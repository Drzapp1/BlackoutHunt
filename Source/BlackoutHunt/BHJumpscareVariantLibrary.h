// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

BLACKOUTHUNT_API TArray<FBHJumpscareVariant> GetResolvedJumpscareVariants();
BLACKOUTHUNT_API bool FindResolvedJumpscareVariantById(FName VariantId, FBHJumpscareVariant& OutVariant);
BLACKOUTHUNT_API FBHJumpscareVariant MakeLegacyScp096ProxyJumpscareVariant();
BLACKOUTHUNT_API FBHJumpscareVariant MakeRealScp096JumpscareVariant();
BLACKOUTHUNT_API TArray<FBHJumpscareVariant> GetResolvedWhisperJumpscareVariants();
BLACKOUTHUNT_API bool IsWhisperJumpscareVariant(const FBHJumpscareVariant& Variant);

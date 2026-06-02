// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

struct FBHJumpscareVariant;

BLACKOUTHUNT_API FVector BHResolveJumpscareCloseFocusLocation(const FVector& ViewLocation, const FVector& ViewForward, const FBHJumpscareVariant& Variant);

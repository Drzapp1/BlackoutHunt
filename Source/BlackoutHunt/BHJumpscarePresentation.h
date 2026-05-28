#pragma once

#include "CoreMinimal.h"

struct FBHJumpscareVariant;

BLACKOUTHUNT_API FVector BHResolveJumpscareCloseFocusLocation(const FVector& ViewLocation, const FVector& ViewForward, const FBHJumpscareVariant& Variant);

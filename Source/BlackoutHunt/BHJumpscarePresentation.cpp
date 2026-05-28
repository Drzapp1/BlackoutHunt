#include "BHJumpscarePresentation.h"

#include "BHTypes.h"

FVector BHResolveJumpscareCloseFocusLocation(const FVector& ViewLocation, const FVector& ViewForward, const FBHJumpscareVariant& Variant)
{
	FVector Forward = ViewForward;
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	const FVector CloseOffset = Variant.CloseVisualOffset;
	const float UpperBodyLift = FMath::Clamp(FocusHeight * 0.78f, 82.0f, 132.0f);
	const float FocusLift = FMath::Clamp(CloseOffset.Z + UpperBodyLift, -42.0f, 16.0f);
	const float FocusDistance = FMath::Clamp(
		CloseOffset.X + FMath::Clamp(FocusHeight * 0.04f, 4.0f, 10.0f),
		72.0f,
		116.0f);

	return ViewLocation + Forward * FocusDistance + FVector::UpVector * FocusLift;
}

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHPlayerCameraManager.h"

#include "BHCharacter.h"
#include "Camera/CameraTypes.h"

void ABHPlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ApplyCameraModifiers(DeltaTime, InOutPOV);

	// Forward somersault for a dodge roll: pitch the camera about its LOCAL right axis so the view tumbles
	// down -> over -> back up (a real forward roll), NOT a sideways barrel twist. The character drives the angle
	// (0..360 over the roll, 0 when idle). Applied to the final POV here -- not the control rotation -- so the full
	// revolution is neither clamped to +-90 by the pitch limit nor allowed to disturb the player's aim.
	if (const ABHCharacter* ViewChar = Cast<ABHCharacter>(GetViewTarget()))
	{
		const float SomersaultDeg = ViewChar->GetViewRollOffsetDeg();
		if (!FMath::IsNearlyZero(SomersaultDeg))
		{
			const FQuat Base = InOutPOV.Rotation.Quaternion();
			// Rotating about the local +Y (right) axis by a positive angle pitches the forward vector downward
			// first -- the "forward/down then up" arc of a real roll.
			const FQuat Somersault(FVector::RightVector, FMath::DegreesToRadians(SomersaultDeg));
			InOutPOV.Rotation = (Base * Somersault).Rotator();
		}
	}
}

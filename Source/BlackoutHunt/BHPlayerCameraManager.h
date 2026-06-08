// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "BHPlayerCameraManager.generated.h"

// First-person camera manager for Blackout Hunt. The pawn's CameraComponent uses bUsePawnControlRotation, so
// cosmetic rotation set on the camera component never reaches the view. Cosmetic VIEW rotations (e.g. the forward
// somersault during a dodge roll) are instead applied here, on the final POV in ApplyCameraModifiers -- after the
// control-rotation pitch limits, so a full 360 forward flip is neither clamped to +-90 nor allowed to corrupt the
// player's aim (the POV is separate from the control rotation).
UCLASS()
class BLACKOUTHUNT_API ABHPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};

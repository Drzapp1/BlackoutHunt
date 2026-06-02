// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHMovementAnimInstance.h"

void UBHMovementAnimInstance::SetBlackoutMovementState(
	EBHMovementSpecialState NewAuthoritativeState,
	EBHMovementSpecialState NewCosmeticState,
	float NewSpeed2D,
	bool bNewProneMoving,
	bool bNewGrounded,
	float NewFailurePulse)
{
	AuthoritativeSpecialState = NewAuthoritativeState;
	CosmeticSpecialState = NewCosmeticState;
	VisualSpecialState = NewAuthoritativeState != EBHMovementSpecialState::None ? NewAuthoritativeState : NewCosmeticState;
	Speed2D = NewSpeed2D;
	bProneMoving = bNewProneMoving;
	bGrounded = bNewGrounded;
	MovementFailurePulse = NewFailurePulse;
}


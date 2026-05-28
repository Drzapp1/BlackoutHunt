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


#include "BHMovementTuningAsset.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"

namespace
{
FBHMovementSpecialTuning BHMakeDefaultSpecialTuning(EBHMovementSpecialState State)
{
	FBHMovementSpecialTuning Tuning;
	Tuning.State = State;
	Tuning.LowStaminaText = FText::FromString(TEXT("Too exhausted for that move."));
	Tuning.CooldownText = FText::FromString(TEXT("Movement cooling down."));
	Tuning.BlockedText = FText::FromString(TEXT("Not enough room for that move."));

	switch (State)
	{
	case EBHMovementSpecialState::Rolling:
		Tuning.DurationSeconds = 0.55f;
		Tuning.StaminaCost = 12.0f;
		Tuning.CooldownSeconds = 1.25f;
		Tuning.NoiseStrength = 0.72f;
		Tuning.Curve.Distance = 420.0f;
		Tuning.Curve.MinForwardClearance = 210.0f;
		Tuning.Curve.MaxDropHeight = 64.0f;
		break;
	case EBHMovementSpecialState::Sliding:
		Tuning.DurationSeconds = 0.75f;
		Tuning.StaminaCost = 16.0f;
		Tuning.CooldownSeconds = 1.60f;
		Tuning.NoiseStrength = 1.06f;
		Tuning.Curve.Distance = 620.0f;
		Tuning.Curve.MinForwardClearance = 310.0f;
		Tuning.Curve.MaxDropHeight = 58.0f;
		break;
	case EBHMovementSpecialState::Diving:
		Tuning.DurationSeconds = 0.65f;
		Tuning.StaminaCost = 22.0f;
		Tuning.CooldownSeconds = 2.40f;
		Tuning.NoiseStrength = 1.24f;
		Tuning.Curve.Distance = 510.0f;
		Tuning.Curve.VerticalImpulse = 115.0f;
		Tuning.Curve.MinForwardClearance = 285.0f;
		Tuning.Curve.MaxDropHeight = 86.0f;
		break;
	default:
		break;
	}

	return Tuning;
}

FBHMovementRoleTuning BHMakeDefaultRoleTuning(EBHPlayerRole Role)
{
	FBHMovementRoleTuning Tuning;
	Tuning.Role = Role;
	Tuning.WalkSpeed = Role == EBHPlayerRole::Hunter ? 315.0f : 360.0f;
	Tuning.SprintSpeed = Role == EBHPlayerRole::Hunter ? 1150.0f : 900.0f;
	Tuning.SprintDrainMultiplier = Role == EBHPlayerRole::Hunter ? 1.75f : 1.0f;
	Tuning.ProneSpeed = 120.0f;
	Tuning.StaminaCostMultiplier = 1.0f;
	Tuning.CooldownMultiplier = 1.0f;
	Tuning.NoiseMultiplier = 1.0f;
	Tuning.ProneNoiseMultiplier = 0.38f;
	Tuning.ProneVisibilityMultiplier = 0.55f;

	if (Role == EBHPlayerRole::FakeHunter)
	{
		Tuning.ProneSpeed = 110.0f;
		Tuning.StaminaCostMultiplier = 1.20f;
		Tuning.NoiseMultiplier = 1.25f;
		Tuning.ProneNoiseMultiplier = 0.48f;
		Tuning.ProneVisibilityMultiplier = 0.68f;
	}
	else if (Role == EBHPlayerRole::Hunter)
	{
		Tuning.ProneSpeed = 95.0f;
		Tuning.StaminaCostMultiplier = 1.40f;
		Tuning.CooldownMultiplier = 1.30f;
		Tuning.NoiseMultiplier = 1.35f;
		Tuning.ProneNoiseMultiplier = 0.54f;
		Tuning.ProneVisibilityMultiplier = 0.82f;
	}

	Tuning.SpecialMoves = {
		BHMakeDefaultSpecialTuning(EBHMovementSpecialState::Rolling),
		BHMakeDefaultSpecialTuning(EBHMovementSpecialState::Sliding),
		BHMakeDefaultSpecialTuning(EBHMovementSpecialState::Diving)
	};
	return Tuning;
}
}

UBHMovementTuningAsset::UBHMovementTuningAsset()
{
	RoleTunings = {
		BHMakeDefaultRoleTuning(EBHPlayerRole::Survivor),
		BHMakeDefaultRoleTuning(EBHPlayerRole::Tester),
		BHMakeDefaultRoleTuning(EBHPlayerRole::FakeHunter),
		BHMakeDefaultRoleTuning(EBHPlayerRole::Hunter)
	};

	AnimationProfile.bPreferAnimBlueprint = true;
	AnimationProfile.Animations.Idle = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Idle.A_BH_Q_Idle")));
	AnimationProfile.Animations.Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Walk.A_BH_Q_Walk")));
	AnimationProfile.Animations.Run = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/A_BH_Q_Run.A_BH_Q_Run")));
	AnimationProfile.Animations.Roll = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Roll.A_BH_FAL_Roll")));
	AnimationProfile.Animations.Slide = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Slide.A_BH_FAL_Slide")));
	AnimationProfile.Animations.Dive = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Dive.A_BH_FAL_Dive")));
	AnimationProfile.Animations.ProneIdle = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Idle.A_BH_FAL_Prone_Idle")));
	AnimationProfile.Animations.ProneCrawl = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary/A_BH_FAL_Prone_Crawl.A_BH_FAL_Prone_Crawl")));
	AnimationProfile.QuaterniusAnimInstanceClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Quaternius/Animations/ABP_BH_Q_Movement.ABP_BH_Q_Movement_C")));
	AnimationProfile.HunterAnimInstanceClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Characters/Hunter/ABP_BH_Hunter_Movement.ABP_BH_Hunter_Movement_C")));
}

FBHMovementRoleTuning UBHMovementTuningAsset::ResolveRoleTuning(EBHPlayerRole Role) const
{
	const EBHPlayerRole EffectiveRole = Role == EBHPlayerRole::Unassigned || Role == EBHPlayerRole::Spectator
		? EBHPlayerRole::Survivor
		: Role;
	for (const FBHMovementRoleTuning& Tuning : RoleTunings)
	{
		if (Tuning.Role == EffectiveRole)
		{
			return Tuning;
		}
	}
	return BHMakeDefaultRoleTuning(EffectiveRole);
}

FBHMovementSpecialTuning UBHMovementTuningAsset::ResolveSpecialTuning(EBHPlayerRole Role, EBHMovementSpecialState State) const
{
	const FBHMovementRoleTuning RoleTuning = ResolveRoleTuning(Role);
	for (const FBHMovementSpecialTuning& Tuning : RoleTuning.SpecialMoves)
	{
		if (Tuning.State == State)
		{
			return Tuning;
		}
	}
	return BHMakeDefaultSpecialTuning(State);
}

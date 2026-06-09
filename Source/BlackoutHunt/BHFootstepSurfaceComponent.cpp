// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHFootstepSurfaceComponent.h"

UBHFootstepSurfaceComponent::UBHFootstepSurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	FootstepSurface = EBHFootstepSurface::Default;
}

EBHFootstepSurface UBHFootstepSurfaceComponent::GetFootstepSurface() const
{
	return FootstepSurface;
}

void UBHFootstepSurfaceComponent::SetFootstepSurface(EBHFootstepSurface NewSurface)
{
	FootstepSurface = NewSurface;
}

EBHFootstepSurface UBHFootstepSurfaceComponent::SurfaceForBlockMaterial(EBHBlockMaterial Material)
{
	switch (Material)
	{
	case EBHBlockMaterial::Concrete:
	case EBHBlockMaterial::Plaster:
		return EBHFootstepSurface::Concrete;
	case EBHBlockMaterial::RustedMetal:
	case EBHBlockMaterial::DiamondPlate:
	case EBHBlockMaterial::PaintedMetal:
	case EBHBlockMaterial::WarningSign:
		return EBHFootstepSurface::Metal;
	case EBHBlockMaterial::Tiles:
		return EBHFootstepSurface::Tile;
	case EBHBlockMaterial::Wood:
	case EBHBlockMaterial::Carpet:
	case EBHBlockMaterial::Leather:
		return EBHFootstepSurface::Soft;
	case EBHBlockMaterial::Marble:
		return EBHFootstepSurface::Tile;   // hard polished stone reads like tile
	default:
		return EBHFootstepSurface::Default;
	}
}

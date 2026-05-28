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
	default:
		return EBHFootstepSurface::Default;
	}
}

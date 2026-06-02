// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHFootstepSurfaceVolume.h"
#include "BHFootstepSurfaceComponent.h"
#include "Components/BoxComponent.h"

ABHFootstepSurfaceVolume::ABHFootstepSurfaceVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorHiddenInGame(true);

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("SurfaceVolume"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(200.0f, 200.0f, 80.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(false);

	SurfaceComponent = CreateDefaultSubobject<UBHFootstepSurfaceComponent>(TEXT("FootstepSurface"));
}

void ABHFootstepSurfaceVolume::ConfigureSurfaceVolume(EBHFootstepSurface Surface, const FVector& Extent)
{
	if (SurfaceComponent)
	{
		SurfaceComponent->SetFootstepSurface(Surface);
	}
	if (Box)
	{
		Box->SetBoxExtent(FVector(FMath::Max(1.0f, Extent.X), FMath::Max(1.0f, Extent.Y), FMath::Max(1.0f, Extent.Z)));
	}
}

EBHFootstepSurface ABHFootstepSurfaceVolume::GetFootstepSurface() const
{
	return SurfaceComponent ? SurfaceComponent->GetFootstepSurface() : EBHFootstepSurface::Default;
}

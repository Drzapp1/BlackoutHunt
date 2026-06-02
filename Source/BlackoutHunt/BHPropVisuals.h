// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace BHPropVisuals
{
	UStaticMesh* CubeMesh();
	UStaticMesh* CylinderMesh();
	UStaticMesh* SphereMesh();

	UMaterialInterface* BasicMaterial();
	UMaterialInterface* PaintedMetalMaterial();
	UMaterialInterface* RustedMetalMaterial();
	UMaterialInterface* DiamondPlateMaterial();
	UMaterialInterface* WarningSignMaterial();
	UMaterialInterface* ReadableTextMaterial();

	void ConfigurePart(
		UStaticMeshComponent* Component,
		UStaticMesh* MeshAsset,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FRotator& RelativeRotation,
		const FVector& RelativeScale,
		bool bCollisionEnabled = false);

	void TintPart(UStaticMeshComponent* Component, const FLinearColor& Color, float EmissiveStrength = 0.0f);
	void SetPartVisible(UStaticMeshComponent* Component, bool bVisible);

	void ConfigureReadableText(
		UTextRenderComponent* Component,
		const FVector& RelativeLocation,
		const FRotator& RelativeRotation,
		float WorldSize,
		const FColor& Color);
}

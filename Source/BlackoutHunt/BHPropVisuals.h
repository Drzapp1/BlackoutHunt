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

	// CC0 ambientCG comfort/games materials (imported via Tools/ImportTrainComfortMaterials.py). Each falls
	// back to BasicMaterial() if the asset is missing, so callers are always safe.
	UMaterialInterface* WoodMaterial();
	UMaterialInterface* CarpetMaterial();
	UMaterialInterface* FeltMaterial();
	UMaterialInterface* LeatherMaterial();
	UMaterialInterface* MarbleMaterial();

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

	// Word-wrap Text so no line exceeds MaxCharsPerLine characters. Existing '\n' breaks are preserved and
	// each segment is wrapped independently on word boundaries (a single word longer than the limit is kept
	// intact on its own line). UTextRenderComponent does not auto-wrap, so callers wrap before SetText to
	// keep panel text from overrunning a narrow module face.
	FString WrapTextToWidth(const FString& Text, int32 MaxCharsPerLine);
}

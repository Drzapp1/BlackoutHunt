// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BHCreateDiagramMaterialCommandlet.generated.h"

// Editor commandlet that creates the unlit emissive material the train bonus-question terminal uses
// to display a diagram render target: M_BH_DiagramRT, with a Texture2D parameter named "RT" wired to
// Emissive Color. Runs without -AllowCommandletRendering (it only authors and compiles an asset, it
// does not render a scene).
//
//   BlackoutHuntEditor-Cmd.exe BlackoutHunt.uproject -run=BHCreateDiagramMaterial
UCLASS()
class BLACKOUTHUNT_API UBHCreateDiagramMaterialCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBHCreateDiagramMaterialCommandlet(const FObjectInitializer& ObjectInitializer);

	virtual int32 Main(const FString& Params) override;
};

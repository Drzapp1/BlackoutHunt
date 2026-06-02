// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BHExportLevelCommandlet.generated.h"

// Editor commandlet that seeds an authored level .umap from the procedural generator. It creates a blank
// map, runs ABHGameMode::BuildLevelForExport for the requested level (Facility/Substation/Foggrounds),
// drops an ABHLevelMarker, and saves /Game/BlackoutHunt/Maps/<Level>. The result preserves the tuned
// blockout coordinates so artists replace blocks with meshes in place instead of re-laying the level.
//
//   BlackoutHuntEditor-Cmd.exe BlackoutHunt.uproject -run=BHExportLevel -Level=Facility
//
// See Docs/FACILITY_VERTICAL_SLICE.md ("Authored Map Pipeline").
UCLASS()
class BLACKOUTHUNT_API UBHExportLevelCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBHExportLevelCommandlet(const FObjectInitializer& ObjectInitializer);

	virtual int32 Main(const FString& Params) override;
};

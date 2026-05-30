#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BHRenderDiagramsCommandlet.generated.h"

// Editor commandlet that renders every revision-diagram type (with representative data and shape
// variants) to PNG files using the shared FBHDiagramRenderer. It is the diagram art-export pipeline
// and a visual-QA tool: the exported PNGs show exactly what the in-game HUD draws, so the diagrams
// can be eyeballed without launching a session.
//
//   BlackoutHuntEditor-Cmd.exe BlackoutHunt.uproject -run=BHRenderDiagrams -AllowCommandletRendering -unattended
//
// Optional args: -Out=<dir> (default <Saved>/DiagramShots), -W=<px> -H=<px> (per-diagram size),
//                -Light (render the clean textbook palette instead of the dark scanner palette).
UCLASS()
class BLACKOUTHUNT_API UBHRenderDiagramsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBHRenderDiagramsCommandlet(const FObjectInitializer& ObjectInitializer);

	virtual int32 Main(const FString& Params) override;
};

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"

class UCanvas;
class UFont;
class UTexture2D;

// Canvas-agnostic renderer for revision-question diagrams.
//
// The diagram drawing used to live inline in ABHHUD::DrawRevisionDiagram, which tied it to
// the player HUD's UCanvas + helper methods. Three surfaces now need the SAME picture:
//   1. the player HUD (objective-station question panel),
//   2. the train bonus-question terminal (drawn into a UCanvasRenderTarget2D),
//   3. the offline diagram->PNG generator commandlet.
// So the logic is extracted here as a pure static renderer that draws onto any UCanvas* via
// the FCanvas item API. It depends only on the diagram data + a draw context (scale, palette,
// font); it never reads the question's answer, keeping diagrams answer-safe by construction.
//
// All internal offsets/fonts/line-thicknesses are multiplied by Context.Scale and horizontal
// absolute offsets are kept width-relative, so a diagram renders correctly at any HUD scale
// and any panel width (the old inline version broke at non-1.0 HudScale and overflowed narrow
// panels for the EnergyChain layout).
struct FBHDiagramDrawContext
{
	// Unifies HudScale (player HUD) and render-target sizing (terminal / PNG bake). 1.0 keeps the
	// historical pixel metrics.
	float Scale = 1.0f;

	// Semantic colours, normally sourced from the HUD's resolved ActivePalette so colorblind /
	// high-contrast modes flow through to diagrams. Defaults reproduce the historical teal/amber.
	FLinearColor Accent = FLinearColor(0.48f, 0.92f, 0.86f, 1.0f);
	FLinearColor Warm = FLinearColor(0.95f, 0.62f, 0.28f, 1.0f);
	FLinearColor Good = FLinearColor(0.52f, 0.90f, 0.54f, 1.0f);
	FLinearColor Bad = FLinearColor(0.96f, 0.24f, 0.16f, 0.95f);
	FLinearColor TextMain = FLinearColor(0.88f, 0.95f, 0.92f, 1.0f);
	FLinearColor TextDim = FLinearColor(0.70f, 0.78f, 0.82f, 0.90f);
	FLinearColor Axis = FLinearColor(0.50f, 0.56f, 0.58f, 1.0f);
	FLinearColor Grid = FLinearColor(0.42f, 0.58f, 0.58f, 0.055f);
	// Panel background fill. Default is the dark "scanner" look; the PNG-bake commandlet overrides
	// it with a light fill for a clean textbook style.
	FLinearColor Background = FLinearColor(0.025f, 0.032f, 0.036f, 0.88f);

	// Font for diagram labels. Null falls back to GEngine->GetSmallFont().
	const UFont* Font = nullptr;

	// Drives the Wave animation. Pass the world time on the HUD; 0 yields a static wave (terminal /
	// PNG bake).
	float TimeSeconds = 0.0f;

	// Background panel + grid + corner brackets. Off for clean PNG bakes on a transparent target.
	bool bDrawChrome = true;

	// The subtopic + "Key idea" captions overlaid on the diagram.
	bool bDrawCaptions = true;

	// Editorial CONCEPT captions baked into each diagram (e.g. "gradient = velocity", "area =
	// distance", "A series | V parallel", "filament: R rises") and the "Key idea: <formula>" caption.
	// These help on apply/calculate questions but REVEAL the answer on identify/recall questions
	// ("what does the gradient represent?"), so callers set this false for those question types.
	// Structural labels (axes, data givens, EM bands, poles) and the subtopic caption are unaffected.
	bool bShowConceptCaptions = true;

	// Master switch (CVar bh.Diagrams.Enhanced). When false, per-question data labels and
	// shape variants are suppressed and the generic schematic is drawn -- a runtime fallback that
	// makes the data-driven behaviour reversible without a rebuild.
	bool bEnhanced = true;
};

class BLACKOUTHUNT_API FBHDiagramRenderer
{
public:
	// Draw a diagram into the rect (X, Y, W, H) on Canvas. If IllustratedImage is non-null it is
	// letterboxed into the rect and the procedural schematic is skipped; otherwise the procedural
	// schematic for Type is drawn from P. Subtopic / Formula are optional captions.
	static void Draw(
		UCanvas* Canvas,
		EBHDiagramType Type,
		const FBHDiagramParams& P,
		const FString& Subtopic,
		const FString& Formula,
		UTexture2D* IllustratedImage,
		float X, float Y, float W, float H,
		const FBHDiagramDrawContext& Context);

	// Preferred unscaled height of the diagram band for a given type. Callers multiply by their
	// scale. Replaces the old fixed 118px so complex types (graphs, circuits) get more room and
	// simple ones (spectrum) less.
	static float BandHeightFor(EBHDiagramType Type);

	// Representative sample data for a diagram type, for previews / QA (the bh.Diagrams.Preview
	// console command and the PNG-export commandlet). Variant selects a shape variant where the
	// type supports one. Fills OutParams with answer-safe placeholder givens and OutName with a
	// short human-readable label.
	static void SampleFor(EBHDiagramType Type, int32 Variant, FBHDiagramParams& OutParams, FString& OutName);

	// Reads the bh.Diagrams.Enhanced console var. When false, callers should build a context with
	// bEnhanced = false so data-driven labels / shape variants are suppressed (generic schematic) --
	// a runtime fallback that makes the data-driven behaviour reversible without a rebuild.
	static bool IsEnhanced();
};

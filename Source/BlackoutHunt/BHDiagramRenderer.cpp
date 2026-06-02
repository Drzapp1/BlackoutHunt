// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHDiagramRenderer.h"

#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "GlobalRenderResources.h" // GWhiteTexture for filled-triangle canvas items

// Master switch for the data-driven / question-accurate diagram behaviour. 1 (default) draws the
// enhanced diagrams; 0 falls back to the generic schematic without per-question labels or shape
// variants, so the whole feature is reversible at runtime without a rebuild.
static TAutoConsoleVariable<int32> CVarBHDiagramsEnhanced(
	TEXT("bh.Diagrams.Enhanced"),
	1,
	TEXT("Revision diagrams: 1 = data-driven question-accurate rendering, 0 = generic schematic fallback."),
	ECVF_Default);

bool FBHDiagramRenderer::IsEnhanced()
{
	return CVarBHDiagramsEnhanced.GetValueOnAnyThread() != 0;
}

void FBHDiagramRenderer::SampleFor(EBHDiagramType Type, int32 Variant, FBHDiagramParams& P, FString& OutName)
{
	P = FBHDiagramParams();
	switch (Type)
	{
	case EBHDiagramType::MotionGraph:
		P.ShapeVariant = Variant > 0 ? Variant : 2; P.XAxis = TEXT("t / s"); P.YAxis = TEXT("d / m"); OutName = TEXT("Motion graph"); break;
	case EBHDiagramType::VelocityGraph:
		P.ShapeVariant = Variant > 0 ? Variant : 2; P.XAxis = TEXT("t / s"); P.YAxis = TEXT("v / m/s"); OutName = TEXT("Velocity graph"); break;
	case EBHDiagramType::ForceArrows:
		P.ValueA = 30.0f; P.ValueB = 20.0f; P.LabelA = TEXT("30 N"); P.LabelB = TEXT("20 N"); P.ShapeVariant = Variant; if (Variant == 1) { P.LabelC = TEXT("weight"); P.LabelD = TEXT("normal"); } OutName = TEXT("Force arrows"); break;
	case EBHDiagramType::SpringGraph:
		P.XAxis = TEXT("x / m"); P.YAxis = TEXT("F / N"); OutName = TEXT("Spring graph"); break;
	case EBHDiagramType::MomentBeam:
		P.LabelA = TEXT("0.5 m"); P.LabelC = TEXT("1.5 m"); P.LabelB = TEXT("12 N"); P.LabelD = TEXT("4 N"); OutName = TEXT("Moment beam"); break;
	case EBHDiagramType::Circuit:
		P.ShapeVariant = Variant > 0 ? Variant : 3; P.LabelA = TEXT("2 ohm"); P.LabelC = TEXT("8 ohm"); P.LabelB = TEXT("6 V"); OutName = TEXT("Circuit"); break;
	case EBHDiagramType::IVGraph:
		P.ShapeVariant = Variant > 0 ? Variant : 1; P.XAxis = TEXT("V / V"); P.YAxis = TEXT("I / A"); OutName = TEXT("I-V graph"); break;
	case EBHDiagramType::StaticCharge:
		OutName = TEXT("Static charge"); break;
	case EBHDiagramType::Wave:
		P.ValueA = 0.30f; P.ValueB = 3.0f; P.LabelA = TEXT("amplitude"); P.LabelB = TEXT("wavelength"); OutName = TEXT("Wave"); break;
	case EBHDiagramType::EMSpectrum:
		P.ShapeVariant = Variant > 0 ? Variant : 4; OutName = TEXT("EM spectrum"); break;
	case EBHDiagramType::RayDiagram:
		P.AngleOrShape = Variant > 0 ? static_cast<float>(Variant) : 40.0f; P.LabelA = TEXT("normal"); OutName = TEXT("Ray diagram"); break;
	case EBHDiagramType::Sankey:
		P.LabelA = TEXT("in 500 J"); P.LabelB = TEXT("useful"); P.LabelC = TEXT("wasted"); OutName = TEXT("Sankey"); break;
	case EBHDiagramType::EnergyChain:
		P.LabelA = TEXT("chemical"); P.LabelB = TEXT("electrical"); P.LabelC = TEXT("light+heat"); OutName = TEXT("Energy chain"); break;
	case EBHDiagramType::Lens:
		P.LabelA = TEXT("object > 2F"); OutName = TEXT("Lens"); break;
	case EBHDiagramType::Transformer:
		P.ValueA = 200.0f; P.ValueB = 50.0f; P.LabelA = TEXT("Np 200"); P.LabelB = TEXT("Ns 50"); OutName = TEXT("Transformer"); break;
	case EBHDiagramType::MagneticField:
		OutName = TEXT("Magnetic field"); break;
	case EBHDiagramType::InclinedPlane:
		P.AngleOrShape = 30.0f; P.LabelA = TEXT("30 deg"); OutName = TEXT("Inclined plane"); break;
	case EBHDiagramType::PressureColumn:
		P.LabelA = TEXT("h"); OutName = TEXT("Pressure column"); break;
	case EBHDiagramType::EnergyBars:
		P.ValueA = 0.9f; P.ValueB = 0.1f; P.ValueC = 0.1f; P.ValueD = 0.9f; P.LabelA = TEXT("GPE"); P.LabelB = TEXT("KE"); P.LabelC = TEXT("GPE"); P.LabelD = TEXT("KE"); OutName = TEXT("Energy bars"); break;
	case EBHDiagramType::ParticleModel:
		OutName = TEXT("Particle model"); break;
	default:
		OutName = TEXT("None"); break;
	}
}

// All drawing goes through these tiny UCanvas primitives so the renderer works on the player
// HUD's canvas, a UCanvasRenderTarget2D canvas (train terminal), or a commandlet's bake target
// without depending on ABHHUD. They mirror ABHHUD's own helpers (drop-shadowed text, tile rects,
// thick lines) so the output matches the historical HUD look.
namespace
{
	void RRect(UCanvas* C, const FLinearColor& Color, float X, float Y, float W, float H)
	{
		if (!C || W <= 0.0f || H <= 0.0f)
		{
			return;
		}
		FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(Tile);
	}

	void RLine(UCanvas* C, float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness)
	{
		if (!C)
		{
			return;
		}
		FCanvasLineItem Line(FVector2D(X0, Y0), FVector2D(X1, Y1));
		Line.SetColor(Color);
		Line.LineThickness = FMath::Max(1.0f, Thickness);
		C->DrawItem(Line);
	}

	void RText(UCanvas* C, const UFont* Font, const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
	{
		if (!C || Text.IsEmpty() || !Font)
		{
			return;
		}
		// Drop shadow then text, exactly like ABHHUD::DrawHudText.
		C->SetDrawColor(FLinearColor(0.0f, 0.0f, 0.0f, Color.A * 0.72f).ToFColor(true));
		C->DrawText(Font, Text, X + 1.0f, Y + 1.0f, Scale, Scale);
		C->SetDrawColor(Color.ToFColor(true));
		C->DrawText(Font, Text, X, Y, Scale, Scale);
	}

	void RTextRight(UCanvas* C, const UFont* Font, const FString& Text, float RightX, float Y, const FLinearColor& Color, float Scale)
	{
		if (!C || Text.IsEmpty() || !Font)
		{
			return;
		}
		float TextW = 0.0f;
		float TextH = 0.0f;
		C->TextSize(Font, Text, TextW, TextH, Scale, Scale);
		RText(C, Font, Text, RightX - TextW, Y, Color, Scale);
	}

	void RTextCentered(UCanvas* C, const UFont* Font, const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
	{
		if (!C || Text.IsEmpty() || !Font)
		{
			return;
		}
		float TextW = 0.0f;
		float TextH = 0.0f;
		C->TextSize(Font, Text, TextW, TextH, Scale, Scale);
		RText(C, Font, Text, CenterX - TextW * 0.5f, Y, Color, Scale);
	}

	// Solid filled triangle. Used for legible arrowheads that stay visible when the diagram is scaled
	// down onto the train terminal's small render target (line-only heads collapsed to sub-pixel dots).
	void RFilledTri(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& Cc, const FLinearColor& Color)
	{
		if (!C)
		{
			return;
		}
		FCanvasTriangleItem Tri(A, B, Cc, GWhiteTexture);
		Tri.SetColor(Color);
		Tri.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(Tri);
	}

	// Left-anchored text kept inside [MinX, MaxX]: shifts left if it would overflow the right edge and
	// never starts before the left edge, so labels stay on-panel at narrow widths / non-1.0 scales.
	void RTextBounded(UCanvas* C, const UFont* Font, const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, float MinX, float MaxX)
	{
		if (!C || Text.IsEmpty() || !Font)
		{
			return;
		}
		float TextW = 0.0f;
		float TextH = 0.0f;
		C->TextSize(Font, Text, TextW, TextH, Scale, Scale);
		float DrawX = X;
		if (DrawX + TextW > MaxX) { DrawX = MaxX - TextW; }
		if (DrawX < MinX) { DrawX = MinX; }
		RText(C, Font, Text, DrawX, Y, Color, Scale);
	}

	// Line with a filled triangular arrowhead at (X1,Y1). Head spreads ~152 degrees off the shaft and
	// has a minimum pixel size so it stays legible on small surfaces; the shaft stops just short of the
	// tip so it does not poke through the fill. Used for force vectors, energy flows and lever loads.
	void RArrow(UCanvas* C, float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness, float HeadSize)
	{
		const float Ang = FMath::Atan2(Y1 - Y0, X1 - X0);
		const float Spread = FMath::DegreesToRadians(152.0f);
		const float Head = FMath::Max(HeadSize, 4.0f);
		const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
		const FVector2D Tip(X1, Y1);
		const FVector2D BackL(X1 + FMath::Cos(Ang + Spread) * Head, Y1 + FMath::Sin(Ang + Spread) * Head);
		const FVector2D BackR(X1 + FMath::Cos(Ang - Spread) * Head, Y1 + FMath::Sin(Ang - Spread) * Head);
		const FVector2D ShaftEnd = Tip - Dir * Head * 0.6f;
		RLine(C, X0, Y0, ShaftEnd.X, ShaftEnd.Y, Color, Thickness);
		RFilledTri(C, Tip, BackL, BackR, Color);
	}

	void RCircle(UCanvas* C, float CX, float CY, float Radius, const FLinearColor& Color, float Thickness, int32 Segments = 16)
	{
		if (!C || Radius <= 0.0f)
		{
			return;
		}
		const int32 N = FMath::Clamp(Segments, 6, 48);
		FVector2D Prev(CX + Radius, CY);
		for (int32 i = 1; i <= N; ++i)
		{
			const float A = (static_cast<float>(i) / N) * 2.0f * PI;
			const FVector2D Cur(CX + FMath::Cos(A) * Radius, CY + FMath::Sin(A) * Radius);
			RLine(C, Prev.X, Prev.Y, Cur.X, Cur.Y, Color, Thickness);
			Prev = Cur;
		}
	}

	void RCornerBrackets(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Color, float Length, float Thickness)
	{
		if (W <= 1.0f || H <= 1.0f)
		{
			return;
		}
		const float L = FMath::Clamp(Length, 4.0f, FMath::Min(W, H) * 0.45f);
		RLine(C, X, Y, X + L, Y, Color, Thickness);
		RLine(C, X, Y, X, Y + L, Color, Thickness);
		RLine(C, X + W, Y, X + W - L, Y, Color, Thickness);
		RLine(C, X + W, Y, X + W, Y + L, Color, Thickness);
		RLine(C, X, Y + H, X + L, Y + H, Color, Thickness);
		RLine(C, X, Y + H, X, Y + H - L, Color, Thickness);
		RLine(C, X + W, Y + H, X + W - L, Y + H, Color, Thickness);
		RLine(C, X + W, Y + H, X + W, Y + H - L, Color, Thickness);
	}
}

float FBHDiagramRenderer::BandHeightFor(EBHDiagramType Type)
{
	// Graphs and circuits need vertical room for axes / loops; spectra and chains are wide+short.
	switch (Type)
	{
	case EBHDiagramType::MotionGraph:
	case EBHDiagramType::VelocityGraph:
	case EBHDiagramType::SpringGraph:
	case EBHDiagramType::IVGraph:
		return 132.0f;
	case EBHDiagramType::Circuit:
		return 128.0f;
	case EBHDiagramType::RayDiagram:
	case EBHDiagramType::Lens:
	case EBHDiagramType::Transformer:
	case EBHDiagramType::InclinedPlane:
	case EBHDiagramType::PressureColumn:
	case EBHDiagramType::EnergyBars:
		return 124.0f;
	case EBHDiagramType::MagneticField:
	case EBHDiagramType::ParticleModel:
		return 110.0f;
	case EBHDiagramType::MomentBeam:
		return 114.0f;
	case EBHDiagramType::Wave:
		return 118.0f;
	case EBHDiagramType::EnergyChain:
		return 116.0f;
	case EBHDiagramType::ForceArrows:
	case EBHDiagramType::StaticCharge:
	case EBHDiagramType::Sankey:
		return 108.0f;
	case EBHDiagramType::EMSpectrum:
		return 100.0f;
	default:
		return 118.0f;
	}
}

void FBHDiagramRenderer::GetClickableRegions(
	EBHDiagramType Type,
	const FBHDiagramParams& P,
	float X, float Y, float W, float H,
	const FBHDiagramDrawContext& Context,
	TArray<FBHDiagramClickRegion>& OutRegions)
{
	OutRegions.Reset();
	(void)P; // Reserved for future per-question region layouts; current types key off geometry only.
	if (W <= 2.0f || H <= 2.0f)
	{
		return;
	}
	const float S = FMath::Max(0.1f, Context.Scale);

	switch (Type)
	{
	case EBHDiagramType::EMSpectrum:
	{
		// Same layout as Draw's EMSpectrum case: seven equal bands across [Left, Right] at MidY. The
		// labels are full band names so the HUD can substring-match them against choice text (the
		// drawn picture only has room for the R/M/IR/... abbreviations).
		static const TCHAR* BandNames[] = {
			TEXT("radio"), TEXT("microwave"), TEXT("infrared"), TEXT("visible"),
			TEXT("ultraviolet"), TEXT("x-ray"), TEXT("gamma")
		};
		const float MidY = Y + H * 0.52f;
		const float Left = X + 26.0f * S;
		const float Right = X + W - 26.0f * S;
		const float SegmentW = (Right - Left) / 7.0f;
		if (SegmentW <= 1.0f)
		{
			return;
		}
		const float SegInner = SegmentW - 3.0f * S;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float SX = Left + SegmentW * Index;
			FBHDiagramClickRegion Region;
			// Slightly taller than the 32*S painted band so the band is comfortable to click.
			Region.Rect = FBox2D(FVector2D(SX, MidY - 20.0f * S), FVector2D(SX + SegInner, MidY + 20.0f * S));
			Region.Label = BandNames[Index];
			OutRegions.Add(MoveTemp(Region));
		}
		break;
	}
	default:
		break;
	}
}

void FBHDiagramRenderer::Draw(
	UCanvas* Canvas,
	EBHDiagramType Type,
	const FBHDiagramParams& P,
	const FString& Subtopic,
	const FString& Formula,
	UTexture2D* IllustratedImage,
	float X, float Y, float W, float H,
	const FBHDiagramDrawContext& Ctx)
{
	if (!Canvas || W <= 2.0f || H <= 2.0f)
	{
		return;
	}

	const UFont* Font = Ctx.Font ? Ctx.Font : (GEngine ? GEngine->GetSmallFont() : nullptr);
	if (!Font)
	{
		return;
	}

	const float S = FMath::Max(0.1f, Ctx.Scale);

	// Optional illustrated image diagram: letterbox it and skip the procedural schematic.
	if (IllustratedImage && IllustratedImage->GetResource())
	{
		RRect(Canvas, FLinearColor(0.02f, 0.025f, 0.028f, 0.92f), X, Y, W, H);
		const float TexW = FMath::Max(1.0f, static_cast<float>(IllustratedImage->GetSizeX()));
		const float TexH = FMath::Max(1.0f, static_cast<float>(IllustratedImage->GetSizeY()));
		const float FitScale = FMath::Max(0.0f, FMath::Min((W - 12.0f * S) / TexW, (H - 12.0f * S) / TexH));
		const float DrawW = TexW * FitScale;
		const float DrawH = TexH * FitScale;
		const float DrawX = X + (W - DrawW) * 0.5f;
		const float DrawY = Y + (H - DrawH) * 0.5f;
		FCanvasTileItem Img(FVector2D(DrawX, DrawY), IllustratedImage->GetResource(), FVector2D(DrawW, DrawH), FLinearColor::White);
		Img.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Img);

		if (Ctx.bDrawCaptions && !Subtopic.IsEmpty())
		{
			RText(Canvas, Font, Subtopic.ToUpper(), X + 10.0f * S, Y + 4.0f * S, FLinearColor(0.66f, 0.82f, 0.94f, 0.92f), 0.62f * S);
		}
		if (Ctx.bDrawCaptions && Ctx.bShowConceptCaptions && !Formula.IsEmpty())
		{
			RTextRight(Canvas, Font, FString::Printf(TEXT("Key idea: %s"), *Formula), X + W - 10.0f * S, Y + H - 18.0f * S, FLinearColor(0.95f, 0.84f, 0.45f, 1.0f), 0.66f * S);
		}
		return;
	}

	// ---- Chrome: background, frame lines, faint grid, corner brackets. ----
	if (Ctx.bDrawChrome)
	{
		RRect(Canvas, Ctx.Background, X, Y, W, H);
		RRect(Canvas, FLinearColor(0.18f, 0.28f, 0.30f, 0.82f), X, Y, W, 1.0f);
		RRect(Canvas, FLinearColor(0.18f, 0.28f, 0.30f, 0.58f), X, Y + H - 1.0f, W, 1.0f);
		for (float GridX = X + 42.0f * S; GridX < X + W - 12.0f * S; GridX += 48.0f * S)
		{
			RRect(Canvas, Ctx.Grid, GridX, Y + 6.0f * S, 1.0f, H - 12.0f * S);
		}
		for (float GridY = Y + 30.0f * S; GridY < Y + H - 8.0f * S; GridY += 28.0f * S)
		{
			RRect(Canvas, Ctx.Grid, X + 8.0f * S, GridY, W - 16.0f * S, 1.0f);
		}
		RCornerBrackets(Canvas, X + 6.0f * S, Y + 6.0f * S, W - 12.0f * S, H - 12.0f * S, FLinearColor(Ctx.Accent.R, Ctx.Accent.G, Ctx.Accent.B, 0.28f), 10.0f * S, 1.0f * S);
	}

	const FLinearColor Line = Ctx.Accent;
	const FLinearColor Warm = Ctx.Warm;
	const FLinearColor Main = Ctx.TextMain;
	const FLinearColor AxisCol = Ctx.Axis;
	const float MidY = Y + H * 0.52f;
	const float Left = X + 26.0f * S;
	const float Right = X + W - 26.0f * S;
	const float Top = Y + 16.0f * S;
	const float Bottom = Y + H - 18.0f * S;
	const float Now = Ctx.TimeSeconds;
	const bool bData = Ctx.bEnhanced && P.HasValues();

	// Editorial concept captions (e.g. "gradient = velocity") reveal the answer on identify/recall
	// questions, so they are gated by bShowConceptCaptions. Data labels (givens) and structural
	// marks are always drawn; only these baked concept statements are suppressed.
	auto CCap = [&](const FString& Text, float CapX, float CapY, const FLinearColor& Col, float Sc)
	{
		if (Ctx.bShowConceptCaptions) { RText(Canvas, Font, Text, CapX, CapY, Col, Sc); }
	};
	auto CCapC = [&](const FString& Text, float CapCX, float CapY, const FLinearColor& Col, float Sc)
	{
		if (Ctx.bShowConceptCaptions) { RTextCentered(Canvas, Font, Text, CapCX, CapY, Col, Sc); }
	};

	switch (Type)
	{
	case EBHDiagramType::MotionGraph:
	{
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		// Displacement-time shape: 0 generic increasing line, 1 constant velocity (straight),
		// 2 accelerating (concave up), 3 decelerating (concave down), 4 accel-then-constant.
		const int32 Variant = Ctx.bEnhanced ? P.ShapeVariant : 0;
		if (Variant == 0)
		{
			RLine(Canvas, Left, Bottom - 8.0f * S, X + W * 0.42f, Y + H * 0.38f, Line, 3.0f * S);
			RLine(Canvas, X + W * 0.42f, Y + H * 0.38f, Right, Y + H * 0.26f, Line, 3.0f * S);
		}
		else
		{
			const int32 Steps = 40;
			const float GraphW = Right - Left;
			const float GraphH = (Bottom - Top) * 0.90f;
			FVector2D Prev(Left, Bottom);
			for (int32 Step = 1; Step <= Steps; ++Step)
			{
				const float T = static_cast<float>(Step) / Steps;
				float F;
				switch (Variant)
				{
				case 2: F = T * T; break;
				case 3: F = 2.0f * T - T * T; break;
				case 4: F = T < 0.6f ? FMath::Square(T / 0.6f) * 0.5f : 0.5f + (T - 0.6f) / 0.4f * 0.5f; break;
				default: F = T; break;
				}
				const float PX = Left + GraphW * T;
				const float PY = Bottom - GraphH * FMath::Clamp(F, 0.0f, 1.0f);
				RLine(Canvas, Prev.X, Prev.Y, PX, PY, Line, 3.0f * S);
				Prev = FVector2D(PX, PY);
			}
		}
		CCap(TEXT("gradient = velocity"), Left + 8.0f * S, Top + 4.0f * S, Warm, 0.72f * S);
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		break;
	}
	case EBHDiagramType::VelocityGraph:
	{
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		// Velocity-time shape: 0 generic (flat then rising), 1 constant, 2 accelerating,
		// 3 decelerating, 4 accel-then-constant. The shaded area under the line shows distance.
		const int32 Variant = Ctx.bEnhanced ? P.ShapeVariant : 0;
		if (Variant == 0)
		{
			RRect(Canvas, FLinearColor(0.30f, 0.60f, 0.78f, 0.24f), Left + 10.0f * S, MidY, W * 0.38f, Bottom - MidY);
			RLine(Canvas, Left + 10.0f * S, MidY, X + W * 0.52f, MidY, Line, 3.0f * S);
			RLine(Canvas, X + W * 0.52f, MidY, Right, Top + 8.0f * S, Line, 3.0f * S);
		}
		else
		{
			const int32 Steps = 40;
			const float GraphW = Right - Left;
			const float GraphH = (Bottom - Top) * 0.86f;
			const float ColW = GraphW / Steps + 1.0f;
			FVector2D Prev(Left, Bottom);
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float T = static_cast<float>(Step) / Steps;
				float VF;
				switch (Variant)
				{
				case 1: VF = 0.60f; break;
				case 3: VF = 0.90f - 0.70f * T; break;
				case 4: VF = T < 0.5f ? T * 1.2f : 0.60f; break;
				default: VF = T; break;
				}
				VF = FMath::Clamp(VF, 0.0f, 1.0f);
				const float PX = Left + GraphW * T;
				const float PY = Bottom - GraphH * VF;
				RRect(Canvas, FLinearColor(0.30f, 0.60f, 0.78f, 0.22f), PX, PY, ColW, Bottom - PY);
				if (Step > 0) { RLine(Canvas, Prev.X, Prev.Y, PX, PY, Line, 3.0f * S); }
				Prev = FVector2D(PX, PY);
			}
		}
		CCap(TEXT("area = distance"), Left + 18.0f * S, Bottom - 34.0f * S, Warm, 0.72f * S);
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		if (bData && !P.LabelA.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelA, Left + 18.0f * S, Top + 6.0f * S, Main, 0.60f * S, X + 6.0f * S, X + W - 6.0f * S); }
		break;
	}
	case EBHDiagramType::ForceArrows:
	{
		// Arrow lengths scale with the labelled force magnitudes (ValueA left / ValueB right) so the
		// picture is quantitative; falls back to fixed lengths when no magnitudes are authored.
		// ShapeVariant 1 adds a vertical weight/normal pair for free-body questions.
		const int32 Variant = Ctx.bEnhanced ? P.ShapeVariant : 0;
		const float CX = X + W * 0.50f;
		const float MagA = (Ctx.bEnhanced && P.ValueA > 0.0f) ? P.ValueA : 0.0f;
		const float MagB = (Ctx.bEnhanced && P.ValueB > 0.0f) ? P.ValueB : 0.0f;
		const float MaxMag = FMath::Max3(MagA, MagB, 1.0f);
		const float MaxLen = W * 0.30f;
		const float LenA = MagA > 0.0f ? FMath::Clamp(MagA / MaxMag, 0.30f, 1.0f) * MaxLen : W * 0.24f;
		const float LenB = MagB > 0.0f ? FMath::Clamp(MagB / MaxMag, 0.30f, 1.0f) * MaxLen : W * 0.24f;
		RArrow(Canvas, CX, MidY, CX - LenA, MidY, Warm, 4.0f * S, 9.0f * S);
		RArrow(Canvas, CX, MidY, CX + LenB, MidY, Line, 4.0f * S, 9.0f * S);
		if (Variant == 1)
		{
			RArrow(Canvas, CX, MidY, CX, MidY + (Bottom - MidY) * 0.82f, Warm, 3.0f * S, 8.0f * S);
			RArrow(Canvas, CX, MidY, CX, Top + 8.0f * S, Line, 3.0f * S, 8.0f * S);
		}
		CCap(Variant == 1 ? FString(TEXT("free-body forces")) : FString(TEXT("resultant force")), X + W * 0.39f, Top + 8.0f * S, Main, 0.78f * S);
		if (bData)
		{
			const float LblMinX = X + 6.0f * S;
			const float LblMaxX = X + W - 6.0f * S;
			if (!P.LabelA.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelA, X + W * 0.16f, MidY - 18.0f * S, Warm, 0.66f * S, LblMinX, LblMaxX); }
			if (!P.LabelB.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelB, X + W * 0.66f, MidY - 18.0f * S, Line, 0.66f * S, LblMinX, LblMaxX); }
			if (Variant == 1 && !P.LabelC.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelC, CX + 8.0f * S, Bottom - 14.0f * S, Warm, 0.62f * S, LblMinX, LblMaxX); }
			if (Variant == 1 && !P.LabelD.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelD, CX + 8.0f * S, Top + 16.0f * S, Line, 0.62f * S, LblMinX, LblMaxX); }
		}
		break;
	}
	case EBHDiagramType::SpringGraph:
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Right - 32.0f * S, Top + 18.0f * S, Line, 3.0f * S);
		CCap(TEXT("gradient = k"), Left + 18.0f * S, Top + 6.0f * S, Warm, 0.76f * S);
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		break;
	case EBHDiagramType::MomentBeam:
		RRect(Canvas, FLinearColor(0.55f, 0.50f, 0.38f, 1.0f), Left, MidY, Right - Left, 8.0f * S);
		RLine(Canvas, X + W * 0.48f, MidY + 8.0f * S, X + W * 0.48f - 14.0f * S, Bottom, Warm, 3.0f * S);
		RLine(Canvas, X + W * 0.48f, MidY + 8.0f * S, X + W * 0.48f + 14.0f * S, Bottom, Warm, 3.0f * S);
		RArrow(Canvas, X + W * 0.78f, MidY - 30.0f * S, X + W * 0.78f, MidY, Line, 4.0f * S, 8.0f * S);
		CCap(TEXT("moment = Fd"), Left + 14.0f * S, Top + 8.0f * S, Main, 0.78f * S);
		if (bData)
		{
			if (!P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + 14.0f * S, MidY - 18.0f * S, Warm, 0.64f * S); }
			if (!P.LabelC.IsEmpty()) { RTextRight(Canvas, Font, P.LabelC, Right - 6.0f * S, MidY - 18.0f * S, Line, 0.64f * S); }
			if (!P.LabelB.IsEmpty()) { RText(Canvas, Font, P.LabelB, Left + 14.0f * S, Bottom - 12.0f * S, Main, 0.60f * S); }
			if (!P.LabelD.IsEmpty()) { RTextRight(Canvas, Font, P.LabelD, Right - 6.0f * S, Bottom - 12.0f * S, Main, 0.60f * S); }
		}
		break;
	case EBHDiagramType::Circuit:
	{
		// ShapeVariant: 0 single resistor, 1 series-2, 2 parallel-2, 3 series-3.
		const int32 Variant = Ctx.bEnhanced ? P.ShapeVariant : 0;
		const float RailT = Top + 16.0f * S;
		const float RailB = Bottom - 10.0f * S;
		const float CX = X + W * 0.50f;
		const float BoxW = 30.0f * S;
		const float BoxH = 16.0f * S;
		// Outer loop.
		RLine(Canvas, Left, RailT, Right, RailT, Line, 2.0f * S);
		RLine(Canvas, Right, RailT, Right, RailB, Line, 2.0f * S);
		RLine(Canvas, Right, RailB, Left, RailB, Line, 2.0f * S);
		RLine(Canvas, Left, RailB, Left, RailT, Line, 2.0f * S);
		// Cell on the bottom rail: long thin plate (+) and short thick plate (-).
		RRect(Canvas, Line, CX - 6.0f * S, RailB - 9.0f * S, 2.0f * S, 18.0f * S);
		RRect(Canvas, Line, CX + 2.0f * S, RailB - 5.0f * S, 4.0f * S, 10.0f * S);
		if (Variant == 2)
		{
			const float B1 = Y + H * 0.40f;
			const float B2 = Y + H * 0.66f;
			RLine(Canvas, Left, B1, Right, B1, Line, 2.0f * S);
			RLine(Canvas, Left, B2, Right, B2, Line, 2.0f * S);
			RRect(Canvas, Warm, CX - BoxW * 0.5f, B1 - BoxH * 0.5f, BoxW, BoxH);
			RRect(Canvas, Warm, CX - BoxW * 0.5f, B2 - BoxH * 0.5f, BoxW, BoxH);
			if (bData)
			{
				if (!P.LabelA.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelA, CX + BoxW * 0.6f, B1 - 6.0f * S, Warm, 0.60f * S, X + 6.0f * S, X + W - 6.0f * S); }
				if (!P.LabelC.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelC, CX + BoxW * 0.6f, B2 - 6.0f * S, Warm, 0.60f * S, X + 6.0f * S, X + W - 6.0f * S); }
			}
		}
		else
		{
			const int32 NumR = Variant == 1 ? 2 : (Variant == 3 ? 3 : 1);
			for (int32 i = 0; i < NumR; ++i)
			{
				const float Frac = static_cast<float>(i + 1) / static_cast<float>(NumR + 1);
				RRect(Canvas, Warm, Left + (Right - Left) * Frac - BoxW * 0.5f, RailT - BoxH * 0.5f, BoxW, BoxH);
			}
			if (bData)
			{
				if (!P.LabelA.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelA, Left + (Right - Left) * (1.0f / (NumR + 1)) - 8.0f * S, RailT + 12.0f * S, Warm, 0.60f * S, X + 6.0f * S, X + W - 6.0f * S); }
				if (NumR >= 2 && !P.LabelC.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelC, Left + (Right - Left) * (2.0f / (NumR + 1)) - 8.0f * S, RailT + 12.0f * S, Warm, 0.60f * S, X + 6.0f * S, X + W - 6.0f * S); }
			}
		}
		const FString CircuitLabel = (Variant == 2) ? FString(TEXT("parallel: same p.d.")) : (Variant >= 1 ? FString(TEXT("series: same current")) : FString(TEXT("A series | V parallel")));
		CCapC(CircuitLabel, CX, RailB - 24.0f * S, Main, 0.66f * S);
		if (bData && !P.LabelB.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelB, Left + 14.0f * S, RailB - 24.0f * S, Line, 0.60f * S, X + 6.0f * S, X + W - 6.0f * S); }
		break;
	}
	case EBHDiagramType::IVGraph:
	{
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		// Curve shape per ShapeVariant: 0 ohmic (straight through origin), 1 filament lamp
		// (concave-down as the hot filament's resistance climbs), 2 diode (almost no current until
		// a threshold p.d., then a steep rise).
		const int32 Variant = Ctx.bEnhanced ? P.ShapeVariant : 0;
		const int32 Steps = 40;
		const float GraphW = Right - Left;
		const float GraphH = (Bottom - Top) * 0.92f;
		FVector2D Prev(Left, Bottom);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float T = static_cast<float>(Step) / Steps;
			float IFrac;
			switch (Variant)
			{
			case 1: IFrac = FMath::Pow(T, 0.55f); break;
			case 2: IFrac = T < 0.45f ? 0.02f : (T - 0.45f) / 0.55f; break;
			default: IFrac = T; break;
			}
			const float PX = Left + GraphW * T;
			const float PY = Bottom - GraphH * FMath::Clamp(IFrac, 0.0f, 1.0f);
			RLine(Canvas, Prev.X, Prev.Y, PX, PY, Line, 3.0f * S);
			Prev = FVector2D(PX, PY);
		}
		if (Ctx.bEnhanced && P.ValueC > 0.0f && P.ValueD > 0.0f)
		{
			const float PtX = Left + (Right - Left) * FMath::Clamp(P.ValueA / P.ValueC, 0.0f, 1.0f);
			const float PtY = Bottom - (Bottom - Top) * FMath::Clamp(P.ValueB / P.ValueD, 0.0f, 1.0f);
			RRect(Canvas, Warm, PtX - 3.0f * S, PtY - 3.0f * S, 6.0f * S, 6.0f * S);
		}
		if (Ctx.bEnhanced && !P.LabelA.IsEmpty())
		{
			RText(Canvas, Font, P.LabelA, Left + 16.0f * S, Top + 8.0f * S, Warm, 0.74f * S);
		}
		else
		{
			CCap(Variant == 1 ? FString(TEXT("filament: R rises")) : (Variant == 2 ? FString(TEXT("diode: one-way")) : FString(TEXT("straight: ohmic"))), Left + 16.0f * S, Top + 8.0f * S, Warm, 0.74f * S);
		}
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		break;
	}
	case EBHDiagramType::StaticCharge:
	{
		// Honour the authored rod labels. Default is the unlike-charge (attraction) figure, but when both
		// rods carry the SAME label (like charges, e.g. LabelA==LabelB=="+ + +") draw matching signs with a
		// diverging "repel" arrow so the diagram matches "like charges repel" prompts instead of
		// contradicting them with a fixed +/- attraction.
		const bool bLikeCharges = !P.LabelA.IsEmpty() && P.LabelA.Equals(P.LabelB);
		if (bLikeCharges)
		{
			RText(Canvas, Font, P.LabelA, Left + 24.0f * S, MidY - 8.0f * S, Warm, 1.0f * S);
			RTextRight(Canvas, Font, P.LabelB, Right - 8.0f * S, MidY - 8.0f * S, Warm, 1.0f * S);
			RArrow(Canvas, X + W * 0.46f, MidY, X + W * 0.30f, MidY, Main, 3.0f * S, 7.0f * S);
			RArrow(Canvas, X + W * 0.54f, MidY, X + W * 0.70f, MidY, Main, 3.0f * S, 7.0f * S);
			CCap(TEXT("like charges repel"), X + W * 0.34f, Top + 8.0f * S, Main, 0.74f * S);
		}
		else
		{
			RText(Canvas, Font, TEXT("+ + +"), Left + 24.0f * S, MidY - 8.0f * S, Warm, 1.0f * S);
			RTextRight(Canvas, Font, TEXT("- - -"), Right - 8.0f * S, MidY - 8.0f * S, Line, 1.0f * S);
			RArrow(Canvas, X + W * 0.42f, MidY, X + W * 0.58f, MidY, Main, 3.0f * S, 7.0f * S);
			CCap(TEXT("opposites attract"), X + W * 0.38f, Top + 8.0f * S, Main, 0.74f * S);
		}
		break;
	}
	case EBHDiagramType::Wave:
	{
		const int32 Segments = 48;
		const float AmpFrac = (Ctx.bEnhanced && P.ValueA > 0.0f) ? FMath::Clamp(P.ValueA, 0.05f, 0.45f) : 0.22f;
		const float Cycles = (Ctx.bEnhanced && P.ValueB > 0.0f) ? FMath::Clamp(P.ValueB, 1.0f, 6.0f) : 2.0f;
		FVector2D Prev(Left, MidY);
		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float T = static_cast<float>(Index) / Segments;
			const float PX = FMath::Lerp(Left, Right, T);
			const float PY = MidY + FMath::Sin(T * PI * 2.0f * Cycles + Now * 2.0f) * H * AmpFrac;
			RLine(Canvas, Prev.X, Prev.Y, PX, PY, Line, 2.5f * S);
			Prev = FVector2D(PX, PY);
		}
		if (bData)
		{
			RText(Canvas, Font, FString::Printf(TEXT("%s | %s"), *P.LabelA, *P.LabelB), Left + 14.0f * S, Top + 6.0f * S, Warm, 0.74f * S);
		}
		else
		{
			CCap(TEXT("amplitude | wavelength"), Left + 14.0f * S, Top + 6.0f * S, Warm, 0.74f * S);
		}
		break;
	}
	case EBHDiagramType::EMSpectrum:
	{
		// Seven bands, long wavelength (left) -> short (right), tinted toward the real spectrum so
		// the ordering reads at a glance instead of the old alternating blue/purple.
		const TCHAR* Labels[] = {TEXT("R"), TEXT("M"), TEXT("IR"), TEXT("VIS"), TEXT("UV"), TEXT("X"), TEXT("G")};
		const FLinearColor BandCols[] = {
			FLinearColor(0.62f, 0.16f, 0.16f, 1.0f), // radio - deep red
			FLinearColor(0.78f, 0.42f, 0.14f, 1.0f), // microwave - orange
			FLinearColor(0.85f, 0.62f, 0.18f, 1.0f), // infrared - amber
			FLinearColor(0.30f, 0.74f, 0.42f, 1.0f), // visible - green (stand-in for rainbow)
			FLinearColor(0.36f, 0.40f, 0.86f, 1.0f), // ultraviolet - indigo
			FLinearColor(0.46f, 0.64f, 0.92f, 1.0f), // x-ray - light blue
			FLinearColor(0.80f, 0.82f, 0.96f, 1.0f)  // gamma - white-violet
		};
		const float SegmentW = (Right - Left) / 7.0f;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float SX = Left + SegmentW * Index;
			const float SegInner = SegmentW - 3.0f * S;
			RRect(Canvas, BandCols[Index], SX, MidY - 16.0f * S, SegInner, 32.0f * S);
			// Fit each band abbreviation to its segment so "VIS"/"IR" never bleed into a neighbour at
			// narrow widths or on the small terminal render target.
			const FString BandLabel(Labels[Index]);
			float LblScale = 0.72f * S;
			float LblW = 0.0f, LblH = 0.0f;
			Canvas->TextSize(Font, BandLabel, LblW, LblH, LblScale, LblScale);
			if (LblW > SegInner * 0.92f && LblW > 0.0f) { LblScale *= (SegInner * 0.92f) / LblW; }
			RTextCentered(Canvas, Font, BandLabel, SX + SegInner * 0.5f, MidY - 5.0f * S, FLinearColor(0.05f, 0.06f, 0.07f, 1.0f), LblScale);
		}
		// ShapeVariant 1..7 brackets the band the question is about (radio..gamma).
		if (Ctx.bEnhanced && P.ShapeVariant >= 1 && P.ShapeVariant <= 7)
		{
			const float HX = Left + SegmentW * (P.ShapeVariant - 1);
			RCornerBrackets(Canvas, HX - 1.0f * S, MidY - 20.0f * S, SegmentW - 1.0f * S, 40.0f * S, Ctx.TextMain, 7.0f * S, 2.0f * S);
		}
		CCap(TEXT("short wavelength -> high frequency"), Left + 10.0f * S, Top + 4.0f * S, Warm, 0.68f * S);
		break;
	}
	case EBHDiagramType::RayDiagram:
	{
		const float CX = X + W * 0.50f;
		RLine(Canvas, CX, Top, CX, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, MidY, Right, MidY, FLinearColor(0.45f, 0.50f, 0.52f, 0.8f), 1.0f * S);
		const float AngleDeg = P.AngleOrShape > 0.0f ? FMath::Clamp(P.AngleOrShape, 5.0f, 80.0f) : 35.0f;
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const float RayLen = (MidY - Top) * 0.92f;
		const float Dx = FMath::Sin(Rad) * RayLen;
		const float Dy = FMath::Cos(Rad) * RayLen;
		RArrow(Canvas, CX - Dx, MidY - Dy, CX, MidY, Warm, 3.0f * S, 8.0f * S);
		RArrow(Canvas, CX, MidY, CX + Dx, MidY - Dy, Line, 3.0f * S, 8.0f * S);
		if (Ctx.bEnhanced && !P.LabelA.IsEmpty())
		{
			RText(Canvas, Font, P.LabelA, Left + 8.0f * S, Top + 6.0f * S, Main, 0.72f * S);
		}
		else
		{
			CCap(TEXT("normal | i = r"), Left + 8.0f * S, Top + 6.0f * S, Main, 0.72f * S);
		}
		break;
	}
	case EBHDiagramType::Sankey:
		RRect(Canvas, Line, Left, MidY - 12.0f * S, W * 0.42f, 24.0f * S);
		RRect(Canvas, Ctx.Good, X + W * 0.50f, MidY - 10.0f * S, W * 0.28f, 20.0f * S);
		RRect(Canvas, Warm, X + W * 0.50f, MidY + 18.0f * S, W * 0.20f, 14.0f * S);
		CCap(TEXT("input -> useful + wasted"), Left + 12.0f * S, Top + 6.0f * S, Main, 0.74f * S);
		if (bData)
		{
			if (!P.LabelA.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelA, Left + 6.0f * S, MidY - 26.0f * S, Main, 0.60f * S, X + 6.0f * S, X + W - 6.0f * S); }
			if (!P.LabelB.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelB, X + W * 0.50f, MidY - 24.0f * S, Ctx.Good, 0.58f * S, X + 6.0f * S, X + W - 6.0f * S); }
			if (!P.LabelC.IsEmpty()) { RTextBounded(Canvas, Font, P.LabelC, X + W * 0.50f, MidY + 34.0f * S, Warm, 0.58f * S, X + 6.0f * S, X + W - 6.0f * S); }
		}
		break;
	case EBHDiagramType::Lens:
	{
		// Converging-lens ray diagram: optical axis, lens symbol, focal points, object + image.
		const float CX = X + W * 0.50f;
		RLine(Canvas, Left, MidY, Right, MidY, AxisCol, 1.0f * S);
		const float LensTop = Top + 8.0f * S;
		const float LensBot = Bottom - 8.0f * S;
		RLine(Canvas, CX, LensTop, CX, LensBot, Line, 2.0f * S);
		RLine(Canvas, CX, LensTop, CX - 6.0f * S, LensTop + 8.0f * S, Line, 2.0f * S);
		RLine(Canvas, CX, LensTop, CX + 6.0f * S, LensTop + 8.0f * S, Line, 2.0f * S);
		RLine(Canvas, CX, LensBot, CX - 6.0f * S, LensBot - 8.0f * S, Line, 2.0f * S);
		RLine(Canvas, CX, LensBot, CX + 6.0f * S, LensBot - 8.0f * S, Line, 2.0f * S);
		const float FOff = W * 0.18f;
		RRect(Canvas, Warm, CX - FOff - 2.0f * S, MidY - 2.0f * S, 4.0f * S, 4.0f * S);
		RRect(Canvas, Warm, CX + FOff - 2.0f * S, MidY - 2.0f * S, 4.0f * S, 4.0f * S);
		RText(Canvas, Font, TEXT("F"), CX - FOff - 3.0f * S, MidY + 4.0f * S, Warm, 0.54f * S);
		RText(Canvas, Font, TEXT("F"), CX + FOff - 3.0f * S, MidY + 4.0f * S, Warm, 0.54f * S);
		const float ObjX = CX - W * 0.34f;
		const float ImgX = CX + FOff * 1.6f;
		const float ObjTop = MidY - (MidY - Top) * 0.7f;
		const float ImgBot = MidY + (Bottom - MidY) * 0.5f;
		RArrow(Canvas, ObjX, MidY, ObjX, ObjTop, Main, 2.5f * S, 7.0f * S);
		RArrow(Canvas, ImgX, MidY, ImgX, ImgBot, Main, 2.5f * S, 7.0f * S);
		RLine(Canvas, ObjX, ObjTop, CX, ObjTop, Warm, 1.5f * S);
		RLine(Canvas, CX, ObjTop, ImgX, ImgBot, Warm, 1.5f * S);
		RLine(Canvas, ObjX, ObjTop, ImgX, ImgBot, Line, 1.0f * S);
		if (Ctx.bEnhanced && !P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + 8.0f * S, Top + 4.0f * S, Warm, 0.70f * S); }
		else { CCap(TEXT("converging lens"), Left + 8.0f * S, Top + 4.0f * S, Warm, 0.70f * S); }
		break;
	}
	case EBHDiagramType::Transformer:
	{
		// Laminated core with primary (left) and secondary (right) coils; Vp/Vs = Np/Ns.
		const float CoreL = X + W * 0.44f;
		const float CoreR = X + W * 0.56f;
		const float CoreTop = Top + 6.0f * S;
		const float CoreBot = Bottom - 6.0f * S;
		RRect(Canvas, FLinearColor(0.45f, 0.45f, 0.50f, 0.9f), CoreL, CoreTop, 3.0f * S, CoreBot - CoreTop);
		RRect(Canvas, FLinearColor(0.45f, 0.45f, 0.50f, 0.9f), CoreR, CoreTop, 3.0f * S, CoreBot - CoreTop);
		// Coil turn counts reflect the labelled ratio when given (ValueA = Np, ValueB = Ns), so the
		// picture matches a step-up / step-down question instead of contradicting it. Equal counts
		// otherwise, so the drawing never implies a ratio that was not specified.
		int32 PrimaryLoops = 4;
		int32 SecondaryLoops = 4;
		if (P.ValueA > 0.0f && P.ValueB > 0.0f)
		{
			const float MaxN = FMath::Max(P.ValueA, P.ValueB);
			PrimaryLoops = FMath::Clamp(FMath::RoundToInt(P.ValueA / MaxN * 6.0f), 2, 8);
			SecondaryLoops = FMath::Clamp(FMath::RoundToInt(P.ValueB / MaxN * 6.0f), 2, 8);
		}
		const float CoilY0 = CoreTop + 10.0f * S;
		const float CoilY1 = CoreBot - 10.0f * S;
		for (int32 i = 0; i < PrimaryLoops; ++i)
		{
			const float ly = FMath::Lerp(CoilY0, CoilY1, PrimaryLoops > 1 ? static_cast<float>(i) / (PrimaryLoops - 1) : 0.5f);
			RCircle(Canvas, CoreL - 6.0f * S, ly, 5.0f * S, Warm, 1.5f * S, 10);
		}
		for (int32 i = 0; i < SecondaryLoops; ++i)
		{
			const float ly = FMath::Lerp(CoilY0, CoilY1, SecondaryLoops > 1 ? static_cast<float>(i) / (SecondaryLoops - 1) : 0.5f);
			RCircle(Canvas, CoreR + 9.0f * S, ly, 5.0f * S, Line, 1.5f * S, 10);
		}
		RText(Canvas, Font, (Ctx.bEnhanced && !P.LabelA.IsEmpty()) ? P.LabelA : FString(TEXT("primary Np")), Left + 8.0f * S, Top + 4.0f * S, Warm, 0.62f * S);
		RTextRight(Canvas, Font, (Ctx.bEnhanced && !P.LabelB.IsEmpty()) ? P.LabelB : FString(TEXT("secondary Ns")), Right - 6.0f * S, Top + 4.0f * S, Line, 0.62f * S);
		CCapC(TEXT("Vp/Vs = Np/Ns"), X + W * 0.50f, Bottom - 14.0f * S, Main, 0.62f * S);
		break;
	}
	case EBHDiagramType::MagneticField:
	{
		// Bar magnet (N warm / S cool) with field-line arcs N -> S above and below.
		const float MagW = W * 0.44f;
		const float MagH = 22.0f * S;
		const float MagL = X + W * 0.5f - MagW * 0.5f;
		const float MagTop = MidY - MagH * 0.5f;
		RRect(Canvas, Warm, MagL, MagTop, MagW * 0.5f, MagH);
		RRect(Canvas, Ctx.Good, MagL + MagW * 0.5f, MagTop, MagW * 0.5f, MagH);
		RTextCentered(Canvas, Font, TEXT("N"), MagL + MagW * 0.25f, MidY - 5.0f * S, FLinearColor(0.05f, 0.05f, 0.06f, 1.0f), 0.70f * S);
		RTextCentered(Canvas, Font, TEXT("S"), MagL + MagW * 0.75f, MidY - 5.0f * S, FLinearColor(0.05f, 0.05f, 0.06f, 1.0f), 0.70f * S);
		const float NX = MagL;
		const float SX = MagL + MagW;
		for (int32 k = 1; k <= 2; ++k)
		{
			const float Bulge = (8.0f + k * 13.0f) * S;
			FVector2D PrevU(NX, MidY);
			FVector2D PrevL(NX, MidY);
			for (int32 s2 = 1; s2 <= 16; ++s2)
			{
				const float T = static_cast<float>(s2) / 16;
				const float PX = FMath::Lerp(NX, SX, T);
				const float UY = MidY - FMath::Sin(T * PI) * Bulge - MagH * 0.5f;
				const float LY = MidY + FMath::Sin(T * PI) * Bulge + MagH * 0.5f;
				RLine(Canvas, PrevU.X, PrevU.Y, PX, UY, Line, 1.2f * S);
				RLine(Canvas, PrevL.X, PrevL.Y, PX, LY, Line, 1.2f * S);
				PrevU = FVector2D(PX, UY);
				PrevL = FVector2D(PX, LY);
			}
		}
		if (Ctx.bEnhanced && !P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + 8.0f * S, Top + 4.0f * S, Main, 0.66f * S); }
		else { CCap(TEXT("field: N -> S"), Left + 8.0f * S, Top + 4.0f * S, Main, 0.66f * S); }
		break;
	}
	case EBHDiagramType::InclinedPlane:
	{
		// Ramp with a block: weight (down) and normal (perpendicular to slope).
		const float AngleDeg = (Ctx.bEnhanced && P.AngleOrShape > 0.0f) ? FMath::Clamp(P.AngleOrShape, 10.0f, 55.0f) : 30.0f;
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const float BaseY = Bottom - 6.0f * S;
		const float BX0 = Left + 10.0f * S;
		const float Run = (Right - Left) * 0.80f;
		const float Rise = FMath::Min(Run * FMath::Tan(Rad), (BaseY - Top) * 0.80f);
		const float BX1 = BX0 + Run;
		const float TopY = BaseY - Rise;
		RLine(Canvas, BX0, BaseY, BX1, BaseY, AxisCol, 1.5f * S);
		RLine(Canvas, BX1, BaseY, BX1, TopY, AxisCol, 1.5f * S);
		RLine(Canvas, BX0, BaseY, BX1, TopY, Line, 2.5f * S);
		const float BkX = FMath::Lerp(BX0, BX1, 0.55f);
		const float BkY = FMath::Lerp(BaseY, TopY, 0.55f);
		RRect(Canvas, Warm, BkX - 7.0f * S, BkY - 7.0f * S, 14.0f * S, 14.0f * S);
		RArrow(Canvas, BkX, BkY, BkX, BkY + 22.0f * S, Main, 2.0f * S, 6.0f * S);
		RArrow(Canvas, BkX, BkY, BkX - FMath::Sin(Rad) * 20.0f * S, BkY - FMath::Cos(Rad) * 20.0f * S, Line, 2.0f * S, 6.0f * S);
		if (Ctx.bEnhanced && !P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + 8.0f * S, Top + 4.0f * S, Warm, 0.66f * S); }
		else { CCap(FString::Printf(TEXT("ramp %d deg"), FMath::RoundToInt(AngleDeg)), Left + 8.0f * S, Top + 4.0f * S, Warm, 0.66f * S); }
		break;
	}
	case EBHDiagramType::PressureColumn:
	{
		// Liquid column: container, fill, depth h, pressure relation.
		const float ConL = X + W * 0.30f;
		const float ConR = X + W * 0.70f;
		const float ConTop = Top + 8.0f * S;
		const float ConBot = Bottom - 8.0f * S;
		RLine(Canvas, ConL, ConTop, ConL, ConBot, AxisCol, 2.0f * S);
		RLine(Canvas, ConR, ConTop, ConR, ConBot, AxisCol, 2.0f * S);
		RLine(Canvas, ConL, ConBot, ConR, ConBot, AxisCol, 2.0f * S);
		const float LiqTop = ConTop + 6.0f * S;
		RRect(Canvas, FLinearColor(0.20f, 0.50f, 0.78f, 0.40f), ConL + 2.0f * S, LiqTop, ConR - ConL - 4.0f * S, ConBot - LiqTop);
		RArrow(Canvas, ConL + 12.0f * S, LiqTop, ConL + 12.0f * S, ConBot, Warm, 1.5f * S, 5.0f * S);
		RArrow(Canvas, ConL + 12.0f * S, ConBot, ConL + 12.0f * S, LiqTop, Warm, 1.5f * S, 5.0f * S);
		RText(Canvas, Font, (Ctx.bEnhanced && !P.LabelA.IsEmpty()) ? P.LabelA : FString(TEXT("h")), ConL + 16.0f * S, (LiqTop + ConBot) * 0.5f - 6.0f * S, Warm, 0.62f * S);
		CCapC(TEXT("P = rho g h"), X + W * 0.5f, ConBot - 16.0f * S, Main, 0.62f * S);
		break;
	}
	case EBHDiagramType::EnergyBars:
	{
		// Up to four labelled bars sized by ValueA..D, e.g. before/after energy stores.
		const float Vals[4] = {P.ValueA, P.ValueB, P.ValueC, P.ValueD};
		const FString* Labs[4] = {&P.LabelA, &P.LabelB, &P.LabelC, &P.LabelD};
		float MaxV = 1.0f;
		int32 Count = 0;
		for (int32 i = 0; i < 4; ++i)
		{
			if (Vals[i] > 0.0f || !Labs[i]->IsEmpty()) { Count = i + 1; }
			MaxV = FMath::Max(MaxV, Vals[i]);
		}
		if (Count == 0) { Count = 2; }
		const float BaseY = Bottom - 16.0f * S;
		const float AreaW = Right - Left;
		const float Slot = AreaW / Count;
		const float BarW = Slot * 0.5f;
		const FLinearColor Cols[4] = {Warm, Line, Ctx.Good, FLinearColor(0.70f, 0.60f, 0.90f, 1.0f)};
		for (int32 i = 0; i < Count; ++i)
		{
			const float Frac = (Ctx.bEnhanced && Vals[i] > 0.0f) ? FMath::Clamp(Vals[i] / MaxV, 0.05f, 1.0f) : 0.5f;
			const float BarH = (BaseY - Top - 8.0f * S) * Frac;
			const float BarX = Left + Slot * i + (Slot - BarW) * 0.5f;
			RRect(Canvas, Cols[i % 4], BarX, BaseY - BarH, BarW, BarH);
			if (Ctx.bEnhanced && !Labs[i]->IsEmpty()) { RTextCentered(Canvas, Font, *Labs[i], BarX + BarW * 0.5f, BaseY + 2.0f * S, Main, 0.52f * S); }
		}
		RLine(Canvas, Left, BaseY, Right, BaseY, AxisCol, 1.5f * S);
		CCap(TEXT("energy is conserved"), Left + 8.0f * S, Top + 4.0f * S, Warm, 0.64f * S);
		break;
	}
	case EBHDiagramType::ParticleModel:
	{
		// Solid (regular grid), liquid (loose), gas (sparse) particle arrangements.
		const float CellW = (Right - Left) / 3.0f;
		const float CellTop = MidY - 18.0f * S;
		const float CellH = 36.0f * S;
		const TCHAR* Names[3] = {TEXT("solid"), TEXT("liquid"), TEXT("gas")};
		const float R = 3.0f * S;
		for (int32 c = 0; c < 3; ++c)
		{
			const float CellX = Left + CellW * c + 4.0f * S;
			const float CW = CellW - 8.0f * S;
			RCornerBrackets(Canvas, CellX, CellTop, CW, CellH, FLinearColor(0.40f, 0.55f, 0.60f, 0.40f), 6.0f * S, 1.0f * S);
			if (c == 0)
			{
				for (int32 gy = 0; gy < 3; ++gy)
				{
					for (int32 gx = 0; gx < 4; ++gx)
					{
						RCircle(Canvas, CellX + 8.0f * S + gx * (CW - 16.0f * S) / 3.0f, CellTop + 8.0f * S + gy * (CellH - 16.0f * S) / 2.0f, R, Line, 1.2f * S, 8);
					}
				}
			}
			else if (c == 1)
			{
				const float Jit[6][2] = {{0.14f, 0.22f}, {0.42f, 0.5f}, {0.7f, 0.26f}, {0.26f, 0.72f}, {0.55f, 0.8f}, {0.82f, 0.6f}};
				for (int32 p2 = 0; p2 < 6; ++p2) { RCircle(Canvas, CellX + Jit[p2][0] * CW, CellTop + Jit[p2][1] * CellH, R, Line, 1.2f * S, 8); }
			}
			else
			{
				const float Sp[4][2] = {{0.22f, 0.26f}, {0.7f, 0.4f}, {0.4f, 0.76f}, {0.85f, 0.78f}};
				for (int32 p2 = 0; p2 < 4; ++p2) { RCircle(Canvas, CellX + Sp[p2][0] * CW, CellTop + Sp[p2][1] * CellH, R, Line, 1.2f * S, 8); }
			}
			RTextCentered(Canvas, Font, Names[c], CellX + CW * 0.5f, CellTop + CellH + 2.0f * S, Main, 0.56f * S);
		}
		CCap(TEXT("particle arrangement"), Left + 8.0f * S, Top + 4.0f * S, Warm, 0.64f * S);
		break;
	}
	case EBHDiagramType::EnergyChain:
	default:
	{
		// Width-relative three-cell layout (the old version used absolute pixel offsets that
		// overflowed narrow panels).
		const float AreaW = Right - Left;
		const float BoxW = AreaW * 0.26f;
		const float Gap = (AreaW - 3.0f * BoxW) * 0.5f;
		const float BoxH = 28.0f * S;
		const float BoxTop = MidY - BoxH * 0.5f;
		const FLinearColor BoxCols[] = {
			FLinearColor(0.26f, 0.42f, 0.34f, 1.0f),
			FLinearColor(0.35f, 0.34f, 0.52f, 1.0f),
			FLinearColor(0.46f, 0.31f, 0.28f, 1.0f)
		};
		float BoxLeft[3];
		for (int32 Index = 0; Index < 3; ++Index)
		{
			BoxLeft[Index] = Left + Index * (BoxW + Gap);
			RRect(Canvas, BoxCols[Index], BoxLeft[Index], BoxTop, BoxW, BoxH);
		}
		for (int32 Index = 0; Index < 2; ++Index)
		{
			RArrow(Canvas, BoxLeft[Index] + BoxW, MidY, BoxLeft[Index + 1], MidY, Line, 3.0f * S, 7.0f * S);
		}
		CCap(TEXT("store -> pathway -> store"), Left + 12.0f * S, Top + 6.0f * S, Main, 0.74f * S);
		if (bData)
		{
			const FString* Labels[3] = {&P.LabelA, &P.LabelB, &P.LabelC};
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (!Labels[Index]->IsEmpty())
				{
					RTextCentered(Canvas, Font, *Labels[Index], BoxLeft[Index] + BoxW * 0.5f, BoxTop + BoxH + 4.0f * S, Main, 0.56f * S);
				}
			}
		}
		break;
	}
	}

	// Subtopic caption (concept under test) + key-idea formula. Answer-safe: never the value.
	if (Ctx.bDrawCaptions && !Subtopic.IsEmpty())
	{
		RText(Canvas, Font, Subtopic.ToUpper(), X + 10.0f * S, Y + 4.0f * S, FLinearColor(0.66f, 0.82f, 0.94f, 0.92f), 0.62f * S);
	}
	if (Ctx.bDrawCaptions && Ctx.bShowConceptCaptions && !Formula.IsEmpty())
	{
		RTextRight(Canvas, Font, FString::Printf(TEXT("Key idea: %s"), *Formula), X + W - 10.0f * S, Y + H - 18.0f * S, FLinearColor(0.95f, 0.84f, 0.45f, 1.0f), 0.66f * S);
	}
}

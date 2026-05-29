#include "BHDiagramRenderer.h"

#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

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

	// Line with a V arrowhead at (X1,Y1). Head segments sit 150 degrees off the shaft (~30 deg
	// half-angle). Used for force vectors, energy flows and lever loads.
	void RArrow(UCanvas* C, float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness, float HeadSize)
	{
		RLine(C, X0, Y0, X1, Y1, Color, Thickness);
		const float Ang = FMath::Atan2(Y1 - Y0, X1 - X0);
		const float Spread = FMath::DegreesToRadians(150.0f);
		RLine(C, X1, Y1, X1 + FMath::Cos(Ang + Spread) * HeadSize, Y1 + FMath::Sin(Ang + Spread) * HeadSize, Color, Thickness);
		RLine(C, X1, Y1, X1 + FMath::Cos(Ang - Spread) * HeadSize, Y1 + FMath::Sin(Ang - Spread) * HeadSize, Color, Thickness);
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
		return 124.0f;
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
		const float FitScale = FMath::Min((W - 12.0f * S) / TexW, (H - 12.0f * S) / TexH);
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
		if (Ctx.bDrawCaptions && !Formula.IsEmpty())
		{
			RTextRight(Canvas, Font, FString::Printf(TEXT("Key idea: %s"), *Formula), X + W - 10.0f * S, Y + H - 18.0f * S, FLinearColor(0.95f, 0.84f, 0.45f, 1.0f), 0.66f * S);
		}
		return;
	}

	// ---- Chrome: background, frame lines, faint grid, corner brackets. ----
	if (Ctx.bDrawChrome)
	{
		RRect(Canvas, FLinearColor(0.025f, 0.032f, 0.036f, 0.88f), X, Y, W, H);
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

	switch (Type)
	{
	case EBHDiagramType::MotionGraph:
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom - 8.0f * S, X + W * 0.42f, Y + H * 0.38f, Line, 3.0f * S);
		RLine(Canvas, X + W * 0.42f, Y + H * 0.38f, Right, Y + H * 0.26f, Line, 3.0f * S);
		RText(Canvas, Font, TEXT("gradient = velocity"), Left + 8.0f * S, Top + 4.0f * S, Warm, 0.72f * S);
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		break;
	case EBHDiagramType::VelocityGraph:
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		RRect(Canvas, FLinearColor(0.30f, 0.60f, 0.78f, 0.24f), Left + 10.0f * S, MidY, W * 0.38f, Bottom - MidY);
		RLine(Canvas, Left + 10.0f * S, MidY, X + W * 0.52f, MidY, Line, 3.0f * S);
		RLine(Canvas, X + W * 0.52f, MidY, Right, Top + 8.0f * S, Line, 3.0f * S);
		RText(Canvas, Font, TEXT("area = distance"), Left + 18.0f * S, Bottom - 34.0f * S, Warm, 0.72f * S);
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		if (bData && !P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + 18.0f * S, Top + 6.0f * S, Main, 0.60f * S); }
		break;
	case EBHDiagramType::ForceArrows:
		RArrow(Canvas, X + W * 0.50f, MidY, X + W * 0.26f, MidY, Warm, 4.0f * S, 9.0f * S);
		RArrow(Canvas, X + W * 0.50f, MidY, X + W * 0.74f, MidY, Line, 4.0f * S, 9.0f * S);
		RText(Canvas, Font, TEXT("resultant force"), X + W * 0.39f, Top + 8.0f * S, Main, 0.78f * S);
		if (bData)
		{
			if (!P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, X + W * 0.16f, MidY - 18.0f * S, Warm, 0.66f * S); }
			if (!P.LabelB.IsEmpty()) { RText(Canvas, Font, P.LabelB, X + W * 0.66f, MidY - 18.0f * S, Line, 0.66f * S); }
		}
		break;
	case EBHDiagramType::SpringGraph:
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Right - 32.0f * S, Top + 18.0f * S, Line, 3.0f * S);
		RText(Canvas, Font, TEXT("gradient = k"), Left + 18.0f * S, Top + 6.0f * S, Warm, 0.76f * S);
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		break;
	case EBHDiagramType::MomentBeam:
		RRect(Canvas, FLinearColor(0.55f, 0.50f, 0.38f, 1.0f), Left, MidY, Right - Left, 8.0f * S);
		RLine(Canvas, X + W * 0.48f, MidY + 8.0f * S, X + W * 0.48f - 14.0f * S, Bottom, Warm, 3.0f * S);
		RLine(Canvas, X + W * 0.48f, MidY + 8.0f * S, X + W * 0.48f + 14.0f * S, Bottom, Warm, 3.0f * S);
		RArrow(Canvas, X + W * 0.78f, MidY - 30.0f * S, X + W * 0.78f, MidY, Line, 4.0f * S, 8.0f * S);
		RText(Canvas, Font, TEXT("moment = Fd"), Left + 14.0f * S, Top + 8.0f * S, Main, 0.78f * S);
		if (bData)
		{
			if (!P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + 14.0f * S, MidY - 18.0f * S, Warm, 0.64f * S); }
			if (!P.LabelC.IsEmpty()) { RTextRight(Canvas, Font, P.LabelC, Right - 6.0f * S, MidY - 18.0f * S, Line, 0.64f * S); }
			if (!P.LabelB.IsEmpty()) { RText(Canvas, Font, P.LabelB, Left + 14.0f * S, Bottom - 12.0f * S, Main, 0.60f * S); }
			if (!P.LabelD.IsEmpty()) { RTextRight(Canvas, Font, P.LabelD, Right - 6.0f * S, Bottom - 12.0f * S, Main, 0.60f * S); }
		}
		break;
	case EBHDiagramType::Circuit:
		RLine(Canvas, Left, Top + 16.0f * S, Right, Top + 16.0f * S, Line, 2.0f * S);
		RLine(Canvas, Right, Top + 16.0f * S, Right, Bottom - 10.0f * S, Line, 2.0f * S);
		RLine(Canvas, Right, Bottom - 10.0f * S, Left, Bottom - 10.0f * S, Line, 2.0f * S);
		RLine(Canvas, Left, Bottom - 10.0f * S, Left, Top + 16.0f * S, Line, 2.0f * S);
		RRect(Canvas, Warm, Left + W * 0.36f, Top + 8.0f * S, 34.0f * S, 16.0f * S);
		RText(Canvas, Font, TEXT("A series | V parallel"), Left + 18.0f * S, MidY - 8.0f * S, Main, 0.74f * S);
		if (bData)
		{
			if (!P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + W * 0.30f, Top + 26.0f * S, Warm, 0.62f * S); }
			if (!P.LabelC.IsEmpty()) { RTextRight(Canvas, Font, P.LabelC, Right - 8.0f * S, Top + 26.0f * S, Warm, 0.62f * S); }
			if (!P.LabelB.IsEmpty()) { RText(Canvas, Font, P.LabelB, Left + 18.0f * S, Bottom - 8.0f * S, Line, 0.62f * S); }
		}
		break;
	case EBHDiagramType::IVGraph:
		RLine(Canvas, Left, Bottom, Right, Bottom, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Left, Top, AxisCol, 1.5f * S);
		RLine(Canvas, Left, Bottom, Right - 24.0f * S, Top + 12.0f * S, Line, 3.0f * S);
		if (Ctx.bEnhanced && P.ValueC > 0.0f && P.ValueD > 0.0f)
		{
			const float PtX = Left + (Right - Left) * FMath::Clamp(P.ValueA / P.ValueC, 0.0f, 1.0f);
			const float PtY = Bottom - (Bottom - Top) * FMath::Clamp(P.ValueB / P.ValueD, 0.0f, 1.0f);
			RRect(Canvas, Warm, PtX - 3.0f * S, PtY - 3.0f * S, 6.0f * S, 6.0f * S);
		}
		{
			const FString IVLabel = (Ctx.bEnhanced && !P.LabelA.IsEmpty()) ? P.LabelA : FString(TEXT("straight: ohmic"));
			RText(Canvas, Font, IVLabel, Left + 16.0f * S, Top + 8.0f * S, Warm, 0.74f * S);
		}
		if (Ctx.bEnhanced && !P.XAxis.IsEmpty()) { RTextRight(Canvas, Font, P.XAxis, Right - 6.0f * S, Bottom - 14.0f * S, Ctx.TextDim, 0.60f * S); }
		if (Ctx.bEnhanced && !P.YAxis.IsEmpty()) { RText(Canvas, Font, P.YAxis, Left + 4.0f * S, Top - 4.0f * S, Ctx.TextDim, 0.60f * S); }
		break;
	case EBHDiagramType::StaticCharge:
		RText(Canvas, Font, TEXT("+ + +"), Left + 24.0f * S, MidY - 8.0f * S, Warm, 1.0f * S);
		RText(Canvas, Font, TEXT("- - -"), Right - 92.0f * S, MidY - 8.0f * S, Line, 1.0f * S);
		RArrow(Canvas, X + W * 0.42f, MidY, X + W * 0.58f, MidY, Main, 3.0f * S, 7.0f * S);
		RText(Canvas, Font, TEXT("opposites attract"), X + W * 0.38f, Top + 8.0f * S, Main, 0.74f * S);
		break;
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
		const FString WaveLabel = bData ? FString::Printf(TEXT("%s | %s"), *P.LabelA, *P.LabelB) : FString(TEXT("amplitude | wavelength"));
		RText(Canvas, Font, WaveLabel, Left + 14.0f * S, Top + 6.0f * S, Warm, 0.74f * S);
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
			RRect(Canvas, BandCols[Index], SX, MidY - 16.0f * S, SegmentW - 3.0f * S, 32.0f * S);
			RTextCentered(Canvas, Font, Labels[Index], SX + (SegmentW - 3.0f * S) * 0.5f, MidY - 5.0f * S, FLinearColor(0.05f, 0.06f, 0.07f, 1.0f), 0.72f * S);
		}
		RText(Canvas, Font, TEXT("long wavelength -> high frequency"), Left + 10.0f * S, Top + 4.0f * S, Warm, 0.68f * S);
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
		const FString RayLabel = (Ctx.bEnhanced && !P.LabelA.IsEmpty()) ? P.LabelA : FString(TEXT("normal | i = r"));
		RText(Canvas, Font, RayLabel, Left + 8.0f * S, Top + 6.0f * S, Main, 0.72f * S);
		break;
	}
	case EBHDiagramType::Sankey:
		RRect(Canvas, Line, Left, MidY - 12.0f * S, W * 0.42f, 24.0f * S);
		RRect(Canvas, Ctx.Good, X + W * 0.50f, MidY - 10.0f * S, W * 0.28f, 20.0f * S);
		RRect(Canvas, Warm, X + W * 0.50f, MidY + 18.0f * S, W * 0.20f, 14.0f * S);
		RText(Canvas, Font, TEXT("input -> useful + wasted"), Left + 12.0f * S, Top + 6.0f * S, Main, 0.74f * S);
		if (bData)
		{
			if (!P.LabelA.IsEmpty()) { RText(Canvas, Font, P.LabelA, Left + 6.0f * S, MidY - 26.0f * S, Main, 0.60f * S); }
			if (!P.LabelB.IsEmpty()) { RText(Canvas, Font, P.LabelB, X + W * 0.50f, MidY - 24.0f * S, Ctx.Good, 0.58f * S); }
			if (!P.LabelC.IsEmpty()) { RText(Canvas, Font, P.LabelC, X + W * 0.50f, MidY + 34.0f * S, Warm, 0.58f * S); }
		}
		break;
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
		RText(Canvas, Font, TEXT("store -> pathway -> store"), Left + 12.0f * S, Top + 6.0f * S, Main, 0.74f * S);
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
	if (Ctx.bDrawCaptions && !Formula.IsEmpty())
	{
		RTextRight(Canvas, Font, FString::Printf(TEXT("Key idea: %s"), *Formula), X + W - 10.0f * S, Y + H - 18.0f * S, FLinearColor(0.95f, 0.84f, 0.45f, 1.0f), 0.66f * S);
	}
}

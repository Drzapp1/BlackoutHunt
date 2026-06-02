// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHRenderDiagramsCommandlet.h"

#if WITH_EDITOR
#include "BHDiagramRenderer.h"
#include "BHTypes.h"
#include "Editor.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BHRenderDiagramsCommandlet)

UBHRenderDiagramsCommandlet::UBHRenderDiagramsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Renders each revision diagram type to a PNG using FBHDiagramRenderer (art export + visual QA).");
	HelpUsage = TEXT("BlackoutHuntEditor-Cmd.exe BlackoutHunt.uproject -run=BHRenderDiagrams -AllowCommandletRendering -unattended");
}

int32 UBHRenderDiagramsCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogTemp, Error, TEXT("BHRenderDiagrams requires an editor target."));
	return 1;
#else
	if (!GEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHRenderDiagrams] GEditor is null; run with an editor build."));
		return 1;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		World = GWorld;
	}
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHRenderDiagrams] No world available for canvas rendering."));
		return 1;
	}

	int32 CellW = 480;
	int32 CellH = 200;
	FParse::Value(*Params, TEXT("W="), CellW);
	FParse::Value(*Params, TEXT("H="), CellH);
	CellW = FMath::Clamp(CellW, 160, 1024);
	CellH = FMath::Clamp(CellH, 100, 1024);

	FString OutDir;
	if (!FParse::Value(*Params, TEXT("Out="), OutDir) || OutDir.IsEmpty())
	{
		OutDir = FPaths::ProjectSavedDir() / TEXT("DiagramShots");
	}

	const bool bLight = FParse::Param(*Params, TEXT("Light"));

	// Draw context: dark "scanner" palette (matches the live HUD) by default, or a clean
	// high-contrast textbook palette when -Light is passed.
	FBHDiagramDrawContext Ctx;
	Ctx.Scale = static_cast<float>(CellH) / 130.0f; // scale fonts/insets to the requested cell size
	Ctx.Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	Ctx.TimeSeconds = 0.0f;
	if (bLight)
	{
		Ctx.Background = FLinearColor(0.96f, 0.96f, 0.94f, 1.0f);
		Ctx.Accent = FLinearColor(0.10f, 0.28f, 0.62f, 1.0f);
		Ctx.Warm = FLinearColor(0.74f, 0.20f, 0.10f, 1.0f);
		Ctx.Good = FLinearColor(0.12f, 0.52f, 0.20f, 1.0f);
		Ctx.TextMain = FLinearColor(0.08f, 0.09f, 0.10f, 1.0f);
		Ctx.TextDim = FLinearColor(0.30f, 0.34f, 0.38f, 1.0f);
		Ctx.Axis = FLinearColor(0.20f, 0.22f, 0.24f, 1.0f);
		Ctx.Grid = FLinearColor(0.20f, 0.30f, 0.40f, 0.10f);
	}

	// Representative sample for every diagram type (and a notable variant), so the export shows
	// the data-driven shapes rather than empty schematics.
	struct FSample
	{
		EBHDiagramType Type;
		FString Name;
		FString Formula;
		FBHDiagramParams P;
	};
	TArray<FSample> Samples;
	auto AddSample = [&Samples](EBHDiagramType Type, const TCHAR* Name, const TCHAR* Formula, const FBHDiagramParams& P)
	{
		Samples.Add({Type, FString(Name), FString(Formula), P});
	};

	{ FBHDiagramParams P; P.ShapeVariant = 2; P.XAxis = TEXT("t / s"); P.YAxis = TEXT("d / m"); AddSample(EBHDiagramType::MotionGraph, TEXT("Motion (accel)"), TEXT("gradient = v"), P); }
	{ FBHDiagramParams P; P.ShapeVariant = 2; P.XAxis = TEXT("t / s"); P.YAxis = TEXT("v / m/s"); AddSample(EBHDiagramType::VelocityGraph, TEXT("Velocity (area)"), TEXT("area = distance"), P); }
	{ FBHDiagramParams P; P.ValueA = 30.0f; P.ValueB = 20.0f; P.LabelA = TEXT("30 N"); P.LabelB = TEXT("20 N"); AddSample(EBHDiagramType::ForceArrows, TEXT("Force arrows"), TEXT("F net"), P); }
	{ FBHDiagramParams P; P.ShapeVariant = 1; P.LabelC = TEXT("weight"); P.LabelD = TEXT("normal"); AddSample(EBHDiagramType::ForceArrows, TEXT("Free body"), TEXT("balanced"), P); }
	{ FBHDiagramParams P; P.XAxis = TEXT("x / m"); P.YAxis = TEXT("F / N"); AddSample(EBHDiagramType::SpringGraph, TEXT("Spring"), TEXT("F = kx"), P); }
	{ FBHDiagramParams P; P.LabelA = TEXT("0.5 m"); P.LabelC = TEXT("1.5 m"); P.LabelB = TEXT("12 N"); P.LabelD = TEXT("4 N"); AddSample(EBHDiagramType::MomentBeam, TEXT("Moment beam"), TEXT("moment = Fd"), P); }
	{ FBHDiagramParams P; P.ShapeVariant = 3; P.LabelA = TEXT("2 ohm"); P.LabelC = TEXT("8 ohm"); AddSample(EBHDiagramType::Circuit, TEXT("Circuit (series-3)"), TEXT("RT = R1+R2+R3"), P); }
	{ FBHDiagramParams P; P.ShapeVariant = 2; P.LabelB = TEXT("6 V"); AddSample(EBHDiagramType::Circuit, TEXT("Circuit (parallel)"), TEXT("same p.d."), P); }
	{ FBHDiagramParams P; P.ShapeVariant = 1; P.XAxis = TEXT("V / V"); P.YAxis = TEXT("I / A"); AddSample(EBHDiagramType::IVGraph, TEXT("I-V (filament)"), TEXT("hot -> higher R"), P); }
	{ FBHDiagramParams P; P.ShapeVariant = 2; P.XAxis = TEXT("V / V"); P.YAxis = TEXT("I / A"); AddSample(EBHDiagramType::IVGraph, TEXT("I-V (diode)"), TEXT("one-way"), P); }
	{ FBHDiagramParams P; AddSample(EBHDiagramType::StaticCharge, TEXT("Static charge"), TEXT("like repel"), P); }
	{ FBHDiagramParams P; P.ValueA = 0.30f; P.ValueB = 3.0f; P.LabelA = TEXT("amplitude"); P.LabelB = TEXT("wavelength"); AddSample(EBHDiagramType::Wave, TEXT("Wave"), TEXT("v = f lambda"), P); }
	{ FBHDiagramParams P; P.ShapeVariant = 4; AddSample(EBHDiagramType::EMSpectrum, TEXT("EM (visible)"), TEXT("radio -> gamma"), P); }
	{ FBHDiagramParams P; P.AngleOrShape = 40.0f; P.LabelA = TEXT("normal"); AddSample(EBHDiagramType::RayDiagram, TEXT("Ray (i = r)"), TEXT("i = r"), P); }
	{ FBHDiagramParams P; P.LabelA = TEXT("in 500 J"); P.LabelB = TEXT("useful"); P.LabelC = TEXT("wasted"); AddSample(EBHDiagramType::Sankey, TEXT("Sankey"), TEXT("efficiency"), P); }
	{ FBHDiagramParams P; P.LabelA = TEXT("chemical"); P.LabelB = TEXT("electrical"); P.LabelC = TEXT("light+heat"); AddSample(EBHDiagramType::EnergyChain, TEXT("Energy chain"), TEXT("store->path->store"), P); }
	{ FBHDiagramParams P; P.LabelA = TEXT("converging"); AddSample(EBHDiagramType::Lens, TEXT("Lens"), TEXT("1/f = 1/u + 1/v"), P); }
	{ FBHDiagramParams P; P.LabelA = TEXT("Np primary"); P.LabelB = TEXT("Ns secondary"); AddSample(EBHDiagramType::Transformer, TEXT("Transformer"), TEXT("Vp/Vs = Np/Ns"), P); }
	{ FBHDiagramParams P; P.LabelA = TEXT("N -> S"); AddSample(EBHDiagramType::MagneticField, TEXT("Magnetic field"), TEXT("field lines"), P); }
	{ FBHDiagramParams P; P.AngleOrShape = 30.0f; AddSample(EBHDiagramType::InclinedPlane, TEXT("Inclined plane"), TEXT("W, N, friction"), P); }
	{ FBHDiagramParams P; P.LabelA = TEXT("h"); AddSample(EBHDiagramType::PressureColumn, TEXT("Pressure column"), TEXT("P = rho g h"), P); }
	{ FBHDiagramParams P; P.ValueA = 0.9f; P.ValueB = 0.1f; P.ValueC = 0.1f; P.ValueD = 0.9f; P.LabelA = TEXT("GPE"); P.LabelB = TEXT("KE"); P.LabelC = TEXT("GPE"); P.LabelD = TEXT("KE"); AddSample(EBHDiagramType::EnergyBars, TEXT("Energy bars"), TEXT("conserved"), P); }
	{ FBHDiagramParams P; AddSample(EBHDiagramType::ParticleModel, TEXT("Particle model"), TEXT("solid/liquid/gas"), P); }

	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	if (!RT)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHRenderDiagrams] Failed to create the render target."));
		return 1;
	}
	RT->RenderTargetFormat = RTF_RGBA8;
	RT->ClearColor = bLight ? FLinearColor(0.96f, 0.96f, 0.94f, 1.0f) : FLinearColor(0.01f, 0.012f, 0.013f, 1.0f);
	RT->bAutoGenerateMips = false;
	RT->InitAutoFormat(CellW, CellH);
	RT->UpdateResourceImmediate(true);

	int32 Exported = 0;
	for (int32 Index = 0; Index < Samples.Num(); ++Index)
	{
		const FSample& S = Samples[Index];

		UCanvas* Canvas = nullptr;
		FVector2D CanvasSize(0.0f, 0.0f);
		FDrawToRenderTargetContext DrawContext;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RT, Canvas, CanvasSize, DrawContext);
		if (Canvas)
		{
			FBHDiagramRenderer::Draw(Canvas, S.Type, S.P, S.Name, S.Formula, nullptr, 0.0f, 0.0f, static_cast<float>(CellW), static_cast<float>(CellH), Ctx);
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, DrawContext);

		const FString FileName = FString::Printf(TEXT("Diagram_%02d_%s%s.png"), Index, *S.Name.Replace(TEXT(" "), TEXT("")).Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT("")).Replace(TEXT("-"), TEXT("")), bLight ? TEXT("_light") : TEXT(""));
		UKismetRenderingLibrary::ExportRenderTarget(World, RT, OutDir, FileName);
		++Exported;
		UE_LOG(LogTemp, Display, TEXT("[BHRenderDiagrams] %s -> %s"), *S.Name, *FileName);
	}

	UE_LOG(LogTemp, Display, TEXT("[BHRenderDiagrams] Exported %d diagram PNG(s) to %s"), Exported, *OutDir);
	return 0;
#endif
}

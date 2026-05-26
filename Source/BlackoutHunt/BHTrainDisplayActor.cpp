#include "BHTrainDisplayActor.h"
#include "BHGameState.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FString BHTrimDisplaySegment(FString Segment)
{
	Segment.TrimStartAndEndInline();
	return Segment;
}

FString BHWrapDisplayText(const FString& SourceText, int32 MaxCharsPerLine, int32 MaxLines)
{
	TArray<FString> SourceLines;
	SourceText.ParseIntoArrayLines(SourceLines, false);
	if (SourceLines.IsEmpty())
	{
		SourceLines.Add(SourceText);
	}

	TArray<FString> OutputLines;
	bool bTruncated = false;
	for (const FString& RawLine : SourceLines)
	{
		FString Remaining = BHTrimDisplaySegment(RawLine);
		if (Remaining.IsEmpty())
		{
			OutputLines.Add(TEXT(""));
			continue;
		}

		while (Remaining.Len() > MaxCharsPerLine)
		{
			int32 BreakIndex = INDEX_NONE;
			for (int32 Index = FMath::Min(MaxCharsPerLine, Remaining.Len() - 1); Index >= MaxCharsPerLine / 2; --Index)
			{
				if (FChar::IsWhitespace(Remaining[Index]))
				{
					BreakIndex = Index;
					break;
				}
			}
			if (BreakIndex == INDEX_NONE)
			{
				BreakIndex = MaxCharsPerLine;
			}

			OutputLines.Add(BHTrimDisplaySegment(Remaining.Left(BreakIndex)));
			Remaining = BHTrimDisplaySegment(Remaining.Mid(BreakIndex));
			if (OutputLines.Num() >= MaxLines)
			{
				bTruncated = true;
				break;
			}
		}

		if (OutputLines.Num() >= MaxLines)
		{
			bTruncated = bTruncated || !Remaining.IsEmpty();
			break;
		}

		OutputLines.Add(Remaining);
		if (OutputLines.Num() >= MaxLines)
		{
			break;
		}
	}

	if (OutputLines.Num() > MaxLines)
	{
		OutputLines.SetNum(MaxLines);
		bTruncated = true;
	}

	if (bTruncated && !OutputLines.IsEmpty())
	{
		FString& LastLine = OutputLines.Last();
		if (LastLine.Len() > MaxCharsPerLine - 3)
		{
			LastLine = BHTrimDisplaySegment(LastLine.Left(FMath::Max(0, MaxCharsPerLine - 3)));
		}
		LastLine += TEXT("...");
	}

	return FString::Join(OutputLines, TEXT("\n"));
}

void BHConfigureDisplayTextComponent(UTextRenderComponent* TextComponent, const FVector& Location, const FRotator& Rotation, float WorldSize, const FColor& Color, UMaterialInterface* TextMaterial)
{
	if (!TextComponent)
	{
		return;
	}

	TextComponent->SetRelativeLocation(Location);
	TextComponent->SetRelativeRotation(Rotation);
	TextComponent->SetHorizontalAlignment(EHTA_Left);
	TextComponent->SetVerticalAlignment(EVRTA_TextTop);
	TextComponent->SetWorldSize(WorldSize);
	TextComponent->SetTextRenderColor(Color);
	TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TextComponent->SetCastShadow(false);
	TextComponent->SetReceivesDecals(false);
	TextComponent->SetTranslucentSortPriority(12);
	TextComponent->SetBoundsScale(2.0f);
	if (TextMaterial)
	{
		TextComponent->SetTextMaterial(TextMaterial);
	}
}

void BHApplyDisplayMeshColor(UStaticMeshComponent* MeshComponent, const FLinearColor& BaseColor, const FLinearColor& EmissiveColor)
{
	if (!MeshComponent)
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0));
	if (!DynamicMaterial)
	{
		DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
	}
}
}

ABHTrainDisplayActor::ABHTrainDisplayActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(2.0f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PanelMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	const auto ConfigurePanelComponent = [&](UStaticMeshComponent* Component, const FVector& Location, const FVector& Scale, bool bCollides)
	{
		if (!Component)
		{
			return;
		}
		Component->SetupAttachment(SceneRoot);
		Component->SetRelativeLocation(Location);
		Component->SetRelativeScale3D(Scale);
		Component->SetCollisionEnabled(bCollides ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Component->SetCollisionResponseToAllChannels(bCollides ? ECR_Block : ECR_Ignore);
		Component->SetCastShadow(false);
		Component->SetReceivesDecals(false);
		if (CubeMesh.Succeeded())
		{
			Component->SetStaticMesh(CubeMesh.Object);
		}
		if (PanelMaterial.Succeeded())
		{
			Component->SetMaterial(0, PanelMaterial.Object);
		}
	};

	BackingPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackingPanel"));
	ConfigurePanelComponent(BackingPanel, FVector::ZeroVector, FVector(5.90f, 0.14f, 2.20f), true);

	ScreenFront = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenFront"));
	ConfigurePanelComponent(ScreenFront, FVector(0.0f, -8.2f, 0.0f), FVector(5.36f, 0.018f, 1.72f), false);

	ScreenBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenBack"));
	ConfigurePanelComponent(ScreenBack, FVector(0.0f, 8.2f, 0.0f), FVector(5.36f, 0.018f, 1.72f), false);

	HeaderRailFront = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeaderRailFront"));
	ConfigurePanelComponent(HeaderRailFront, FVector(0.0f, -9.3f, 92.0f), FVector(5.52f, 0.020f, 0.075f), false);

	HeaderRailBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeaderRailBack"));
	ConfigurePanelComponent(HeaderRailBack, FVector(0.0f, 9.3f, 92.0f), FVector(5.52f, 0.020f, 0.075f), false);

	FooterRailFront = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FooterRailFront"));
	ConfigurePanelComponent(FooterRailFront, FVector(0.0f, -9.3f, -92.0f), FVector(5.52f, 0.020f, 0.075f), false);

	FooterRailBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FooterRailBack"));
	ConfigurePanelComponent(FooterRailBack, FVector(0.0f, 9.3f, -92.0f), FVector(5.52f, 0.020f, 0.075f), false);

	TopFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopFrame"));
	ConfigurePanelComponent(TopFrame, FVector(0.0f, 0.0f, 112.0f), FVector(6.08f, 0.22f, 0.10f), false);

	BottomFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BottomFrame"));
	ConfigurePanelComponent(BottomFrame, FVector(0.0f, 0.0f, -112.0f), FVector(6.08f, 0.22f, 0.10f), false);

	LeftFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFrame"));
	ConfigurePanelComponent(LeftFrame, FVector(-306.0f, 0.0f, 0.0f), FVector(0.10f, 0.22f, 2.28f), false);

	RightFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFrame"));
	ConfigurePanelComponent(RightFrame, FVector(306.0f, 0.0f, 0.0f), FVector(0.10f, 0.22f, 2.28f), false);

	LeftHanger = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHanger"));
	ConfigurePanelComponent(LeftHanger, FVector(-214.0f, 0.0f, 178.0f), FVector(0.065f, 0.065f, 1.10f), false);

	RightHanger = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHanger"));
	ConfigurePanelComponent(RightHanger, FVector(214.0f, 0.0f, 178.0f), FVector(0.065f, 0.065f, 1.10f), false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TextMaterial(TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
	UMaterialInterface* ReadableTextMaterial = TextMaterial.Succeeded() ? TextMaterial.Object : nullptr;

	HeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeaderText"));
	HeaderText->SetupAttachment(SceneRoot);
	BHConfigureDisplayTextComponent(HeaderText, FVector(-248.0f, -12.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f), 20.0f, FColor(255, 190, 76), ReadableTextMaterial);

	BodyText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BodyText"));
	BodyText->SetupAttachment(SceneRoot);
	BHConfigureDisplayTextComponent(BodyText, FVector(-248.0f, -12.5f, 42.0f), FRotator(0.0f, -90.0f, 0.0f), 11.0f, FColor(226, 245, 232), ReadableTextMaterial);

	HeaderTextBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeaderTextBack"));
	HeaderTextBack->SetupAttachment(SceneRoot);
	BHConfigureDisplayTextComponent(HeaderTextBack, FVector(248.0f, 12.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f), 20.0f, FColor(255, 190, 76), ReadableTextMaterial);

	BodyTextBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BodyTextBack"));
	BodyTextBack->SetupAttachment(SceneRoot);
	BHConfigureDisplayTextComponent(BodyTextBack, FVector(248.0f, 12.5f, 42.0f), FRotator(0.0f, 90.0f, 0.0f), 11.0f, FColor(226, 245, 232), ReadableTextMaterial);

	FrontReadLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FrontReadLight"));
	FrontReadLight->SetupAttachment(SceneRoot);
	FrontReadLight->SetRelativeLocation(FVector(0.0f, -135.0f, 48.0f));
	FrontReadLight->SetIntensity(180.0f);
	FrontReadLight->SetAttenuationRadius(360.0f);
	FrontReadLight->SetCastShadows(false);

	BackReadLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BackReadLight"));
	BackReadLight->SetupAttachment(SceneRoot);
	BackReadLight->SetRelativeLocation(FVector(0.0f, 135.0f, 48.0f));
	BackReadLight->SetIntensity(180.0f);
	BackReadLight->SetAttenuationRadius(360.0f);
	BackReadLight->SetCastShadows(false);

	DisplayHeader = TEXT("BLACKOUT TRANSIT");
	DisplayBody = TEXT("Awaiting class data.");
	Accent = FLinearColor(0.10f, 0.66f, 0.62f, 1.0f);
	CountdownStationName = DisplayHeader;
	CountdownDestinationText = TEXT("");
	NextCountdownRefreshTime = 0.0f;
	LastCountdownSeconds = INDEX_NONE;
	LastCountdownBreakersCompleted = INDEX_NONE;
	LastCountdownBreakersRequired = INDEX_NONE;
	LastCountdownSideObjectivesCompleted = INDEX_NONE;
	LastCountdownSideObjectivesRequired = INDEX_NONE;
	bExitCountdownDisplay = false;
	bLastCountdownExitUnlocked = false;
}

void ABHTrainDisplayActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bExitCountdownDisplay || !HasAuthority() || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextCountdownRefreshTime)
	{
		UpdateExitCountdownDisplay(false);
		NextCountdownRefreshTime = Now + 0.5f;
	}
}

void ABHTrainDisplayActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainDisplayActor, DisplayHeader);
	DOREPLIFETIME(ABHTrainDisplayActor, DisplayBody);
	DOREPLIFETIME(ABHTrainDisplayActor, Accent);
}

void ABHTrainDisplayActor::ConfigureDisplay(const FString& NewHeader, const FString& NewBody, const FLinearColor& AccentColor)
{
	bExitCountdownDisplay = false;
	DisplayHeader = NewHeader;
	DisplayBody = NewBody;
	Accent = AccentColor;
	ApplyText();
}

void ABHTrainDisplayActor::ConfigureExitCountdownDisplay(const FString& NewStationName, const FString& NewDestinationText, const FLinearColor& AccentColor)
{
	bExitCountdownDisplay = true;
	CountdownStationName = NewStationName;
	CountdownDestinationText = NewDestinationText;
	Accent = AccentColor;
	LastCountdownSeconds = INDEX_NONE;
	LastCountdownBreakersCompleted = INDEX_NONE;
	LastCountdownBreakersRequired = INDEX_NONE;
	LastCountdownSideObjectivesCompleted = INDEX_NONE;
	LastCountdownSideObjectivesRequired = INDEX_NONE;
	bLastCountdownExitUnlocked = false;
	UpdateExitCountdownDisplay(true);
}

void ABHTrainDisplayActor::SetBodyText(const FString& NewBody)
{
	bExitCountdownDisplay = false;
	DisplayBody = NewBody;
	ApplyText();
}

FString ABHTrainDisplayActor::GetBodyText() const
{
	return DisplayBody;
}

void ABHTrainDisplayActor::OnRep_DisplayText()
{
	ApplyText();
}

void ABHTrainDisplayActor::UpdateExitCountdownDisplay(bool bForce)
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const int32 RemainingSeconds = BHGS ? BHGS->RemainingTime : 0;
	const int32 BreakersCompleted = BHGS ? BHGS->BreakersCompleted : 0;
	const int32 BreakersRequired = BHGS ? BHGS->BreakersRequired : 0;
	const int32 SideObjectivesCompleted = BHGS ? BHGS->SideObjectivesCompleted : 0;
	const int32 SideObjectivesRequired = BHGS ? BHGS->SideObjectivesRequired : 0;
	const bool bExitUnlocked = BHGS && BHGS->bExitUnlocked;

	if (!bForce
		&& RemainingSeconds == LastCountdownSeconds
		&& BreakersCompleted == LastCountdownBreakersCompleted
		&& BreakersRequired == LastCountdownBreakersRequired
		&& SideObjectivesCompleted == LastCountdownSideObjectivesCompleted
		&& SideObjectivesRequired == LastCountdownSideObjectivesRequired
		&& bExitUnlocked == bLastCountdownExitUnlocked)
	{
		return;
	}

	LastCountdownSeconds = RemainingSeconds;
	LastCountdownBreakersCompleted = BreakersCompleted;
	LastCountdownBreakersRequired = BreakersRequired;
	LastCountdownSideObjectivesCompleted = SideObjectivesCompleted;
	LastCountdownSideObjectivesRequired = SideObjectivesRequired;
	bLastCountdownExitUnlocked = bExitUnlocked;

	DisplayHeader = CountdownStationName.IsEmpty() ? TEXT("BLACKOUT TRANSIT") : CountdownStationName;
	const FString Destination = CountdownDestinationText.IsEmpty() ? TEXT("FINAL ROUTE") : CountdownDestinationText;

	if (bExitUnlocked)
	{
		DisplayBody = FString::Printf(TEXT("BOARDING OPEN\nNEXT STOP: %s\nUse the green subway doorway."), *Destination);
		Accent = FLinearColor(0.18f, 1.0f, 0.46f, 1.0f);
	}
	else
	{
		const FString TimerText = (BHGS && (BHGS->bPracticeMode || BHGS->bTestMode))
			? FString(BHGS->bTestMode ? TEXT("TEST LOOP") : TEXT("PRACTICE"))
			: FString::Printf(TEXT("T-%02d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60);
		DisplayBody = FString::Printf(
			TEXT("EXIT LOCKED  %s\nPOWER %d/%d  TASKS %d/%d\nNEXT STOP: %s"),
			*TimerText,
			BreakersCompleted,
			BreakersRequired,
			SideObjectivesCompleted,
			SideObjectivesRequired,
			*Destination);
		Accent = RemainingSeconds > 0 && RemainingSeconds <= 90
			? FLinearColor(1.0f, 0.24f, 0.12f, 1.0f)
			: FLinearColor(1.0f, 0.68f, 0.18f, 1.0f);
	}

	ApplyText();
}

void ABHTrainDisplayActor::ApplyText()
{
	const FString Header = BHWrapDisplayText(DisplayHeader.Left(72), 28, 2);
	const FString Body = BHWrapDisplayText(DisplayBody.Left(720), 52, 7);
	const FColor HeaderColor = Accent.ToFColor(true);
	const FColor BodyColor(226, 245, 232);

	if (HeaderText)
	{
		HeaderText->SetText(FText::FromString(Header));
		HeaderText->SetTextRenderColor(HeaderColor);
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(Body));
		BodyText->SetTextRenderColor(BodyColor);
	}
	if (HeaderTextBack)
	{
		HeaderTextBack->SetText(FText::FromString(Header));
		HeaderTextBack->SetTextRenderColor(HeaderColor);
	}
	if (BodyTextBack)
	{
		BodyTextBack->SetText(FText::FromString(Body));
		BodyTextBack->SetTextRenderColor(BodyColor);
	}
	if (BackingPanel)
	{
		BHApplyDisplayMeshColor(BackingPanel, FLinearColor(0.010f, 0.014f, 0.016f, 1.0f), Accent * 0.05f);
	}
	BHApplyDisplayMeshColor(ScreenFront, FLinearColor(0.006f, 0.010f, 0.012f, 1.0f), Accent * 0.10f);
	BHApplyDisplayMeshColor(ScreenBack, FLinearColor(0.006f, 0.010f, 0.012f, 1.0f), Accent * 0.10f);
	BHApplyDisplayMeshColor(HeaderRailFront, Accent * 0.72f, Accent * 0.30f);
	BHApplyDisplayMeshColor(HeaderRailBack, Accent * 0.72f, Accent * 0.30f);
	BHApplyDisplayMeshColor(FooterRailFront, Accent * 0.46f, Accent * 0.18f);
	BHApplyDisplayMeshColor(FooterRailBack, Accent * 0.46f, Accent * 0.18f);
	const FLinearColor FrameColor(0.040f, 0.048f, 0.050f, 1.0f);
	const FLinearColor HangerColor(0.090f, 0.096f, 0.092f, 1.0f);
	BHApplyDisplayMeshColor(TopFrame, FrameColor, Accent * 0.04f);
	BHApplyDisplayMeshColor(BottomFrame, FrameColor, Accent * 0.04f);
	BHApplyDisplayMeshColor(LeftFrame, FrameColor, Accent * 0.035f);
	BHApplyDisplayMeshColor(RightFrame, FrameColor, Accent * 0.035f);
	BHApplyDisplayMeshColor(LeftHanger, HangerColor, Accent * 0.025f);
	BHApplyDisplayMeshColor(RightHanger, HangerColor, Accent * 0.025f);
	if (FrontReadLight)
	{
		FrontReadLight->SetLightColor(Accent);
	}
	if (BackReadLight)
	{
		BackReadLight->SetLightColor(Accent);
	}
}

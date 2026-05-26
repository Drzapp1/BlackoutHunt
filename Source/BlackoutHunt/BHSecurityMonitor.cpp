#include "BHSecurityMonitor.h"
#include "BHCharacter.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "BHSecurityCamera.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

ABHSecurityMonitor::ABHSecurityMonitor()
{
	InteractionLabel = FText::FromString(TEXT("Check Security Feed"));
	MonitorRange = 7800.0f;
	bLiveFeedEnabled = true;
	LiveFeedMaterial = nullptr;

	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(RootComponent);
	ScanBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanBar"));
	ScanBar->SetupAttachment(RootComponent);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.92f, 0.20f, 0.62f), true);
	BHPropVisuals::ConfigurePart(Screen, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(49.0f, 0.0f, 9.0f), FRotator::ZeroRotator, FVector(0.020f, 0.25f, 0.34f));
	BHPropVisuals::ConfigurePart(ScanBar, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(51.0f, 0.0f, 9.0f), FRotator::ZeroRotator, FVector(0.012f, 0.23f, 0.025f));
	BHPropVisuals::TintPart(Screen, FLinearColor(0.03f, 0.42f, 0.46f, 1.0f), 1.8f);
	BHPropVisuals::TintPart(ScanBar, FLinearColor(0.88f, 0.96f, 0.92f, 1.0f), 2.6f);
}

void ABHSecurityMonitor::BeginPlay()
{
	Super::BeginPlay();

	ApplyStaticScreenVisual();
	if (GetNetMode() == NM_DedicatedServer || !bLiveFeedEnabled || !GetWorld())
	{
		return;
	}

	GetWorldTimerManager().SetTimerForNextTick(this, &ABHSecurityMonitor::RefreshLiveFeed);
	GetWorldTimerManager().SetTimer(LiveFeedRefreshTimerHandle, this, &ABHSecurityMonitor::RefreshLiveFeed, 2.0f, true, 1.0f);
}

void ABHSecurityMonitor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(LiveFeedRefreshTimerHandle);
	}

	ReleaseLiveFeedCamera();
	Super::EndPlay(EndPlayReason);
}

bool ABHSecurityMonitor::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	return BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHSecurityMonitor::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	ABHSecurityCamera* Camera = FindBestCameraFor(Character);
	if (Camera)
	{
		Camera->TriggerManualGlitch(Character);
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(TEXT("Security feed acquired. The image is not alone."), 2.8f);
		}
	}
	else if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(TEXT("No security camera signal in range."), 2.5f);
	}
}

FText ABHSecurityMonitor::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return FText::FromString(TEXT("Check Security Feed"));
}

ABHSecurityCamera* ABHSecurityMonitor::FindBestCameraFor(ABHCharacter* Character) const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	const FVector Origin = Character ? Character->GetActorLocation() : GetActorLocation();
	ABHSecurityCamera* Best = nullptr;
	float BestDistSq = FMath::Square(MonitorRange);
	for (TActorIterator<ABHSecurityCamera> It(GetWorld()); It; ++It)
	{
		ABHSecurityCamera* Camera = *It;
		if (!Camera || Camera->IsCameraDisabled())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Camera->GetActorLocation(), Origin);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Camera;
		}
	}
	return Best;
}

void ABHSecurityMonitor::RefreshLiveFeed()
{
	if (GetNetMode() == NM_DedicatedServer || !bLiveFeedEnabled || !Screen)
	{
		ReleaseLiveFeedCamera();
		return;
	}

	ABHSecurityCamera* Camera = FindBestCameraFor(nullptr);
	if (!Camera)
	{
		ReleaseLiveFeedCamera();
		ApplyStaticScreenVisual();
		return;
	}

	UTextureRenderTarget2D* FeedTarget = Camera->GetOrCreateLiveFeedTarget(ResolveLiveFeedResolution());
	if (!FeedTarget)
	{
		Camera->SetLiveFeedEnabled(false);
		ReleaseLiveFeedCamera();
		ApplyStaticScreenVisual();
		return;
	}

	if (LiveFeedCamera.Get() == Camera && LiveFeedMaterial && Screen->GetMaterial(0) == LiveFeedMaterial.Get())
	{
		LiveFeedMaterial->SetTextureParameterValue(TEXT("SlateUI"), FeedTarget);
		LiveFeedMaterial->SetTextureParameterValue(TEXT("Texture"), FeedTarget);
		Camera->SetLiveFeedEnabled(true);
		return;
	}

	if (LiveFeedCamera.IsValid() && LiveFeedCamera.Get() != Camera)
	{
		ReleaseLiveFeedCamera();
	}

	static UMaterialInterface* FeedBaseMaterial = nullptr;
	static bool bSearchedFeedMaterial = false;
	if (!bSearchedFeedMaterial)
	{
		static const TCHAR* FeedMaterialPaths[] = {
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked_OneSided.Widget3DPassThrough_Masked_OneSided")
		};

		for (const TCHAR* MaterialPath : FeedMaterialPaths)
		{
			FeedBaseMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
			if (FeedBaseMaterial)
			{
				break;
			}
		}
		bSearchedFeedMaterial = true;
	}

	if (!FeedBaseMaterial)
	{
		Camera->SetLiveFeedEnabled(false);
		ReleaseLiveFeedCamera();
		ApplyStaticScreenVisual();
		return;
	}

	LiveFeedMaterial = UMaterialInstanceDynamic::Create(FeedBaseMaterial, this);
	if (!LiveFeedMaterial)
	{
		Camera->SetLiveFeedEnabled(false);
		ReleaseLiveFeedCamera();
		ApplyStaticScreenVisual();
		return;
	}

	LiveFeedMaterial->SetTextureParameterValue(TEXT("SlateUI"), FeedTarget);
	LiveFeedMaterial->SetTextureParameterValue(TEXT("Texture"), FeedTarget);
	LiveFeedMaterial->SetTextureParameterValue(TEXT("VideoTexture"), FeedTarget);
	LiveFeedMaterial->SetTextureParameterValue(TEXT("CameraFeed"), FeedTarget);
	Screen->SetMaterial(0, LiveFeedMaterial);
	LiveFeedCamera = Camera;
	Camera->SetLiveFeedEnabled(true);

	BHPropVisuals::TintPart(ScanBar, FLinearColor(0.88f, 0.96f, 0.92f, 1.0f), 2.6f);
}

void ABHSecurityMonitor::ReleaseLiveFeedCamera()
{
	if (ABHSecurityCamera* Camera = LiveFeedCamera.Get())
	{
		Camera->SetLiveFeedEnabled(false);
	}
	LiveFeedCamera.Reset();
	LiveFeedMaterial = nullptr;
}

int32 ABHSecurityMonitor::ResolveLiveFeedResolution() const
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	int32 QualityLevel = Settings ? Settings->GetOverallScalabilityLevel() : 2;
	if (QualityLevel < 0 && Settings)
	{
		QualityLevel = Settings->GetTextureQuality();
	}

	switch (QualityLevel)
	{
	case 0:
		return 128;
	case 1:
		return 256;
	case 2:
		return 384;
	default:
		return 512;
	}
}

void ABHSecurityMonitor::ApplyStaticScreenVisual()
{
	if (Screen)
	{
		Screen->SetMaterial(0, BHPropVisuals::BasicMaterial());
		BHPropVisuals::TintPart(Screen, FLinearColor(0.03f, 0.42f, 0.46f, 1.0f), 1.8f);
	}
	if (ScanBar)
	{
		BHPropVisuals::TintPart(ScanBar, FLinearColor(0.88f, 0.96f, 0.92f, 1.0f), 2.6f);
	}
}

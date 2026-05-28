#include "BHSecurityMonitor.h"
#include "BHGameState.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "BHSecurityCamera.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameUserSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace
{
constexpr float BHHallMonitorFalseCCTVCooldownSeconds = 22.0f;
constexpr float BHHallMonitorFalseCCTVMarkerSeconds = 3.6f;
constexpr float BHHallMonitorFalseCCTVMemorySeconds = 75.0f;
constexpr float BHCCTVMonitorInspectionMaxDistance = 560.0f;

UStaticMesh* BHImportedSecurityComputerMesh()
{
	static UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SecurityCameras/Meshes/SM_Computer.SM_Computer"));
	return Mesh;
}

UMaterialInterface* BHImportedComputerMaterial()
{
	static UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SecurityCameras/Materials/Computer/M_Computer_Grey.M_Computer_Grey"));
	return Material;
}

UMaterialInterface* BHImportedScreenOffMaterial()
{
	static UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SecurityCameras/Materials/Computer/M_Computer_Screen_Off.M_Computer_Screen_Off"));
	return Material;
}

FString BHMonitorCompassFromDelta(const FVector& Delta)
{
	const FVector Flat(Delta.X, Delta.Y, 0.0f);
	if (Flat.IsNearlyZero())
	{
		return TEXT("nearby");
	}

	const float Angle = FMath::Atan2(Flat.Y, Flat.X);
	const float Degrees = FMath::UnwindDegrees(FMath::RadiansToDegrees(Angle));
	if (Degrees >= -22.5f && Degrees < 22.5f)
	{
		return TEXT("east");
	}
	if (Degrees >= 22.5f && Degrees < 67.5f)
	{
		return TEXT("north-east");
	}
	if (Degrees >= 67.5f && Degrees < 112.5f)
	{
		return TEXT("north");
	}
	if (Degrees >= 112.5f && Degrees < 157.5f)
	{
		return TEXT("north-west");
	}
	if (Degrees >= -67.5f && Degrees < -22.5f)
	{
		return TEXT("south-east");
	}
	if (Degrees >= -112.5f && Degrees < -67.5f)
	{
		return TEXT("south");
	}
	if (Degrees >= -157.5f && Degrees < -112.5f)
	{
		return TEXT("south-west");
	}
	return TEXT("west");
}
}

ABHSecurityMonitor::ABHSecurityMonitor()
{
	InteractionLabel = FText::FromString(TEXT("Check Security Feed"));
	MonitorRange = 7800.0f;
	bLiveFeedEnabled = true;
	LiveFeedMaterial = nullptr;
	LastFalsePingPruneTime = -999.0f;

	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(RootComponent);
	ScanBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScanBar"));
	ScanBar->SetupAttachment(RootComponent);

	if (UStaticMesh* ImportedComputerMesh = BHImportedSecurityComputerMesh())
	{
		BHPropVisuals::ConfigurePart(Mesh, ImportedComputerMesh, BHImportedComputerMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.72f), true);
	}
	else
	{
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.92f, 0.20f, 0.62f), true);
	}
	BHPropVisuals::ConfigurePart(Screen, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(49.0f, 0.0f, 9.0f), FRotator::ZeroRotator, FVector(0.020f, 0.25f, 0.34f));
	BHPropVisuals::ConfigurePart(ScanBar, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(51.0f, 0.0f, 9.0f), FRotator::ZeroRotator, FVector(0.012f, 0.23f, 0.025f));
	BHPropVisuals::TintPart(Screen, FLinearColor(0.03f, 0.42f, 0.46f, 1.0f), 1.8f);
	BHPropVisuals::TintPart(ScanBar, FLinearColor(0.88f, 0.96f, 0.92f, 1.0f), 2.6f);
}

void ABHSecurityMonitor::BeginPlay()
{
	Super::BeginPlay();

	ApplyStaticScreenVisual();
	if (HasAuthority() && GetWorld())
	{
		GetWorldTimerManager().SetTimer(ActiveInspectionPruneTimerHandle, this, &ABHSecurityMonitor::PruneActiveInspections, 1.0f, true, 1.0f);
	}

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
		GetWorldTimerManager().ClearTimer(ActiveInspectionPruneTimerHandle);
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}

	for (const TPair<TWeakObjectPtr<ABHCharacter>, TWeakObjectPtr<ABHSecurityCamera>>& ActiveInspection : ActiveInspectionCameras)
	{
		if (ABHSecurityCamera* Camera = ActiveInspection.Value.Get())
		{
			Camera->UnregisterActiveInspector(ActiveInspection.Key.Get());
		}
	}
	ActiveInspectionCameras.Reset();
	LastFalsePingTimes.Reset();
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

	ABHSecurityCamera* Camera = GetCurrentSignalCamera();
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	ABHPlayerController* InteractingPC = Cast<ABHPlayerController>(Character->GetController());
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		TrySendHallMonitorFalsePing(Character, Camera);
		return;
	}

	if (BHPS && BHPS->IsAliveHunter() && Camera)
	{
		if (TWeakObjectPtr<ABHSecurityCamera>* PreviousCamera = ActiveInspectionCameras.Find(Character))
		{
			if (ABHSecurityCamera* Previous = PreviousCamera->Get())
			{
				Previous->UnregisterActiveInspector(Character);
			}
		}
		Camera->RegisterActiveInspector(Character);
		ActiveInspectionCameras.Add(Character, Camera);
		if (InteractingPC)
		{
			InteractingPC->ClientShowStatusMessage(TEXT("Security feed armed. Hold to pulse camera sightings."), 2.4f);
		}
	}
	else if (Camera)
	{
		if (InteractingPC)
		{
			InteractingPC->ClientShowStatusMessage(TEXT("Security feed live. Watch the screen, but it will not mark players for you."), 2.8f);
		}
	}
	else if (InteractingPC)
	{
		InteractingPC->ClientShowStatusMessage(TEXT("No security camera signal in range."), 2.5f);
	}
}

void ABHSecurityMonitor::EndInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	if (TWeakObjectPtr<ABHSecurityCamera>* CameraPtr = ActiveInspectionCameras.Find(Character))
	{
		if (ABHSecurityCamera* Camera = CameraPtr->Get())
		{
			Camera->UnregisterActiveInspector(Character);
		}
		ActiveInspectionCameras.Remove(Character);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void ABHSecurityMonitor::DebugPruneActiveInspections()
{
	PruneActiveInspections();
}
#endif

FText ABHSecurityMonitor::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		return FText::FromString(TEXT("Spoof CCTV Motion"));
	}
	if (BHPS && BHPS->IsAliveHunter())
	{
		return FText::FromString(TEXT("Track Security Feed"));
	}
	return FText::FromString(TEXT("Check Security Feed"));
}

FBHInteractionPromptInfo ABHSecurityMonitor::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	const bool bAlive = CanInteract_Implementation(Character);
	const bool bHasSignal = GetCurrentSignalCamera() != nullptr;
	Info.bCanInteract = bAlive && bHasSignal;
	Info.HoldSeconds = 0.8f;
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		Info.RiskText = FText::FromString(TEXT("FALSE PING"));
	}
	else if (BHPS && BHPS->IsAliveHunter())
	{
		Info.RiskText = FText::FromString(TEXT("TRACKING"));
	}
	else
	{
		Info.RiskText = FText::FromString(TEXT("EXPOSED"));
	}
	Info.DisabledReason = bAlive ? FText::FromString(TEXT("NO SIGNAL")) : FText::FromString(TEXT("LOCKED"));
	Info.bDangerous = true;
	return Info;
}

void ABHSecurityMonitor::PruneActiveInspections()
{
	if (!HasAuthority())
	{
		return;
	}

	for (auto It = ActiveInspectionCameras.CreateIterator(); It; ++It)
	{
		ABHCharacter* Character = It.Key().Get();
		ABHSecurityCamera* Camera = It.Value().Get();
		if (IsActiveInspectionStillValid(Character, Camera))
		{
			continue;
		}

		if (Camera)
		{
			Camera->UnregisterActiveInspector(Character);
		}
		It.RemoveCurrent();
	}
}

bool ABHSecurityMonitor::IsActiveInspectionStillValid(const ABHCharacter* Character, const ABHSecurityCamera* Camera) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!Character || !Camera || Camera->IsCameraDisabled() || !BHPS || !BHPS->IsAliveHunter()
		|| !Cast<ABHPlayerController>(Character->GetController()))
	{
		return false;
	}

	return FVector::DistSquared(Character->GetActorLocation(), GetActorLocation()) <= FMath::Square(BHCCTVMonitorInspectionMaxDistance);
}

ABHSecurityCamera* ABHSecurityMonitor::GetCurrentSignalCamera() const
{
	if (ABHSecurityCamera* Camera = LiveFeedCamera.Get())
	{
		if (!Camera->IsCameraDisabled())
		{
			return Camera;
		}
	}

	return FindBestCameraFor(nullptr);
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
			TEXT("/Game/SecurityCameras/Materials/Special/M_Camera_Screen.M_Camera_Screen"),
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
		if (UMaterialInterface* ScreenOffMaterial = BHImportedScreenOffMaterial())
		{
			Screen->SetMaterial(0, ScreenOffMaterial);
		}
		else
		{
			Screen->SetMaterial(0, BHPropVisuals::BasicMaterial());
			BHPropVisuals::TintPart(Screen, FLinearColor(0.03f, 0.42f, 0.46f, 1.0f), 1.8f);
		}
	}
	if (ScanBar)
	{
		BHPropVisuals::TintPart(ScanBar, FLinearColor(0.88f, 0.96f, 0.92f, 1.0f), 2.6f);
	}
}

bool ABHSecurityMonitor::TrySendHallMonitorFalsePing(ABHCharacter* Character, ABHSecurityCamera* Camera)
{
	if (!HasAuthority() || !Character)
	{
		return false;
	}

	ABHPlayerController* InteractingPC = Cast<ABHPlayerController>(Character->GetController());
	ABHPlayerState* BHPS = Character->GetBHPlayerState();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHPS || BHPS->PlayerRole != EBHPlayerRole::FakeHunter || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}

	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		if (InteractingPC)
		{
			InteractingPC->ClientShowStatusMessage(TEXT("False CCTV pings unlock when the Hunt phase begins."), 2.5f);
		}
		return false;
	}

	if (!Camera || Camera->IsCameraDisabled())
	{
		if (InteractingPC)
		{
			InteractingPC->ClientShowStatusMessage(TEXT("No live CCTV signal to spoof."), 2.5f);
		}
		return false;
	}

	if (const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		FString MonitorBlockReason;
		if (!BHGM->CanUseHallMonitorTools(BHPS, MonitorBlockReason))
		{
			if (InteractingPC)
			{
				InteractingPC->ClientShowStatusMessage(MonitorBlockReason, 3.2f);
			}
			return false;
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	PruneFalsePingCooldowns(Now);
	if (const float* LastPingTime = LastFalsePingTimes.Find(Character))
	{
		const float Remaining = BHHallMonitorFalseCCTVCooldownSeconds - (Now - *LastPingTime);
		if (Remaining > 0.0f)
		{
			if (InteractingPC)
			{
				InteractingPC->ClientShowStatusMessage(FString::Printf(TEXT("False CCTV ping cooling down: %ds."), FMath::CeilToInt(Remaining)), 2.4f);
			}
			return false;
		}
	}

	LastFalsePingTimes.Add(Character, Now);
	const FVector FakeLocation = ResolveFalsePingLocation(Character, Camera);
	int32 HunterRecipients = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		const ABHPlayerState* TargetPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !TargetPS || !Pawn || !TargetPS->IsAliveHunter())
		{
			continue;
		}

		const FVector Delta = FakeLocation - Pawn->GetActorLocation();
		const float DistanceMeters = Delta.Size() / 100.0f;
		PC->ClientShowCCTVReveal(nullptr, FakeLocation + FVector(0.0f, 0.0f, 80.0f), TEXT("MOTION"), BHHallMonitorFalseCCTVMarkerSeconds);
		PC->ClientShowStatusMessage(FString::Printf(TEXT("CCTV motion: %s, %.0fm away. Verify before chase."), *BHMonitorCompassFromDelta(Delta), DistanceMeters), 3.0f);
		++HunterRecipients;
	}

	if (InteractingPC)
	{
		InteractingPC->ClientShowStatusMessage(HunterRecipients > 0
			? TEXT("False CCTV motion sent. It creates doubt, not a capture.")
			: TEXT("False CCTV ping armed, but no Teacher is watching."),
			3.0f);
	}
	return HunterRecipients > 0;
}

FVector ABHSecurityMonitor::ResolveFalsePingLocation(const ABHCharacter* Character, const ABHSecurityCamera* Camera) const
{
	if (!Camera)
	{
		return GetActorLocation();
	}

	FVector Forward = Camera->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	FVector Right = Camera->GetActorRightVector();
	Right.Z = 0.0f;
	Right = Right.GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const int32 PlayerSalt = BHPS ? BHPS->GetPlayerId() : 17;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FRandomStream Stream(FMath::RoundToInt(Now * 10.0f) + PlayerSalt * 113 + GetUniqueID() * 7);
	const float Range = Camera->GetDetectionRange();
	const float Distance = FMath::Clamp(Range * Stream.FRandRange(0.34f, 0.62f), 1050.0f, 3400.0f);
	const float ConeRadians = FMath::DegreesToRadians(Camera->GetDetectionConeDegrees() * 0.5f);
	const float MaxLateral = FMath::Clamp(FMath::Tan(ConeRadians) * Distance * 0.58f, 180.0f, 1200.0f);
	const float Lateral = Stream.FRandRange(-MaxLateral, MaxLateral);
	FVector Location = Camera->GetActorLocation() + Forward * Distance + Right * Lateral;
	Location.Z = 120.0f;
	return Location;
}

void ABHSecurityMonitor::PruneFalsePingCooldowns(float Now)
{
	if (Now - LastFalsePingPruneTime < 10.0f)
	{
		return;
	}

	LastFalsePingPruneTime = Now;
	for (auto It = LastFalsePingTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value() > BHHallMonitorFalseCCTVMemorySeconds)
		{
			It.RemoveCurrent();
		}
	}
}

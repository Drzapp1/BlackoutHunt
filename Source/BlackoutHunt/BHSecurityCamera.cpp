#include "BHSecurityCamera.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
constexpr float BHCCTVInspectorRevealCooldownSeconds = 7.5f;
constexpr float BHCCTVInspectorRevealSeconds = 5.0f;
constexpr float BHCCTVHunterRevealCooldownSeconds = 8.5f;
constexpr float BHCCTVHunterOutlineSeconds = 2.35f;
constexpr float BHCCTVSurvivorWarningCooldownSeconds = 5.0f;
constexpr float BHCCTVDetectionMemoryLifetimeSeconds = 45.0f;
const TCHAR* BHCCTVStaticSoundPath = TEXT("/Game/SecurityCameras/Sounds/Wav_Static_CCTV.Wav_Static_CCTV");

UStaticMesh* BHImportedSecurityCameraMesh()
{
	static UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SecurityCameras/Meshes/SM_Security_Camera.SM_Security_Camera"));
	return Mesh;
}

UMaterialInterface* BHImportedCameraMetalMaterial()
{
	static UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SecurityCameras/Materials/Camera/M_Camera_Metal.M_Camera_Metal"));
	return Material;
}

bool BHIsCameraLowProfileState(EBHMovementSpecialState State)
{
	return State == EBHMovementSpecialState::Prone
		|| State == EBHMovementSpecialState::Sliding
		|| State == EBHMovementSpecialState::Diving;
}

float BHCameraSightHeightForState(EBHMovementSpecialState State)
{
	if (State == EBHMovementSpecialState::Prone)
	{
		return 36.0f;
	}

	return BHIsCameraLowProfileState(State) ? 48.0f : 72.0f;
}

float BHCameraRangeMultiplierForState(EBHMovementSpecialState State)
{
	if (State == EBHMovementSpecialState::Prone)
	{
		return 0.56f;
	}

	return BHIsCameraLowProfileState(State) ? 0.72f : 1.0f;
}
}

ABHSecurityCamera::ABHSecurityCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.35f;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Lens = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lens"));
	Lens->SetupAttachment(SceneRoot);
	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(SceneRoot);
	FeedCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FeedCapture"));
	FeedCapture->SetupAttachment(SceneRoot);

	if (UStaticMesh* ImportedCameraMesh = BHImportedSecurityCameraMesh())
	{
		BHPropVisuals::ConfigurePart(Body, ImportedCameraMesh, BHImportedCameraMetalMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.82f), true);
		BHPropVisuals::ConfigurePart(Lens, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(46.0f, 0.0f, 4.0f), FRotator::ZeroRotator, FVector(0.075f));
	}
	else
	{
		BHPropVisuals::ConfigurePart(Body, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.52f, 0.28f, 0.22f), true);
		BHPropVisuals::ConfigurePart(Lens, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(42.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.14f, 0.14f, 0.06f));
	}
	StatusLight->SetRelativeLocation(FVector(48.0f, 0.0f, 22.0f));
	StatusLight->SetAttenuationRadius(450.0f);
	StatusLight->SetIntensity(900.0f);
	StatusLight->SetCastShadows(false);
	FeedCapture->SetRelativeLocation(FVector(54.0f, 0.0f, 0.0f));
	FeedCapture->FOVAngle = 44.0f;
	// Capture the fully post-processed, tonemapped LDR image with an opaque alpha. The
	// default SCS_SceneColorHDR writes linear HDR colour with a non-opaque alpha, which
	// renders black/transparent once it is sampled by the monitor's UI feed material.
	FeedCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	// CaptureScene() is also driven manually (and at a low tick rate), so persist the
	// rendering state between captures to avoid a black first frame on the feed.
	FeedCapture->bAlwaysPersistRenderingState = true;
	FeedCapture->bCaptureEveryFrame = false;
	FeedCapture->bCaptureOnMovement = false;
	FeedCapture->SetActive(false);

	bDisabled = false;
	bRoundOffline = false;
	bGlitching = false;
	CircuitId = 0;
	DetectionRange = 4200.0f;
	DetectionConeDegrees = 44.0f;
	LastDetectionTime = -999.0f;
	LastGlitchTime = -999.0f;
	LastZoneAlertTime = -999.0f;
	LastDetectionMemoryPruneTime = -999.0f;
	FeedRenderTarget = nullptr;
	if (FeedCapture)
	{
		FeedCapture->FOVAngle = DetectionConeDegrees;
		FeedCapture->MaxViewDistanceOverride = DetectionRange;
	}
}

void ABHSecurityCamera::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(HasAuthority());
	ApplyCameraVisuals();
}

void ABHSecurityCamera::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(GlitchTimerHandle);
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}

	ActiveInspectors.Reset();
	if (FeedCapture)
	{
		FeedCapture->TextureTarget = nullptr;
		FeedCapture->SetActive(false);
	}
	FeedRenderTarget = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ABHSecurityCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bDisabled || bGlitching)
	{
		return;
	}

	if (ABHCharacter* Survivor = FindBestVisibleSurvivor())
	{
		ProcessCameraDetection(Survivor, this, TEXT("CCTV camera detection"));
	}
}

void ABHSecurityCamera::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHSecurityCamera, bDisabled);
	DOREPLIFETIME(ABHSecurityCamera, bGlitching);
	DOREPLIFETIME(ABHSecurityCamera, CircuitId);
	DOREPLIFETIME(ABHSecurityCamera, DetectionRange);
	DOREPLIFETIME(ABHSecurityCamera, DetectionConeDegrees);
}

void ABHSecurityCamera::ConfigureCamera(float NewRange, float NewConeDegrees, int32 NewCircuitId)
{
	DetectionRange = FMath::Max(500.0f, NewRange);
	DetectionConeDegrees = FMath::Clamp(NewConeDegrees, 8.0f, 120.0f);
	CircuitId = NewCircuitId;
	if (FeedCapture)
	{
		FeedCapture->FOVAngle = DetectionConeDegrees;
		FeedCapture->MaxViewDistanceOverride = DetectionRange;
	}
}

void ABHSecurityCamera::SetCameraDisabled(bool bNewDisabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bDisabled = bNewDisabled || bRoundOffline;
	if (bDisabled)
	{
		GetWorldTimerManager().ClearTimer(GlitchTimerHandle);
		bGlitching = false;
	}
	ApplyCameraVisuals();
	ForceNetUpdate();
}

void ABHSecurityCamera::SetRoundOffline(bool bNewRoundOffline)
{
	if (!HasAuthority())
	{
		return;
	}

	bRoundOffline = bNewRoundOffline;
	SetCameraDisabled(false);
}

void ABHSecurityCamera::ResetCCTVState()
{
	if (!HasAuthority())
	{
		return;
	}

	LastDetectionTime = -999.0f;
	LastGlitchTime = -999.0f;
	LastZoneAlertTime = -999.0f;
	LastDetectionMemoryPruneTime = -999.0f;
	ActiveInspectors.Reset();
	LastHunterRevealTimes.Reset();
	LastInspectorRevealTimes.Reset();
	LastSurvivorWarningTimes.Reset();
	GetWorldTimerManager().ClearTimer(GlitchTimerHandle);
	bGlitching = false;
	ApplyCameraVisuals();
	ForceNetUpdate();
}

void ABHSecurityCamera::TriggerManualGlitch(ABHCharacter* InstigatorCharacter)
{
	if (!HasAuthority())
	{
		return;
	}

	TriggerGlitch(InstigatorCharacter, TEXT("manual CCTV ping"));
}

bool ABHSecurityCamera::IsCameraDisabled() const
{
	return bDisabled;
}

bool ABHSecurityCamera::IsRoundOffline() const
{
	return bRoundOffline;
}

bool ABHSecurityCamera::IsGlitching() const
{
	return bGlitching;
}

#if WITH_DEV_AUTOMATION_TESTS
FVector ABHSecurityCamera::DebugResolveHunterMotionMarkerLocation(const ABHCharacter* Survivor, float Now) const
{
	return ResolveHunterMotionMarkerLocation(Survivor, Now);
}
#endif

int32 ABHSecurityCamera::GetCircuitId() const
{
	return CircuitId;
}

float ABHSecurityCamera::GetDetectionRange() const
{
	return DetectionRange;
}

float ABHSecurityCamera::GetDetectionConeDegrees() const
{
	return DetectionConeDegrees;
}

bool ABHSecurityCamera::CanSeeSurvivor(const ABHCharacter* Survivor) const
{
	if (bDisabled || bGlitching || !IsValid(Survivor) || !GetWorld())
	{
		return false;
	}

	const ABHGameState* BHGS = GetWorld()->GetGameState<ABHGameState>();
	const ABHPlayerState* SurvivorPS = Survivor->GetPlayerState<ABHPlayerState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt || !SurvivorPS || !SurvivorPS->IsAliveSurvivor() || Survivor->IsHiddenInLocker())
	{
		return false;
	}

	const FVector CameraLocation = GetActorLocation();
	const FVector SurvivorLocation = Survivor->GetActorLocation();
	const EBHMovementSpecialState SurvivorMoveState = Survivor->GetMovementSpecialState();
	const FVector SurvivorSightLocation = SurvivorLocation + FVector(0.0f, 0.0f, BHCameraSightHeightForState(SurvivorMoveState));
	const FVector ToSurvivor = SurvivorSightLocation - CameraLocation;
	const float EffectiveDetectionRange = FMath::Max(900.0f, DetectionRange * BHCameraRangeMultiplierForState(SurvivorMoveState));
	if (ToSurvivor.SizeSquared() > FMath::Square(EffectiveDetectionRange))
	{
		return false;
	}

	const FVector Forward = GetActorForwardVector().GetSafeNormal();
	const float ConeDot = FMath::Cos(FMath::DegreesToRadians(DetectionConeDegrees * 0.5f));
	if (FVector::DotProduct(Forward, ToSurvivor.GetSafeNormal()) < ConeDot)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHSecurityCameraZoneSight), false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Survivor);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, SurvivorSightLocation, ECC_Visibility, Params);
}

ABHCharacter* ABHSecurityCamera::FindBestVisibleSurvivor() const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	ABHCharacter* BestSurvivor = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Candidate = *It;
		if (!CanSeeSurvivor(Candidate))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestSurvivor = Candidate;
		}
	}
	return BestSurvivor;
}

bool ABHSecurityCamera::ProcessCameraDetection(ABHCharacter* Survivor, AActor* SourceActor, const FString& AlertLabel)
{
	if (!HasAuthority() || !GetWorld() || !CanSeeSurvivor(Survivor))
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	LastDetectionTime = Now;
	PruneDetectionMemory(Now);
	if (ShouldWarnSurvivor(Survivor, Now))
	{
		if (ABHPlayerController* SurvivorPC = Cast<ABHPlayerController>(Survivor->GetController()))
		{
			const FString Warning = Survivor->IsProne()
				? TEXT("CCTV is searching. Stay low, break line of sight, or open the security circuit.")
				: TEXT("CCTV has a line on you. Break sight, go prone, or open the security circuit.");
			SurvivorPC->ClientShowStatusMessage(Warning, 3.1f);
		}
	}
	NotifyHunterCCTVReveal(Survivor, BHCCTVHunterOutlineSeconds, Now);
	NotifyActiveInspectors(Survivor, Now);

	if (Now - LastZoneAlertTime < 10.0f)
	{
		return false;
	}

	LastZoneAlertTime = Now;
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyCCTVDetection(this, SourceActor ? SourceActor : this, Survivor, AlertLabel.IsEmpty() ? TEXT("CCTV camera") : AlertLabel);
	}

	if (Now - LastGlitchTime > 12.0f)
	{
		TriggerGlitch(Survivor, TEXT("CCTV zone"));
	}

	return true;
}

bool ABHSecurityCamera::TryTriggerZoneAlert(ABHCharacter* Survivor, AActor* ZoneActor, const FString& AlertLabel)
{
	return ProcessCameraDetection(Survivor, ZoneActor, AlertLabel.IsEmpty() ? TEXT("CCTV zone") : AlertLabel);
}

void ABHSecurityCamera::RegisterActiveInspector(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	PruneActiveInspectors();
	ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController());
	if (!PC)
	{
		return;
	}

	ActiveInspectors.AddUnique(PC);
}

void ABHSecurityCamera::UnregisterActiveInspector(ABHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController());
	if (!PC)
	{
		PruneActiveInspectors();
		return;
	}

	ActiveInspectors.RemoveAll([PC](const TWeakObjectPtr<ABHPlayerController>& Candidate)
	{
		return !Candidate.IsValid() || Candidate.Get() == PC;
	});
}

int32 ABHSecurityCamera::GetActiveInspectorCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ABHPlayerController>& Candidate : ActiveInspectors)
	{
		if (Candidate.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

UTextureRenderTarget2D* ABHSecurityCamera::GetOrCreateLiveFeedTarget(int32 Resolution)
{
	if (GetNetMode() == NM_DedicatedServer || !FeedCapture)
	{
		return nullptr;
	}

	const int32 ClampedResolution = FMath::Clamp(Resolution, 64, 1024);
	if (!FeedRenderTarget || FeedRenderTarget->SizeX != ClampedResolution || FeedRenderTarget->SizeY != ClampedResolution)
	{
		FeedRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		if (!FeedRenderTarget)
		{
			return nullptr;
		}
		FeedRenderTarget->ClearColor = FLinearColor::Black;
		FeedRenderTarget->TargetGamma = 2.2f;
		FeedRenderTarget->InitAutoFormat(ClampedResolution, ClampedResolution);
		FeedRenderTarget->UpdateResourceImmediate(true);
		FeedCapture->TextureTarget = FeedRenderTarget;
	}

	SetLiveFeedEnabled(!bDisabled);
	return FeedRenderTarget;
}

void ABHSecurityCamera::SetLiveFeedEnabled(bool bEnabled)
{
	if (GetNetMode() == NM_DedicatedServer || !FeedCapture)
	{
		return;
	}

	const bool bShouldCapture = bEnabled && !bDisabled && FeedRenderTarget;
	FeedCapture->SetActive(bShouldCapture);
	FeedCapture->SetComponentTickEnabled(bShouldCapture);
	FeedCapture->bCaptureEveryFrame = bShouldCapture;
	FeedCapture->bCaptureOnMovement = bShouldCapture;
	if (bShouldCapture)
	{
		FeedCapture->CaptureScene();
	}
}

void ABHSecurityCamera::OnRep_CameraState()
{
	ApplyCameraVisuals();
}

void ABHSecurityCamera::TriggerGlitch(ABHCharacter* Target, const FString& Reason)
{
	if (!HasAuthority() || !GetWorld() || bDisabled)
	{
		return;
	}

	LastGlitchTime = GetWorld()->GetTimeSeconds();
	bGlitching = true;
	ApplyCameraVisuals();
	ForceNetUpdate();

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		FBHScareEventSpec Spec;
		Spec.EventType = EBHScareEventType::CCTVGlitch;
		Spec.Target = Target;
		Spec.Origin = GetActorLocation();
		Spec.Intensity = 0.72f;
		Spec.Message = Reason.Contains(TEXT("manual")) ? TEXT("The monitor snaps to a corrupted feed.") : TEXT("A camera found you.");
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		Spec.AudioAsset = Settings && !Settings->CCTVStaticSound.IsNull()
			? Settings->CCTVStaticSound
			: TSoftObjectPtr<USoundBase>(FSoftObjectPath(BHCCTVStaticSoundPath));
		BHGM->TriggerAtmosphereCue(Spec);
	}

	FTimerDelegate ClearDelegate;
	TWeakObjectPtr<ABHSecurityCamera> WeakThis(this);
	ClearDelegate.BindLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->bGlitching = false;
			WeakThis->ApplyCameraVisuals();
			WeakThis->ForceNetUpdate();
		}
	});
	GetWorldTimerManager().SetTimer(GlitchTimerHandle, ClearDelegate, 2.25f, false);
}

bool ABHSecurityCamera::NotifyHunterCCTVReveal(ABHCharacter* Survivor, float DurationSeconds, float Now)
{
	if (!HasAuthority() || !Survivor || !GetWorld())
	{
		return false;
	}

	if (!ShouldPulseHunterReveal(Survivor, Now))
	{
		return false;
	}

	const FVector RevealLocation = ResolveHunterMotionMarkerLocation(Survivor, Now);
	bool bSentReveal = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		const ABHPlayerState* HunterPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!PC || !HunterPS || !HunterPS->IsAliveHunter())
		{
			continue;
		}

		PC->ClientShowCCTVReveal(nullptr, RevealLocation, TEXT("MOTION"), DurationSeconds);
		bSentReveal = true;
	}
	return bSentReveal;
}

bool ABHSecurityCamera::NotifyActiveInspectors(ABHCharacter* Survivor, float Now)
{
	if (!HasAuthority() || !Survivor)
	{
		return false;
	}

	PruneActiveInspectors();
	if (ActiveInspectors.IsEmpty() || !ShouldPulseInspectorReveal(Survivor, Now))
	{
		return false;
	}

	const FVector RevealLocation = Survivor->GetActorLocation() + FVector(0.0f, 0.0f, 88.0f);
	const FString RevealName = Survivor->GetPlayerState() ? Survivor->GetPlayerState()->GetPlayerName() : TEXT("Survivor");
	bool bSentReveal = false;
	for (const TWeakObjectPtr<ABHPlayerController>& Inspector : ActiveInspectors)
	{
		ABHPlayerController* PC = Inspector.Get();
		const ABHPlayerState* InspectorPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!PC || !InspectorPS || !InspectorPS->IsAliveHunter())
		{
			continue;
		}

		PC->ClientShowCCTVReveal(Survivor, RevealLocation, RevealName.IsEmpty() ? TEXT("Survivor") : RevealName, BHCCTVInspectorRevealSeconds);
		if (const UBHGameSettings* Settings = GetDefault<UBHGameSettings>())
		{
			FBHGameplayAudioCue Cue;
			Cue.AudioAsset = Settings->CCTVStaticSound.IsNull()
				? TSoftObjectPtr<USoundBase>(FSoftObjectPath(BHCCTVStaticSoundPath))
				: Settings->CCTVStaticSound;
			Cue.Location = GetActorLocation();
			Cue.Caption = TEXT("CCTV feed static: movement acquired.");
			Cue.Volume = 0.34f;
			Cue.Pitch = 1.04f;
			Cue.CaptionSeconds = 1.4f;
			Cue.bSpatial = false;
			PC->ClientPlayGameplayAudioCue(Cue);
		}
		bSentReveal = true;
	}
	return bSentReveal;
}

bool ABHSecurityCamera::ShouldPulseHunterReveal(ABHCharacter* Survivor, float Now)
{
	if (!Survivor)
	{
		return false;
	}

	if (const float* LastReveal = LastHunterRevealTimes.Find(Survivor))
	{
		if (Now - *LastReveal < BHCCTVHunterRevealCooldownSeconds)
		{
			return false;
		}
	}

	LastHunterRevealTimes.Add(Survivor, Now);
	return true;
}

bool ABHSecurityCamera::ShouldPulseInspectorReveal(ABHCharacter* Survivor, float Now)
{
	if (!Survivor)
	{
		return false;
	}

	if (const float* LastReveal = LastInspectorRevealTimes.Find(Survivor))
	{
		if (Now - *LastReveal < BHCCTVInspectorRevealCooldownSeconds)
		{
			return false;
		}
	}

	LastInspectorRevealTimes.Add(Survivor, Now);
	return true;
}

bool ABHSecurityCamera::ShouldWarnSurvivor(ABHCharacter* Survivor, float Now)
{
	if (!Survivor)
	{
		return false;
	}

	if (const float* LastWarning = LastSurvivorWarningTimes.Find(Survivor))
	{
		if (Now - *LastWarning < BHCCTVSurvivorWarningCooldownSeconds)
		{
			return false;
		}
	}

	LastSurvivorWarningTimes.Add(Survivor, Now);
	return true;
}

FVector ABHSecurityCamera::ResolveHunterMotionMarkerLocation(const ABHCharacter* Survivor, float Now) const
{
	if (!Survivor)
	{
		return GetActorLocation();
	}

	const ABHPlayerState* SurvivorPS = Survivor->GetPlayerState<ABHPlayerState>();
	const int32 PlayerSalt = SurvivorPS ? SurvivorPS->GetPlayerId() : 23;
	const int32 TimeBucket = FMath::FloorToInt(Now / FMath::Max(1.0f, BHCCTVHunterRevealCooldownSeconds));
	FRandomStream Stream(GetUniqueID() * 37 + PlayerSalt * 113 + TimeBucket * 1009);

	const float Radius = Stream.FRandRange(180.0f, 420.0f);
	const float Angle = Stream.FRandRange(-PI, PI);
	const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
	return Survivor->GetActorLocation() + Offset + FVector(0.0f, 0.0f, 88.0f);
}

void ABHSecurityCamera::PruneDetectionMemory(float Now)
{
	if (Now - LastDetectionMemoryPruneTime < 8.0f)
	{
		return;
	}

	LastDetectionMemoryPruneTime = Now;
	for (auto It = LastHunterRevealTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value() > BHCCTVDetectionMemoryLifetimeSeconds)
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = LastInspectorRevealTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value() > BHCCTVDetectionMemoryLifetimeSeconds)
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = LastSurvivorWarningTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value() > BHCCTVDetectionMemoryLifetimeSeconds)
		{
			It.RemoveCurrent();
		}
	}
}

void ABHSecurityCamera::PruneActiveInspectors()
{
	ActiveInspectors.RemoveAll([](const TWeakObjectPtr<ABHPlayerController>& Candidate)
	{
		const ABHPlayerController* PC = Candidate.Get();
		const ABHPlayerState* InspectorPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		return !PC || !InspectorPS || !InspectorPS->IsAliveHunter();
	});
}

void ABHSecurityCamera::ApplyCameraVisuals()
{
	const FLinearColor BodyColor = bDisabled ? FLinearColor(0.05f, 0.05f, 0.05f, 1.0f) : FLinearColor(0.13f, 0.15f, 0.16f, 1.0f);
	const FLinearColor LensColor = bGlitching ? FLinearColor(1.0f, 0.08f, 0.04f, 1.0f) : (bDisabled ? FLinearColor(0.03f, 0.03f, 0.03f, 1.0f) : FLinearColor(0.08f, 0.86f, 0.92f, 1.0f));
	BHPropVisuals::TintPart(Body, BodyColor);
	BHPropVisuals::TintPart(Lens, LensColor, bDisabled ? 0.0f : (bGlitching ? 5.0f : 2.2f));
	if (StatusLight)
	{
		StatusLight->SetVisibility(!bDisabled);
		StatusLight->SetLightColor(LensColor);
		StatusLight->SetIntensity(bGlitching ? 2600.0f : 850.0f);
	}
	SetLiveFeedEnabled(!bDisabled && FeedRenderTarget != nullptr);
}

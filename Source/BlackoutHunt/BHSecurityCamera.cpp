#include "BHSecurityCamera.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABHSecurityCamera::ABHSecurityCamera()
{
	PrimaryActorTick.bCanEverTick = false;
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

	BHPropVisuals::ConfigurePart(Body, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.52f, 0.28f, 0.22f), true);
	BHPropVisuals::ConfigurePart(Lens, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(42.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.14f, 0.14f, 0.06f));
	StatusLight->SetRelativeLocation(FVector(48.0f, 0.0f, 22.0f));
	StatusLight->SetAttenuationRadius(450.0f);
	StatusLight->SetIntensity(900.0f);
	StatusLight->SetCastShadows(false);
	FeedCapture->SetRelativeLocation(FVector(54.0f, 0.0f, 0.0f));
	FeedCapture->FOVAngle = 44.0f;
	FeedCapture->bCaptureEveryFrame = false;
	FeedCapture->bCaptureOnMovement = false;
	FeedCapture->SetActive(false);

	bDisabled = false;
	bGlitching = false;
	CircuitId = 0;
	DetectionRange = 4200.0f;
	DetectionConeDegrees = 44.0f;
	LastDetectionTime = -999.0f;
	LastGlitchTime = -999.0f;
	LastZoneAlertTime = -999.0f;
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
	ApplyCameraVisuals();
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

	bDisabled = bNewDisabled;
	if (bDisabled)
	{
		GetWorldTimerManager().ClearTimer(GlitchTimerHandle);
		bGlitching = false;
	}
	ApplyCameraVisuals();
	ForceNetUpdate();
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

bool ABHSecurityCamera::IsGlitching() const
{
	return bGlitching;
}

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
	const FVector SurvivorSightLocation = SurvivorLocation + FVector(0.0f, 0.0f, 72.0f);
	const FVector ToSurvivor = SurvivorSightLocation - CameraLocation;
	if (ToSurvivor.SizeSquared() > FMath::Square(DetectionRange))
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

bool ABHSecurityCamera::TryTriggerZoneAlert(ABHCharacter* Survivor, AActor* ZoneActor, const FString& AlertLabel)
{
	if (!HasAuthority() || !GetWorld() || !CanSeeSurvivor(Survivor))
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastZoneAlertTime < 10.0f)
	{
		return false;
	}

	LastZoneAlertTime = Now;
	LastDetectionTime = Now;
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyCCTVDetection(this, ZoneActor, Survivor, AlertLabel.IsEmpty() ? TEXT("CCTV zone") : AlertLabel);
	}

	if (Now - LastGlitchTime > 12.0f)
	{
		TriggerGlitch(Survivor, TEXT("CCTV zone"));
	}

	return true;
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

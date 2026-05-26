#include "BHCCTVZone.h"

#include "BHCharacter.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "BHSecurityCamera.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ABHCCTVZone::ABHCCTVZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.35f;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(3.0f);
	SetMinNetUpdateFrequency(0.5f);

	ZoneTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneTrigger"));
	SetRootComponent(ZoneTrigger);
	ZoneTrigger->InitBoxExtent(FVector(1200.0f, 650.0f, 120.0f));
	ZoneTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	ZoneTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneTrigger->SetGenerateOverlapEvents(true);
	ZoneTrigger->SetCanEverAffectNavigation(false);

	ZonePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZonePlate"));
	ZonePlate->SetupAttachment(ZoneTrigger);
	WarningStripeA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeA"));
	WarningStripeA->SetupAttachment(ZoneTrigger);
	WarningStripeB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeB"));
	WarningStripeB->SetupAttachment(ZoneTrigger);
	WarningStripeC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeC"));
	WarningStripeC->SetupAttachment(ZoneTrigger);
	WarningStripeD = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningStripeD"));
	WarningStripeD->SetupAttachment(ZoneTrigger);

	LinkedCamera = nullptr;
	bZoneEnabled = true;
	bZoneVisible = true;
	CircuitId = 0;
	AlertLabel = TEXT("CCTV zone");

	RefreshMarkerScale(ZoneTrigger->GetUnscaledBoxExtent());
	ApplyZoneVisuals();
}

void ABHCCTVZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && ZoneTrigger)
	{
		ZoneTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABHCCTVZone::OnZoneBeginOverlap);
	}

	SetActorTickEnabled(HasAuthority());
	ApplyZoneVisuals();
}

void ABHCCTVZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bZoneEnabled || !ZoneTrigger)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	ZoneTrigger->GetOverlappingActors(OverlappingActors, ABHCharacter::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryAlertForActor(OverlappingActor);
	}
}

void ABHCCTVZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABHCCTVZone, LinkedCamera);
	DOREPLIFETIME(ABHCCTVZone, bZoneEnabled);
	DOREPLIFETIME(ABHCCTVZone, bZoneVisible);
	DOREPLIFETIME(ABHCCTVZone, CircuitId);
	DOREPLIFETIME(ABHCCTVZone, AlertLabel);
}

void ABHCCTVZone::ConfigureZone(ABHSecurityCamera* NewCamera, int32 NewCircuitId, const FString& NewAlertLabel, const FVector& NewBoxExtent, bool bNewVisible)
{
	LinkedCamera = NewCamera;
	CircuitId = NewCircuitId;
	AlertLabel = NewAlertLabel.IsEmpty() ? TEXT("CCTV zone") : NewAlertLabel;
	bZoneVisible = bNewVisible;

	const FVector SafeExtent(
		FMath::Max(240.0f, NewBoxExtent.X),
		FMath::Max(180.0f, NewBoxExtent.Y),
		FMath::Max(80.0f, NewBoxExtent.Z));
	if (ZoneTrigger)
	{
		ZoneTrigger->SetBoxExtent(SafeExtent);
	}
	RefreshMarkerScale(SafeExtent);
	ApplyZoneVisuals();
	ForceNetUpdate();
}

void ABHCCTVZone::SetZoneEnabled(bool bNewEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bZoneEnabled = bNewEnabled;
	if (ZoneTrigger)
	{
		ZoneTrigger->SetGenerateOverlapEvents(bZoneEnabled);
		ZoneTrigger->SetCollisionEnabled(bZoneEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	ApplyZoneVisuals();
	ForceNetUpdate();
}

void ABHCCTVZone::SetZoneVisible(bool bNewVisible)
{
	if (!HasAuthority())
	{
		return;
	}

	bZoneVisible = bNewVisible;
	ApplyZoneVisuals();
	ForceNetUpdate();
}

int32 ABHCCTVZone::GetCircuitId() const
{
	return CircuitId;
}

bool ABHCCTVZone::IsZoneEnabled() const
{
	return bZoneEnabled;
}

void ABHCCTVZone::OnRep_ZoneState()
{
	ApplyZoneVisuals();
}

void ABHCCTVZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryAlertForActor(OtherActor);
}

void ABHCCTVZone::TryAlertForActor(AActor* OtherActor)
{
	if (!HasAuthority() || !bZoneEnabled || !LinkedCamera)
	{
		return;
	}

	ABHCharacter* Survivor = Cast<ABHCharacter>(OtherActor);
	const ABHPlayerState* SurvivorPS = Survivor ? Survivor->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!Survivor || !SurvivorPS || !SurvivorPS->IsAliveSurvivor() || Survivor->IsHiddenInLocker())
	{
		return;
	}

	LinkedCamera->TryTriggerZoneAlert(Survivor, this, AlertLabel);
}

void ABHCCTVZone::ApplyZoneVisuals()
{
	if (ZoneTrigger)
	{
		ZoneTrigger->SetGenerateOverlapEvents(bZoneEnabled);
		ZoneTrigger->SetCollisionEnabled(bZoneEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	const bool bShowMarkers = bZoneVisible;
	const FLinearColor PlateColor = bZoneEnabled
		? FLinearColor(0.03f, 0.42f, 0.48f, 0.72f)
		: FLinearColor(0.05f, 0.055f, 0.06f, 0.45f);
	const FLinearColor StripeColor = bZoneEnabled
		? FLinearColor(1.0f, 0.78f, 0.18f, 1.0f)
		: FLinearColor(0.18f, 0.18f, 0.16f, 1.0f);
	const float PlateGlow = bZoneEnabled ? 0.55f : 0.0f;
	const float StripeGlow = bZoneEnabled ? 1.2f : 0.0f;

	BHPropVisuals::SetPartVisible(ZonePlate, bShowMarkers);
	BHPropVisuals::SetPartVisible(WarningStripeA, bShowMarkers);
	BHPropVisuals::SetPartVisible(WarningStripeB, bShowMarkers);
	BHPropVisuals::SetPartVisible(WarningStripeC, bShowMarkers);
	BHPropVisuals::SetPartVisible(WarningStripeD, bShowMarkers);

	BHPropVisuals::TintPart(ZonePlate, PlateColor, PlateGlow);
	BHPropVisuals::TintPart(WarningStripeA, StripeColor, StripeGlow);
	BHPropVisuals::TintPart(WarningStripeB, StripeColor, StripeGlow);
	BHPropVisuals::TintPart(WarningStripeC, StripeColor, StripeGlow);
	BHPropVisuals::TintPart(WarningStripeD, StripeColor, StripeGlow);
}

void ABHCCTVZone::RefreshMarkerScale(const FVector& BoxExtent)
{
	const float MarkerZ = -BoxExtent.Z + 3.0f;
	const float PlateScaleX = FMath::Max(0.08f, BoxExtent.X / 50.0f);
	const float PlateScaleY = FMath::Max(0.08f, BoxExtent.Y / 50.0f);
	const float StripeScaleX = FMath::Max(0.08f, BoxExtent.X / 50.0f);
	const float StripeScaleY = FMath::Max(0.08f, BoxExtent.Y / 50.0f);
	const float StripeWidth = 0.055f;
	const float StripeInset = 28.0f;

	BHPropVisuals::ConfigurePart(ZonePlate, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, MarkerZ), FRotator::ZeroRotator, FVector(PlateScaleX, PlateScaleY, 0.018f), false);
	BHPropVisuals::ConfigurePart(WarningStripeA, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, BoxExtent.Y - StripeInset, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeScaleX, StripeWidth, 0.022f), false);
	BHPropVisuals::ConfigurePart(WarningStripeB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -BoxExtent.Y + StripeInset, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeScaleX, StripeWidth, 0.022f), false);
	BHPropVisuals::ConfigurePart(WarningStripeC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(BoxExtent.X - StripeInset, 0.0f, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeWidth, StripeScaleY, 0.022f), false);
	BHPropVisuals::ConfigurePart(WarningStripeD, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(-BoxExtent.X + StripeInset, 0.0f, MarkerZ + 2.0f), FRotator::ZeroRotator, FVector(StripeWidth, StripeScaleY, 0.022f), false);
}

#include "BHNoiseDecoy.h"
#include "BHPropVisuals.h"
#include "BHSynthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ABHNoiseDecoy::ABHNoiseDecoy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(12.0f);
	SetMinNetUpdateFrequency(4.0f);
	InitialLifeSpan = 8.0f;
	AgeSeconds = 0.0f;

	NoiseRadius = CreateDefaultSubobject<USphereComponent>(TEXT("NoiseRadius"));
	SetRootComponent(NoiseRadius);
	NoiseRadius->InitSphereRadius(300.0f);
	NoiseRadius->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NoiseRadius->SetCollisionResponseToAllChannels(ECR_Ignore);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(NoiseRadius);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpeakerFace = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpeakerFace"));
	SpeakerFace->SetupAttachment(NoiseRadius);
	Antenna = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Antenna"));
	Antenna->SetupAttachment(NoiseRadius);
	SignalLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignalLight"));
	SignalLight->SetupAttachment(NoiseRadius);

	Synth = CreateDefaultSubobject<UBHSynthComponent>(TEXT("Synth"));
	Synth->SetupAttachment(NoiseRadius);
	Synth->Configure(420.0f, 0.26f, 0.06f, 3.5f);

	BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, -24.0f), FRotator::ZeroRotator, FVector(0.30f, 0.20f, 0.22f));
	BHPropVisuals::ConfigurePart(SpeakerFace, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(19.0f, 0.0f, -22.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.105f, 0.105f, 0.018f));
	BHPropVisuals::ConfigurePart(Antenna, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(-6.0f, 0.0f, 10.0f), FRotator(18.0f, 0.0f, 0.0f), FVector(0.018f, 0.018f, 0.45f));
	BHPropVisuals::ConfigurePart(SignalLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(10.0f, -13.0f, -7.0f), FRotator::ZeroRotator, FVector(0.045f));
	BHPropVisuals::TintPart(SpeakerFace, FLinearColor(0.018f, 0.020f, 0.022f, 1.0f));
	BHPropVisuals::TintPart(Antenna, FLinearColor(0.68f, 0.66f, 0.56f, 1.0f));
	BHPropVisuals::TintPart(SignalLight, FLinearColor(0.22f, 0.85f, 1.0f, 1.0f), 2.4f);
}

void ABHNoiseDecoy::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
	}

	if (Synth && GetNetMode() != NM_DedicatedServer)
	{
		Synth->Start();
	}
}

void ABHNoiseDecoy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AgeSeconds += DeltaSeconds;

	const float Pulse = 1.0f + FMath::Sin(AgeSeconds * 8.0f) * 0.25f;
	if (SpeakerFace)
	{
		SpeakerFace->SetRelativeScale3D(FVector(0.105f * Pulse, 0.105f * Pulse, 0.018f));
	}
	if (SignalLight)
	{
		SignalLight->SetRelativeScale3D(FVector(0.045f * Pulse));
		BHPropVisuals::TintPart(SignalLight, FLinearColor(0.22f, 0.85f, 1.0f, 1.0f), 1.4f + Pulse);
	}
}

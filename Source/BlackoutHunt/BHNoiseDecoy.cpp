// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHNoiseDecoy.h"
#include "BHGameMode.h"
#include "BHPropVisuals.h"
#include "BHSynthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABHNoiseDecoy::ABHNoiseDecoy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(12.0f);
	SetMinNetUpdateFrequency(4.0f);
	InitialLifeSpan = 9.0f;
	AgeSeconds = 0.0f;
	NoisePingIndex = 0;

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
	// Footsteps at a brisk ~1.9 steps/sec plus a breath bed, so a thrown decoy sounds like a fleeing student
	// rather than the old electronic ping that instantly read as fake.
	Synth->ConfigureDecoyFootsteps(1.9f, 0.5f);

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

	// The authority drives the lure: emit a believable survivor noise immediately, then keep pinging at a
	// brisk footstep cadence for the decoy's whole life so the Teacher keeps closing on it. The alternating
	// step pitch and the breath bed in the synth keep it from reading as a metronome.
	if (HasAuthority())
	{
		EmitDecoyNoise();
		GetWorldTimerManager().SetTimer(NoiseTimerHandle, this, &ABHNoiseDecoy::EmitDecoyNoise, 1.9f, true, 1.9f);
	}
}

void ABHNoiseDecoy::EmitDecoyNoise()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>();
	if (!BHGM)
	{
		return;
	}

	// Realistic survivor noises only -- never the word "decoy". To the Teacher and bots this is just another
	// student moving around.
	static const TCHAR* Reasons[] = {
		TEXT("hurried footstep"),
		TEXT("running footstep"),
		TEXT("careful footstep"),
		TEXT("panicked breathing")
	};
	const int32 Count = UE_ARRAY_COUNT(Reasons);
	const int32 Index = NoisePingIndex++;
	const FString Reason = Reasons[Index % Count];
	const FVector Location = GetActorLocation();

	// First ping = the loud lure: it drives the Teacher's on-screen "Noise: ..." alert and a single presence
	// nudge, exactly like a real survivor being heard. Every later ping uses the lighter footstep stimulus so
	// bots keep tracking and the atmosphere keeps ticking, but we never re-spike presence over and over (a real
	// runner's footsteps don't each raise a full alarm). The decoy's own spatial footstep/breath audio carries
	// the lure for human Teachers in between.
	if (Index == 0)
	{
		BHGM->NotifyLoudNoise(Location, Reason);
	}
	else
	{
		BHGM->ReportBotStimulus(EBHBotStimulusType::Noise, Location, this, this, Reason, 0.92f);
		BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Footstep, Location, this, this, 0.7f, Reason);
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

void ABHNoiseDecoy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Defensive: actor destruction auto-clears actor-bound timers, but clear explicitly so an early
	// EndPlay (level transition / manual destroy before the lifespan elapses) can never leave the
	// repeating noise timer pinging the perception system.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoiseTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

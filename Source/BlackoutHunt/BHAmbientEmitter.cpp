#include "BHAmbientEmitter.h"
#include "BHSynthComponent.h"
#include "Net/UnrealNetwork.h"

ABHAmbientEmitter::ABHAmbientEmitter()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);
	SetMinNetUpdateFrequency(0.25f);

	Synth = CreateDefaultSubobject<UBHSynthComponent>(TEXT("Synth"));
	SetRootComponent(Synth);
	ConfigFrequency = 55.0f;
	ConfigVolume = 0.10f;
	ConfigNoise = 0.02f;
	ConfigPulse = 0.22f;
}

void ABHAmbientEmitter::BeginPlay()
{
	Super::BeginPlay();

	if (Synth && GetNetMode() != NM_DedicatedServer)
	{
		ApplyAudioConfig();
		Synth->Start();
	}
}

void ABHAmbientEmitter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHAmbientEmitter, ConfigFrequency);
	DOREPLIFETIME(ABHAmbientEmitter, ConfigVolume);
	DOREPLIFETIME(ABHAmbientEmitter, ConfigNoise);
	DOREPLIFETIME(ABHAmbientEmitter, ConfigPulse);
}

void ABHAmbientEmitter::Configure(float Frequency, float Volume, float Noise, float Pulse)
{
	ConfigFrequency = Frequency;
	ConfigVolume = Volume;
	ConfigNoise = Noise;
	ConfigPulse = Pulse;
	ApplyAudioConfig();
}

void ABHAmbientEmitter::OnRep_AudioConfig()
{
	ApplyAudioConfig();
}

void ABHAmbientEmitter::ApplyAudioConfig()
{
	if (Synth)
	{
		Synth->Configure(ConfigFrequency, ConfigVolume, ConfigNoise, ConfigPulse);
	}
}

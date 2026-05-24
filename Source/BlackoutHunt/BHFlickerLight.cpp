#include "BHFlickerLight.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"

ABHFlickerLight::ABHFlickerLight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	SetReplicateMovement(false);
	NetUpdateFrequency = 2.0f;
	MinNetUpdateFrequency = 0.5f;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	SetRootComponent(Light);
	Light->SetAttenuationRadius(900.0f);
	Light->SetLightColor(FLinearColor(0.75f, 0.88f, 1.0f));
	Light->SetCastShadows(true);

	BaseIntensity = 1100.0f;
	FlickerAmount = 650.0f;
	FlickerSpeed = 5.0f;
	LightColor = FLinearColor(0.75f, 0.88f, 1.0f);
	LightRadius = 900.0f;
	CircuitId = 0;
	bPowered = true;
	Phase = FMath::FRandRange(0.0f, 100.0f);
}

void ABHFlickerLight::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
	}
}

void ABHFlickerLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Light)
	{
		return;
	}

	if (!bPowered)
	{
		Light->SetIntensity(0.0f);
		return;
	}

	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Noise = FMath::Sin(Time * FlickerSpeed + Phase) * 0.5f + FMath::Sin(Time * 17.0f + Phase * 0.37f) * 0.5f;
	Light->SetIntensity(FMath::Max(80.0f, BaseIntensity + Noise * FlickerAmount));
}

void ABHFlickerLight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHFlickerLight, CircuitId);
	DOREPLIFETIME(ABHFlickerLight, bPowered);
	DOREPLIFETIME(ABHFlickerLight, BaseIntensity);
	DOREPLIFETIME(ABHFlickerLight, LightColor);
	DOREPLIFETIME(ABHFlickerLight, LightRadius);
}

void ABHFlickerLight::Configure(int32 NewCircuitId, const FLinearColor& NewColor, float NewBaseIntensity, float NewRadius)
{
	CircuitId = NewCircuitId;
	BaseIntensity = NewBaseIntensity;
	LightColor = NewColor;
	LightRadius = NewRadius;
	ApplyLightConfig();
	ApplyLightState();
}

void ABHFlickerLight::SetPowered(bool bNewPowered)
{
	bPowered = bNewPowered;
	ApplyLightState();
}

int32 ABHFlickerLight::GetCircuitId() const
{
	return CircuitId;
}

bool ABHFlickerLight::IsPowered() const
{
	return bPowered;
}

void ABHFlickerLight::OnRep_Powered()
{
	ApplyLightState();
}

void ABHFlickerLight::OnRep_LightConfig()
{
	ApplyLightConfig();
}

void ABHFlickerLight::ApplyLightState()
{
	if (Light)
	{
		Light->SetVisibility(bPowered);
		if (!bPowered)
		{
			Light->SetIntensity(0.0f);
		}
	}
}

void ABHFlickerLight::ApplyLightConfig()
{
	if (Light)
	{
		Light->SetLightColor(LightColor);
		Light->SetAttenuationRadius(LightRadius);
	}
}

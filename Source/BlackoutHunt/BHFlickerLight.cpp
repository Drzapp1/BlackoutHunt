#include "BHFlickerLight.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

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
	bFlickerBurstActive = false;
	FlickerBurstEndTime = -1.0f;
	FlickerBurstIntensityMultiplier = 1.0f;
	FlickerBurstSpeed = 10.0f;
	FlickerBurstColor = FLinearColor::White;
	bFlickerBurstForcePowered = true;
	Phase = FMath::FRandRange(0.0f, 100.0f);
	FlickerBurstRemainingSeconds = -1.0f;
}

void ABHFlickerLight::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
	}
}

void ABHFlickerLight::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FlickerBurstTimerHandle);
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ABHFlickerLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Light)
	{
		return;
	}

	if (bFlickerBurstActive && FlickerBurstRemainingSeconds > 0.0f)
	{
		FlickerBurstRemainingSeconds -= DeltaSeconds;
	}

	const bool bBurstExpiredByDelta = bFlickerBurstActive && FlickerBurstRemainingSeconds <= 0.0f;
	const bool bBurstExpiredByWorld = bFlickerBurstActive && GetWorld() && FlickerBurstEndTime > 0.0f && GetWorld()->GetTimeSeconds() >= FlickerBurstEndTime;
	if (bBurstExpiredByDelta || bBurstExpiredByWorld)
	{
		EndFlickerBurst();
	}

	if (!bPowered && !(bFlickerBurstActive && bFlickerBurstForcePowered))
	{
		Light->SetIntensity(0.0f);
		return;
	}

	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float ActiveSpeed = bFlickerBurstActive ? FlickerBurstSpeed : FlickerSpeed;
	const float ActiveIntensity = bFlickerBurstActive ? BaseIntensity * FMath::Max(0.1f, FlickerBurstIntensityMultiplier) : BaseIntensity;
	const float Noise = FMath::Sin(Time * ActiveSpeed + Phase) * 0.5f + FMath::Sin(Time * 17.0f + Phase * 0.37f) * 0.5f;
	Light->SetIntensity(FMath::Max(80.0f, ActiveIntensity + Noise * FlickerAmount * (bFlickerBurstActive ? 1.8f : 1.0f)));
}

void ABHFlickerLight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHFlickerLight, CircuitId);
	DOREPLIFETIME(ABHFlickerLight, bPowered);
	DOREPLIFETIME(ABHFlickerLight, BaseIntensity);
	DOREPLIFETIME(ABHFlickerLight, LightColor);
	DOREPLIFETIME(ABHFlickerLight, LightRadius);
	DOREPLIFETIME(ABHFlickerLight, bFlickerBurstActive);
	DOREPLIFETIME(ABHFlickerLight, FlickerBurstEndTime);
	DOREPLIFETIME(ABHFlickerLight, FlickerBurstIntensityMultiplier);
	DOREPLIFETIME(ABHFlickerLight, FlickerBurstSpeed);
	DOREPLIFETIME(ABHFlickerLight, FlickerBurstColor);
	DOREPLIFETIME(ABHFlickerLight, bFlickerBurstForcePowered);
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

void ABHFlickerLight::TriggerFlickerBurst(float DurationSeconds, float IntensityMultiplier, const FLinearColor& BurstColor, float BurstSpeed, bool bForcePowered)
{
	if (!HasAuthority())
	{
		return;
	}

	bFlickerBurstActive = DurationSeconds > 0.0f;
	const float ClampedDuration = FMath::Max(0.05f, DurationSeconds);
	FlickerBurstEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() + ClampedDuration : -1.0f;
	FlickerBurstRemainingSeconds = bFlickerBurstActive ? ClampedDuration : -1.0f;
	FlickerBurstIntensityMultiplier = FMath::Max(0.1f, IntensityMultiplier);
	FlickerBurstSpeed = FMath::Max(0.1f, BurstSpeed);
	FlickerBurstColor = BurstColor;
	bFlickerBurstForcePowered = bForcePowered;
	ApplyLightConfig();
	ApplyLightState();

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FlickerBurstTimerHandle);
		GetWorldTimerManager().SetTimer(FlickerBurstTimerHandle, this, &ABHFlickerLight::EndFlickerBurst, ClampedDuration, false);
	}
}

int32 ABHFlickerLight::GetCircuitId() const
{
	return CircuitId;
}

bool ABHFlickerLight::IsPowered() const
{
	return bPowered;
}

bool ABHFlickerLight::IsFlickerBurstActive() const
{
	return bFlickerBurstActive;
}

void ABHFlickerLight::OnRep_Powered()
{
	ApplyLightState();
}

void ABHFlickerLight::OnRep_LightConfig()
{
	ApplyLightConfig();
}

void ABHFlickerLight::OnRep_FlickerBurst()
{
	FlickerBurstRemainingSeconds = (bFlickerBurstActive && GetWorld() && FlickerBurstEndTime > 0.0f)
		? FMath::Max(0.0f, FlickerBurstEndTime - GetWorld()->GetTimeSeconds())
		: -1.0f;
	ApplyLightConfig();
	ApplyLightState();
}

void ABHFlickerLight::EndFlickerBurst()
{
	if (!HasAuthority())
	{
		return;
	}

	bFlickerBurstActive = false;
	FlickerBurstEndTime = -1.0f;
	FlickerBurstIntensityMultiplier = 1.0f;
	FlickerBurstSpeed = FlickerSpeed;
	FlickerBurstColor = LightColor;
	bFlickerBurstForcePowered = true;
	FlickerBurstRemainingSeconds = -1.0f;
	ApplyLightConfig();
	ApplyLightState();
}

void ABHFlickerLight::ApplyLightState()
{
	if (Light)
	{
		const bool bVisible = bPowered || (bFlickerBurstActive && bFlickerBurstForcePowered);
		Light->SetVisibility(bVisible);
		if (!bVisible)
		{
			Light->SetIntensity(0.0f);
		}
	}
}

void ABHFlickerLight::ApplyLightConfig()
{
	if (Light)
	{
		Light->SetLightColor(bFlickerBurstActive ? FlickerBurstColor : LightColor);
		Light->SetAttenuationRadius(LightRadius);
	}
}

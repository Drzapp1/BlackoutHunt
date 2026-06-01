#include "BHFlickerLight.h"
#include "BHPlayerController.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABHFlickerLight::ABHFlickerLight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(2.0f);
	SetMinNetUpdateFrequency(0.5f);
	// The flicker animation runs client-side in Tick from replicated state, so the actor only
	// needs to replicate when its state actually changes (powered/burst/config). Go dormant and
	// flush on each authority-side mutation, and cull far clients, so a full class does not pay a
	// continuous per-light heartbeat. New/relevant connections still receive the current state.
	SetNetCullDistanceSquared(8000.0f * 8000.0f);
	NetDormancy = DORM_DormantAll;

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
		if (HasAuthority())
		{
			EndFlickerBurst();
		}
		else
		{
			// Clients tick too: stop the burst visual locally as soon as it expires.
			// The authoritative state still arrives via OnRep_FlickerBurst; this just
			// avoids the burst lingering until the next (throttled) replication tick.
			bFlickerBurstActive = false;
			FlickerBurstRemainingSeconds = -1.0f;
			ApplyLightConfig();
			ApplyLightState();
		}
	}

	if (!bPowered && !(bFlickerBurstActive && bFlickerBurstForcePowered))
	{
		Light->SetIntensity(0.0f);
		return;
	}

	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	// When the local player has Reduced flash enabled, attenuate the burst boost toward the
	// steady base flicker. The light still reacts to the power event (so gameplay reads the
	// same), but the harsh strobe (extra brightness, swing, and speed) is muted.
	const float FlashScale = bFlickerBurstActive ? FMath::Clamp(CachedReducedFlashScale, 0.0f, 1.0f) : 1.0f;
	const float ActiveSpeed = bFlickerBurstActive ? FMath::Lerp(FlickerSpeed, FlickerBurstSpeed, FlashScale) : FlickerSpeed;
	const float EffectiveBurstMultiplier = FMath::Lerp(1.0f, FMath::Max(0.1f, FlickerBurstIntensityMultiplier), FlashScale);
	const float ActiveIntensity = bFlickerBurstActive ? BaseIntensity * EffectiveBurstMultiplier : BaseIntensity;
	const float AmplitudeBoost = bFlickerBurstActive ? FMath::Lerp(1.0f, 1.8f, FlashScale) : 1.0f;
	// Give every fixture its own rhythm rather than the same two-sine shifted only by phase: derive a
	// stable per-light frequency jitter + secondary frequency from Phase, and add a slow tertiary sine
	// so the decay wanders organically instead of looping a fixed pattern. All terms are cheap and need
	// no extra state (FMath::Frac(Phase*k) is a stable pseudo-random per light). Weights sum to ~1.0 so
	// the overall swing matches the historical look.
	const float FreqJitter = 0.85f + 0.30f * FMath::Frac(Phase * 0.137f);   // ~0.85..1.15 per light
	const float SecondaryHz = 13.0f + 9.0f * FMath::Frac(Phase * 0.061f);   // ~13..22 Hz per light
	const float SlowHz = 0.6f + 0.25f * FMath::Frac(Phase * 0.290f);        // ~0.6..0.85 Hz per light
	const float Noise =
		FMath::Sin(Time * ActiveSpeed * FreqJitter + Phase) * 0.46f +
		FMath::Sin(Time * SecondaryHz + Phase * 0.37f) * 0.34f +
		FMath::Sin(Time * SlowHz + Phase * 1.7f) * 0.20f;
	Light->SetIntensity(FMath::Max(80.0f, ActiveIntensity + Noise * FlickerAmount * AmplitudeBoost));
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
	if (HasAuthority())
	{
		FlushNetDormancy();
	}
}

void ABHFlickerLight::SetPowered(bool bNewPowered)
{
	bPowered = bNewPowered;
	ApplyLightState();
	if (HasAuthority())
	{
		FlushNetDormancy();
	}
}

void ABHFlickerLight::TriggerFlickerBurst(float DurationSeconds, float IntensityMultiplier, const FLinearColor& BurstColor, float BurstSpeed, bool bForcePowered)
{
	if (!HasAuthority())
	{
		return;
	}

	bFlickerBurstActive = DurationSeconds > 0.0f;
	if (bFlickerBurstActive)
	{
		// Listen-server host ticks this light too, so refresh its local comfort scale here.
		RefreshLocalReducedFlashScale();
	}
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

	FlushNetDormancy();
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
	if (bFlickerBurstActive)
	{
		// A burst just started on this client; capture the local player's Reduced-flash choice.
		RefreshLocalReducedFlashScale();
	}
	FlickerBurstRemainingSeconds = (bFlickerBurstActive && GetWorld() && FlickerBurstEndTime > 0.0f)
		? FMath::Max(0.0f, FlickerBurstEndTime - GetWorld()->GetTimeSeconds())
		: -1.0f;
	ApplyLightConfig();
	ApplyLightState();
}

float ABHFlickerLight::ComputeLocalReducedFlashScale() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	// The local player on this machine drives comfort. On a listen-server host this is the
	// teacher's controller; on a student client it is that student's controller.
	if (const ABHPlayerController* LocalBHPC = Cast<ABHPlayerController>(World->GetFirstPlayerController()))
	{
		// Mirror the 0.25x factor used for jumpscare/horror-cue flash so behavior is consistent.
		return LocalBHPC->IsReducedFlashEnabled() ? 0.25f : 1.0f;
	}

	return 1.0f;
}

void ABHFlickerLight::RefreshLocalReducedFlashScale()
{
	CachedReducedFlashScale = ComputeLocalReducedFlashScale();
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
	FlushNetDormancy();
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

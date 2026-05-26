#include "BHAtmosphereDirector.h"
#include "BHAmbientEmitter.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
FString BHAtmosphereStimulusName(EBHAtmosphereStimulusType Type)
{
	const UEnum* Enum = StaticEnum<EBHAtmosphereStimulusType>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Type)) : TEXT("Unknown");
}

FString BHScareEventName(EBHScareEventType Type)
{
	const UEnum* Enum = StaticEnum<EBHScareEventType>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Type)) : TEXT("Unknown");
}
}

void UBHAtmosphereDirector::Initialize(ABHGameMode* InOwner)
{
	OwnerGameMode = InOwner;
}

void UBHAtmosphereDirector::ReportAtmosphereStimulus(EBHAtmosphereStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, float Strength, const FString& Reason)
{
	UWorld* World = OwnerGameMode ? OwnerGameMode->GetWorld() : nullptr;
	if (!OwnerGameMode || !World)
	{
		return;
	}

	LastStimulusType = Type;
	LastStimulusLocation = Location;
	LastStimulusReason = Reason;
	LastStimulusTime = World->GetTimeSeconds();

	const float PresenceSpike = ResolvePresenceSpike(Type, Strength);
	if (PresenceSpike > 0.0f)
	{
		OwnerGameMode->ApplyPresenceSpike(Location, PresenceSpike, FString::Printf(TEXT("%s pressure: %s"), *BHAtmosphereStimulusName(Type), Reason.IsEmpty() ? TEXT("unknown") : *Reason));
	}

	if (Type == EBHAtmosphereStimulusType::CCTV && LastStimulusTime - LastCCTVGlitchTime > 8.0f)
	{
		if (ABHCharacter* Target = Cast<ABHCharacter>(TargetActor))
		{
			FBHScareEventSpec Spec;
			Spec.EventType = EBHScareEventType::CCTVGlitch;
			Spec.Target = Target;
			Spec.Origin = Location;
			Spec.Intensity = FMath::Clamp(Strength, 0.15f, 1.0f);
			Spec.Message = TEXT("Security feed distortion: something saw you first.");
			TriggerAtmosphereCue(Spec);
			LastCCTVGlitchTime = LastStimulusTime;
		}
	}
}

bool UBHAtmosphereDirector::TriggerAtmosphereCue(const FBHScareEventSpec& Spec)
{
	UWorld* World = OwnerGameMode ? OwnerGameMode->GetWorld() : nullptr;
	if (!OwnerGameMode || !World)
	{
		return false;
	}

	ABHCharacter* Target = ResolveTarget(Spec.Target);
	const FVector Origin = Spec.Origin.IsNearlyZero() && Target ? Target->GetActorLocation() : Spec.Origin;
	LastCueType = Spec.EventType;
	LastCueLocation = Origin;
	LastCueTargetName = GetNameSafe(Target);
	LastCueTime = World->GetTimeSeconds();

	switch (Spec.EventType)
	{
	case EBHScareEventType::MonsterCharge:
		if (Target)
		{
			OwnerGameMode->TriggerMonsterChargeJumpscare(Target);
			return true;
		}
		return false;

	case EBHScareEventType::LightCut:
		return TriggerBlackoutPulse(Origin, Spec.LightRadius, Spec.LockSeconds > 0.0f ? Spec.LockSeconds : 6.0f);

	case EBHScareEventType::CCTVGlitch:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 95.0f), 620.0f, 0.22f + Spec.Intensity * 0.18f, 0.18f, 8.5f, 2.6f);
		if (Target)
		{
			Target->AddFear(8.0f + Spec.Intensity * 12.0f);
			Target->AddDread(8.0f + Spec.Intensity * 10.0f);
			SendClientCue(Target, Spec, Origin + FVector(0.0f, 0.0f, 120.0f), false);
		}
		return true;

	case EBHScareEventType::LockerKnock:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 80.0f), 210.0f, 0.32f, 0.22f, 7.2f, 3.0f);
		if (Target)
		{
			Target->AddFear(10.0f + Spec.Intensity * 14.0f);
			Target->AddDread(14.0f + Spec.Intensity * 18.0f);
			SendClientCue(Target, Spec, Origin + FVector(0.0f, 0.0f, 90.0f), false);
		}
		return true;

	case EBHScareEventType::FaceFlash:
	case EBHScareEventType::AudioStinger:
	case EBHScareEventType::Ambient:
	default:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 86.0f), FMath::Lerp(160.0f, 680.0f, Spec.Intensity), 0.20f + Spec.Intensity * 0.18f, 0.18f, 5.0f + Spec.Intensity * 4.0f, 3.25f);
		if (Target)
		{
			Target->AddFear(8.0f + Spec.Intensity * 22.0f);
			Target->AddDread(8.0f + Spec.Intensity * 18.0f);
			SendClientCue(Target, Spec, Origin + FVector(0.0f, 0.0f, 120.0f), Spec.LockSeconds > 0.0f);
		}
		return true;
	}
}

bool UBHAtmosphereDirector::TriggerAtmosphereCue(ABHCharacter* Target, EBHScareEventType ScareType, const FVector& Origin, float Intensity)
{
	FBHScareEventSpec Spec;
	Spec.EventType = ScareType;
	Spec.Target = Target;
	Spec.Origin = Origin;
	Spec.Intensity = Intensity;
	return TriggerAtmosphereCue(Spec);
}

bool UBHAtmosphereDirector::TriggerBlackoutPulse(const FVector& Location, float Radius, float DurationSeconds)
{
	if (!OwnerGameMode)
	{
		return false;
	}

	OwnerGameMode->CutLightsForJumpscare(Location, Location, Radius, FMath::Max(0.25f, DurationSeconds));
	LastCueType = EBHScareEventType::LightCut;
	LastCueLocation = Location;
	LastCueTime = OwnerGameMode->GetWorld() ? OwnerGameMode->GetWorld()->GetTimeSeconds() : LastCueTime;
	return true;
}

bool UBHAtmosphereDirector::TriggerManualScare(ABHCharacter* Target, EBHScareEventType ScareType)
{
	UWorld* World = OwnerGameMode ? OwnerGameMode->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	LastManualScareTime = World->GetTimeSeconds();
	FBHScareEventSpec Spec;
	Spec.EventType = ScareType;
	Spec.Target = Target;
	Spec.Origin = Target ? Target->GetActorLocation() + Target->GetActorForwardVector() * 320.0f : FVector::ZeroVector;
	Spec.Intensity = 0.92f;
	Spec.LockSeconds = ScareType == EBHScareEventType::MonsterCharge ? 0.0f : 1.4f;
	Spec.LightRadius = 2200.0f;
	Spec.Message = ScareType == EBHScareEventType::CCTVGlitch
		? TEXT("The monitor blinks with your silhouette.")
		: TEXT("Something moves where the light should be.");
	return TriggerAtmosphereCue(Spec);
}

FString UBHAtmosphereDirector::GetDebugStatus() const
{
	const float Now = OwnerGameMode && OwnerGameMode->GetWorld() ? OwnerGameMode->GetWorld()->GetTimeSeconds() : 0.0f;
	const float LastStimulusAge = LastStimulusTime <= -9000.0f ? -1.0f : Now - LastStimulusTime;
	const float LastCueAge = LastCueTime <= -9000.0f ? -1.0f : Now - LastCueTime;
	return FString::Printf(TEXT("Atmosphere cue=%s target=%s cueAge=%.1fs stimulus=%s age=%.1fs reason=%s loc=(%.0f,%.0f,%.0f)"),
		*BHScareEventName(LastCueType),
		*LastCueTargetName,
		LastCueAge,
		*BHAtmosphereStimulusName(LastStimulusType),
		LastStimulusAge,
		*LastStimulusReason,
		LastStimulusLocation.X,
		LastStimulusLocation.Y,
		LastStimulusLocation.Z);
}

ABHCharacter* UBHAtmosphereDirector::ResolveTarget(ABHCharacter* PreferredTarget) const
{
	if (PreferredTarget)
	{
		const ABHPlayerState* PreferredPS = PreferredTarget->GetPlayerState<ABHPlayerState>();
		if (!PreferredPS || PreferredPS->LifeState == EBHPlayerLifeState::Alive)
		{
			return PreferredTarget;
		}
	}

	UWorld* World = OwnerGameMode ? OwnerGameMode->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ABHCharacter> It(World); It; ++It)
	{
		ABHCharacter* Candidate = *It;
		const ABHPlayerState* CandidatePS = Candidate ? Candidate->GetPlayerState<ABHPlayerState>() : nullptr;
		if (Candidate && CandidatePS && CandidatePS->IsAliveSurvivor() && !Candidate->IsHiddenInLocker())
		{
			return Candidate;
		}
	}
	return nullptr;
}

void UBHAtmosphereDirector::SendClientCue(ABHCharacter* Target, const FBHScareEventSpec& Spec, const FVector& FocusLocation, bool bLockInput) const
{
	if (!Target)
	{
		return;
	}

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController()))
	{
		const FBHJumpscareVariant Variant = OwnerGameMode ? OwnerGameMode->ChooseJumpscareVariant(Spec.EventType) : FBHJumpscareVariant();
		FBHClientHorrorCue Cue;
		Cue.EventType = Spec.EventType;
		Cue.FocusLocation = FocusLocation;
		Cue.Message = Spec.Message;
		Cue.DurationSeconds = 2.0f + Spec.Intensity * 1.2f;
		Cue.LockSeconds = Spec.LockSeconds;
		Cue.ShakeIntensity = FMath::Clamp(FMath::Max(Spec.Intensity, Variant.CameraShakeIntensity * 0.68f), 0.0f, 1.0f);
		Cue.bSnapToFocus = Spec.EventType != EBHScareEventType::Ambient;
		Cue.bLockInput = bLockInput;
		Cue.AudioAsset = Spec.AudioAsset.IsNull() ? Variant.LaunchSound : Spec.AudioAsset;
		Cue.AudioVolume = Spec.EventType == EBHScareEventType::AudioStinger ? 1.0f : 0.82f;
		if (Spec.VisualActorClass)
		{
			Cue.VisualActorClass = TSoftClassPtr<AActor>(Spec.VisualActorClass.Get());
		}
		else
		{
			Cue.VisualActorClass = Variant.VisualActorClass;
		}
		Cue.VariantId = Spec.VariantId.IsNone() ? Variant.VariantId : Spec.VariantId;
		Cue.CloseVisualOffset = Variant.CloseVisualOffset;
		Cue.CloseVisualRotation = Variant.CloseVisualRotation;
		Cue.CloseVisualScale = Variant.CloseVisualScale;
		Cue.FlashIntensity = Spec.FlashIntensity > 0.0f
			? Spec.FlashIntensity
			: FMath::Clamp(Variant.FlashIntensity * (Spec.EventType == EBHScareEventType::FaceFlash ? 1.0f : 0.45f), 0.0f, 1.0f);
		Cue.FlashColor = Spec.FlashColor.A > 0.0f ? Spec.FlashColor : Variant.LightColor;
		Cue.CameraJitterDuration = FMath::Max(0.25f, Variant.CameraJitterDuration * FMath::Clamp(Spec.Intensity, 0.35f, 1.0f));
		Cue.CameraJitterFrequency = Spec.EventType == EBHScareEventType::AudioStinger ? 42.0f : 34.0f;
		Cue.bCloseRangeFocus = Spec.EventType == EBHScareEventType::FaceFlash || Spec.EventType == EBHScareEventType::AudioStinger;
		Cue.bUpperBodyCloseVisual = Cue.bCloseRangeFocus;
		PC->ClientPlayHorrorCue(Cue);
	}
}

float UBHAtmosphereDirector::ResolvePresenceSpike(EBHAtmosphereStimulusType Type, float Strength) const
{
	const float ClampedStrength = FMath::Clamp(Strength, 0.0f, 2.0f);
	switch (Type)
	{
	case EBHAtmosphereStimulusType::CCTV:
		return 44.0f * ClampedStrength;
	case EBHAtmosphereStimulusType::Objective:
		return 38.0f * ClampedStrength;
	case EBHAtmosphereStimulusType::Power:
		return 35.0f * ClampedStrength;
	case EBHAtmosphereStimulusType::Locker:
		return 30.0f * ClampedStrength;
	case EBHAtmosphereStimulusType::Monster:
	case EBHAtmosphereStimulusType::Manual:
		return 60.0f * ClampedStrength;
	case EBHAtmosphereStimulusType::Footstep:
		return ClampedStrength >= 0.85f ? 18.0f * ClampedStrength : 0.0f;
	case EBHAtmosphereStimulusType::Noise:
	default:
		return 0.0f;
	}
}

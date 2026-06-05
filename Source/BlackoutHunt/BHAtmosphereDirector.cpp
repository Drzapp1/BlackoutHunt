// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHAtmosphereDirector.h"
#include "BHAmbientEmitter.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
const TCHAR* BHCCTVStaticSoundPath = TEXT("/Game/SecurityCameras/Sounds/Wav_Static_CCTV.Wav_Static_CCTV");

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

FString BHDefaultScareCueMessage(EBHScareEventType Type)
{
	switch (Type)
	{
	case EBHScareEventType::MonsterCharge:
		return TEXT("Something sees you.");
	case EBHScareEventType::FaceFlash:
		return TEXT("A face flashes at the edge of your vision.");
	case EBHScareEventType::AudioStinger:
		return TEXT("A sharp sound cuts through the dark.");
	case EBHScareEventType::LightCut:
		return TEXT("The lights fold inward for a moment.");
	case EBHScareEventType::CCTVGlitch:
		return TEXT("Security feed distortion: something saw you first.");
	case EBHScareEventType::LockerKnock:
		return TEXT("The locker knocks once, then waits.");
	case EBHScareEventType::Whisper:
		return TEXT("A whisper traces your route.");
	case EBHScareEventType::FootstepEcho:
		return TEXT("Footsteps match your pace, then stop.");
	case EBHScareEventType::FaceImage:
		return TEXT("It is suddenly, impossibly close.");
	case EBHScareEventType::Peek:
		return TEXT("Something leans out, just at the edge of sight.");
	case EBHScareEventType::BehindYou:
		return TEXT("SOMETHING IS BEHIND YOU");
	case EBHScareEventType::Ambient:
	default:
		return TEXT("The room shifts around you.");
	}
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

	// CCTV glitch cue is throttled PER PLAYER (the 8s cooldown lives in the target survivor's scare memory),
	// so one survivor's camera cue no longer suppresses everyone else's for 8s -- matching the per-player
	// scare budgets. A CCTV stimulus still within this survivor's cooldown falls through to the pressure cue
	// below, exactly as before.
	ABHCharacter* CCTVTarget = Cast<ABHCharacter>(TargetActor);
	FBHPlayerScareMemory* CCTVMemory = (Type == EBHAtmosphereStimulusType::CCTV && CCTVTarget)
		? &GetOrCreateMemory(CCTVTarget, LastStimulusTime)
		: nullptr;
	const float TargetLastCCTVGlitch = CCTVMemory ? CCTVMemory->LastCCTVGlitchTime : -9999.0f;
	if (Type == EBHAtmosphereStimulusType::CCTV && CCTVMemory && LastStimulusTime - TargetLastCCTVGlitch > 8.0f)
	{
		FBHScareEventSpec Spec;
		Spec.EventType = EBHScareEventType::CCTVGlitch;
		Spec.Target = CCTVTarget;
		Spec.Origin = Location;
		Spec.Intensity = FMath::Clamp(Strength, 0.15f, 1.0f);
		Spec.Message = TEXT("Security feed distortion: something saw you first.");
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		Spec.AudioAsset = Settings && !Settings->CCTVStaticSound.IsNull()
			? Settings->CCTVStaticSound
			: TSoftObjectPtr<USoundBase>(FSoftObjectPath(BHCCTVStaticSoundPath));
		if (TriggerAtmosphereCue(Spec))
		{
			CCTVMemory->LastCCTVGlitchTime = LastStimulusTime;
		}
	}
	else if (LastStimulusTime - LastPressureCueAttemptTime > 7.5f)
	{
		FBHScareEventSpec Spec;
		Spec.Target = Cast<ABHCharacter>(TargetActor);
		Spec.Origin = Location;
		Spec.Intensity = FMath::Clamp(Strength, 0.15f, 0.72f);

		bool bShouldTryCue = false;
		switch (Type)
		{
		case EBHAtmosphereStimulusType::Locker:
			bShouldTryCue = Strength >= 0.70f;
			Spec.EventType = EBHScareEventType::LockerKnock;
			Spec.Message = TEXT("The locker knocks back.");
			break;
		case EBHAtmosphereStimulusType::Footstep:
			bShouldTryCue = Strength >= 1.05f;
			Spec.EventType = EBHScareEventType::FootstepEcho;
			Spec.Message = TEXT("A second set of footsteps copies the rhythm.");
			break;
		case EBHAtmosphereStimulusType::Power:
			bShouldTryCue = Strength >= 0.95f;
			Spec.EventType = EBHScareEventType::LightCut;
			Spec.LightRadius = 1400.0f;
			Spec.LockSeconds = 2.4f;
			Spec.Message = TEXT("The circuit answers with a blackout blink.");
			break;
		case EBHAtmosphereStimulusType::Objective:
			bShouldTryCue = Strength >= 1.10f;
			Spec.EventType = EBHScareEventType::Whisper;
			Spec.Message = TEXT("The completed task whispers too loudly.");
			break;
		default:
			break;
		}

		if (bShouldTryCue)
		{
			if (const UBHGameSettings* Settings = GetDefault<UBHGameSettings>())
			{
				if (Spec.EventType == EBHScareEventType::LockerKnock)
				{
					Spec.AudioAsset = Settings->LockerKnockSound;
					Spec.FlashIntensity = 0.02f;
				}
			}
			LastPressureCueAttemptTime = LastStimulusTime;
			TriggerAtmosphereCue(Spec);
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

	FBHScareEventSpec WorkingSpec = Spec;
	ABHCharacter* Target = ResolveTarget(WorkingSpec.Target);
	FVector Origin = WorkingSpec.Origin.IsNearlyZero() && Target ? Target->GetActorLocation() : WorkingSpec.Origin;

	// Suppress autonomous jumpscares (monster charge / SCP-096, in-your-face, behind-you, corner peek) when the
	// teacher is right next to the target — a random monster would step on the teacher's own approach.
	// Deliberate teacher scares and test-menu scares set bBypassDirectorBudget / bGameplayCritical and are exempt.
	if (!WorkingSpec.bBypassDirectorBudget && !WorkingSpec.bGameplayCritical
		&& (IsJumpscareCue(WorkingSpec.EventType) || WorkingSpec.EventType == EBHScareEventType::Peek))
	{
		const FVector GateLocation = Target ? Target->GetActorLocation() : Origin;
		if (OwnerGameMode->IsTeacherNearby(GateLocation))
		{
			LastBudgetDecision = TEXT("suppressed: teacher nearby");
			return false;
		}
	}

	FString BudgetReason;
	if (!TryApplyBudget(Target, WorkingSpec, Origin, true, BudgetReason))
	{
		return false;
	}
	if (WorkingSpec.Message.IsEmpty())
	{
		WorkingSpec.Message = BHDefaultScareCueMessage(WorkingSpec.EventType);
	}

	LastCueType = WorkingSpec.EventType;
	LastCueLocation = Origin;
	LastCueTargetName = GetNameSafe(Target);
	LastCueTime = World->GetTimeSeconds();

	switch (WorkingSpec.EventType)
	{
	case EBHScareEventType::MonsterCharge:
		if (Target)
		{
			OwnerGameMode->TriggerMonsterChargeJumpscare(Target);
			return true;
		}
		return false;

	case EBHScareEventType::FaceImage:
		if (Target)
		{
			OwnerGameMode->TriggerFaceImageJumpscare(Target);
			return true;
		}
		return false;

	case EBHScareEventType::BehindYou:
		if (Target)
		{
			OwnerGameMode->TriggerBehindYouScare(Target);
			return true;
		}
		return false;

	case EBHScareEventType::Peek:
		if (Target)
		{
			OwnerGameMode->TriggerCornerPeek(Target);
			return true;
		}
		return false;

	case EBHScareEventType::LightCut:
		return TriggerBlackoutPulse(Origin, WorkingSpec.LightRadius, WorkingSpec.LockSeconds > 0.0f ? WorkingSpec.LockSeconds : 6.0f);

	case EBHScareEventType::CCTVGlitch:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 95.0f), 620.0f, 0.22f + WorkingSpec.Intensity * 0.18f, 0.18f, 8.5f, 2.6f);
		if (Target)
		{
			Target->AddFear(8.0f + WorkingSpec.Intensity * 12.0f);
			Target->AddDread(8.0f + WorkingSpec.Intensity * 10.0f);
			SendClientCue(Target, WorkingSpec, Origin + FVector(0.0f, 0.0f, 120.0f), false);
		}
		return true;

	case EBHScareEventType::LockerKnock:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 80.0f), 210.0f, 0.32f, 0.22f, 7.2f, 3.0f);
		if (Target)
		{
			Target->AddFear(10.0f + WorkingSpec.Intensity * 14.0f);
			Target->AddDread(14.0f + WorkingSpec.Intensity * 18.0f);
			SendClientCue(Target, WorkingSpec, Origin + FVector(0.0f, 0.0f, 90.0f), false);
		}
		return true;

	case EBHScareEventType::Whisper:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 84.0f), 165.0f + WorkingSpec.Intensity * 110.0f, 0.13f + WorkingSpec.Intensity * 0.08f, 0.10f, 4.6f + WorkingSpec.Intensity * 1.8f, 3.0f);
		if (Target)
		{
			Target->AddFear(5.0f + WorkingSpec.Intensity * 8.0f);
			Target->AddDread(7.0f + WorkingSpec.Intensity * 10.0f);
			SendClientCue(Target, WorkingSpec, Origin + FVector(0.0f, 0.0f, 96.0f), false);
		}
		return true;

	case EBHScareEventType::FootstepEcho:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 38.0f), 95.0f + WorkingSpec.Intensity * 95.0f, 0.16f + WorkingSpec.Intensity * 0.08f, 0.08f, 3.4f + WorkingSpec.Intensity * 1.4f, 2.8f);
		if (Target)
		{
			Target->AddFear(4.0f + WorkingSpec.Intensity * 7.0f);
			Target->AddDread(5.0f + WorkingSpec.Intensity * 8.0f);
			SendClientCue(Target, WorkingSpec, Origin + FVector(0.0f, 0.0f, 64.0f), false);
		}
		return true;

	case EBHScareEventType::FaceFlash:
	case EBHScareEventType::AudioStinger:
	case EBHScareEventType::Ambient:
	default:
		OwnerGameMode->SpawnAmbient(Origin + FVector(0.0f, 0.0f, 86.0f), FMath::Lerp(160.0f, 680.0f, WorkingSpec.Intensity), 0.20f + WorkingSpec.Intensity * 0.18f, 0.18f, 5.0f + WorkingSpec.Intensity * 4.0f, 3.25f);
		if (Target)
		{
			Target->AddFear(8.0f + WorkingSpec.Intensity * 22.0f);
			Target->AddDread(8.0f + WorkingSpec.Intensity * 18.0f);
			SendClientCue(Target, WorkingSpec, Origin + FVector(0.0f, 0.0f, 120.0f), WorkingSpec.LockSeconds > 0.0f);
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
	if (const UBHGameSettings* Settings = GetDefault<UBHGameSettings>())
	{
		FBHGameplayAudioCue Cue;
		Cue.AudioAsset = Settings->PowerLossStingerSound;
		Cue.Location = Location + FVector(0.0f, 0.0f, 96.0f);
		Cue.Caption = TEXT("Power drops out.");
		Cue.Volume = 0.58f;
		Cue.Pitch = 0.92f;
		Cue.MaxAudibleDistance = FMath::Clamp(Radius > 0.0f ? Radius * 1.15f : 2600.0f, 1200.0f, 5200.0f);
		Cue.CaptionSeconds = 1.45f;
		Cue.bSpatial = true;
		Cue.bApplyHorrorComfort = true;
		OwnerGameMode->BroadcastGameplayAudioCue(Cue);
	}
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
	Spec.bBypassDirectorBudget = true;
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
	return FString::Printf(TEXT("Atmosphere cue=%s target=%s cueAge=%.1fs budget=%.2f decision=%s stimulus=%s age=%.1fs reason=%s loc=(%.0f,%.0f,%.0f)"),
		*BHScareEventName(LastCueType),
		*LastCueTargetName,
		LastCueAge,
		LastBudgetValue,
		LastBudgetDecision.IsEmpty() ? TEXT("none") : *LastBudgetDecision,
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
		// Align with the scan branch below: only return the preferred target if it is itself a valid,
		// non-hidden, alive survivor. A null-PS or non-survivor preferred target now falls through to
		// pick a real survivor (or nullptr) instead of being scared.
		if (PreferredPS && PreferredPS->IsAliveSurvivor() && !PreferredTarget->IsHiddenInLocker())
		{
			return PreferredTarget;
		}
	}

	UWorld* World = OwnerGameMode ? OwnerGameMode->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	ABHCharacter* BestCandidate = nullptr;
	float BestCandidateScore = -1.0f;
	for (TActorIterator<ABHCharacter> It(World); It; ++It)
	{
		ABHCharacter* Candidate = *It;
		const ABHPlayerState* CandidatePS = Candidate ? Candidate->GetPlayerState<ABHPlayerState>() : nullptr;
		if (Candidate && CandidatePS && CandidatePS->IsAliveSurvivor() && !Candidate->IsHiddenInLocker())
		{
			const float BudgetWeight = GetDirectorCueWeight(Candidate, EBHScareEventType::Ambient, 0.45f);
			const float StableTieBreak = static_cast<float>(FMath::Abs(MakeCueSalt(Candidate, Candidate->GetActorLocation(), 17)) % 100) / 1000.0f;
			const float CandidateScore = BudgetWeight + StableTieBreak;
			if (CandidateScore > BestCandidateScore)
			{
				BestCandidate = Candidate;
				BestCandidateScore = CandidateScore;
			}
		}
	}
	return BestCandidate;
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
		const float SensoryScale = OwnerGameMode ? OwnerGameMode->GetScareSensoryScale() : 1.0f;
		const bool bSubtleCue = IsSubtleCue(Spec.EventType);
		const bool bHardVisualCue = Spec.EventType == EBHScareEventType::FaceFlash || Spec.EventType == EBHScareEventType::MonsterCharge;
		const float AudioSensoryScale = SensoryScale <= 0.0f ? (bSubtleCue ? 0.20f : 0.0f) : FMath::Lerp(bSubtleCue ? 0.22f : 0.35f, 1.0f, SensoryScale);
		const float VisualSensoryScale = SensoryScale <= 0.0f && bSubtleCue ? 0.18f : SensoryScale;
		FBHClientHorrorCue Cue;
		Cue.EventType = Spec.EventType;
		Cue.FocusLocation = FocusLocation;
		Cue.Message = Spec.Message;
		Cue.DurationSeconds = bSubtleCue ? 1.65f + Spec.Intensity * 0.85f : 2.0f + Spec.Intensity * 1.2f;
		Cue.LockSeconds = Spec.LockSeconds;
		Cue.ShakeIntensity = FMath::Clamp((bSubtleCue ? Spec.Intensity * 0.22f : FMath::Max(Spec.Intensity, Variant.CameraShakeIntensity * 0.68f)) * VisualSensoryScale, 0.0f, 1.0f);
		Cue.bSnapToFocus = Spec.EventType != EBHScareEventType::Ambient
			&& Spec.EventType != EBHScareEventType::Whisper
			&& Spec.EventType != EBHScareEventType::FootstepEcho;
		Cue.bLockInput = bLockInput;
		Cue.AudioAsset = Spec.AudioAsset.IsNull() ? Variant.LaunchSound : Spec.AudioAsset;
		Cue.AudioVolume = (Spec.EventType == EBHScareEventType::AudioStinger ? 1.0f : (bSubtleCue ? 0.46f : 0.82f)) * AudioSensoryScale;
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
		Cue.FlashIntensity = (Spec.FlashIntensity > 0.0f
			? Spec.FlashIntensity
			: FMath::Clamp(Variant.FlashIntensity * (Spec.EventType == EBHScareEventType::FaceFlash ? 1.0f : (bSubtleCue ? 0.08f : 0.45f)), 0.0f, 1.0f)) * VisualSensoryScale;
		Cue.FlashColor = Spec.FlashColor.A > 0.0f ? Spec.FlashColor : Variant.LightColor;
		Cue.CameraJitterDuration = FMath::Max(0.25f, Variant.CameraJitterDuration * FMath::Clamp(Spec.Intensity, bSubtleCue ? 0.12f : 0.35f, 1.0f)) * VisualSensoryScale;
		Cue.CameraJitterFrequency = Spec.EventType == EBHScareEventType::AudioStinger ? 42.0f : (Spec.EventType == EBHScareEventType::FootstepEcho ? 18.0f : 34.0f);
		Cue.bCloseRangeFocus = bHardVisualCue || (Spec.EventType == EBHScareEventType::AudioStinger && Spec.Intensity >= 0.72f);
		Cue.bUpperBodyCloseVisual = Cue.bCloseRangeFocus;
		PC->ClientPlayHorrorCue(Cue);
	}
}

bool UBHAtmosphereDirector::ReserveDirectorCue(ABHCharacter* Target, EBHScareEventType ScareType, const FVector& Origin, float Intensity, const FString& Reason, FString& OutReason)
{
	if (!Target)
	{
		OutReason = TEXT("no target");
		return false;
	}

	FBHScareEventSpec Spec;
	Spec.EventType = ScareType;
	Spec.Target = Target;
	Spec.Origin = Origin;
	Spec.Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	Spec.Message = Reason.IsEmpty() ? BHDefaultScareCueMessage(ScareType) : Reason;
	Spec.bAllowCueTypeSubstitution = false;
	FVector WorkingOrigin = Origin.IsNearlyZero() ? Target->GetActorLocation() : Origin;
	return TryApplyBudget(Target, Spec, WorkingOrigin, true, OutReason);
}

float UBHAtmosphereDirector::GetDirectorCueWeight(ABHCharacter* Target, EBHScareEventType ScareType, float Intensity) const
{
	if (!Target)
	{
		return 0.0f;
	}
	if (GetScareIntensity() <= 0 && !IsSubtleCue(ScareType))
	{
		return 0.0f;
	}

	const float Now = OwnerGameMode && OwnerGameMode->GetWorld() ? OwnerGameMode->GetWorld()->GetTimeSeconds() : 0.0f;
	const FBHPlayerScareMemory* Memory = FindMemory(Target);
	if (!Memory)
	{
		return 1.25f;
	}

	const float UpdatedBudget = FMath::Clamp(
		Memory->PressureBudget + FMath::Max(0.0f, Now - Memory->LastBudgetUpdateTime) * GetBudgetRegenPerSecond(),
		0.0f,
		GetMaxBudget());
	const float Cost = FMath::Max(0.05f, GetCueCost(ScareType, Intensity));
	float Weight = FMath::Clamp(UpdatedBudget / Cost, 0.0f, 1.35f);

	if (const float* LastTypeTime = Memory->LastCueTimesByType.Find(ScareType))
	{
		const float SameTypeAge = Now - *LastTypeTime;
		if (SameTypeAge < GetSameTypeCooldown(ScareType))
		{
			Weight *= FMath::Clamp(SameTypeAge / FMath::Max(1.0f, GetSameTypeCooldown(ScareType)), 0.0f, 1.0f);
		}
	}

	if (IsJumpscareCue(ScareType) && Now - Memory->LastJumpscareTime < GetHeavyCueCooldown(ScareType))
	{
		Weight = 0.0f;
	}
	else if (IsHeavyCue(ScareType) && Now - Memory->LastHeavyCueTime < GetHeavyCueCooldown(ScareType) * 0.72f)
	{
		Weight *= 0.25f;
	}

	if (Memory->TotalCueCount == 0)
	{
		Weight += 0.18f;
	}
	return Weight;
}

EBHScareEventType UBHAtmosphereDirector::ChoosePressureCueType(ABHCharacter* Target, const FVector& Origin, float PressureAlpha, bool bAllowMonsterBeat) const
{
	const float ClampedPressure = FMath::Clamp(PressureAlpha, 0.0f, 1.0f);
	TArray<EBHScareEventType> Candidates;
	if (bAllowMonsterBeat && ClampedPressure >= 0.82f)
	{
		Candidates.Add(EBHScareEventType::MonsterCharge);
	}
	if (Target && Target->IsHiddenInLocker())
	{
		Candidates.Add(EBHScareEventType::LockerKnock);
	}
	if (ClampedPressure >= 0.66f)
	{
		Candidates.Add(EBHScareEventType::AudioStinger);
		Candidates.Add(EBHScareEventType::LightCut);
		// Heavy client-side jumpscares that don't need the monster-beat gate; the shared jumpscare
		// cooldown (IsJumpscareCue) keeps them from stacking with a monster charge.
		Candidates.Add(EBHScareEventType::FaceImage);
		Candidates.Add(EBHScareEventType::BehindYou);
	}
	if (ClampedPressure >= 0.12f)
	{
		// Passive tension cues are the backbone of the dread loop — keep them available almost always (from very
		// low pressure) and weighted heavily (extra copies) so the director leans on whispers / footsteps / corner
		// peeks far more than the heavy jumpscares. The hard scares still gate on high pressure above.
		Candidates.Add(EBHScareEventType::FootstepEcho);
		Candidates.Add(EBHScareEventType::Whisper);
		Candidates.Add(EBHScareEventType::Peek);
		Candidates.Add(EBHScareEventType::Whisper);
		Candidates.Add(EBHScareEventType::FootstepEcho);
		Candidates.Add(EBHScareEventType::Peek);
	}
	Candidates.Add(EBHScareEventType::Ambient);

	if (Candidates.IsEmpty())
	{
		return EBHScareEventType::Ambient;
	}

	const int32 StartIndex = FMath::Abs(MakeCueSalt(Target, Origin, FMath::FloorToInt(ClampedPressure * 100.0f))) % Candidates.Num();
	EBHScareEventType BestType = Candidates[StartIndex];
	float BestWeight = -1.0f;
	for (int32 Offset = 0; Offset < Candidates.Num(); ++Offset)
	{
		const EBHScareEventType Candidate = Candidates[(StartIndex + Offset) % Candidates.Num()];
		const float CandidateIntensity = IsSubtleCue(Candidate) ? FMath::Clamp(0.30f + ClampedPressure * 0.34f, 0.20f, 0.62f) : FMath::Clamp(0.48f + ClampedPressure * 0.34f, 0.35f, 0.88f);
		const float Weight = GetDirectorCueWeight(Target, Candidate, CandidateIntensity) + (Candidate != BestType ? 0.01f * Offset : 0.0f);
		if (Weight > BestWeight)
		{
			BestWeight = Weight;
			BestType = Candidate;
		}
	}
	return BestType;
}

UBHAtmosphereDirector::FBHPlayerScareMemory& UBHAtmosphereDirector::GetOrCreateMemory(ABHCharacter* Target, float Now)
{
	check(Target);
	// Drop entries whose pawn has been destroyed (each respawn gets a new UniqueID, so the old key
	// would otherwise linger). Keeps the map sized to live players instead of growing per respawn.
	for (auto It = PlayerScareMemory.CreateIterator(); It; ++It)
	{
		if (!It.Value().Target.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const int32 Key = Target->GetUniqueID();
	FBHPlayerScareMemory& Memory = PlayerScareMemory.FindOrAdd(Key);
	if (Memory.Target.Get() != Target)
	{
		Memory = FBHPlayerScareMemory();
		Memory.Target = Target;
		Memory.PressureBudget = GetMaxBudget();
		Memory.LastBudgetUpdateTime = Now;
	}
	else
	{
		UpdateBudget(Memory, Now);
	}
	return Memory;
}

const UBHAtmosphereDirector::FBHPlayerScareMemory* UBHAtmosphereDirector::FindMemory(ABHCharacter* Target) const
{
	const FBHPlayerScareMemory* Memory = Target ? PlayerScareMemory.Find(Target->GetUniqueID()) : nullptr;
	return Memory && Memory->Target.Get() == Target ? Memory : nullptr;
}

void UBHAtmosphereDirector::UpdateBudget(FBHPlayerScareMemory& Memory, float Now) const
{
	if (Memory.LastBudgetUpdateTime <= -9000.0f)
	{
		Memory.PressureBudget = GetMaxBudget();
		Memory.LastBudgetUpdateTime = Now;
		return;
	}

	const float DeltaSeconds = FMath::Max(0.0f, Now - Memory.LastBudgetUpdateTime);
	Memory.PressureBudget = FMath::Clamp(Memory.PressureBudget + DeltaSeconds * GetBudgetRegenPerSecond(), 0.0f, GetMaxBudget());
	Memory.LastBudgetUpdateTime = Now;
}

bool UBHAtmosphereDirector::TryApplyBudget(ABHCharacter* Target, FBHScareEventSpec& InOutSpec, FVector& InOutOrigin, bool bConsumeBudget, FString& OutReason)
{
	if (!Target || !OwnerGameMode || !OwnerGameMode->GetWorld())
	{
		OutReason = TEXT("no target budget");
		return true;
	}

	const float Now = OwnerGameMode->GetWorld()->GetTimeSeconds();
	FBHPlayerScareMemory& Memory = GetOrCreateMemory(Target, Now);
	const EBHScareEventType DesiredType = InOutSpec.EventType;
	const bool bBypassBudget = InOutSpec.bBypassDirectorBudget || InOutSpec.bGameplayCritical || !bConsumeBudget;

	if (bBypassBudget)
	{
		RecordCue(Memory, DesiredType, InOutOrigin, InOutSpec.Intensity, TEXT("bypassed"));
		OutReason = TEXT("bypassed");
		return true;
	}

	FString BlockReason;
	EBHScareEventType SelectedType = DesiredType;
	if (!CanUseCueType(Memory, DesiredType, InOutOrigin, InOutSpec.Intensity, BlockReason))
	{
		if (!InOutSpec.bAllowCueTypeSubstitution || !TrySubstituteCueType(Memory, DesiredType, InOutOrigin, InOutSpec.Intensity, SelectedType, BlockReason))
		{
			RecordRejectedCue(Memory, DesiredType, BlockReason);
			OutReason = BlockReason;
			return false;
		}
	}

	if (SelectedType != DesiredType)
	{
		InOutSpec.EventType = SelectedType;
		InOutSpec.Intensity = IsSubtleCue(SelectedType)
			? FMath::Clamp(InOutSpec.Intensity * 0.62f, 0.18f, 0.52f)
			: FMath::Clamp(InOutSpec.Intensity, 0.25f, 0.82f);
		InOutSpec.LockSeconds = IsHeavyCue(SelectedType) ? InOutSpec.LockSeconds : 0.0f;
		if (InOutSpec.Message.IsEmpty() || IsHeavyCue(DesiredType))
		{
			InOutSpec.Message = BHDefaultScareCueMessage(SelectedType);
		}
		OutReason = FString::Printf(TEXT("substituted %s after %s"), *BHScareEventName(SelectedType), *BlockReason);
	}
	else
	{
		OutReason = TEXT("spent");
	}

	const float Cost = GetCueCost(InOutSpec.EventType, InOutSpec.Intensity);
	Memory.PressureBudget = FMath::Clamp(Memory.PressureBudget - Cost, 0.0f, GetMaxBudget());
	RecordCue(Memory, InOutSpec.EventType, InOutOrigin, InOutSpec.Intensity, OutReason);
	return true;
}

bool UBHAtmosphereDirector::TrySubstituteCueType(const FBHPlayerScareMemory& Memory, EBHScareEventType DesiredType, const FVector& Origin, float Intensity, EBHScareEventType& OutType, FString& OutReason) const
{
	TArray<EBHScareEventType> Candidates;
	const ABHCharacter* Target = Memory.Target.Get();
	if (Target && Target->IsHiddenInLocker())
	{
		Candidates.Add(EBHScareEventType::LockerKnock);
	}
	if (DesiredType == EBHScareEventType::CCTVGlitch)
	{
		Candidates.Add(EBHScareEventType::Whisper);
		Candidates.Add(EBHScareEventType::FootstepEcho);
		Candidates.Add(EBHScareEventType::Ambient);
	}
	else if (DesiredType == EBHScareEventType::LightCut)
	{
		Candidates.Add(EBHScareEventType::Whisper);
		Candidates.Add(EBHScareEventType::FootstepEcho);
		Candidates.Add(EBHScareEventType::Ambient);
	}
	else
	{
		Candidates.Add(EBHScareEventType::FootstepEcho);
		Candidates.Add(EBHScareEventType::Whisper);
		Candidates.Add(EBHScareEventType::LightCut);
		Candidates.Add(EBHScareEventType::Ambient);
	}

	if (Candidates.IsEmpty())
	{
		OutReason = TEXT("no fallback cues");
		return false;
	}

	const int32 StartIndex = FMath::Abs(MakeCueSalt(Memory.Target.Get(), Origin, Memory.TotalCueCount + static_cast<int32>(DesiredType) * 19)) % Candidates.Num();
	FString CandidateReason;
	for (int32 Offset = 0; Offset < Candidates.Num(); ++Offset)
	{
		const EBHScareEventType Candidate = Candidates[(StartIndex + Offset) % Candidates.Num()];
		if (Candidate == DesiredType)
		{
			continue;
		}
		if (CanUseCueType(Memory, Candidate, Origin, IsSubtleCue(Candidate) ? FMath::Min(Intensity, 0.45f) : Intensity, CandidateReason))
		{
			OutType = Candidate;
			return true;
		}
	}

	OutReason = CandidateReason.IsEmpty() ? TEXT("fallback cues cooling down") : CandidateReason;
	return false;
}

bool UBHAtmosphereDirector::CanUseCueType(const FBHPlayerScareMemory& Memory, EBHScareEventType ScareType, const FVector& Origin, float Intensity, FString& OutReason) const
{
	const float Now = OwnerGameMode && OwnerGameMode->GetWorld() ? OwnerGameMode->GetWorld()->GetTimeSeconds() : 0.0f;
	if (GetScareIntensity() <= 0 && !IsSubtleCue(ScareType))
	{
		OutReason = TEXT("scare intensity off");
		return false;
	}

	if (const float* LastTypeTime = Memory.LastCueTimesByType.Find(ScareType))
	{
		const float Cooldown = GetSameTypeCooldown(ScareType);
		if (Now - *LastTypeTime < Cooldown)
		{
			OutReason = FString::Printf(TEXT("%s same-type cooldown %.1fs remaining"), *BHScareEventName(ScareType), Cooldown - (Now - *LastTypeTime));
			return false;
		}
	}

	if (IsJumpscareCue(ScareType) && Now - Memory.LastJumpscareTime < GetHeavyCueCooldown(ScareType))
	{
		OutReason = FString::Printf(TEXT("jumpscare cooldown %.1fs remaining"), GetHeavyCueCooldown(ScareType) - (Now - Memory.LastJumpscareTime));
		return false;
	}

	if (IsHeavyCue(ScareType) && Now - Memory.LastHeavyCueTime < GetHeavyCueCooldown(ScareType) * 0.72f)
	{
		OutReason = FString::Printf(TEXT("heavy cue cooldown %.1fs remaining"), GetHeavyCueCooldown(ScareType) * 0.72f - (Now - Memory.LastHeavyCueTime));
		return false;
	}

	if (!Memory.LastCueLocation.IsNearlyZero()
		&& FVector::DistSquared2D(Memory.LastCueLocation, Origin) <= FMath::Square(IsSubtleCue(ScareType) ? 360.0f : 820.0f)
		&& Now - Memory.LastCueTime < (IsSubtleCue(ScareType) ? 5.0f : 12.0f))
	{
		OutReason = TEXT("recent cue was at the same location");
		return false;
	}

	const float Cost = GetCueCost(ScareType, Intensity);
	if (Memory.PressureBudget + KINDA_SMALL_NUMBER < Cost)
	{
		OutReason = FString::Printf(TEXT("budget %.2f below %.2f"), Memory.PressureBudget, Cost);
		return false;
	}

	OutReason = TEXT("available");
	return true;
}

void UBHAtmosphereDirector::RecordCue(FBHPlayerScareMemory& Memory, EBHScareEventType ScareType, const FVector& Origin, float Intensity, const FString& Reason)
{
	const float Now = OwnerGameMode && OwnerGameMode->GetWorld() ? OwnerGameMode->GetWorld()->GetTimeSeconds() : 0.0f;
	Memory.LastCueTime = Now;
	Memory.LastCueType = ScareType;
	Memory.LastCueLocation = Origin;
	Memory.LastCueTimesByType.FindOrAdd(ScareType) = Now;
	if (IsHeavyCue(ScareType))
	{
		Memory.LastHeavyCueTime = Now;
	}
	if (IsJumpscareCue(ScareType))
	{
		Memory.LastJumpscareTime = Now;
	}
	Memory.RecentCueTypes.Add(ScareType);
	while (Memory.RecentCueTypes.Num() > 5)
	{
		Memory.RecentCueTypes.RemoveAt(0);
	}
	++Memory.TotalCueCount;
	Memory.LastDecisionReason = FormatDecision(TEXT("spent"), Memory.Target.Get(), ScareType, Reason);
	LastCueType = ScareType;
	LastCueLocation = Origin;
	LastCueTargetName = GetNameSafe(Memory.Target.Get());
	LastCueTime = Now;
	LastBudgetDecision = Memory.LastDecisionReason;
	LastBudgetValue = Memory.PressureBudget;
}

void UBHAtmosphereDirector::RecordRejectedCue(FBHPlayerScareMemory& Memory, EBHScareEventType ScareType, const FString& Reason)
{
	const float Now = OwnerGameMode && OwnerGameMode->GetWorld() ? OwnerGameMode->GetWorld()->GetTimeSeconds() : 0.0f;
	Memory.LastRejectedTime = Now;
	++Memory.RejectedCueCount;
	Memory.LastDecisionReason = FormatDecision(TEXT("blocked"), Memory.Target.Get(), ScareType, Reason);
	LastBudgetDecision = Memory.LastDecisionReason;
	LastBudgetValue = Memory.PressureBudget;
}

float UBHAtmosphereDirector::GetMaxBudget() const
{
	switch (GetScareIntensity())
	{
	case 0:
		return 0.58f;
	case 1:
		return 1.00f;
	case 2:
		return 1.34f;
	case 3:
	default:
		return 1.72f;
	}
}

float UBHAtmosphereDirector::GetBudgetRegenPerSecond() const
{
	switch (GetScareIntensity())
	{
	case 0:
		return 0.010f;
	case 1:
		return 0.017f;
	case 2:
		return 0.026f;
	case 3:
	default:
		return 0.038f;
	}
}

float UBHAtmosphereDirector::GetCueCost(EBHScareEventType ScareType, float Intensity) const
{
	const float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	switch (ScareType)
	{
	case EBHScareEventType::MonsterCharge:
		return 1.12f + ClampedIntensity * 0.24f;
	case EBHScareEventType::FaceFlash:
		return 0.82f + ClampedIntensity * 0.18f;
	case EBHScareEventType::AudioStinger:
		return 0.58f + ClampedIntensity * 0.18f;
	case EBHScareEventType::CCTVGlitch:
		return 0.44f + ClampedIntensity * 0.14f;
	case EBHScareEventType::LockerKnock:
		return 0.42f + ClampedIntensity * 0.13f;
	case EBHScareEventType::LightCut:
		return 0.36f + ClampedIntensity * 0.12f;
	case EBHScareEventType::Whisper:
		return 0.24f + ClampedIntensity * 0.08f;
	case EBHScareEventType::FootstepEcho:
		return 0.22f + ClampedIntensity * 0.08f;
	case EBHScareEventType::FaceImage:
		return 0.96f + ClampedIntensity * 0.20f;
	case EBHScareEventType::BehindYou:
		return 1.06f + ClampedIntensity * 0.22f;
	case EBHScareEventType::Peek:
		return 0.30f + ClampedIntensity * 0.10f;
	case EBHScareEventType::Ambient:
	default:
		return 0.26f + ClampedIntensity * 0.10f;
	}
}

float UBHAtmosphereDirector::GetSameTypeCooldown(EBHScareEventType ScareType) const
{
	const float IntensityScale = GetScareIntensity() <= 1 ? 1.32f : (GetScareIntensity() >= 3 ? 0.78f : 1.0f);
	switch (ScareType)
	{
	case EBHScareEventType::MonsterCharge:
		return 74.0f * IntensityScale;
	case EBHScareEventType::FaceFlash:
		return 52.0f * IntensityScale;
	case EBHScareEventType::AudioStinger:
		return 28.0f * IntensityScale;
	case EBHScareEventType::CCTVGlitch:
		return 20.0f * IntensityScale;
	case EBHScareEventType::LockerKnock:
		return 24.0f * IntensityScale;
	case EBHScareEventType::LightCut:
		return 18.0f * IntensityScale;
	case EBHScareEventType::Whisper:
		return 12.0f * IntensityScale;
	case EBHScareEventType::FootstepEcho:
		return 11.0f * IntensityScale;
	case EBHScareEventType::FaceImage:
		return 50.0f * IntensityScale;
	case EBHScareEventType::BehindYou:
		return 82.0f * IntensityScale;
	case EBHScareEventType::Peek:
		return 16.0f * IntensityScale;
	case EBHScareEventType::Ambient:
	default:
		return 10.0f * IntensityScale;
	}
}

float UBHAtmosphereDirector::GetHeavyCueCooldown(EBHScareEventType ScareType) const
{
	const float IntensityScale = GetScareIntensity() <= 1 ? 1.35f : (GetScareIntensity() >= 3 ? 0.82f : 1.0f);
	return (ScareType == EBHScareEventType::MonsterCharge ? 70.0f : 42.0f) * IntensityScale;
}

bool UBHAtmosphereDirector::IsHeavyCue(EBHScareEventType ScareType) const
{
	return ScareType == EBHScareEventType::MonsterCharge
		|| ScareType == EBHScareEventType::FaceImage
		|| ScareType == EBHScareEventType::BehindYou
		|| ScareType == EBHScareEventType::FaceFlash
		|| ScareType == EBHScareEventType::AudioStinger
		|| ScareType == EBHScareEventType::CCTVGlitch;
}

bool UBHAtmosphereDirector::IsJumpscareCue(EBHScareEventType ScareType) const
{
	return ScareType == EBHScareEventType::MonsterCharge
		|| ScareType == EBHScareEventType::FaceImage
		|| ScareType == EBHScareEventType::BehindYou
		|| ScareType == EBHScareEventType::FaceFlash;
}

bool UBHAtmosphereDirector::IsSubtleCue(EBHScareEventType ScareType) const
{
	return ScareType == EBHScareEventType::Ambient
		|| ScareType == EBHScareEventType::Whisper
		|| ScareType == EBHScareEventType::FootstepEcho;
}

int32 UBHAtmosphereDirector::GetScareIntensity() const
{
	return OwnerGameMode ? OwnerGameMode->GetEffectiveScareIntensity() : 3;
}

int32 UBHAtmosphereDirector::MakeCueSalt(ABHCharacter* Target, const FVector& Origin, int32 ExtraSalt) const
{
	const int32 RoundSeed = OwnerGameMode ? OwnerGameMode->RoundSeed : 24681357;
	const int32 TargetSalt = Target ? Target->GetUniqueID() * 196613 : 0;
	const int32 LocationSalt = FMath::RoundToInt(Origin.X * 0.07f) * 31
		^ FMath::RoundToInt(Origin.Y * 0.07f) * 131
		^ FMath::RoundToInt(Origin.Z * 0.03f) * 17;
	return RoundSeed ^ TargetSalt ^ LocationSalt ^ (ExtraSalt * 7919);
}

FString UBHAtmosphereDirector::FormatDecision(const FString& Prefix, ABHCharacter* Target, EBHScareEventType ScareType, const FString& Reason) const
{
	return FString::Printf(TEXT("%s %s target=%s reason=%s"),
		*Prefix,
		*BHScareEventName(ScareType),
		*GetNameSafe(Target),
		Reason.IsEmpty() ? TEXT("none") : *Reason);
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

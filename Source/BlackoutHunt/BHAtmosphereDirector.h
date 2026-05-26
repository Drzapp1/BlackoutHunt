#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "UObject/Object.h"
#include "BHAtmosphereDirector.generated.h"

class ABHCharacter;
class ABHGameMode;

UCLASS(Transient)
class BLACKOUTHUNT_API UBHAtmosphereDirector : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABHGameMode* InOwner);

	void ReportAtmosphereStimulus(EBHAtmosphereStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, float Strength, const FString& Reason);
	bool TriggerAtmosphereCue(const FBHScareEventSpec& Spec);
	bool TriggerAtmosphereCue(ABHCharacter* Target, EBHScareEventType ScareType, const FVector& Origin = FVector::ZeroVector, float Intensity = 0.65f);
	bool TriggerBlackoutPulse(const FVector& Location, float Radius, float DurationSeconds);
	bool TriggerManualScare(ABHCharacter* Target, EBHScareEventType ScareType);
	FString GetDebugStatus() const;

private:
	ABHCharacter* ResolveTarget(ABHCharacter* PreferredTarget) const;
	void SendClientCue(ABHCharacter* Target, const FBHScareEventSpec& Spec, const FVector& FocusLocation, bool bLockInput) const;
	float ResolvePresenceSpike(EBHAtmosphereStimulusType Type, float Strength) const;

	UPROPERTY(Transient)
	TObjectPtr<ABHGameMode> OwnerGameMode;

	EBHAtmosphereStimulusType LastStimulusType = EBHAtmosphereStimulusType::Noise;
	EBHScareEventType LastCueType = EBHScareEventType::Ambient;
	FVector LastStimulusLocation = FVector::ZeroVector;
	FVector LastCueLocation = FVector::ZeroVector;
	FString LastStimulusReason;
	FString LastCueTargetName;
	float LastStimulusTime = -9999.0f;
	float LastCueTime = -9999.0f;
	float LastManualScareTime = -9999.0f;
	float LastCCTVGlitchTime = -9999.0f;
};

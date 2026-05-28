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
	bool ReserveDirectorCue(ABHCharacter* Target, EBHScareEventType ScareType, const FVector& Origin, float Intensity, const FString& Reason, FString& OutReason);
	float GetDirectorCueWeight(ABHCharacter* Target, EBHScareEventType ScareType, float Intensity) const;
	EBHScareEventType ChoosePressureCueType(ABHCharacter* Target, const FVector& Origin, float PressureAlpha, bool bAllowMonsterBeat) const;
	FString GetDebugStatus() const;

private:
	struct FBHPlayerScareMemory
	{
		TWeakObjectPtr<ABHCharacter> Target;
		float PressureBudget = 0.0f;
		float LastBudgetUpdateTime = -9999.0f;
		float LastCueTime = -9999.0f;
		float LastHeavyCueTime = -9999.0f;
		float LastJumpscareTime = -9999.0f;
		float LastRejectedTime = -9999.0f;
		EBHScareEventType LastCueType = EBHScareEventType::Ambient;
		FVector LastCueLocation = FVector::ZeroVector;
		TArray<EBHScareEventType> RecentCueTypes;
		TMap<EBHScareEventType, float> LastCueTimesByType;
		int32 TotalCueCount = 0;
		int32 RejectedCueCount = 0;
		FString LastDecisionReason;
	};

	ABHCharacter* ResolveTarget(ABHCharacter* PreferredTarget) const;
	void SendClientCue(ABHCharacter* Target, const FBHScareEventSpec& Spec, const FVector& FocusLocation, bool bLockInput) const;
	float ResolvePresenceSpike(EBHAtmosphereStimulusType Type, float Strength) const;
	FBHPlayerScareMemory& GetOrCreateMemory(ABHCharacter* Target, float Now);
	const FBHPlayerScareMemory* FindMemory(ABHCharacter* Target) const;
	void UpdateBudget(FBHPlayerScareMemory& Memory, float Now) const;
	bool TryApplyBudget(ABHCharacter* Target, FBHScareEventSpec& InOutSpec, FVector& InOutOrigin, bool bConsumeBudget, FString& OutReason);
	bool TrySubstituteCueType(const FBHPlayerScareMemory& Memory, EBHScareEventType DesiredType, const FVector& Origin, float Intensity, EBHScareEventType& OutType, FString& OutReason) const;
	bool CanUseCueType(const FBHPlayerScareMemory& Memory, EBHScareEventType ScareType, const FVector& Origin, float Intensity, FString& OutReason) const;
	void RecordCue(FBHPlayerScareMemory& Memory, EBHScareEventType ScareType, const FVector& Origin, float Intensity, const FString& Reason);
	void RecordRejectedCue(FBHPlayerScareMemory& Memory, EBHScareEventType ScareType, const FString& Reason);
	float GetMaxBudget() const;
	float GetBudgetRegenPerSecond() const;
	float GetCueCost(EBHScareEventType ScareType, float Intensity) const;
	float GetSameTypeCooldown(EBHScareEventType ScareType) const;
	float GetHeavyCueCooldown(EBHScareEventType ScareType) const;
	bool IsHeavyCue(EBHScareEventType ScareType) const;
	bool IsJumpscareCue(EBHScareEventType ScareType) const;
	bool IsSubtleCue(EBHScareEventType ScareType) const;
	int32 GetScareIntensity() const;
	int32 MakeCueSalt(ABHCharacter* Target, const FVector& Origin, int32 ExtraSalt) const;
	FString FormatDecision(const FString& Prefix, ABHCharacter* Target, EBHScareEventType ScareType, const FString& Reason) const;

	UPROPERTY(Transient)
	TObjectPtr<ABHGameMode> OwnerGameMode;

	TMap<int32, FBHPlayerScareMemory> PlayerScareMemory;
	EBHAtmosphereStimulusType LastStimulusType = EBHAtmosphereStimulusType::Noise;
	EBHScareEventType LastCueType = EBHScareEventType::Ambient;
	FVector LastStimulusLocation = FVector::ZeroVector;
	FVector LastCueLocation = FVector::ZeroVector;
	FString LastStimulusReason;
	FString LastCueTargetName;
	FString LastBudgetDecision;
	float LastBudgetValue = 0.0f;
	float LastStimulusTime = -9999.0f;
	float LastCueTime = -9999.0f;
	float LastPressureCueAttemptTime = -9999.0f;
	float LastManualScareTime = -9999.0f;
	float LastCCTVGlitchTime = -9999.0f;
};

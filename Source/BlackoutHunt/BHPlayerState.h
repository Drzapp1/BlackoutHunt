#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BHTypes.h"
#include "BHPlayerState.generated.h"

UCLASS()
class BLACKOUTHUNT_API ABHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABHPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsAliveSurvivor() const;

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt")
	bool IsAliveHunter() const;

	void SetReady(bool bNewReady);
	void SetRole(EBHPlayerRole NewRole);
	void SetDesiredRole(EBHPlayerRole NewRole);
	void SetLifeState(EBHPlayerLifeState NewLifeState);
	void SetHiddenInLocker(bool bNewHidden);
	void SetAvatarIndex(int32 NewAvatarIndex);
	void SetAvatarColor(const FLinearColor& NewAvatarColor);
	void SetAvatarHeadwearIndex(int32 NewHeadwearIndex);
	void SetAvatarGearIndex(int32 NewGearIndex);
	void SetMapVote(const FString& NewMapVote);
	void SetFogPresetVote(EBHFogPreset NewFogPresetVote);
	void ClearFogPresetVote();
	void SetFakeHunterEligible(bool bNewEligible);
	void SetSpectatorRolePreference(EBHPlayerRole NewRole);
	void ClearSpectatorSupportState(bool bClearRolePreference = true);
	void AddSpectatorEncouragement();
	void ResetRevisionStats();
	void AddQuestionPoints(int32 Points);
	bool SpendQuestionPoints(int32 Points);
	int32 ApplyCaughtQuestionPointPenalty(float PenaltyFraction = 0.25f);
	void AddHunterPoints(int32 Points);
	bool SpendHunterPoints(int32 Points);
	int32 GetPowerupCharges(EBHPowerupType Type) const;
	bool AddPowerupCharge(EBHPowerupType Type, int32 MaxCharges);
	bool ConsumePowerupCharge(EBHPowerupType Type);
	void SetPowerupCooldown(EBHPowerupType Type, float CooldownEndServerTime);
	float GetPowerupCooldownEnd(EBHPowerupType Type) const;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bReady;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHPlayerRole PlayerRole;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHPlayerRole DesiredRole;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHPlayerLifeState LifeState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bHiddenInLocker;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 AvatarIndex;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	FLinearColor AvatarColor;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 AvatarHeadwearIndex;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 AvatarGearIndex;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	FString MapVote;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bHasFogPresetVote;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	EBHFogPreset FogPresetVote;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	bool bFakeHunterEligible;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Spectator")
	EBHPlayerRole SpectatorRolePreference;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Spectator")
	int32 SpectatorEncouragementCount;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FBHPlayerRevisionStats RevisionStats;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	int32 QuestionPoints;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	int32 LifetimeQuestionPoints;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	int32 HunterPoints;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	int32 LifetimeHunterPoints;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	TArray<FBHPowerupInventoryEntry> Powerups;
};

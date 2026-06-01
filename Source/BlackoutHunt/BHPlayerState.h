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
	// Records that the player tried a warmup action during Prep. Server-authoritative and a
	// no-op outside the live role warmup, so it can be called unconditionally from action sites.
	// Never touches scoring/mastery/reports. Sends a one-time "you're ready" toast on completion.
	void MarkWarmupStep(EBHWarmupStep Step);
	// Clears the warmup checklist so it never carries between warmup and the live Hunt.
	void ResetWarmupChecklist();
	// Per-player spaced-repetition review queue: question IDs the player answered
	// incorrectly, oldest first, re-surfaced until answered correctly.
	void EnqueueRevisionReview(const FString& QuestionId);
	bool DequeueRevisionReview(const FString& QuestionId);
	FString PeekRevisionReview() const;
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

	// Advisory teaching state for the role warmup checklist (see EBHWarmupStep). Owner-only
	// replication: only the player's own HUD needs the mask; the host reads it server-side for
	// the classroom-board coverage count. Cleared before the live Hunt; never affects reports.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Warmup")
	uint8 WarmupChecklistMask;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Warmup")
	bool bWarmupComplete;

	// Question IDs the player missed, oldest first. Replicated to the owner so the
	// client HUD can frame a re-asked question as a review. Server-authoritative.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	TArray<FString> RevisionReviewQueue;

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

	// Server-side only (deliberately NOT replicated): the secret per-client reconnect token. Set from the
	// join URL in ABHGameMode::InitNewPlayer when a client echoes a prior token, or freshly generated in
	// PostLogin and pushed to the owning client via ABHPlayerController::ClientReceiveReconnectToken. The
	// mid-round reconnect match (UBHGameInstance::TryGetReconnectProgress) keys on this token rather than
	// the spoofable display name, so same-named students can't be restored into each other's slot.
	FString ReconnectToken;
};

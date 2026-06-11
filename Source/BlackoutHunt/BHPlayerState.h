// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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
	// Per-part clothing colours (registry-indexed by BHColorableMaterialNames; value = palette colour index+1,
	// 0 = the skin's authored colour). Replicated so other players see your recolours.
	void SetAvatarSlotColors(const TArray<uint8>& NewSlotColors);
	// Nameplate flair (achievement-gated; 0 = none). Replicated so other players see your title/emblem.
	void SetSelectedTitleIndex(int32 NewTitleIndex);
	void SetSelectedEmblemIndex(int32 NewEmblemIndex);
	void SetMapVote(const FString& NewMapVote);
	void SetFogPresetVote(EBHFogPreset NewFogPresetVote);
	void ClearFogPresetVote();
	void SetFakeHunterEligible(bool bNewEligible);
	void SetSpectatorRolePreference(EBHPlayerRole NewRole);
	void ClearSpectatorSupportState(bool bClearRolePreference = true);
	void AddSpectatorEncouragement();
	void ResetRevisionStats();
	// Clears only the per-round contribution counter (the "did real work this round" gate) while leaving
	// durable mastery and the spaced-review queue intact, so later session stages keep cumulative mastery.
	void ResetRevisionRoundContribution();
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
	// Returns the oldest queued review id, skipping ExcludeQuestionId (case-insensitive). The
	// exclusion lets a station avoid re-serving the question that was JUST missed while its
	// correction text (containing the answer) is still on screen; the excluded id stays queued
	// and surfaces on a later peek. Empty result = nothing (else) to review.
	FString PeekRevisionReview(const FString& ExcludeQuestionId = FString()) const;
	void AddQuestionPoints(int32 Points);
	bool SpendQuestionPoints(int32 Points);
	int32 ApplyCaughtQuestionPointPenalty(float PenaltyFraction = 0.25f);
	void AddHunterPoints(int32 Points);
	bool SpendHunterPoints(int32 Points);
	// Prop-hunt round score (manual-taunt bonus now; survival/catch/MVP land with the P6 match wrapper).
	// Server-authoritative; clamped at zero.
	void AddPropHuntScore(int32 Points);
	void ResetPropHuntScore();
	int32 GetPowerupCharges(EBHPowerupType Type) const;
	bool AddPowerupCharge(EBHPowerupType Type, int32 MaxCharges);
	bool ConsumePowerupCharge(EBHPowerupType Type);
	void SetPowerupCooldown(EBHPowerupType Type, float CooldownEndServerTime);
	float GetPowerupCooldownEnd(EBHPowerupType Type) const;

	// The minigame table (blackjack / chess) this player is currently using. Set server-side by the table
	// actors so the owning client's HUD can draw the player's hand/board status on-screen.
	void SetActiveMinigameTable(AActor* Table);
	AActor* GetActiveMinigameTable() const { return ActiveMinigameTable; }

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

	// Per-part clothing colours; see SetAvatarSlotColors. Registry-indexed; 0 = authored, else palette index+1.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	TArray<uint8> AvatarSlotColors;

	// Nameplate flair indices (into EBHCosmeticCategory::Title / Emblem; 0 = none). Cosmetic only.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 SelectedTitleIndex;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt")
	int32 SelectedEmblemIndex;

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

	// Prop-hunt cumulative score for the current round/match (replicated to everyone: the scoreboard is public).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Prop Hunt")
	int32 PropHuntScore = 0;

	// Rounds this player has STARTED as the seeker this match (server-only bookkeeping for the fewest-first
	// rotation; persisted across the round travel with the other progress fields, never replicated).
	int32 PropHuntTimesSeeker = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Powerups")
	TArray<FBHPowerupInventoryEntry> Powerups;

	// The minigame table (blackjack / chess) this player is currently using; see SetActiveMinigameTable.
	// Owner-only so only the player's own HUD reads it.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Minigame")
	TObjectPtr<AActor> ActiveMinigameTable;

	// Train jukebox track this player has selected (PER-PERSON: each client plays only its own choice). -1 = off,
	// else an index into the jukebox playlist. Owner-only -- only the player's own client needs it to play locally.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Blackout Hunt|Train")
	int32 JukeboxTrackIndex = -1;

	// Server-side only (deliberately NOT replicated): the secret per-client reconnect token. Set from the
	// join URL in ABHGameMode::InitNewPlayer when a client echoes a prior token, or freshly generated in
	// PostLogin and pushed to the owning client via ABHPlayerController::ClientReceiveReconnectToken. The
	// mid-round reconnect match (UBHGameInstance::TryGetReconnectProgress) keys on this token rather than
	// the spoofable display name, so same-named students can't be restored into each other's slot.
	FString ReconnectToken;
};

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHCosmeticUnlocks.h"
#include "BHTypes.h"
#include "HttpFwd.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BHAccountSubsystem.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct FBHAccountProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString ProviderSubject;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString Email;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString AvatarUrl;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString DeviceId;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	bool bGuest = true;

	FString SessionToken;
	FString LastLoginUtc;
};

USTRUCT(BlueprintType)
struct FBHAccountProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 ProfileVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 RoundsPlayed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 HunterWins = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SurvivorWins = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 Escapes = 0;

	// Current consecutive-win streak (for the On a Roll achievement). Resets on any non-winning round.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 CurrentWinStreak = 0;

	// Best consecutive-win streak ever (lifetime stat shown on the profile).
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 BestWinStreak = 0;

	// Bitmask of train-intermission activity types ever completed (for the Tourist achievement). 4 low bits.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 TrainActivityMask = 0;

	// Consecutive-correct answer streak (Honor Roll) + bitmask of physics topics ever answered correctly
	// (Polymath). Cosmetic/local; never touches the revision mastery model or classroom reports.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 CurrentAnswerStreak = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 TopicsEverCorrectMask = 0;

	// Per-topic lifetime answer tallies (index = EBHPhysicsTopic 0..3) behind the player's mastery readout.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	TArray<int32> TopicAnswerCounts;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	TArray<int32> TopicCorrectCounts;

	// Lifetime captures made as the Teacher (drives the Truant Officer achievement). Cosmetic/local.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 LifetimeCaptures = 0;

	// Lifetime distinct locker-hide events (drives Ghost in the Walls). Counts entries, not frames. Cosmetic/local.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 LockerHideCount = 0;

	// Lifetime perfect momentum chains landed (drives Momentum Maestro). Cosmetic/local.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 PerfectChainCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 XP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString SelectedAvatarUrl;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SelectedAvatarIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SelectedAvatarColorIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SelectedAvatarHeadwearIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SelectedAvatarGearIndex = 0;

	// Per-part clothing colours, registry-indexed by BHColorableMaterialNames (0 = the skin's authored colour,
	// else palette colour index+1). Lets the player recolour each garment slot of their skin independently.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	TArray<uint8> AvatarSlotColors;

	// Nameplate flair selections (achievement-gated; 0 = none). See EBHCosmeticCategory::Title / Emblem.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SelectedTitleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SelectedEmblemIndex = 0;

	// Selected UI menu theme (index into the BHMenuTheme registry). Local UI-only preference -- never replicated.
	// UNLOCK-GATED by XP/achievements (see BHMenuThemeIsUnlocked); 0 = Classic (always free).
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	int32 SelectedThemeIndex = 0;

	// UI menu theme STRENGTH (0 = faint/near-neutral, 1 = full theme colour). Local UI-only preference; never
	// replicated. 1 = full-strength (today's look).
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	float SelectedThemeStrength = 1.0f;

	// UI per-section CONTRAST (0 = very faint/near-monotone, 1 = very high per-section identity). Local UI-only
	// preference; never replicated. 1 = full identity (today's look).
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	float SelectedSectionContrast = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString LastUpdatedUtc;

	// Cosmetic achievements earned (ids; see the registry in BHAccountSubsystem.cpp). Some unlock the hidden
	// prestige tints. Local + persisted; never affect gameplay.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	TArray<FName> UnlockedAchievements;

	// Achievements already SEEN in the Awards tab (for the "newly unlocked" indicator: unlocked-but-unseen = new).
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	TArray<FName> SeenAchievements;

	// Hidden collectable relics found (stable per-site ids; see ABHCollectable). Local + persisted; drives the
	// milestone / Collector rewards and the Awards-tab "Relics N/M" tracker. Never affects gameplay.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	TArray<FName> CollectablesFound;
};

// One achievement's display data for the menu's Achievements tab, built from the registry + the local account's
// earned set. Plain struct (C++/Slate only -- not exposed to Blueprint).
struct FBHAchievementDisplay
{
	FName Id;
	FString Title;
	FString Description;
	FString RewardLabel;   // e.g. "Afterimage tint" -- empty if the reward is XP only
	int32 Difficulty = 1;  // 1..5 (the badge's star meter + tier colour)
	bool bUnlocked = false;
	bool bHidden = false;  // a secret achievement: the tab shows "???" until it is earned
	// A cryptic, non-spoiler nudge shown for a still-hidden secret achievement instead of a blank "???", so the
	// player has a rough idea what to chase. Empty for non-secret (or unhinted) achievements.
	FString Hint;
	// Progress toward a countable achievement (e.g. 18/25 rounds). ProgressTarget == 0 means "not countable"
	// (an event/binary achievement) and the badge draws no progress bar.
	int32 ProgressCurrent = 0;
	int32 ProgressTarget = 0;
	bool bIsNew = false;   // unlocked but not yet viewed in the Awards tab (drives the "NEW" marker).
};

UCLASS()
class BLACKOUTHUNT_API UBHAccountSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(Exec)
	void AccountGuest();

	UFUNCTION(Exec)
	void LoginGoogle();

	UFUNCTION(Exec)
	void LoginMicrosoft();

	UFUNCTION(Exec)
	void AccountPollLogin();

	UFUNCTION(Exec)
	void AccountSync();

	UFUNCTION(Exec)
	void AccountSignOut();

	UFUNCTION(Exec)
	void AccountCreateLocal(const FString& Username, const FString& Password);

	UFUNCTION(Exec)
	void AccountLoginLocal(const FString& Username, const FString& Password);

	UFUNCTION(Exec)
	void AccountForgetLocal();

	UFUNCTION(Exec)
	void AccountResetLocalClassroomData();

	// Developer-only cosmetic unlock for playtesting (console: BHUnlockAllCosmetics). The body is an inert stub
	// in Shipping -- and the console is disabled in Shipping anyway -- so it can never grant cosmetics in the
	// packaged classroom build. Undo with AccountResetLocalClassroomData.
	UFUNCTION(Exec)
	void BHUnlockAllCosmetics();

	bool ContinueAsGuest(FString& OutMessage);
	bool BeginProviderLogin(const FString& Provider, FString& OutMessage);
	bool PollProviderLogin(FString& OutMessage);
	bool SyncProgress(FString& OutMessage);
	bool SignOut(FString& OutMessage);
	bool CreateOrUpdateLocalCredential(const FString& Username, const FString& Password, FString& OutMessage);
	bool LoginLocalCredential(const FString& Username, const FString& Password, FString& OutMessage);
	bool ForgetLocalCredential(FString& OutMessage);
	bool ResetLocalClassroomData(FString& OutMessage);
	bool SetLocalDisplayName(const FString& DisplayName, FString& OutMessage);
	void RecordRoundResult(EBHPlayerRole Role, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase);
	// Records that the local player completed a train-intermission activity (type 0..3); earns Tourist once all four are done.
	void RecordTrainActivityUse(int32 ActivityIndex);
	// Records a graded answer (physics topic 0..3, correct?). Updates the answer streak / topic mask and earns
	// the Honor Roll / Polymath achievements. Cosmetic/local only; never affects scoring, mastery, or reports.
	void RecordQuestionResult(int32 TopicIndex, bool bCorrect);
	// Records one capture made as the Teacher; earns Truant Officer at the 50-capture milestone. Cosmetic/local.
	void RecordCapture();
	// Records one distinct locker-hide entry; earns Ghost in the Walls at 25 hides. Cosmetic/local.
	void RecordLockerHide();
	// Records one perfect momentum chain; earns Momentum Maestro at 10 chains. Cosmetic/local.
	void RecordPerfectChain();

	// Cosmetic achievements (local, persisted). UnlockAchievement is idempotent: the first time only, it awards
	// the achievement's XP, unlocks any tint gated on it, saves, and shows a one-line toast. Never affects play.
	void UnlockAchievement(FName AchievementId);
	bool HasAchievement(FName AchievementId) const;
	// Collectable relics (see ABHCollectable). Found-set is client-local + persisted; recording a new relic grants
	// XP and rolls up the milestone (25/50/100%) + Collector achievements. GetTotalCollectables() is the N/M target.
	void RecordCollectableFound(FName CollectableId);
	bool HasCollectable(FName CollectableId) const;
	int32 GetCollectableFoundCount() const;
	int32 GetTotalCollectables() const;

	// Every achievement with its metadata + whether THIS account has earned it, for the menu's Achievements tab.
	// Ordered hardest-last is handled by the caller; this preserves registry order.
	TArray<FBHAchievementDisplay> GetAchievementsForDisplay() const;
	// Earned / total counts for the tab header.
	void GetAchievementCounts(int32& OutEarned, int32& OutTotal) const;

	// "Newly unlocked" indicator: count of unlocked-but-unseen achievements, and a way to mark every current
	// unlock as seen (called when the Awards tab is opened so the badge / tab count clears).
	int32 GetUnseenAchievementCount() const;
	void MarkAchievementsSeen();

	const FBHAccountProfile& GetProfile() const;
	const FBHAccountProgress& GetProgress() const;
	const FString& GetLastAccountMessage() const;
	bool IsLoginPending() const;
	bool IsLoggedIn() const;
	bool HasLocalCredential() const;
	bool IsCosmeticUnlocked(EBHCosmeticCategory Category, int32 Index) const;
	int32 GetSelectedCosmeticIndex(EBHCosmeticCategory Category) const;
	bool SetSelectedCosmetic(EBHCosmeticCategory Category, int32 Index, FString& OutMessage);
	// Persist the selected UI menu theme (BHMenuTheme registry index). UI-only; saves immediately.
	void SetSelectedThemeIndex(int32 ThemeIndex);
	// Select + persist a menu theme, but only if it is unlocked for this account. Returns false (with OutMessage) when
	// locked, mirroring SetSelectedCosmetic's gate. Use this from the UI; SetSelectedThemeIndex stays the raw setter.
	bool SetSelectedThemeIndexChecked(int32 ThemeIndex, FString& OutMessage);
	// Persist the UI menu theme STRENGTH (0..1). UI-only; saves immediately.
	void SetSelectedThemeStrength(float Strength);
	// Persist the UI per-section CONTRAST (0..1). UI-only; saves immediately.
	void SetSelectedSectionContrast(float Contrast);
	// Recolour one clothing slot of the current skin. SlotIndex is a BHColorableMaterialNames registry index;
	// ColorIndex is into the 18-entry avatar palette, or -1 to clear back to the skin's authored colour. Persists.
	bool SetAvatarSlotColor(int32 SlotIndex, int32 ColorIndex, FString& OutMessage);
	FString GetCosmeticSummary() const;
	FString GetAccountSummary() const;

private:
	FString GetAccountDirectory() const;
	FString GetProfilePath() const;
	FString GetProgressPath() const;
	FString GetCredentialPath() const;
	FString GetBackendBaseUrl() const;
	FString MakeDeviceId();
	void EnsureDeviceId();
	void LaunchPendingProviderLogin();
	void StartLoginPolling();
	void StopLoginPolling();
	void SetLastAccountMessage(const FString& Message);
	void LoadProfile();
	void LoadProgress();
	void SaveProfile() const;
	void SaveProgress() const;
	void SanitizeProgressCosmetics();
	TSharedRef<FJsonObject> ProfileToJson() const;
	TSharedRef<FJsonObject> ProgressToJson() const;
	void ApplyProfileJson(const TSharedPtr<FJsonObject>& JsonObject);
	void ApplyProgressJson(const TSharedPtr<FJsonObject>& JsonObject);
	void ApplyBackendPlayerJson(const TSharedPtr<FJsonObject>& PlayerObject, const FString& SessionToken);
	void HandleProviderHealthResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleLoginPollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleSyncResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FBHAccountProfile Profile;
	FBHAccountProgress Progress;
	// Set when the on-disk progress file declares a profile_version newer than this build supports.
	// While locked we refuse to overwrite it, so a newer build's data is not clobbered by an older one.
	bool bProgressSaveLocked = false;
	FString LastAccountMessage;
	FString PendingProvider;
	bool bLoginPending = false;
	bool bLoginStartRequestInFlight = false;
	bool bLoginRequestInFlight = false;
	bool bSyncRequestInFlight = false;
	FTimerHandle LoginPollTimerHandle;

	// Wall-clock deadline (FPlatformTime::Seconds) after which the repeating login poll gives up, so an
	// abandoned OAuth flow that keeps returning "pending" can't poll the backend forever in the background.
	double LoginPollDeadlineSeconds = 0.0;
};

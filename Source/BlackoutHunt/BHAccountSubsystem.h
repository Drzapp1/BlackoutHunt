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

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	FString LastUpdatedUtc;

	// Cosmetic achievements earned (ids; see the registry in BHAccountSubsystem.cpp). Some unlock the hidden
	// prestige tints. Local + persisted; never affect gameplay.
	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Account")
	TArray<FName> UnlockedAchievements;
};

// One achievement's display data for the menu's Achievements tab, built from the registry + the local account's
// earned set. Plain struct (C++/Slate only -- not exposed to Blueprint).
struct FBHAchievementDisplay
{
	FName Id;
	FString Title;
	FString Description;
	FString RewardLabel;   // e.g. "Top Hat", "Afterimage tint" -- empty if the reward is XP only
	int32 Difficulty = 1;  // 1..5 (the badge's star meter + tier colour)
	bool bUnlocked = false;
	bool bHidden = false;  // a secret achievement: the tab shows "???" until it is earned
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

	// Cosmetic achievements (local, persisted). UnlockAchievement is idempotent: the first time only, it awards
	// the achievement's XP, unlocks any tint gated on it, saves, and shows a one-line toast. Never affects play.
	void UnlockAchievement(FName AchievementId);
	bool HasAchievement(FName AchievementId) const;

	// Every achievement with its metadata + whether THIS account has earned it, for the menu's Achievements tab.
	// Ordered hardest-last is handled by the caller; this preserves registry order.
	TArray<FBHAchievementDisplay> GetAchievementsForDisplay() const;
	// Earned / total counts for the tab header.
	void GetAchievementCounts(int32& OutEarned, int32& OutTotal) const;

	const FBHAccountProfile& GetProfile() const;
	const FBHAccountProgress& GetProgress() const;
	const FString& GetLastAccountMessage() const;
	bool IsLoginPending() const;
	bool IsLoggedIn() const;
	bool HasLocalCredential() const;
	bool IsCosmeticUnlocked(EBHCosmeticCategory Category, int32 Index) const;
	int32 GetSelectedCosmeticIndex(EBHCosmeticCategory Category) const;
	bool SetSelectedCosmetic(EBHCosmeticCategory Category, int32 Index, FString& OutMessage);
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

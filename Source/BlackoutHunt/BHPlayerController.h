#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "GameFramework/PlayerController.h"
#include "BHPlayerController.generated.h"

class APlayerState;
class UAudioComponent;
class USoundBase;
class SWindow;
class SWidget;

UCLASS()
class BLACKOUTHUNT_API ABHPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABHPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(Exec)
	void HostGame();

	UFUNCTION(Exec)
	void HostSubstationGame();

	UFUNCTION(Exec)
	void HostFoggroundsGame();

	UFUNCTION(Exec)
	void HostPracticeGame();

	UFUNCTION(Exec)
	void HostTestRound();

	UFUNCTION(Exec)
	void HostSubstationTestRound();

	UFUNCTION(Exec)
	void HostFoggroundsTestRound();

	UFUNCTION(Exec)
	void HostBotGame();

	UFUNCTION(Exec)
	void HostBotSubstationGame();

	UFUNCTION(Exec)
	void HostBotFoggroundsGame();

	UFUNCTION(Exec)
	void HostPhysicsClassroom();

	UFUNCTION(Exec)
	void JoinGame(const FString& Address);

	UFUNCTION(Exec)
	void HostOnlineGame();

	UFUNCTION(Exec)
	void HostOnlineSubstationGame();

	UFUNCTION(Exec)
	void HostOnlineFoggroundsGame();

	UFUNCTION(Exec)
	void FindOnlineGames();

	UFUNCTION(Exec)
	void JoinOnlineGame(int32 SessionIndex);

	UFUNCTION(Exec)
	void DestroyOnlineSession();

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

	UFUNCTION(Exec)
	void CreateGameHotspot();

	UFUNCTION(Exec)
	void StopGameHotspot();

	UFUNCTION(Exec)
	void StartInternetTunnel(int32 LocalPort = 7777);

	UFUNCTION(Exec)
	void StopInternetTunnel();

	UFUNCTION(Exec)
	void OpenInternetTunnelSetup(int32 LocalPort = 7777);

	UFUNCTION(Exec)
	void ForceStartRound();

	UFUNCTION(Exec)
	void ToggleClassroomBoard();

	UFUNCTION(Exec)
	void ShowClassroomBoard();

	UFUNCTION(Exec)
	void HideClassroomBoard();

	UFUNCTION(Exec)
	void ToggleHudMap();

	UFUNCTION(Exec)
	void SetHudMapVisible(bool bVisible);

	UFUNCTION(Exec)
	void CycleCrosshairStyle();

	UFUNCTION(Exec)
	void SetCrosshairStyle(int32 StyleIndex);

	UFUNCTION(Exec)
	void SetNextLevel(const FString& LevelName);

	UFUNCTION(Exec)
	void SetAvatar(int32 AvatarIndex);

	UFUNCTION(Exec)
	void SetAvatarColor(int32 ColorIndex);

	UFUNCTION(Exec)
	void SetAvatarHeadwear(int32 HeadwearIndex);

	UFUNCTION(Exec)
	void SetAvatarGear(int32 GearIndex);

	UFUNCTION(Exec)
	void VoteMap(const FString& LevelName);

	UFUNCTION(Exec)
	void VoteFogPreset(const FString& Preset);

	UFUNCTION(Exec)
	void SetFogPreset(const FString& Preset);

	UFUNCTION(Exec)
	void UseFogPresetVotes();

	UFUNCTION(Exec)
	void SetHunterCount(int32 HunterCount);

	UFUNCTION(Exec)
	void SetObjectiveIntensity(int32 Intensity);

	UFUNCTION(Exec)
	void SetBotCount(int32 BotCount);

	UFUNCTION(Exec)
	void SetBotDifficulty(const FString& Difficulty);

	UFUNCTION(Exec)
	void BotStatus();

	UFUNCTION(Exec)
	void BotDumpMemory();

	UFUNCTION(Exec)
	void BotNavCheck();

	UFUNCTION(Exec)
	void BotForceHunt();

	UFUNCTION(Exec)
	void BotSoak(const FString& LevelName, int32 Seconds, int32 BotCount);

	UFUNCTION(Exec)
	void SetPhysicsTopics(const FString& Topics);

	UFUNCTION(Exec)
	void SetRevisionDifficultyMix(const FString& DifficultyMix);

	UFUNCTION(Exec)
	void SetRevisionThresholds(float ClassPercent, float IndividualPercent);

	UFUNCTION(Exec)
	void SetScareIntensity(int32 Intensity);

	UFUNCTION(Exec)
	void ForceReview();

	UFUNCTION(Exec)
	void RevisionStatus();

	UFUNCTION(Exec)
	void ToggleInfectionMode();

	UFUNCTION(Exec)
	void TogglePaceMode();

	UFUNCTION(Exec)
	void RefreshPracticeRound();

	UFUNCTION(Exec)
	void TriggerPracticeJumpscare();

	void ShowMainMenu();
	void HideMainMenu();
	void ToggleMainMenu();
	void ReturnToMainMenu();
	void QuitGame();
	bool HostOnlineGameForMenu(const FString& LevelName, FString& OutMessage);
	bool HostPracticeGameForMenu(FString& OutMessage);
	bool HostTestRoundForMenu(const FString& LevelName, FString& OutMessage);
	bool HostPhysicsClassroomForMenu(FString& OutMessage);
	bool HostLiveClassroomForMenu(FString& OutMessage);
	bool HostBotGameForMenu(const FString& LevelName, FString& OutMessage);
	bool FindOnlineGamesForMenu(FString& OutMessage);
	bool JoinOnlineGameForMenu(int32 SessionIndex, FString& OutMessage);
	bool DestroyOnlineSessionForMenu(FString& OutMessage);
	bool ContinueAsGuestForMenu(FString& OutMessage);
	bool BeginAccountLoginForMenu(const FString& Provider, FString& OutMessage);
	bool PollAccountLoginForMenu(FString& OutMessage);
	bool SyncAccountForMenu(FString& OutMessage);
	bool SignOutAccountForMenu(FString& OutMessage);
	bool CreateOrUpdateLocalCredentialForMenu(const FString& Username, const FString& Password, FString& OutMessage);
	bool LoginLocalCredentialForMenu(const FString& Username, const FString& Password, FString& OutMessage);
	bool ForgetLocalCredentialForMenu(FString& OutMessage);
	bool ResetLocalClassroomDataForMenu(FString& OutMessage);
	bool OpenClassroomBoardForMenu(FString& OutMessage);
	bool CreateGameHotspotForMenu(FString& OutMessage);
	bool StopGameHotspotForMenu(FString& OutMessage);
	bool StartInternetTunnelForMenu(FString& OutMessage, int32 LocalPort = 7777);
	bool StopInternetTunnelForMenu(FString& OutMessage);
	bool OpenInternetTunnelSetupForMenu(FString& OutMessage, int32 LocalPort = 7777);
	bool SetNextLevelForMenu(const FString& LevelName, FString& OutMessage);
	bool VoteMapForMenu(const FString& LevelName, FString& OutMessage);
	bool VoteFogPresetForMenu(EBHFogPreset FogPreset, FString& OutMessage);
	bool SetFogPresetOverrideForMenu(EBHFogPreset FogPreset, FString& OutMessage);
	bool UseFogPresetVotesForMenu(FString& OutMessage);
	bool SetHunterCountForMenu(int32 HunterCount, FString& OutMessage);
	bool SetObjectiveIntensityForMenu(int32 Intensity, FString& OutMessage);
	bool SetBotCountForMenu(int32 BotCount, FString& OutMessage);
	bool SetBotDifficultyForMenu(EBHBotDifficulty Difficulty, FString& OutMessage);
	bool ToggleInfectionModeForMenu(FString& OutMessage);
	bool TogglePaceModeForMenu(FString& OutMessage);
	bool SetPracticeRoleForMenu(EBHPlayerRole NewRole, FString& OutMessage);
	bool SetPracticeModifierForMenu(EBHRoundModifier NewModifier, FString& OutMessage);
	bool RefreshPracticeRoundForMenu(FString& OutMessage);
	bool TriggerPracticeJumpscareForMenu(FString& OutMessage);
	bool TriggerTargetedJumpscareForMenu(APlayerState* TargetPlayerState, FString& OutMessage);
	bool CycleAvatarForMenu(FString& OutMessage);
	bool SetAvatarForMenu(int32 AvatarIndex, FString& OutMessage);
	bool SetAvatarColorForMenu(int32 ColorIndex, FString& OutMessage);
	bool SetAvatarHeadwearForMenu(int32 HeadwearIndex, FString& OutMessage);
	bool SetAvatarGearForMenu(int32 GearIndex, FString& OutMessage);
	bool SetPhysicsTopicsForMenu(const FString& TopicList, FString& OutMessage);
	bool SetRevisionDifficultyMixForMenu(EBHRevisionDifficultyMix DifficultyMix, FString& OutMessage);
	bool SetRevisionThresholdsForMenu(float ClassPercent, float IndividualPercent, FString& OutMessage);
	bool SetScareIntensityForMenu(int32 Intensity, FString& OutMessage);
	bool ForceReviewForMenu(FString& OutMessage);
	bool RevisionStatusForMenu(FString& OutMessage);
	bool ApplyGraphicsPresetForMenu(int32 Quality, FString& OutMessage);
	bool ApplyResolutionForMenu(int32 Width, int32 Height, bool bFullscreen, FString& OutMessage);
	bool ApplyFrameRateLimitForMenu(int32 FrameRateLimit, FString& OutMessage);
	void SetDesiredRole(APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole);
	bool KickPlayerForMenu(APlayerState* TargetPlayerState, FString& OutMessage);
	void ShowLocalStatusMessage(const FString& Message, float DurationSeconds = 3.0f);
	const FString& GetStatusMessage() const;
	bool HasActiveStatusMessage() const;
	float GetStatusMessageAlpha() const;
	void PlayMenuSelectionSound();
	float GetMasterVolumeForMenu() const;
	float GetMusicVolumeForMenu() const;
	float GetUiVolumeForMenu() const;
	void SetMasterVolumeForMenu(float Volume);
	void SetMusicVolumeForMenu(float Volume);
	void SetUiVolumeForMenu(float Volume);
	bool IsHudMapVisible() const;
	int32 GetCrosshairStyle() const;

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerForceStartRound();

	UFUNCTION(Server, Reliable)
	void ServerSetDesiredRole(APlayerState* TargetPlayerState, EBHPlayerRole DesiredRole);

	UFUNCTION(Server, Reliable)
	void ServerKickPlayer(APlayerState* TargetPlayerState);

	UFUNCTION(Server, Reliable)
	void ServerSetNextLevel(const FString& LevelName);

	UFUNCTION(Server, Reliable)
	void ServerSetAvatar(int32 AvatarIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetAvatarColor(const FLinearColor& AvatarColor, int32 ColorIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetAvatarHeadwear(int32 HeadwearIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetAvatarGear(int32 GearIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetMapVote(const FString& LevelName);

	UFUNCTION(Server, Reliable)
	void ServerSetFogPresetVote(EBHFogPreset FogPreset);

	UFUNCTION(Server, Reliable)
	void ServerSetFogPresetOverride(EBHFogPreset FogPreset);

	UFUNCTION(Server, Reliable)
	void ServerClearFogPresetOverride();

	UFUNCTION(Server, Reliable)
	void ServerSetTargetHunterCount(int32 HunterCount);

	UFUNCTION(Server, Reliable)
	void ServerSetObjectiveIntensity(int32 Intensity);

	UFUNCTION(Server, Reliable)
	void ServerSetBotCount(int32 BotCount);

	UFUNCTION(Server, Reliable)
	void ServerSetBotDifficulty(EBHBotDifficulty Difficulty);

	UFUNCTION(Server, Reliable)
	void ServerBotStatus();

	UFUNCTION(Server, Reliable)
	void ServerBotDumpMemory();

	UFUNCTION(Server, Reliable)
	void ServerBotNavCheck();

	UFUNCTION(Server, Reliable)
	void ServerBotForceHunt();

	UFUNCTION(Server, Reliable)
	void ServerBotSoak(const FString& LevelName, int32 Seconds, int32 BotCount);

	UFUNCTION(Server, Reliable)
	void ServerSetPhysicsTopics(const FString& Topics);

	UFUNCTION(Server, Reliable)
	void ServerSetRevisionDifficultyMix(const FString& DifficultyMix);

	UFUNCTION(Server, Reliable)
	void ServerSetRevisionThresholds(float ClassPercent, float IndividualPercent);

	UFUNCTION(Server, Reliable)
	void ServerSetScareIntensity(int32 Intensity);

	UFUNCTION(Server, Reliable)
	void ServerForceReview();

	UFUNCTION(Server, Reliable)
	void ServerRevisionStatus();

	UFUNCTION(Server, Reliable)
	void ServerToggleInfectionMode();

	UFUNCTION(Server, Reliable)
	void ServerTogglePaceMode();

	UFUNCTION(Server, Reliable)
	void ServerSetPracticeRole(EBHPlayerRole NewRole);

	UFUNCTION(Server, Reliable)
	void ServerSetPracticeModifier(EBHRoundModifier NewModifier);

	UFUNCTION(Server, Reliable)
	void ServerRefreshPracticeRound();

	UFUNCTION(Server, Reliable)
	void ServerTriggerPracticeJumpscare();

	UFUNCTION(Server, Reliable)
	void ServerTriggerTargetedJumpscare(APlayerState* TargetPlayerState);

	UFUNCTION(Client, Reliable)
	void ClientShowStatusMessage(const FString& Message, float DurationSeconds);

	UFUNCTION(Client, Reliable)
	void ClientSetJumpscareInputLocked(bool bLocked);

	UFUNCTION(Client, Reliable)
	void ClientSnapViewToFlatFocus(const FVector& FocusLocation);

	UFUNCTION(Client, Reliable)
	void ClientRecordRoundResult(EBHPlayerRole AccountRole, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase);

private:
	void ApplyGameplayInputMode();
	void EnsureAudioPreferencesLoaded();
	void SaveAudioPreference(const TCHAR* Key, float Value) const;
	void EnsureAmbientMusic();
	void UpdateAmbientMusic();
	bool ShouldPlayAmbientMusic() const;
	void ApplyAmbientMusicVolume();
	float GetEffectiveMusicVolume() const;
	float GetEffectiveUiVolume() const;
	USoundBase* GetAmbientMusicSound();
	USoundBase* GetMenuSelectionSound();
	bool IsLocalHostAdminContext() const;
	bool RequireLocalHostAdmin(FString& OutMessage, const TCHAR* ActionDescription);
	bool TryOpenClassroomBoardWindow(FString& OutMessage);
	void OnClassroomBoardWindowClosed(const TSharedRef<SWindow>& ClosedWindow);

	TSharedPtr<SWidget> MainMenuWidget;
	TSharedPtr<SWindow> ClassroomBoardWindow;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbientMusicComponent;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> AmbientMusicSound;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> MenuSelectionSound;
	FString StatusMessage;
	float StatusMessageStartTime = 0.0f;
	float StatusMessageEndTime = 0.0f;
	float StatusMessageDuration = 0.0f;
	float MasterVolume = 1.0f;
	float MusicVolume = 0.85f;
	float UiVolume = 0.9f;
	float LastMenuSelectionSoundTime = -100.0f;
	bool bHudMapVisible = false;
	int32 CrosshairStyle = 0;
	bool bAmbientMusicStarted = false;
	bool bAudioPreferencesLoaded = false;
};

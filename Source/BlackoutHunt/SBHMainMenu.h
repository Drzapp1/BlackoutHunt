#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class APlayerState;
class ABHPlayerController;
class SEditableTextBox;
class SVerticalBox;
class SWidgetSwitcher;
class SWidget;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;

class SBHMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBHMainMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABHPlayerController>, PlayerController)
	SLATE_END_ARGS()

	~SBHMainMenu();

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	enum class EBHMainMenuTab : uint8
	{
		Play = 0,
		Classroom,
		Character,
		Match,
		Network,
		Account,
		Controls,
		Settings
	};

	FReply OnStartClicked();
	FReply OnStartAccountClicked();
	FReply OnToggleStartCredentialsClicked();
	FReply OnHostClicked();
	FReply OnHostSubstationClicked();
	FReply OnHostFoggroundsClicked();
	FReply OnHostPracticeClicked();
	FReply OnHostTestRoundClicked();
	FReply OnHostSubstationTestRoundClicked();
	FReply OnHostFoggroundsTestRoundClicked();
	FReply OnHostPhysicsClassroomClicked();
	FReply OnHostLiveClassroomClicked();
	FReply OnHostBotClicked();
	FReply OnHostBotSubstationClicked();
	FReply OnHostBotFoggroundsClicked();
	FReply OnHostOnlineClicked();
	FReply OnHostOnlineSubstationClicked();
	FReply OnHostOnlineFoggroundsClicked();
	FReply OnFindOnlineSessionsClicked();
	FReply OnJoinFirstOnlineSessionClicked();
	FReply OnDestroyOnlineSessionClicked();
	FReply OnGuestAccountClicked();
	FReply OnGoogleLoginClicked();
	FReply OnMicrosoftLoginClicked();
	FReply OnSyncAccountClicked();
	FReply OnSignOutAccountClicked();
	FReply OnCreateLocalCredentialClicked(bool bFromStartScreen);
	FReply OnLoginLocalCredentialClicked(bool bFromStartScreen);
	FReply OnForgetLocalCredentialClicked();
	FReply OnResetLocalClassroomDataClicked();
	FReply OnCreateHotspotClicked();
	FReply OnStopHotspotClicked();
	FReply OnStartInternetTunnelClicked();
	FReply OnOpenInternetTunnelSetupClicked();
	FReply OnStopInternetTunnelClicked();
	FReply OnCopyJoinInviteClicked();
	FReply OnJoinClicked();
	FReply OnJoinLocalClicked();
	FReply OnResumeClicked();
	FReply OnForceStartClicked();
	FReply OnNextFacilityClicked();
	FReply OnNextSubstationClicked();
	FReply OnNextFoggroundsClicked();
	FReply OnVoteFacilityClicked();
	FReply OnVoteSubstationClicked();
	FReply OnVoteFoggroundsClicked();
	FReply OnFogVoteClicked(EBHFogPreset FogPreset);
	FReply OnFogOverrideClicked(EBHFogPreset FogPreset);
	FReply OnFogVoteModeClicked();
	FReply OnHunterCountClicked(int32 HunterCount);
	FReply OnObjectiveIntensityClicked(int32 Intensity);
	FReply OnBotCountClicked(int32 BotCount);
	FReply OnBotDifficultyClicked(EBHBotDifficulty Difficulty);
	FReply OnToggleInfectionClicked();
	FReply OnTogglePaceClicked();
	FReply OnPracticeRoleClicked(EBHPlayerRole NewRole);
	FReply OnPracticeModifierClicked(EBHRoundModifier NewModifier);
	FReply OnPracticeRefreshClicked();
	FReply OnPracticeJumpscareClicked();
	FReply OnAvatarClicked();
	FReply OnAvatarPresetClicked(int32 AvatarIndex);
	FReply OnAvatarColorClicked(int32 ColorIndex);
	FReply OnAvatarHeadwearClicked(int32 HeadwearIndex);
	FReply OnAvatarGearClicked(int32 GearIndex);
	FReply OnMenuTabClicked(EBHMainMenuTab NewTab);
	FReply OnOpenClassroomBoardClicked();
	FReply OnGraphicsPresetClicked(int32 Quality);
	FReply OnResolutionClicked(int32 Width, int32 Height, bool bFullscreen);
	FReply OnFrameRateClicked(int32 FrameRateLimit);
	FReply OnRevisionTopicsClicked(int32 TopicMask);
	FReply OnRevisionDifficultyClicked(EBHRevisionDifficultyMix DifficultyMix);
	FReply OnRevisionThresholdClicked(int32 ClassPercent, int32 IndividualPercent);
	FReply OnRevisionScareIntensityClicked(int32 Intensity);
	FReply OnForceReviewClicked();
	FReply OnRevisionStatusClicked();
	void OnMasterVolumeChanged(float Volume);
	void OnMusicVolumeChanged(float Volume);
	void OnUiVolumeChanged(float Volume);
	FReply OnAssignRoleClicked(TWeakObjectPtr<APlayerState> TargetPlayerState, EBHPlayerRole DesiredRole);
	FReply OnKickPlayerClicked(TWeakObjectPtr<APlayerState> TargetPlayerState);
	FReply OnTargetScareClicked(TWeakObjectPtr<APlayerState> TargetPlayerState);
	FReply OnDisconnectClicked();
	FReply OnQuitClicked();
	void OnAddressCommitted(const FText& Text, ETextCommit::Type CommitType);
	FText GetStatusText() const;
	FText GetAccountText() const;
	FText GetLocalCredentialStatusText() const;
	FText GetSuggestedAddressText() const;
	FText GetPlayerIdentityText() const;
	FText GetRoundDirectorText() const;
	FText GetOnlineSessionBrowserText() const;
	FText GetAvatarSummaryText() const;
	FText GetAvatarModelNameText() const;
	FSlateColor GetAvatarShirtColor() const;
	FSlateColor GetAvatarChestColor() const;
	FSlateColor GetAvatarPantsColor() const;
	FSlateColor GetAvatarSkinColor() const;
	FSlateColor GetAvatarHairColor() const;
	FSlateColor GetAvatarBadgeColor() const;
	FSlateColor GetAvatarPreviewAccentColor() const;
	FSlateColor GetMenuTabColor(EBHMainMenuTab Tab) const;
	FSlateColor GetMenuTabTextColor(EBHMainMenuTab Tab) const;
	float GetMasterVolumeValue() const;
	float GetMusicVolumeValue() const;
	float GetUiVolumeValue() const;
	FText GetMasterVolumeText() const;
	FText GetMusicVolumeText() const;
	FText GetUiVolumeText() const;
	EVisibility GetStartCredentialsVisibility() const;
	const FSlateBrush* GetStartBackgroundBrush() const;
	int32 GetRootWidgetIndex() const;
	bool CanJoinFirstOnlineSession() const;
	bool IsInNetworkedGame() const;
	bool IsPracticeMode() const;
	bool IsTestMode() const;
	bool CanEditRoles() const;
	bool CanOpenClassroomBoard() const;
	FString GetEnteredAddress() const;
	FString GetEnteredHost() const;
	int32 GetEnteredPort() const;
	void EnsureAvatarPreviewScene();
	void UpdateAvatarPreviewMesh();
	void DestroyAvatarPreviewScene();
	TSharedRef<SWidget> BuildMenuTabButton(EBHMainMenuTab Tab, const FText& Label);
	TSharedRef<SWidget> BuildStartScreen();
	void BuildPlayActionList(TSharedRef<SVerticalBox> ActionList, bool bInGame, bool bPracticeMode, bool bTestMode);
	TSharedRef<SWidget> BuildPlayJoinAddressPanel();
	TSharedRef<SWidget> BuildAccountPanel();
	TSharedRef<SWidget> BuildLocalCredentialPanel(bool bForStartScreen);
	TSharedRef<SWidget> BuildNetworkPanel();
	TSharedRef<SWidget> BuildStatusPanel();
	TSharedRef<SWidget> BuildCharacterCustomizationPanel();
	TSharedRef<SWidget> BuildAvatarPreview();
	TSharedRef<SWidget> BuildRoundOptionsPanel();
	TSharedRef<SWidget> BuildRevisionControlsPanel();
	TSharedRef<SWidget> BuildClassroomPanel();
	TSharedRef<SWidget> BuildControlsPanel();
	TSharedRef<SWidget> BuildGraphicsPanel();
	TSharedRef<SWidget> BuildPracticePanel();
	TSharedRef<SWidget> BuildRoleAssignmentPanel();

	void PlayMenuSelectionSound() const;
	static FString ResolveLocalAddress();
	static FString NormalizeAddress(FString Address);

	TWeakObjectPtr<ABHPlayerController> PlayerController;
	TSharedPtr<SEditableTextBox> AddressTextBox;
	TSharedPtr<SEditableTextBox> JoinHostTextBox;
	TSharedPtr<SEditableTextBox> JoinPortTextBox;
	TSharedPtr<SEditableTextBox> StartLocalUsernameTextBox;
	TSharedPtr<SEditableTextBox> StartLocalPasswordTextBox;
	TSharedPtr<SEditableTextBox> LocalUsernameTextBox;
	TSharedPtr<SEditableTextBox> LocalPasswordTextBox;
	TSharedPtr<SWidgetSwitcher> MainTabSwitcher;
	TWeakObjectPtr<AActor> AvatarPreviewActor;
	TWeakObjectPtr<USkeletalMeshComponent> AvatarPreviewMeshComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewHeadwearComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewHeadwearAccentComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewHeadwearDetailComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewGearComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewGearAccentComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewGearLeftStrapComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewGearRightStrapComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewGearDetailComponent;
	TWeakObjectPtr<USceneCaptureComponent2D> AvatarPreviewCaptureComponent;
	TStrongObjectPtr<UTextureRenderTarget2D> AvatarPreviewRenderTarget;
	TStrongObjectPtr<UTexture2D> StartBackgroundTexture;
	FSlateBrush StartBackgroundBrush;
	FSlateBrush StartBackgroundFallbackBrush;
	FSlateBrush AvatarPreviewBrush;
	FText StatusText;
	FString SuggestedAddress;
	FString LastAvatarPreviewMeshPath;
	FLinearColor LastAvatarPreviewColor = FLinearColor::Transparent;
	int32 LastAvatarPreviewHeadwearIndex = INDEX_NONE;
	int32 LastAvatarPreviewGearIndex = INDEX_NONE;
	EBHMainMenuTab ActiveMenuTab = EBHMainMenuTab::Play;
	bool bShowingStartScreen = false;
	bool bShowStartCredentials = false;
};

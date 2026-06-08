// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHCosmeticUnlocks.h"
#include "BHTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class APlayerState;
class ABHPlayerController;
class SCheckBox;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class SVerticalBox;
class SWidgetSwitcher;
class SBox;
class SWidget;
enum class EBHFeedbackKind : uint8;
struct FBHAchievementDisplay;
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

private:
	enum class EBHMainMenuTab : uint8
	{
		Play = 0,
		Guide,
		Classroom,
		Character,
		Match,
		Network,
		Account,
		Controls,
		Settings,
		Feedback,
		Achievements,
		Mastery
	};

	enum class EBHClassroomRunbookStep : uint8
	{
		Preflight,
		MapPreset,
		ManualQuestions,
		Tunnel,
		Students,
		Roles,
		ReadyGate,
		Warmup,
		Hunt,
		Board,
		Export
	};

	FReply OnStartClicked();
	FReply OnStartAccountClicked();
	FReply OnStartGuideClicked();
	FReply OnToggleStartCredentialsClicked();
	FReply OnHostClicked();
	FReply OnHostSubstationClicked();
	FReply OnHostFoggroundsClicked();
	FReply OnHostPracticeClicked();
	FReply OnHostTutorialClicked();
	// Launch a specific tutorial (Survivor / Teacher / Monitor). bChain=true is the full chained course.
	FReply OnHostTutorialPhaseClicked(EBHTutorialPhase StartPhase, bool bChain);
	FReply OnHostTestRoundClicked();
	FReply OnHostSubstationTestRoundClicked();
	FReply OnHostFoggroundsTestRoundClicked();
	FReply OnHostPhysicsClassroomClicked();
	FReply OnHostLiveClassroomClicked();
	FReply OnHostLiveClassroomMapClicked(FString LevelName);
	FReply OnHostSelectedLiveClassroomClicked();
	FReply OnSelectLiveClassroomMapClicked(FString LevelName);
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
	FReply OnFeedbackKindClicked(EBHFeedbackKind Kind);
	FReply OnFeedbackRatingClicked(int32 Rating);
	FReply OnSubmitFeedbackClicked();
	FReply OnSurveyOverallClicked(int32 Overall);
	FReply OnSurveyDifficultyClicked(FString Difficulty);
	FReply OnSurveyRecommendClicked(bool bRecommend);
	FReply OnSubmitSurveyClicked();
	FReply OnDismissSurveyClicked();
	FReply OnCreateHotspotClicked();
	FReply OnStopHotspotClicked();
	FReply OnStartInternetTunnelClicked();
	FReply OnOpenInternetTunnelSetupClicked();
	FReply OnStopInternetTunnelClicked();
	FReply OnCopyJoinInviteClicked();
	FReply OnJoinClicked();
	FReply OnJoinSavedEndpointClicked(FString Endpoint);
	FReply OnJoinLocalClicked();
	FReply OnReadyClicked();
	FReply OnSpectatorEncourageClicked();
	FReply OnSpectatorRolePreferenceClicked(EBHPlayerRole DesiredRole);
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
	FReply OnBotCountStepClicked(int32 Delta);
	FReply OnFillBotsClicked();
	FReply OnBotDifficultyClicked(EBHBotDifficulty Difficulty);
	FReply OnToggleInfectionClicked();
	FReply OnTogglePaceClicked();
	FReply OnPracticeRoleClicked(EBHPlayerRole NewRole);
	FReply OnPracticeModifierClicked(EBHRoundModifier NewModifier);
	FReply OnPracticeRefreshClicked();
	FReply OnPracticeJumpscareClicked();
	FReply OnJumpscareVariantClicked(FString VariantToken);
	FReply OnOpenAtmosphereConsoleClicked();
	FReply OnAtmosphereTestClicked(FString Command);
	FReply OnTesterResourcesClicked();
	FReply OnTesterTrainClicked();
	FReply OnTesterTrainPhaseClicked();
	FReply OnTesterFinalStationClicked();
	FReply OnTesterFinalEscapeClicked();
	FReply OnTesterFinalRecapClicked();
	FReply OnAvatarClicked();
	FReply OnAvatarPresetClicked(int32 AvatarIndex);
	FReply OnAvatarColorClicked(int32 ColorIndex);
	FReply OnAvatarHeadwearClicked(int32 HeadwearIndex);
	// Per-part recolour: pick a part of the current skin, then a colour to apply to just that part.
	FReply OnRecolorSlotSelected(int32 RegistryIndex);
	FReply OnRecolorSwatchClicked(int32 ColorIndex);
	TSharedRef<SWidget> BuildRecolorSection();
	// UI theme (menu colour scheme): a cosmetic, local UI preference. The picker sets the active theme; colours
	// routed through the getters below re-read it, so switching is instant.
	FReply OnThemeClicked(int32 ThemeIndex);
	TSharedRef<SWidget> BuildThemeSection();
	FSlateColor GetThemeBackgroundColor() const;
	FSlateColor GetThemePanelColor() const;
	// Theme unlock-gating UI helpers (mirror IsCosmeticUnlockedForMenu / GetCosmeticButtonText / GetCosmeticButtonColor).
	bool IsThemeUnlockedForMenu(int32 ThemeIndex) const;
	FText GetThemeButtonText(int32 ThemeIndex) const;
	FSlateColor GetThemeSwatchColor(int32 ThemeIndex) const;
	// Theme STRENGTH presets (faint/normal/strong): set the active global + persist, like OnThemeClicked.
	FReply OnThemeStrengthClicked(float Strength);
	FSlateColor GetThemeStrengthButtonColor(float Strength) const;
	FReply OnAvatarTitleClicked(int32 TitleIndex);
	FReply OnAvatarEmblemClicked(int32 EmblemIndex);
	FReply OnMenuTabClicked(EBHMainMenuTab NewTab);
	FReply OnOpenClassroomBoardClicked();
	FReply OnOpenClassroomSupportFolderClicked();
	FReply OnOpenClassroomLogFolderClicked();
	FReply OnOpenClassroomPackageFolderClicked();
	FReply OnOpenClassroomDeploymentGuideClicked();
	FReply OnCreateClassroomSupportBundleClicked();
	FReply OnRefreshClassroomPreflightClicked();
	FReply OnCopyClassroomJoinCodeClicked();
	FReply OnGraphicsPresetClicked(int32 Quality);
	FReply OnAutoGraphicsClicked();
	FReply OnAdaptiveGraphicsClicked(bool bEnabled);
	FReply OnAdaptiveFrameRateGoalClicked(int32 FpsGoal);
	FReply OnRenderScaleClicked(int32 Percent);
	FReply OnTextureQualityClicked(int32 Quality);
	FReply OnShadowQualityClicked(int32 Quality);
	FReply OnEffectsQualityClicked(int32 Quality);
	FReply OnResolutionClicked(int32 Width, int32 Height, bool bFullscreen);
	FReply OnFrameRateClicked(int32 FrameRateLimit);
	FReply OnComfortOptionClicked(FName OptionName, bool bEnabled);
	// Dodge-roll camera intensity (motion sickness): 0=Off,1=Subtle,2=Dip,3=Full. Persists [BlackoutHunt.Comfort] RollCamStyle.
	FReply OnRollCamStyleClicked(int32 Style);
	FReply OnRevisionTopicsClicked(int32 TopicMask);
	FReply OnRevisionDifficultyClicked(EBHRevisionDifficultyMix DifficultyMix);
	FReply OnRevisionThresholdClicked(int32 ClassPercent, int32 IndividualPercent);
	FReply OnRevisionScareIntensityClicked(int32 Intensity);
	FReply OnLessonPresetClicked(FString PresetId);
	FReply OnSaveLessonPresetClicked();
	FReply OnGenerateManualQuestionSetClicked();
	FReply OnForceReviewClicked();
	FReply OnRevisionStatusClicked();
	FReply OnExportRevisionReportClicked();
	FReply OnExportPlaytestTelemetryClicked();
	void OnMasterVolumeChanged(float Volume);
	void OnMusicVolumeChanged(float Volume);
	void OnUiVolumeChanged(float Volume);
	// HUD customization sliders (0..1 slider value mapped to the option's clamp range).
	// HudScale = widget/chrome size; HudTextScale = independent text size.
	float GetHudScaleValue() const;
	FText GetHudScaleText() const;
	void OnHudScaleChanged(float SliderValue);
	float GetHudTextScaleValue() const;
	FText GetHudTextScaleText() const;
	void OnHudTextScaleChanged(float SliderValue);
	// UI-size quick presets (Small/Default/Large/Huge) for the widget and text axes.
	FReply OnHudSizePresetClicked(FName OptionName, int32 Percent);
	FSlateColor GetHudSizePresetButtonColor(FName OptionName, int32 Percent) const;
	float GetHudPanelOpacityValue() const;
	FText GetHudPanelOpacityText() const;
	void OnHudPanelOpacityChanged(float SliderValue);
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
	FText GetDefaultJoinNameText() const;
	FText GetReadyButtonText() const;
	FText GetPlayerIdentityText() const;
	FText GetRoundDirectorText() const;
	FText GetOnlineSessionBrowserText() const;
	FText GetAvatarSummaryText() const;
	FText GetAvatarModelNameText() const;
	FText GetCosmeticProgressText() const;
	FText GetCosmeticButtonText(EBHCosmeticCategory Category, int32 Index) const;
	FSlateColor GetAvatarShirtColor() const;
	FSlateColor GetAvatarChestColor() const;
	FSlateColor GetAvatarPantsColor() const;
	FSlateColor GetAvatarSkinColor() const;
	FSlateColor GetAvatarHairColor() const;
	FSlateColor GetAvatarBadgeColor() const;
	FSlateColor GetAvatarPreviewAccentColor() const;
	FSlateColor GetCosmeticButtonColor(EBHCosmeticCategory Category, int32 Index) const;
	FSlateColor GetMenuTabColor(EBHMainMenuTab Tab) const;
	// Dynamic tab label: the base label, except the Awards tab appends the unlocked-but-unseen count.
	FText GetMenuTabLabel(EBHMainMenuTab Tab, FText BaseLabel) const;
	FSlateColor GetMenuTabTextColor(EBHMainMenuTab Tab) const;
	static int32 MenuTabToWidgetIndex(EBHMainMenuTab Tab);
	float GetMasterVolumeValue() const;
	float GetMusicVolumeValue() const;
	float GetUiVolumeValue() const;
	FText GetMasterVolumeText() const;
	FText GetMusicVolumeText() const;
	FText GetUiVolumeText() const;
	FText GetGraphicsSummaryText() const;
	FSlateColor GetComfortBoolOptionButtonColor(FName OptionName, bool bValue) const;
	FSlateColor GetRollCamStyleButtonColor(int32 Style) const;
	EVisibility GetStartCredentialsVisibility() const;
	const FSlateBrush* GetStartBackgroundBrush() const;
	const FSlateBrush* GetGuideHudBrush() const;
	int32 GetRootWidgetIndex() const;
	bool CanJoinFirstOnlineSession() const;
	bool IsInNetworkedGame() const;
	bool IsPracticeMode() const;
	bool IsTestMode() const;
	bool CanEditRoles() const;
	bool CanUseClassroomHostControls() const;
	bool CanEditRevisionControls() const;
	bool CanManageLessonPresets() const;
	bool CanOpenClassroomBoard() const;
	bool CanReadyFromMenu() const;
	bool IsCosmeticUnlockedForMenu(EBHCosmeticCategory Category, int32 Index) const;
	FString GetEnteredAddress() const;
	FString GetEnteredHost() const;
	int32 GetEnteredPort() const;
	bool EnsureJoinDisplayName();
	void SetJoinAddressFields(const FString& Address);
	FString ResolvePreferredAddress() const;
	FText GetLiveClassroomMapText() const;
	FSlateColor GetLiveClassroomMapButtonColor(FString LevelName) const;
	EVisibility GetClassroomHostOnlyVisibility() const;
	FText GetClassroomPreflightStatusText() const;
	FText GetClassroomPreflightText() const;
	FSlateColor GetClassroomPreflightStatusColor() const;
	FText GetClassroomRunbookStepStatusText(EBHClassroomRunbookStep Step) const;
	FSlateColor GetClassroomRunbookStepStatusColor(EBHClassroomRunbookStep Step) const;
	bool CanRunbookStartHunt() const;
	bool CanRunbookExportReport() const;
	EVisibility GetRevisionControlsVisibility() const;
	FText GetRevisionControlsSummaryText() const;
	FSlateColor GetRevisionControlsSummaryColor() const;
	FSlateColor GetRevisionTopicButtonColor(int32 TopicMask) const;
	FSlateColor GetRevisionDifficultyButtonColor(EBHRevisionDifficultyMix DifficultyMix) const;
	FSlateColor GetRevisionThresholdButtonColor(int32 ClassPercent, int32 IndividualPercent) const;
	FSlateColor GetRevisionScareButtonColor(int32 Intensity) const;
	EVisibility GetLessonPresetPanelVisibility() const;
	FText GetLessonPresetSummaryText() const;
	FSlateColor GetLessonPresetSummaryColor() const;
	FSlateColor GetLessonPresetButtonColor(FString PresetId) const;
	FSlateColor GetGraphicsBoolOptionButtonColor(FName OptionName, bool bValue) const;
	FSlateColor GetGraphicsOptionButtonColor(FName OptionName, int32 Value) const;
	FSlateColor GetGraphicsResolutionButtonColor(int32 Width, int32 Height, bool bFullscreen) const;
	void EnsureAvatarPreviewScene();
	void UpdateAvatarPreviewMesh();
	void DestroyAvatarPreviewScene();
	TSharedRef<SWidget> BuildMenuTabButton(EBHMainMenuTab Tab, const FText& Label);
	TSharedRef<SWidget> BuildStartScreen();
	void BuildPlayActionList(TSharedRef<SVerticalBox> ActionList, bool bInGame, bool bPracticeMode, bool bTestMode);
	TSharedRef<SWidget> BuildPlayJoinAddressPanel();
	TSharedRef<SWidget> BuildClassroomJoinListPanel();
	TSharedRef<SWidget> BuildAccountPanel();
	TSharedRef<SWidget> BuildLocalCredentialPanel(bool bForStartScreen);
	TSharedRef<SWidget> BuildFeedbackPanel();
	TSharedRef<SWidget> BuildAchievementsPanel();
	// Revision tab: the player's per-topic mastery (from the locally-saved account) plus a browser of
	// example questions for every topic and difficulty level.
	TSharedRef<SWidget> BuildMasteryPanel();
	TSharedRef<SWidget> BuildEndOfRoundSurveyPanel();
	FSlateColor GetFeedbackKindButtonColor(EBHFeedbackKind Kind) const;
	FSlateColor GetFeedbackRatingButtonColor(int32 Rating) const;
	FSlateColor GetSurveyOverallButtonColor(int32 Overall) const;
	FSlateColor GetSurveyDifficultyButtonColor(FString Difficulty) const;
	FSlateColor GetSurveyRecommendButtonColor(bool bRecommend) const;
	FText GetFeedbackStatusText() const;
	ECheckBoxState GetIncludeDiagnosticsState() const;
	void OnIncludeDiagnosticsChanged(ECheckBoxState NewState);
	TSharedRef<SWidget> BuildNetworkPanel();
	TSharedRef<SWidget> BuildStatusPanel();
	TSharedRef<SWidget> BuildCharacterCustomizationPanel();
	TSharedRef<SWidget> BuildAvatarPreview();
	TSharedRef<SWidget> BuildRoundOptionsPanel();
	TSharedRef<SWidget> BuildLessonPresetPanel();
	TSharedRef<SWidget> BuildRevisionControlsPanel();
	TSharedRef<SWidget> BuildClassroomPreflightPanel();
	TSharedRef<SWidget> BuildClassroomRunbookPanel();
	TSharedRef<SWidget> BuildClassroomRunbookStep(int32 StepNumber, EBHClassroomRunbookStep Step, const FText& Title, const FText& ActionLabel, const FOnClicked& OnClicked, TAttribute<bool> ActionEnabled);
	TSharedRef<SWidget> BuildClassroomPanel();
	TSharedRef<SWidget> BuildGuidePanel();
	TSharedRef<SWidget> BuildControlsPanel();
	TSharedRef<SWidget> BuildGraphicsPanel();
	TSharedRef<SWidget> BuildPracticePanel();
	TSharedRef<SWidget> BuildTestCommandPanel();
	TSharedRef<SWidget> BuildRoleAssignmentPanel();

	void PlayMenuSelectionSound() const;
	void RefreshLessonPresetList();
	static FString ResolveLocalAddress();
	static FString NormalizeAddress(FString Address);

	TWeakObjectPtr<ABHPlayerController> PlayerController;
	TSharedPtr<SEditableTextBox> AddressTextBox;
	TSharedPtr<SEditableTextBox> JoinHostTextBox;
	TSharedPtr<SEditableTextBox> JoinPortTextBox;
	TSharedPtr<SEditableTextBox> JoinNameTextBox;
	TSharedPtr<SEditableTextBox> StartLocalUsernameTextBox;
	TSharedPtr<SEditableTextBox> StartLocalPasswordTextBox;
	TSharedPtr<SEditableTextBox> LocalUsernameTextBox;
	TSharedPtr<SEditableTextBox> LocalPasswordTextBox;
	TSharedPtr<SEditableTextBox> LessonPresetNameTextBox;
	TSharedPtr<SVerticalBox> LessonPresetListBox;
	TSharedPtr<SWidgetSwitcher> MainTabSwitcher;
	// Awards-tab content holder so it can be rebuilt on tab-open (refreshing the "newly unlocked" markers + stats).
	TSharedPtr<SBox> AchievementsPanelContainer;
	TWeakObjectPtr<AActor> AvatarPreviewActor;
	TWeakObjectPtr<USkeletalMeshComponent> AvatarPreviewMeshComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewHeadwearComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewHeadwearAccentComponent;
	TWeakObjectPtr<UStaticMeshComponent> AvatarPreviewHeadwearDetailComponent;
	TWeakObjectPtr<USceneCaptureComponent2D> AvatarPreviewCaptureComponent;
	TStrongObjectPtr<UTextureRenderTarget2D> AvatarPreviewRenderTarget;
	TStrongObjectPtr<UTexture2D> StartBackgroundTexture;
	TStrongObjectPtr<UTexture2D> GuideHudTexture;
	FSlateBrush StartBackgroundBrush;
	FSlateBrush StartBackgroundFallbackBrush;
	FSlateBrush GuideHudBrush;
	FSlateBrush GuideHudFallbackBrush;
	FSlateBrush AvatarPreviewBrush;
	FText StatusText;
	FString SuggestedAddress;
	FString SelectedLiveClassroomMap = TEXT("Facility");
	FString LastAvatarPreviewMeshPath;
	FLinearColor LastAvatarPreviewColor = FLinearColor::Transparent;
	int32 LastAvatarPreviewHeadwearIndex = INDEX_NONE;
	EBHMainMenuTab ActiveMenuTab = EBHMainMenuTab::Play;
	bool bShowingStartScreen = false;
	bool bShowStartCredentials = false;
	// --- atmospheric-horror animation state (UI-only; advanced in Tick, read by start-screen visuals via
	// TAttribute lambdas). No gameplay/replication. ---
	float HorrorTimeAccum = 0.0f;   // seconds the menu has been alive
	float FlickerAlpha = 1.0f;      // ~0.78..1.0 failing-light multiplier (accent/title)
	float GrainPhase = 0.0f;        // 0..1 grain shimmer phase
	float VignettePulse = 1.0f;     // ~0.92..1.0 slow vignette "breath"
	// Progress through the Konami code (easter egg, cosmetic). See Docs/EASTER_EGGS.md.
	int32 KonamiProgress = 0;
	// Per-part recolour: which skin material slot (a BHColorableMaterialNames registry index) the swatches recolour.
	int32 RecolorSelectedSlot = INDEX_NONE;
	// Rebuildable container for the recolour parts, so switching skin re-enumerates the new skin's slots.
	TSharedPtr<SBox> RecolorSectionBox;
	// When the local player is the listen-server host, leaving ends the whole class session,
	// so the first LEAVE press only arms a confirm prompt; the second press actually leaves.
	bool bLeaveConfirmPending = false;

	// Feedback / bug report / survey form state.
	TSharedPtr<SMultiLineEditableTextBox> FeedbackMessageTextBox;
	TSharedPtr<SEditableTextBox> FeedbackContactTextBox;
	TSharedPtr<SMultiLineEditableTextBox> SurveyCommentTextBox;
	EBHFeedbackKind PendingFeedbackKind;
	int32 PendingFeedbackRating = 0;
	bool bIncludeDiagnostics = true;
	int32 SurveyOverallRating = 0;
	FString SurveyDifficulty;
	bool bSurveyWouldRecommend = true;
};

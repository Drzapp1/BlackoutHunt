// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "BHHudTheme.h"
#include "BHQuestionInteraction.h"
#include "GameFramework/HUD.h"
#include "BHHUD.generated.h"

class ABHCharacter;
class UFont;

UCLASS()
class BLACKOUTHUNT_API ABHHUD : public AHUD
{
	GENERATED_BODY()

public:
	ABHHUD();

	virtual void DrawHUD() override;

	// Hit-test the most recently drawn question panel against a viewport-pixel cursor position
	// (matches APlayerController::GetMousePosition). Called by the local character's mouse handlers;
	// returns false when nothing interactive is under the cursor.
	bool GetQuestionRegionAtCursor(const FVector2D& ScreenPos, FBHQuestionHitRegion& OutRegion) const;

protected:
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& FillColor, const FLinearColor& AccentColor);
	void DrawCornerBrackets(float X, float Y, float W, float H, const FLinearColor& Color, float Length = 14.0f, float Thickness = 1.0f);
	void DrawCircle(float CenterX, float CenterY, float Radius, const FLinearColor& Color, float Thickness = 1.0f, int32 Segments = 36);
	void DrawKeyBox(const FString& Key, float X, float Y, float W, float H, const FLinearColor& AccentColor, bool bLit = true);
	void DrawStatusPill(const FString& Label, float X, float Y, float W, const FLinearColor& AccentColor, bool bLit = true);
	void DrawHudText(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font = nullptr, float Scale = 1.0f) const;
	void DrawRightAlignedText(const FString& Text, float RightX, float Y, const FLinearColor& Color, const UFont* Font = nullptr, float Scale = 1.0f) const;
	float DrawWrappedHudText(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, const UFont* Font = nullptr, float Scale = 1.0f, float LineHeight = 16.0f, int32 MaxLines = 2) const;
	void DrawProgressBar(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, const FString& ValueText = FString());
	void DrawRawMeter(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, bool bHighIsBad = false);
	void DrawVisibleHunterArrow(const ABHCharacter* Character, const FVector& HunterLocation, float DistanceCm, float CueStrength);
	void DrawCCTVRevealMarker(const ABHCharacter* Character, const class ABHPlayerController* PlayerController);
	void DrawNodeMarker(const ABHCharacter* Character, const class ABHPlayerController* PlayerController);
	void DrawCrosshair(float DangerAlpha = 0.0f);
	void DrawHorrorOverlay(const ABHCharacter* Character, const class ABHGameState* GameState);
	void DrawHeatSensor(const ABHCharacter* Character, const class ABHGameState* GameState, float X, float Y);
	void DrawObjectiveBeats(const ABHCharacter* Character, const class ABHGameState* GameState);
	void DrawInteractionPrompt(ABHCharacter* Character);
	// Bottom-left readout for the minigame the local player is at (blackjack hand/chips, or the chess board
	// oriented for their colour + whose move). Driven by ABHPlayerState::ActiveMinigameTable.
	void DrawMinigameStatus(ABHCharacter* Character);
	// "Press O to return to the cabin" banner shown while the local player is up on the train roof (Z > 320).
	void DrawRoofPrompt(ABHCharacter* Character);
	void DrawNearbyNameTags(const ABHCharacter* Character);
	void DrawEquipmentStrip(const class ABHGameState* GameState);
	void DrawSpectatorSupportPanel(const class ABHGameState* GameState, const class ABHPlayerState* PlayerState);
	// Lobby-only roster: lists every connected player and bot with ready state, so a small group can
	// see who is in and how many bots are filling the empty slots before the host starts the round.
	void DrawLobbyRoster(const class ABHGameState* GameState, const class ABHPlayerState* LocalPlayerState);
	void DrawQuestionPanel(const class ABHObjectiveStation* Station);
	void DrawRevisionDiagram(const class ABHObjectiveStation* Station, float X, float Y, float W, float H, float S = 1.0f);
	// Dev/QA overlay: when the bh.Diagrams.PreviewType console var is set, draws a sample of that
	// diagram type in the corner so every diagram can be eyeballed in-game without a quiz node.
	void DrawDiagramPreview();
	// Resolve (and cache) an optional illustrated diagram texture by object path. Returns
	// null when the path is empty or the asset is missing/uncooked, so the caller falls
	// back to the procedural diagram.
	class UTexture2D* ResolveDiagramTexture(const FString& ObjectPath);
	void DrawPhaseBanner(const class ABHGameState* GameState, const ABHCharacter* Character);
	// One-shot role onboarding card shown at warmup start (driven by ClientShowRoleIntro).
	void DrawRoleIntroCard(const class ABHPlayerController* BHPC, const class ABHGameState* GameState);
	// Full-screen tutorial transition snapshot (dark wash + centred title/body), driven by PC card state.
	void DrawTutorialCard(const class ABHPlayerController* BHPC);
	// Tutorial step guidance on its own banner at the top of the screen (its own channel, so noise/round
	// status messages in the shared toast can never cut it off).
	void DrawTutorialPrompt(const class ABHPlayerController* BHPC);

	// Refresh cached per-frame UI preferences (palette, scale, opacity, element toggles)
	// from the owning player controller. Called once at the top of DrawHUD().
	void RefreshHudPreferences(const class ABHPlayerController* PlayerController, const class ABHGameState* GameState);

	// Semantic colour accessors backed by the resolved ActivePalette. MainText/MutedText
	// keep their historical names so existing call sites need no edits.
	FLinearColor MainText() const;
	FLinearColor MutedText() const;

	// Clickable regions of the question panel in viewport pixels, rebuilt every frame inside
	// DrawQuestionPanel so the geometry always matches what is drawn at the current HudScale.
	TArray<FBHQuestionHitRegion> QuestionHitRegions;

	// Object-path -> loaded diagram texture, populated lazily by ResolveDiagramTexture.
	TMap<FString, TWeakObjectPtr<class UTexture2D>> DiagramTextureCache;

	// Object paths known to be absent/uncooked. A TWeakObjectPtr can't cache a null, so without this
	// a missing teacher-authored diagram path would trigger a synchronous LoadObject every HUD frame.
	TSet<FString> MissingDiagramTexturePaths;

	// Per-frame cached UI preferences (refreshed in RefreshHudPreferences).
	BHHudTheme::FBHHudPalette ActivePalette = BHHudTheme::MakeStandardPalette();
	// Independent player-set UI sizes (see the "UI Size" editor in the settings menu).
	//   HudWidgetScale  scales widget/chrome GEOMETRY: meter bars, the minimap box, equipment pills,
	//                   roster/panel boxes and the readout spacing. Edge-anchored elements are re-pinned
	//                   from their scaled size so they stay on-screen.
	//   HudTextScale    scales TEXT/labels everywhere (it is injected at the text draw helpers).
	// Self-contained centered popups (question/checkpoint, banners, role/tutorial cards, prompt, toast)
	// scale geometry+text together by max(widget,text) so the box always grows to fit enlarged text.
	// Both default to 1.0, which reproduces the original HUD pixel-for-pixel.
	float HudWidgetScale = 1.0f;
	float HudTextScale = 1.0f;
	float HudPanelOpacity = 1.0f;
	bool bColorblindPalette = false;
	bool bShowMinimap = true;
	bool bShowNameplates = true;
	bool bShowVitals = true;
	bool bShowObjectiveBeats = true;
	bool bShowThreatArrow = true;
	bool bShowCrosshairDanger = true;

	EBHRoundPhase LastSeenPhase;
	bool bHasSeenPhase;
	float PhaseBannerEndTime;
	// One-shot teaching flags (SHARED-D): each fires once per session, then the HUD defers to its
	// steady-state readouts. Pure client-side; never touch gameplay state, stats, or reports.
	bool bTaughtFear;
	bool bTaughtStamina;
	bool bTaughtTeacherNear;
	bool bTaughtQuestionFormat;
	bool bTaughtGuideAccess;
	bool bTaughtSpectator;
	// One-time "the faculty remembers" easter-egg greeting (cosmetic, client-local). See Docs/EASTER_EGGS.md.
	bool bShownPhysicistGreeting = false;
	int32 LastSeenPresencePulse;
	float PresencePulseEndTime;
	bool bHasVisibleHunterCue;
	FVector LastVisibleHunterLocation;
	float LastVisibleHunterDistanceCm;
	float VisibleHunterCueUntilTime;
	float SmoothedVisibleHunterArrowX;
};

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "BHHudTheme.h"
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
	void DrawCrosshair(float DangerAlpha = 0.0f);
	void DrawHorrorOverlay(const ABHCharacter* Character, const class ABHGameState* GameState);
	void DrawHeatSensor(const ABHCharacter* Character, const class ABHGameState* GameState, float X, float Y);
	void DrawObjectiveBeats(const ABHCharacter* Character, const class ABHGameState* GameState);
	void DrawInteractionPrompt(ABHCharacter* Character);
	void DrawNearbyNameTags(const ABHCharacter* Character);
	void DrawEquipmentStrip(const class ABHGameState* GameState);
	void DrawSpectatorSupportPanel(const class ABHGameState* GameState, const class ABHPlayerState* PlayerState);
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

	// Refresh cached per-frame UI preferences (palette, scale, opacity, element toggles)
	// from the owning player controller. Called once at the top of DrawHUD().
	void RefreshHudPreferences(const class ABHPlayerController* PlayerController, const class ABHGameState* GameState);

	// Semantic colour accessors backed by the resolved ActivePalette. MainText/MutedText
	// keep their historical names so existing call sites need no edits.
	FLinearColor MainText() const;
	FLinearColor MutedText() const;

	// Object-path -> loaded diagram texture, populated lazily by ResolveDiagramTexture.
	TMap<FString, TWeakObjectPtr<class UTexture2D>> DiagramTextureCache;

	// Per-frame cached UI preferences (refreshed in RefreshHudPreferences).
	BHHudTheme::FBHHudPalette ActivePalette = BHHudTheme::MakeStandardPalette();
	// Readability scale applied to the centered, self-contained panels (question/checkpoint
	// and phase/announcement banners). Scoped to centered panels because they recompute their
	// X from width and stay on-screen at any scale; edge-anchored telemetry is left at 1.0.
	float HudScale = 1.0f;
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
	int32 LastSeenPresencePulse;
	float PresencePulseEndTime;
	bool bHasVisibleHunterCue;
	FVector LastVisibleHunterLocation;
	float LastVisibleHunterDistanceCm;
	float VisibleHunterCueUntilTime;
	float SmoothedVisibleHunterArrowX;
};

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
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
	void DrawRevisionDiagram(const class ABHObjectiveStation* Station, float X, float Y, float W, float H);
	void DrawPhaseBanner(const class ABHGameState* GameState, const ABHCharacter* Character);

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

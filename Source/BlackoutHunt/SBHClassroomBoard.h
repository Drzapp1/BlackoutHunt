// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABHGameState;
class ABHPlayerController;
class ABHPlayerState;
class SVerticalBox;
class SWidget;

class SBHClassroomBoard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBHClassroomBoard) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABHPlayerController>, PlayerController)
		SLATE_ARGUMENT(bool, bStandaloneWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	const ABHGameState* GetBHGameState() const;
	void RebuildPlayerRows();
	void RebuildTopicBars();
	FString BuildRosterSignature() const;
	TSharedRef<SWidget> BuildMetricCard(const FText& Label, const TAttribute<FText>& Value, const FLinearColor& AccentColor) const;

	FText GetSessionText() const;
	FText GetRosterText() const;
	FText GetRoleMixText() const;
	FText GetObjectiveText() const;
	FText GetRevisionText() const;
	FText GetJoinText() const;

	EVisibility GetHuntStatusVisibility() const;
	FText GetInPlayText() const;
	FText GetCaughtText() const;
	FText GetEscapedText() const;

	EVisibility GetPresenceVisibility() const;
	TOptional<float> GetPresencePercent() const;
	FText GetPresenceLabelText() const;

	TWeakObjectPtr<ABHPlayerController> PlayerController;
	TSharedPtr<SVerticalBox> TopicBarsBox;
	FString LastRosterSignature;
	float RosterSignatureCheckAccumulator = 0.0f;
	bool bStandaloneWindow = false;
};

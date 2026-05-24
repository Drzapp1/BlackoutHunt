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
	FString BuildRosterSignature() const;
	TSharedRef<SWidget> BuildMetricCard(const FText& Label, const TAttribute<FText>& Value, const FLinearColor& AccentColor) const;
	TSharedRef<SWidget> BuildPlayerHeader() const;
	TSharedRef<SWidget> BuildPlayerRow(const ABHPlayerState* PlayerState, int32 RowIndex) const;

	FText GetSessionText() const;
	FText GetRosterText() const;
	FText GetRoleMixText() const;
	FText GetObjectiveText() const;
	FText GetRevisionText() const;
	FText GetJoinText() const;

	TWeakObjectPtr<ABHPlayerController> PlayerController;
	TSharedPtr<SVerticalBox> PlayerRowsBox;
	FString LastRosterSignature;
	bool bStandaloneWindow = false;
};

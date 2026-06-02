// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "BHTrainDisplayActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EBHTrainDisplayProfile : uint8
{
	Standard UMETA(DisplayName = "Standard"),
	TrainWallRecap UMETA(DisplayName = "Train Wall Recap"),
	StationCountdownCompact UMETA(DisplayName = "Station Countdown Compact")
};

UCLASS()
class BLACKOUTHUNT_API ABHTrainDisplayActor : public AActor
{
	GENERATED_BODY()

public:
	ABHTrainDisplayActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ConfigureDisplay(const FString& NewHeader, const FString& NewBody, const FLinearColor& AccentColor);
	void ConfigureExitCountdownDisplay(const FString& NewStationName, const FString& NewDestinationText, const FLinearColor& AccentColor);
	void ConfigureTrainPhaseDisplay(const FString& NewDestinationText, const FLinearColor& AccentColor);
	void SetDisplayProfile(EBHTrainDisplayProfile NewProfile);
	void SetBodyText(const FString& NewBody);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Train")
	FString GetBodyText() const;

protected:
	UFUNCTION()
	void OnRep_DisplayText();

	UFUNCTION()
	void OnRep_DisplayProfile();

	void ApplyText();
	void ApplyDisplayProfile();
	void UpdateExitCountdownDisplay(bool bForce);
	void UpdateTrainPhaseDisplay(bool bForce);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackingPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ScreenFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ScreenBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeaderRailFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeaderRailBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FooterRailFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FooterRailBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TopFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BottomFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftHanger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightHanger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> HeaderText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> BodyText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> HeaderTextBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> BodyTextBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> FrontReadLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> BackReadLight;

	UPROPERTY(ReplicatedUsing = OnRep_DisplayText)
	FString DisplayHeader;

	UPROPERTY(ReplicatedUsing = OnRep_DisplayText)
	FString DisplayBody;

	UPROPERTY(ReplicatedUsing = OnRep_DisplayText)
	FLinearColor Accent;

	UPROPERTY(ReplicatedUsing = OnRep_DisplayProfile)
	EBHTrainDisplayProfile DisplayProfile;

	FString CountdownStationName;
	FString CountdownDestinationText;
	FString TrainPhaseDestinationText;
	float NextCountdownRefreshTime;
	float NextTrainPhaseRefreshTime;
	int32 LastCountdownSeconds;
	int32 LastCountdownBreakersCompleted;
	int32 LastCountdownBreakersRequired;
	int32 LastCountdownSideObjectivesCompleted;
	int32 LastCountdownSideObjectivesRequired;
	int32 LastTrainPhaseCountdownSeconds;
	EBHTrainPhase LastTrainDisplayPhase;
	FString LastTrainAnnouncement;
	FString LastTrainDestination;
	bool bExitCountdownDisplay;
	bool bLastCountdownExitUnlocked;
	bool bTrainPhaseDisplay;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "BHTrainDisplayActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class USceneComponent;

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
	void SetBodyText(const FString& NewBody);

	UFUNCTION(BlueprintPure, Category = "Blackout Hunt|Train")
	FString GetBodyText() const;

protected:
	UFUNCTION()
	void OnRep_DisplayText();

	void ApplyText();
	void UpdateExitCountdownDisplay(bool bForce);

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

	FString CountdownStationName;
	FString CountdownDestinationText;
	float NextCountdownRefreshTime;
	int32 LastCountdownSeconds;
	int32 LastCountdownBreakersCompleted;
	int32 LastCountdownBreakersRequired;
	int32 LastCountdownSideObjectivesCompleted;
	int32 LastCountdownSideObjectivesRequired;
	bool bExitCountdownDisplay;
	bool bLastCountdownExitUnlocked;
};

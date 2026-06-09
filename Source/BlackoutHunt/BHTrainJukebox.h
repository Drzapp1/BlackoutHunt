// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "BHInteractableActor.h"
#include "BHTrainJukebox.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class UPointLightComponent;
class USoundBase;

// A train-lobby jukebox of CC0 lofi loops. TAP to cycle YOUR OWN track: off -> 1 -> 2 -> ... -> off. The
// selection is PER-PERSON (stored on ABHPlayerState::JukeboxTrackIndex, owner-only replicated): each client
// plays only its own chosen track, spatialised so it fades along the train, with a glowing front that pulses
// while playing. The jukebox plays ONLY inside the train Lobby -- it is silent everywhere else.
UCLASS()
class BLACKOUTHUNT_API ABHTrainJukebox : public ABHInteractableActor
{
	GENERATED_BODY()

public:
	ABHTrainJukebox();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract_Implementation(ABHCharacter* Character) const override;
	virtual void BeginInteract_Implementation(ABHCharacter* Character) override;
	virtual FText GetInteractionLabel_Implementation(ABHCharacter* Character) const override;
	virtual FBHInteractionPromptInfo GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const override;

	void GetHudLinesForPlayer(int32 PlayerId, TArray<FString>& OutLines, FLinearColor& OutColor) const;

protected:
	void BuildTrackList();
	void ApplyAudio();
	void ApplyVisuals();
	// The track THIS client's local player has selected (-1 = off), read from their PlayerState. Returns -1 when
	// not in the train Lobby, so the jukebox stays silent everywhere else.
	int32 GetLocalSelectedTrack() const;
	// True only while this client is inside the train Lobby.
	bool IsInLobby() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Arch;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Speaker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> MusicAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	// Resolved playlist (whichever candidate assets exist). NOT replicated -- the per-person selection lives on
	// ABHPlayerState::JukeboxTrackIndex, so each client hears only its own choice.
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Tracks;
	TArray<FString> TrackNames;
	// What MusicAudio currently reflects on this client, so ApplyAudio only (re)starts on a real change.
	int32 AppliedTrackIndex = -2;
};

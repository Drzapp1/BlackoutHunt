// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHDoor.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
constexpr float BHDoorSlamTeacherInterruptRange = 380.0f;
constexpr float BHDoorSlamTeacherRecoverySeconds = 1.20f;
}

ABHDoor::ABHDoor()
{
	bOpen = false;
	InteractionLabel = FText::FromString(TEXT("Door"));
	SetActorScale3D(FVector(0.15f, 1.35f, 2.6f));
}

void ABHDoor::BeginPlay()
{
	Super::BeginPlay();
	// GetActorRotation() is the door's current pose. On a late-joining client the replicated spawn transform
	// is the server's CURRENT pose, which may already be open (a survivor opened the door before this client
	// connected), so back out the open offset to recover the true closed pose. OnRep_Open is suppressed until
	// bClosedRotationCaptured is set, so this reads the clean spawn transform rather than a value a premature
	// OnRep wrote from the zero-default baseline.
	ClosedRotation = bOpen ? GetActorRotation() - FRotator(0.0f, 90.0f, 0.0f) : GetActorRotation();
	bClosedRotationCaptured = true;
	ApplyDoorState();
}

void ABHDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHDoor, bOpen);
}

void ABHDoor::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bWasOpen = bOpen;
	SetOpen(!bOpen);
	if (bWasOpen && !bOpen && Character)
	{
		if (ABHPlayerState* DoorUserPS = Character->GetPlayerState<ABHPlayerState>())
		{
			if (DoorUserPS->IsAliveSurvivor())
			{
				UWorld* World = GetWorld();
				if (!World)
				{
					return;
				}

				if (ABHGameMode* BHGM = World->GetAuthGameMode<ABHGameMode>())
				{
					BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("door slam"));
				}

				bool bInterruptedTeacher = false;
				for (TActorIterator<ABHCharacter> It(World); It; ++It)
				{
					ABHCharacter* OtherCharacter = *It;
					const ABHPlayerState* OtherPS = OtherCharacter ? OtherCharacter->GetPlayerState<ABHPlayerState>() : nullptr;
					if (!OtherCharacter || OtherCharacter == Character || !OtherPS || !OtherPS->IsAliveHunter())
					{
						continue;
					}

					if (FVector::DistSquared2D(OtherCharacter->GetActorLocation(), GetActorLocation()) > FMath::Square(BHDoorSlamTeacherInterruptRange))
					{
						continue;
					}

					if (OtherCharacter->InterruptTeacherCaptureAttack(TEXT("Door slam broke your swing. Recovering."), BHDoorSlamTeacherRecoverySeconds))
					{
						bInterruptedTeacher = true;
					}
				}

				if (bInterruptedTeacher)
				{
					if (ABHPlayerController* DoorUserPC = Cast<ABHPlayerController>(Character->GetController()))
					{
						DoorUserPC->ClientShowStatusMessage(TEXT("Door slam interrupted the Teacher. It made noise."), 3.0f);
					}
				}
			}
		}
	}
}

FText ABHDoor::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	return bOpen ? FText::FromString(TEXT("Close Door")) : FText::FromString(TEXT("Open Door"));
}

void ABHDoor::SetOpen(bool bNewOpen)
{
	bOpen = bNewOpen;
	ApplyDoorState();
}

bool ABHDoor::IsOpen() const
{
	return bOpen;
}

void ABHDoor::OnRep_Open()
{
	// For a dynamically-replicated door, OnRep can arrive before BeginPlay. Defer until BeginPlay has
	// captured the closed pose; its own ApplyDoorState() then renders the correct state.
	if (!bClosedRotationCaptured)
	{
		return;
	}
	ApplyDoorState();
}

void ABHDoor::ApplyDoorState()
{
	SetActorRotation(bOpen ? ClosedRotation + FRotator(0.0f, 90.0f, 0.0f) : ClosedRotation);
}

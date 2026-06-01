#include "BHTrainDoor.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FString BHTrainDoorClock(float EndServerTime, const UWorld* World)
{
	const AGameStateBase* BaseGameState = World ? World->GetGameState() : nullptr;
	const float Now = BaseGameState ? BaseGameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
	const int32 RemainingSeconds = FMath::Max(0, FMath::CeilToInt(EndServerTime - Now));
	return FString::Printf(TEXT("%02d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60);
}

FString BHTrainDoorMotionLabel(bool bOpen, float DoorOpenAlpha)
{
	if (bOpen && DoorOpenAlpha < 0.92f)
	{
		return TEXT("Opening");
	}
	if (!bOpen && DoorOpenAlpha > 0.08f)
	{
		return TEXT("Closing");
	}
	return bOpen ? TEXT("Open") : TEXT("Closed");
}
}

ABHTrainDoor::ABHTrainDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(8.0f);

	InteractionLabel = FText::FromString(TEXT("Train Door"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetHiddenInGame(true);

	LeftPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPanel"));
	LeftPanel->SetupAttachment(SceneRoot);
	LeftPanel->SetRelativeLocation(FVector(0.0f, -42.0f, 72.0f));
	LeftPanel->SetRelativeScale3D(FVector(0.16f, 0.82f, 1.48f));
	LeftPanel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	LeftPanel->SetCollisionResponseToAllChannels(ECR_Block);

	RightPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPanel"));
	RightPanel->SetupAttachment(SceneRoot);
	RightPanel->SetRelativeLocation(FVector(0.0f, 42.0f, 72.0f));
	RightPanel->SetRelativeScale3D(FVector(0.16f, 0.82f, 1.48f));
	RightPanel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RightPanel->SetCollisionResponseToAllChannels(ECR_Block);

	HeaderLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeaderLight"));
	HeaderLight->SetupAttachment(SceneRoot);
	HeaderLight->SetRelativeLocation(FVector(-8.0f, 0.0f, 174.0f));
	HeaderLight->SetRelativeScale3D(FVector(0.08f, 1.9f, 0.08f));
	HeaderLight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		LeftPanel->SetStaticMesh(CubeMesh.Object);
		RightPanel->SetStaticMesh(CubeMesh.Object);
		HeaderLight->SetStaticMesh(CubeMesh.Object);
	}

	EscapeVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("EscapeVolume"));
	EscapeVolume->SetupAttachment(SceneRoot);
	EscapeVolume->SetRelativeLocation(FVector(72.0f, 0.0f, 86.0f));
	EscapeVolume->SetBoxExtent(FVector(110.0f, 128.0f, 118.0f));
	EscapeVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EscapeVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	EscapeVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EscapeVolume->OnComponentBeginOverlap.AddDynamic(this, &ABHTrainDoor::OnEscapeVolumeBeginOverlap);

	DoorBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBlocker"));
	DoorBlocker->SetupAttachment(SceneRoot);
	DoorBlocker->SetRelativeLocation(FVector(0.0f, 0.0f, 86.0f));
	DoorBlocker->SetBoxExtent(FVector(52.0f, 158.0f, 122.0f));
	DoorBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorBlocker->SetCollisionResponseToAllChannels(ECR_Block);

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(SceneRoot);
	StatusLight->SetRelativeLocation(FVector(-40.0f, 0.0f, 178.0f));
	StatusLight->SetAttenuationRadius(520.0f);
	StatusLight->SetIntensity(600.0f);

	bOpen = false;
	bEscapeDoor = false;
	DoorName = TEXT("Train Door");
	DoorOpenAlpha = 0.0f;
	HeaderLightMaterial = nullptr;
}

void ABHTrainDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EscapeVolume)
	{
		EscapeVolume->OnComponentBeginOverlap.RemoveDynamic(this, &ABHTrainDoor::OnEscapeVolumeBeginOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

void ABHTrainDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyDoorVisuals(DeltaSeconds);
}

void ABHTrainDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHTrainDoor, bOpen);
	DOREPLIFETIME(ABHTrainDoor, bEscapeDoor);
	DOREPLIFETIME(ABHTrainDoor, DoorName);
}

bool ABHTrainDoor::CanInteract_Implementation(ABHCharacter* Character) const
{
	return CanEscapeThroughDoor(Character);
}

void ABHTrainDoor::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority())
	{
		EscapeCharacter(Character);
	}
}

FText ABHTrainDoor::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (bEscapeDoor)
	{
		if (CanEscapeThroughDoor(Character))
		{
			return FText::FromString(TEXT("Board Evacuation Train"));
		}

		if (BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::Cutscene)
		{
			return FText::FromString(TEXT("Evacuation Door Unlocking"));
		}
		if (BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive && bOpen)
		{
			return FText::FromString(TEXT("Survivor Evacuation Door"));
		}
		if (BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::Departed)
		{
			return FText::FromString(TEXT("Evacuation Train Departed"));
		}
		if (BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::Failed)
		{
			return FText::FromString(TEXT("Evacuation Door Sealed"));
		}
		return FText::FromString(TEXT("Evacuation Door Locked"));
	}

	if (BHGS && BHGS->TrainPhase == EBHTrainPhase::Departing)
	{
		return FText::FromString(FString::Printf(TEXT("%s Departing"), *DoorName));
	}
	return FText::FromString(FString::Printf(TEXT("%s %s"), *DoorName, *BHTrainDoorMotionLabel(bOpen, DoorOpenAlpha)));
}

FBHInteractionPromptInfo ABHTrainDoor::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	if (Info.bCanInteract)
	{
		Info.RiskText = FText::FromString(bEscapeDoor ? TEXT("BOARD FINAL TRAIN") : TEXT("BOARD NOW"));
		return Info;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!bEscapeDoor)
	{
		if (bOpen)
		{
			Info.DisabledReason = FText::FromString(TEXT("WALK THROUGH - NO E NEEDED"));
		}
		else if (BHGS && BHGS->TrainPhase == EBHTrainPhase::Departing)
		{
			Info.DisabledReason = FText::FromString(TEXT("DEPARTING / DOORS CLOSED"));
		}
		else if (BHGS && (BHGS->TrainPhase == EBHTrainPhase::Recap || BHGS->TrainPhase == EBHTrainPhase::BonusQuestion || BHGS->TrainPhase == EBHTrainPhase::Shop))
		{
			Info.DisabledReason = FText::FromString(TEXT("TRAIN MOVING / DOORS CLOSED"));
		}
		else
		{
			Info.DisabledReason = FText::FromString(TEXT("DOORS CLOSED"));
		}
	}
	else if (!bOpen)
	{
		if (BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::Cutscene)
		{
			Info.DisabledReason = FText::FromString(FString::Printf(TEXT("UNLOCKING %s / TEACHER HELD"), *BHTrainDoorClock(BHGS->FinalEscapeCutsceneEndServerTime, GetWorld())));
		}
		else if (BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::Departed)
		{
			Info.DisabledReason = FText::FromString(TEXT("TRAIN DEPARTED"));
		}
		else if (BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::Failed)
		{
			Info.DisabledReason = FText::FromString(TEXT("EVACUATION FAILED"));
		}
		else
		{
			Info.DisabledReason = FText::FromString(TEXT("DOOR CLOSED"));
		}
	}
	else if (!BHGS || BHGS->FinalEscapeState != EBHFinalEscapeState::EscapeActive)
	{
		Info.DisabledReason = FText::FromString(TEXT("FINAL ESCAPE NOT ACTIVE"));
	}
	else if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		Info.DisabledReason = FText::FromString(TEXT("READY UP FIRST"));
	}
	else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		Info.DisabledReason = FText::FromString(TEXT("HALL MONITORS CANNOT BOARD"));
	}
	else if (BHPS->IsAliveHunter())
	{
		Info.DisabledReason = FText::FromString(TEXT("TEACHER CANNOT BOARD"));
	}
	else
	{
		Info.DisabledReason = FText::FromString(TEXT("SURVIVOR EXIT ONLY"));
	}
	return Info;
}

void ABHTrainDoor::ConfigureDoor(bool bNewEscapeDoor, const FString& NewDoorName)
{
	bEscapeDoor = bNewEscapeDoor;
	DoorName = NewDoorName;
}

void ABHTrainDoor::SetDoorOpen(bool bNewOpen)
{
	bOpen = bNewOpen;
	ApplyDoorVisuals(0.0f);
}

bool ABHTrainDoor::IsDoorOpen() const
{
	return bOpen;
}

bool ABHTrainDoor::IsEscapeDoor() const
{
	return bEscapeDoor;
}

void ABHTrainDoor::OnRep_DoorState()
{
	ApplyDoorVisuals(0.0f);
}

void ABHTrainDoor::OnEscapeVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (HasAuthority())
	{
		EscapeCharacter(Cast<ABHCharacter>(OtherActor));
	}
}

void ABHTrainDoor::ApplyDoorVisuals(float DeltaSeconds)
{
	const float TargetAlpha = bOpen ? 1.0f : 0.0f;
	DoorOpenAlpha = DeltaSeconds > 0.0f
		? FMath::FInterpTo(DoorOpenAlpha, TargetAlpha, DeltaSeconds, 3.5f)
		: TargetAlpha;

	// Blocking collision follows the replicated authoritative open state, NOT the interpolated
	// DoorOpenAlpha. The server snaps bOpen instantly, so a client must clear the doorway the moment
	// bOpen replicates; gating collision on alpha>0.92 left the doorway solid for the ~0.3s slide and
	// bounced survivors sprinting for a just-opened door. Only the cosmetic panel slide stays on alpha.
	const ECollisionEnabled::Type BlockingCollision = bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics;
	if (LeftPanel)
	{
		LeftPanel->SetRelativeLocation(FVector(0.0f, FMath::Lerp(-42.0f, -116.0f, DoorOpenAlpha), 72.0f));
		LeftPanel->SetCollisionEnabled(BlockingCollision);
	}
	if (RightPanel)
	{
		RightPanel->SetRelativeLocation(FVector(0.0f, FMath::Lerp(42.0f, 116.0f, DoorOpenAlpha), 72.0f));
		RightPanel->SetCollisionEnabled(BlockingCollision);
	}
	if (DoorBlocker)
	{
		DoorBlocker->SetCollisionEnabled(BlockingCollision);
	}
	// A door still sliding (DoorOpenAlpha mid-range) is "in motion"; without this a closing door snaps
	// to a static red light and reads as jammed during the escape. Closing pulses a touch slower than
	// opening so the two directions stay distinguishable, and settles to steady once fully shut.
	const bool bMoving = DoorOpenAlpha > 0.06f && DoorOpenAlpha < 0.94f;
	const float TimeNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FLinearColor OpenColor(0.12f, 1.0f, 0.48f, 1.0f);
	const FLinearColor ShutColor(1.0f, 0.10f, 0.06f, 1.0f);
	if (HeaderLight)
	{
		if (!HeaderLightMaterial)
		{
			HeaderLightMaterial = HeaderLight->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (HeaderLightMaterial)
		{
			const FLinearColor Color = bOpen ? OpenColor : ShutColor;
			const float PulseHz = bOpen ? 5.5f : 4.0f;
			const float Pulse = (bOpen || bMoving) ? 1.0f + 0.18f * FMath::Sin(TimeNow * PulseHz) : 1.0f;
			const float EmissiveScale = bOpen ? (3.5f * Pulse) : (bMoving ? 2.8f * Pulse : 2.2f);
			HeaderLightMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			HeaderLightMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
			HeaderLightMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * EmissiveScale);
		}
	}
	if (StatusLight)
	{
		const float Pulse = (bOpen || bMoving) ? 0.5f + 0.5f * FMath::Sin(TimeNow * (bOpen ? 5.5f : 4.0f)) : 0.0f;
		StatusLight->SetLightColor((bOpen ? OpenColor : ShutColor).ToFColor(true));
		const float ShutIntensity = bMoving ? 760.0f + Pulse * 200.0f : (bEscapeDoor ? 660.0f : 520.0f);
		StatusLight->SetIntensity(bOpen ? 900.0f + Pulse * 260.0f : ShutIntensity);
	}
}

bool ABHTrainDoor::CanEscapeThroughDoor(ABHCharacter* Character) const
{
	if (!bEscapeDoor || !bOpen || !Character)
	{
		return false;
	}

	const ABHPlayerState* BHPS = Character->GetBHPlayerState();
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHPS && BHPS->IsAliveSurvivor() && BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive;
}

void ABHTrainDoor::EscapeCharacter(ABHCharacter* Character)
{
	if (!CanEscapeThroughDoor(Character))
	{
		return;
	}

	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->NotifySurvivorEscaped(Character);
	}
}

#include "BHTrainDoor.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

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
	if (bEscapeDoor)
	{
		return CanEscapeThroughDoor(Character) ? FText::FromString(TEXT("Board Evacuation Train")) : FText::FromString(TEXT("Escape Door Locked"));
	}
	return bOpen ? FText::FromString(TEXT("Doors Open")) : FText::FromString(TEXT("Doors Closed"));
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

	if (LeftPanel)
	{
		LeftPanel->SetRelativeLocation(FVector(0.0f, FMath::Lerp(-42.0f, -116.0f, DoorOpenAlpha), 72.0f));
		LeftPanel->SetCollisionEnabled(DoorOpenAlpha > 0.92f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	if (RightPanel)
	{
		RightPanel->SetRelativeLocation(FVector(0.0f, FMath::Lerp(42.0f, 116.0f, DoorOpenAlpha), 72.0f));
		RightPanel->SetCollisionEnabled(DoorOpenAlpha > 0.92f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	if (DoorBlocker)
	{
		DoorBlocker->SetCollisionEnabled(DoorOpenAlpha > 0.92f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	if (HeaderLight)
	{
		UMaterialInstanceDynamic* DynamicMaterial = HeaderLight->CreateAndSetMaterialInstanceDynamic(0);
		if (DynamicMaterial)
		{
			const FLinearColor Color = bOpen ? FLinearColor(0.12f, 1.0f, 0.48f, 1.0f) : FLinearColor(1.0f, 0.10f, 0.06f, 1.0f);
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
			DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 3.0f);
		}
	}
	if (StatusLight)
	{
		StatusLight->SetLightColor((bOpen ? FLinearColor(0.12f, 1.0f, 0.48f, 1.0f) : FLinearColor(1.0f, 0.10f, 0.06f, 1.0f)).ToFColor(true));
		StatusLight->SetIntensity(bOpen ? 1050.0f : 520.0f);
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

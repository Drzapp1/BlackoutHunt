#include "BHAlarmTrap.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHSynthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABHAlarmTrap::ABHAlarmTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;
	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 10.0f;
	MinNetUpdateFrequency = 2.0f;
	InitialLifeSpan = 55.0f;
	AgeSeconds = 0.0f;
	bTriggered = false;

	TriggerRadius = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerRadius"));
	SetRootComponent(TriggerRadius);
	TriggerRadius->InitSphereRadius(145.0f);
	TriggerRadius->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerRadius->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerRadius->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerRadius->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(TriggerRadius);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -42.0f));
	Mesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.08f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Synth = CreateDefaultSubobject<UBHSynthComponent>(TEXT("Synth"));
	Synth->SetupAttachment(TriggerRadius);
	Synth->Configure(115.0f, 0.09f, 0.02f, 1.2f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void ABHAlarmTrap::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
	}

	if (Synth && GetNetMode() != NM_DedicatedServer)
	{
		Synth->Start();
	}
}

void ABHAlarmTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AgeSeconds += DeltaSeconds;
	if (Mesh)
	{
		const float Pulse = 1.0f + FMath::Sin(AgeSeconds * 5.5f) * 0.10f;
		Mesh->SetRelativeScale3D(FVector(0.28f * Pulse, 0.28f * Pulse, 0.08f));
	}
}

void ABHAlarmTrap::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!HasAuthority() || bTriggered)
	{
		return;
	}

	ABHCharacter* Survivor = Cast<ABHCharacter>(OtherActor);
	ABHPlayerState* SurvivorPS = Survivor ? Survivor->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!Survivor || !SurvivorPS || !SurvivorPS->IsAliveSurvivor() || Survivor->IsHiddenInLocker())
	{
		return;
	}

	bTriggered = true;
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Survivor->GetController()))
	{
		PC->ClientShowStatusMessage(TEXT("You tripped a fake seeker trap."), 2.75f);
	}

	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("trap alarm"));
	}

	Destroy();
}

#include "BHCrawlSpaceVolume.h"
#include "BHCharacter.h"
#include "BHPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABHCrawlSpaceVolume::ABHCrawlSpaceVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = false;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	SetRootComponent(Volume);
	Volume->SetBoxExtent(FVector(320.0f, 125.0f, 155.0f));
	Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
	Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Volume->SetGenerateOverlapEvents(true);
	Volume->SetCanEverAffectNavigation(false);
	Volume->SetHiddenInGame(true);
}

void ABHCrawlSpaceVolume::BeginPlay()
{
	Super::BeginPlay();
	if (Volume)
	{
		Volume->OnComponentBeginOverlap.AddDynamic(this, &ABHCrawlSpaceVolume::OnVolumeBeginOverlap);
	}
}

void ABHCrawlSpaceVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !Volume)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	Volume->GetOverlappingActors(OverlappingActors, ABHCharacter::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		if (ABHCharacter* Character = Cast<ABHCharacter>(Actor))
		{
			if (!CanCharacterUseCrawlSpace(Character))
			{
				RejectCharacter(Character);
			}
		}
	}
}

void ABHCrawlSpaceVolume::Configure(const FVector& NewBoxExtent)
{
	if (Volume)
	{
		Volume->SetBoxExtent(NewBoxExtent);
	}
}

void ABHCrawlSpaceVolume::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ABHCharacter* Character = Cast<ABHCharacter>(OtherActor))
	{
		if (!CanCharacterUseCrawlSpace(Character))
		{
			RejectCharacter(Character);
		}
	}
}

bool ABHCrawlSpaceVolume::CanCharacterUseCrawlSpace(const ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	return BHPS
		&& (BHPS->PlayerRole == EBHPlayerRole::Survivor || BHPS->PlayerRole == EBHPlayerRole::Tester)
		&& BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHCrawlSpaceVolume::RejectCharacter(ABHCharacter* Character)
{
	if (!Character || !Volume)
	{
		return;
	}

	const FTransform VolumeTransform = GetActorTransform();
	FVector LocalLocation = VolumeTransform.InverseTransformPosition(Character->GetActorLocation());
	const FVector Extent = Volume->GetScaledBoxExtent();
	const float CapsuleRadius = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;
	const float Direction = LocalLocation.X >= 0.0f ? 1.0f : -1.0f;
	LocalLocation.X = Direction * (Extent.X + CapsuleRadius + 34.0f);
	LocalLocation.Y = FMath::Clamp(LocalLocation.Y, -Extent.Y + CapsuleRadius, Extent.Y - CapsuleRadius);

	FVector RejectedLocation = VolumeTransform.TransformPosition(LocalLocation);
	RejectedLocation.Z = Character->GetActorLocation().Z;
	Character->SetActorLocation(RejectedLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
}

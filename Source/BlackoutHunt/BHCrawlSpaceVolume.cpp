#include "BHCrawlSpaceVolume.h"
#include "BHCharacter.h"
#include "BHGameState.h"
#include "BHPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
constexpr int32 BHCrawlRejectAxisX = 0;
constexpr int32 BHCrawlRejectAxisY = 1;
constexpr float BHCrawlRejectPadding = 34.0f;
constexpr float BHCrawlRejectVelocityThreshold = 12.0f;

float BHClampToCrawlInnerExtent(float Value, float Extent, float CapsuleRadius)
{
	const float InnerExtent = FMath::Max(0.0f, Extent - CapsuleRadius);
	return FMath::Clamp(Value, -InnerExtent, InnerExtent);
}

FIntPoint BHChooseCrawlRejectDirection(const FVector& LocalLocation, const FVector& LocalVelocity, const FVector& Extent)
{
	const float AbsVelocityX = FMath::Abs(LocalVelocity.X);
	const float AbsVelocityY = FMath::Abs(LocalVelocity.Y);
	if (FMath::Max(AbsVelocityX, AbsVelocityY) >= BHCrawlRejectVelocityThreshold)
	{
		if (AbsVelocityX >= AbsVelocityY)
		{
			return FIntPoint(BHCrawlRejectAxisX, LocalVelocity.X >= 0.0f ? -1 : 1);
		}
		return FIntPoint(BHCrawlRejectAxisY, LocalVelocity.Y >= 0.0f ? -1 : 1);
	}

	const float DistanceToXFace = FMath::Max(0.0f, Extent.X - FMath::Abs(LocalLocation.X));
	const float DistanceToYFace = FMath::Max(0.0f, Extent.Y - FMath::Abs(LocalLocation.Y));
	if (DistanceToXFace <= DistanceToYFace)
	{
		return FIntPoint(BHCrawlRejectAxisX, LocalLocation.X >= 0.0f ? 1 : -1);
	}
	return FIntPoint(BHCrawlRejectAxisY, LocalLocation.Y >= 0.0f ? 1 : -1);
}
}

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
		Volume->OnComponentEndOverlap.AddDynamic(this, &ABHCrawlSpaceVolume::OnVolumeEndOverlap);
	}
}

void ABHCrawlSpaceVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Volume)
	{
		Volume->OnComponentBeginOverlap.RemoveDynamic(this, &ABHCrawlSpaceVolume::OnVolumeBeginOverlap);
		Volume->OnComponentEndOverlap.RemoveDynamic(this, &ABHCrawlSpaceVolume::OnVolumeEndOverlap);
	}

	ActiveRejectDirections.Reset();
	Super::EndPlay(EndPlayReason);
}

void ABHCrawlSpaceVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !Volume)
	{
		return;
	}

	for (auto It = ActiveRejectDirections.CreateIterator(); It; ++It)
	{
		ABHCharacter* CachedCharacter = It.Key().Get();
		if (!CachedCharacter || !Volume->IsOverlappingActor(CachedCharacter))
		{
			It.RemoveCurrent();
		}
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

void ABHCrawlSpaceVolume::OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ABHCharacter* Character = Cast<ABHCharacter>(OtherActor))
	{
		ActiveRejectDirections.Remove(TWeakObjectPtr<ABHCharacter>(Character));
	}
}

bool ABHCrawlSpaceVolume::CanCharacterUseCrawlSpace(const ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}

	if (BHPS->PlayerRole == EBHPlayerRole::Survivor || BHPS->PlayerRole == EBHPlayerRole::Tester)
	{
		return true;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->bTestMode;
}

void ABHCrawlSpaceVolume::RejectCharacter(ABHCharacter* Character)
{
	if (!Character || !Volume)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector IncomingVelocity = Movement ? Movement->Velocity : Character->GetVelocity();
	if (Movement)
	{
		Movement->StopMovementImmediately();
	}

	const FTransform VolumeTransform = Volume->GetComponentTransform();
	const FVector CurrentLocation = Character->GetActorLocation();
	FVector LocalLocation = VolumeTransform.InverseTransformPosition(Character->GetActorLocation());
	const FVector LocalVelocity = VolumeTransform.InverseTransformVectorNoScale(IncomingVelocity);
	const FVector Extent = Volume->GetScaledBoxExtent();
	const float CapsuleRadius = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;

	const TWeakObjectPtr<ABHCharacter> CharacterKey(Character);
	const FIntPoint* CachedRejectDirection = ActiveRejectDirections.Find(CharacterKey);
	const FIntPoint RejectDirection = CachedRejectDirection
		? *CachedRejectDirection
		: BHChooseCrawlRejectDirection(LocalLocation, LocalVelocity, Extent);
	ActiveRejectDirections.Add(CharacterKey, RejectDirection);

	const float DirectionSign = RejectDirection.Y >= 0 ? 1.0f : -1.0f;
	if (RejectDirection.X == BHCrawlRejectAxisY)
	{
		LocalLocation.X = BHClampToCrawlInnerExtent(LocalLocation.X, Extent.X, CapsuleRadius);
		LocalLocation.Y = DirectionSign * (Extent.Y + CapsuleRadius + BHCrawlRejectPadding);
	}
	else
	{
		LocalLocation.X = DirectionSign * (Extent.X + CapsuleRadius + BHCrawlRejectPadding);
		LocalLocation.Y = BHClampToCrawlInnerExtent(LocalLocation.Y, Extent.Y, CapsuleRadius);
	}

	FVector RejectedLocation = VolumeTransform.TransformPosition(LocalLocation);
	RejectedLocation.Z = CurrentLocation.Z;

	FHitResult SweepHit;
	Character->SetActorLocation(RejectedLocation, true, &SweepHit, ETeleportType::None);

	if (Movement)
	{
		Movement->StopMovementImmediately();
	}

	if (!Volume->IsOverlappingActor(Character))
	{
		ActiveRejectDirections.Remove(CharacterKey);
	}
}

// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHCrawlSpaceVolume.h"
#include "BHCharacter.h"
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

bool BHIsCrawlLowProfileState(EBHMovementSpecialState State)
{
	return State == EBHMovementSpecialState::Prone
		|| State == EBHMovementSpecialState::Sliding
		|| State == EBHMovementSpecialState::Diving;
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
			if (!TryAdmitOrAutoProne(Character))
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

FVector ABHCrawlSpaceVolume::GetConfiguredExtent() const
{
	return Volume ? Volume->GetUnscaledBoxExtent() : FVector::ZeroVector;
}

bool ABHCrawlSpaceVolume::IsCharacterSheltering(const ABHCharacter* Character) const
{
	// Reuse CanCharacterUseCrawlSpace so "is sheltered" stays in lockstep with "is allowed to stay" -- the same
	// predicate the Tick uses to decide whom to reject. Overlap is server-authoritative (the volume tracks pawn
	// overlaps even though it never replicates), and the Teacher's capture path that calls this runs on authority.
	return Character
		&& Volume
		&& Volume->IsOverlappingActor(Character)
		&& CanCharacterUseCrawlSpace(Character);
}

#if WITH_DEV_AUTOMATION_TESTS
bool ABHCrawlSpaceVolume::DebugCanCharacterUseCrawlSpace(const ABHCharacter* Character) const
{
	return CanCharacterUseCrawlSpace(Character);
}
#endif

void ABHCrawlSpaceVolume::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ABHCharacter* Character = Cast<ABHCharacter>(OtherActor))
	{
		// Auto-prone an eligible survivor on contact (instead of bouncing them), so dropping into cover at the
		// mouth is immediate with no one-tick standing window; eject everyone else.
		if (!TryAdmitOrAutoProne(Character))
		{
			QueueRejectCharacter(Character);
		}
	}
}

void ABHCrawlSpaceVolume::OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ABHCharacter* Character = Cast<ABHCharacter>(OtherActor))
	{
		ActiveRejectDirections.Remove(TWeakObjectPtr<ABHCharacter>(Character));
	}
}

bool ABHCrawlSpaceVolume::CanCharacterUseCrawlSpace(const ABHCharacter* Character) const
{
	return IsCharacterEligibleSurvivor(Character)
		&& BHIsCrawlLowProfileState(Character->GetMovementSpecialState());
}

bool ABHCrawlSpaceVolume::IsCharacterEligibleSurvivor(const ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		return false;
	}
	return BHPS->PlayerRole == EBHPlayerRole::Survivor || BHPS->PlayerRole == EBHPlayerRole::Tester;
}

bool ABHCrawlSpaceVolume::TryAdmitOrAutoProne(ABHCharacter* Character)
{
	// Already a low-profile survivor -> sheltering, nothing to do.
	if (CanCharacterUseCrawlSpace(Character))
	{
		return true;
	}

	// An eligible survivor at the mouth in the wrong pose: drop them to prone so a fleeing player flows into cover
	// instead of being bounced off the lip. The Teacher / Hall Monitor / dead never qualify -> they fall through
	// and are ejected (the Teacher must never be auto-proned or admitted).
	if (Character && IsCharacterEligibleSurvivor(Character))
	{
		if (Character->TryEnterCrawlSpacePose())
		{
			return true;
		}
		// Couldn't prone this instant only because of a transient roll -> let the roll resolve and re-check next
		// tick rather than bouncing them mid-move.
		if (Character->GetMovementSpecialState() == EBHMovementSpecialState::Rolling)
		{
			return true;
		}
	}

	return false;
}

void ABHCrawlSpaceVolume::QueueRejectCharacter(ABHCharacter* Character)
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
	const FVector LocalLocation = VolumeTransform.InverseTransformPosition(Character->GetActorLocation());
	const FVector LocalVelocity = VolumeTransform.InverseTransformVectorNoScale(IncomingVelocity);
	const FVector Extent = Volume->GetScaledBoxExtent();
	ActiveRejectDirections.FindOrAdd(TWeakObjectPtr<ABHCharacter>(Character)) = BHChooseCrawlRejectDirection(LocalLocation, LocalVelocity, Extent);
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

	// Sweep the push-out so the capsule stops at any wall in the way. A non-swept teleport
	// could shove the rejected character (e.g. the Teacher) straight through a wall the
	// crawlspace gap is embedded in, letting them phase out the far side.
	FHitResult RejectSweep;
	Character->SetActorLocation(RejectedLocation, true, &RejectSweep, ETeleportType::TeleportPhysics);

	if (Movement)
	{
		Movement->StopMovementImmediately();
	}

	if (!Volume->IsOverlappingActor(Character))
	{
		ActiveRejectDirections.Remove(CharacterKey);
	}
}

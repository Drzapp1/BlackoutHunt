#include "BHBreakableGlassPane.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameSettings.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

namespace
{
	// Converts a physics impulse magnitude (NormalImpulse.Size()) into damage units
	// comparable to the crack/break damage thresholds.
	constexpr float GlassImpulseToDamageScale = 10000.0f;
}

ABHBreakableGlassPane::ABHBreakableGlassPane()
{
	InteractionLabel = FText::FromString(TEXT("Break Glass"));
	GlassState = EBHBreakableGlassState::Intact;
	CrackDamageThreshold = 12.0f;
	BreakDamageThreshold = 34.0f;
	BreakNoiseStrength = 1.15f;
	SetNetUpdateFrequency(8.0f);
	SetMinNetUpdateFrequency(1.0f);

	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->SetNotifyRigidBodyCollision(true);
		Mesh->OnComponentHit.AddDynamic(this, &ABHBreakableGlassPane::OnPaneHit);
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, FVector(1.0f, 0.055f, 1.0f), true);
	}
	ApplyGlassVisuals();
}

void ABHBreakableGlassPane::BeginPlay()
{
	Super::BeginPlay();

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings)
	{
		CrackDamageThreshold = Settings->GlassCrackDamageThreshold;
		BreakDamageThreshold = Settings->GlassBreakDamageThreshold;
		BreakNoiseStrength = Settings->GlassBreakNoiseStrength;
	}
	ApplyGlassVisuals();
}

void ABHBreakableGlassPane::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Mesh)
	{
		Mesh->OnComponentHit.RemoveDynamic(this, &ABHBreakableGlassPane::OnPaneHit);
	}

	Super::EndPlay(EndPlayReason);
}

void ABHBreakableGlassPane::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHBreakableGlassPane, GlassState);
}

float ABHBreakableGlassPane::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || GlassState == EBHBreakableGlassState::Broken)
	{
		return AppliedDamage;
	}

	if (AppliedDamage >= BreakDamageThreshold)
	{
		BreakGlass(DamageCauser ? DamageCauser : this, TEXT("impact glass"));
	}
	else if (AppliedDamage >= CrackDamageThreshold)
	{
		CrackGlass(DamageCauser ? DamageCauser : this, TEXT("cracked glass"));
	}
	return AppliedDamage;
}

bool ABHBreakableGlassPane::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	return GlassState != EBHBreakableGlassState::Broken && BHPS && BHPS->LifeState == EBHPlayerLifeState::Alive;
}

void ABHBreakableGlassPane::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority() && CanInteract_Implementation(Character))
	{
		BreakGlass(Character, TEXT("shattered glass"));
	}
}

FText ABHBreakableGlassPane::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	if (GlassState == EBHBreakableGlassState::Broken)
	{
		return FText::FromString(TEXT("Glass Broken"));
	}
	if (GlassState == EBHBreakableGlassState::Cracked)
	{
		return FText::FromString(TEXT("Finish Glass"));
	}
	return InteractionLabel;
}

FBHInteractionPromptInfo ABHBreakableGlassPane::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = 0.0f;
	Info.Progress = GlassState == EBHBreakableGlassState::Cracked ? 0.5f : 0.0f;
	Info.RiskText = GlassState == EBHBreakableGlassState::Broken ? FText::GetEmpty() : FText::FromString(TEXT("LOUD"));
	Info.DisabledReason = GlassState == EBHBreakableGlassState::Broken ? FText::FromString(TEXT("BROKEN")) : FText::GetEmpty();
	Info.bNoisy = GlassState != EBHBreakableGlassState::Broken;
	Info.bDangerous = GlassState != EBHBreakableGlassState::Broken;
	return Info;
}

void ABHBreakableGlassPane::ConfigurePane(const FText& NewLabel, float NewBreakNoiseStrength)
{
	if (!NewLabel.IsEmpty())
	{
		InteractionLabel = NewLabel;
	}
	BreakNoiseStrength = FMath::Max(0.0f, NewBreakNoiseStrength);
}

bool ABHBreakableGlassPane::CrackGlass(AActor* InstigatorActor, const FString& Reason)
{
	if (!HasAuthority() || GlassState != EBHBreakableGlassState::Intact)
	{
		return false;
	}

	GlassState = EBHBreakableGlassState::Cracked;
	ApplyGlassVisuals();
	MulticastGlassCue(GlassState, GetActorLocation());
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Noise, GetActorLocation(), this, InstigatorActor, 0.55f, Reason);
	}
	return true;
}

bool ABHBreakableGlassPane::BreakGlass(AActor* InstigatorActor, const FString& Reason)
{
	if (!HasAuthority() || GlassState == EBHBreakableGlassState::Broken)
	{
		return false;
	}

	GlassState = EBHBreakableGlassState::Broken;
	ApplyGlassVisuals();
	MulticastGlassCue(GlassState, GetActorLocation());
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		const FString NoiseReason = Reason.IsEmpty() ? FString(TEXT("shattered glass")) : Reason;
		BHGM->NotifyLoudNoise(GetActorLocation(), NoiseReason);
		BHGM->ReportBotStimulus(EBHBotStimulusType::Noise, GetActorLocation(), this, InstigatorActor, NoiseReason, BreakNoiseStrength);
		BHGM->ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Noise, GetActorLocation(), this, InstigatorActor, BreakNoiseStrength, NoiseReason);
	}
	return true;
}

EBHBreakableGlassState ABHBreakableGlassPane::GetGlassState() const
{
	return GlassState;
}

bool ABHBreakableGlassPane::IsBroken() const
{
	return GlassState == EBHBreakableGlassState::Broken;
}

void ABHBreakableGlassPane::OnRep_GlassState()
{
	ApplyGlassVisuals();
}

void ABHBreakableGlassPane::OnPaneHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || GlassState == EBHBreakableGlassState::Broken)
	{
		return;
	}

	const float ImpactStrength = NormalImpulse.Size() / GlassImpulseToDamageScale;
	if (ImpactStrength >= BreakDamageThreshold)
	{
		BreakGlass(OtherActor, TEXT("impact glass"));
	}
	else if (ImpactStrength >= CrackDamageThreshold)
	{
		CrackGlass(OtherActor, TEXT("cracked glass"));
	}
}

void ABHBreakableGlassPane::MulticastGlassCue_Implementation(EBHBreakableGlassState NewState, FVector CueLocation)
{
	if (NewState == EBHBreakableGlassState::Broken)
	{
		if (USoundBase* LoadedSound = BreakSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, LoadedSound, CueLocation, 1.0f, 1.0f);
		}
	}
	ApplyGlassVisuals();
}

void ABHBreakableGlassPane::ApplyGlassVisuals()
{
	if (!Mesh)
	{
		return;
	}

	switch (GlassState)
	{
	case EBHBreakableGlassState::Broken:
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false, true);
		Mesh->SetHiddenInGame(true, true);
		break;
	case EBHBreakableGlassState::Cracked:
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetVisibility(true, true);
		Mesh->SetHiddenInGame(false, true);
		BHPropVisuals::TintPart(Mesh, FLinearColor(0.58f, 0.82f, 0.92f, 0.56f), 0.45f);
		break;
	case EBHBreakableGlassState::Intact:
	default:
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetVisibility(true, true);
		Mesh->SetHiddenInGame(false, true);
		BHPropVisuals::TintPart(Mesh, FLinearColor(0.20f, 0.48f, 0.58f, 0.42f), 0.28f);
		break;
	}
}

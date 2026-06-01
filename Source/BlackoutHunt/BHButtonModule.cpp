#include "BHButtonModule.h"
#include "BHCharacter.h"
#include "BHModuleReceiverInterface.h"
#include "BHPlayerState.h"
#include "BHPropVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

namespace
{
UStaticMesh* BHButtonModuleMesh(const TCHAR* AssetName)
{
	const FString Name(AssetName);
	const FString Path = FString::Printf(TEXT("/Game/SmartBasicInterfaces/Meshes/%s.%s"), *Name, *Name);
	return LoadObject<UStaticMesh>(nullptr, *Path);
}

UMaterialInterface* BHButtonModuleMaterial(const TCHAR* AssetName)
{
	const FString Name(AssetName);
	const FString Path = FString::Printf(TEXT("/Game/SmartBasicInterfaces/Materials/%s.%s"), *Name, *Name);
	return LoadObject<UMaterialInterface>(nullptr, *Path);
}
}

ABHButtonModule::ABHButtonModule()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;

	ModuleId = FName(TEXT("Module"));
	Mode = EBHButtonModuleMode::Press;
	ModuleLabel = FText::FromString(TEXT("Use Module"));
	InteractionLabel = ModuleLabel;
	HoldSeconds = 1.25f;
	CooldownSeconds = 0.50f;
	bActive = false;
	bConsumed = false;
	HoldProgress = 0.0f;
	CooldownEndServerTime = 0.0f;

	ButtonBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonBaseMesh"));
	ButtonBaseMesh->SetupAttachment(RootComponent);
	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetupAttachment(RootComponent);
	ButtonRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonRingMesh"));
	ButtonRingMesh->SetupAttachment(RootComponent);
	StatusLightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLightMesh"));
	StatusLightMesh->SetupAttachment(RootComponent);

	UStaticMesh* PanelMesh = BHButtonModuleMesh(TEXT("SM_panel"));
	UStaticMesh* BaseMesh = BHButtonModuleMesh(TEXT("SM_powerbtnbase"));
	UStaticMesh* ButtonAsset = BHButtonModuleMesh(TEXT("SM_powerbtn"));
	UStaticMesh* RingMesh = BHButtonModuleMesh(TEXT("SM_powerbtnring"));
	UMaterialInterface* PanelMaterial = BHButtonModuleMaterial(TEXT("MI_Panel"));
	UMaterialInterface* ButtonMaterial = BHButtonModuleMaterial(TEXT("MI_PowerBTN"));

	BHPropVisuals::ConfigurePart(Mesh, PanelMesh ? PanelMesh : BHPropVisuals::CubeMesh(), PanelMaterial ? PanelMaterial : BHPropVisuals::PaintedMetalMaterial(), FVector::ZeroVector, FRotator::ZeroRotator, PanelMesh ? FVector(1.0f) : FVector(0.54f, 0.16f, 0.68f), true);
	BHPropVisuals::ConfigurePart(ButtonBaseMesh, BaseMesh ? BaseMesh : BHPropVisuals::CylinderMesh(), ButtonMaterial ? ButtonMaterial : BHPropVisuals::BasicMaterial(), FVector(24.0f, 0.0f, -4.0f), FRotator(0.0f, 90.0f, 0.0f), BaseMesh ? FVector(0.55f) : FVector(0.16f, 0.16f, 0.035f));
	BHPropVisuals::ConfigurePart(ButtonRingMesh, RingMesh ? RingMesh : BHPropVisuals::CylinderMesh(), ButtonMaterial ? ButtonMaterial : BHPropVisuals::BasicMaterial(), FVector(26.0f, 0.0f, -4.0f), FRotator(0.0f, 90.0f, 0.0f), RingMesh ? FVector(0.55f) : FVector(0.19f, 0.19f, 0.020f));
	BHPropVisuals::ConfigurePart(ButtonMesh, ButtonAsset ? ButtonAsset : BHPropVisuals::CylinderMesh(), ButtonMaterial ? ButtonMaterial : BHPropVisuals::BasicMaterial(), FVector(29.0f, 0.0f, -4.0f), FRotator(0.0f, 90.0f, 0.0f), ButtonAsset ? FVector(0.55f) : FVector(0.125f, 0.125f, 0.055f));
	BHPropVisuals::ConfigurePart(StatusLightMesh, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(29.0f, 0.0f, 24.0f), FRotator::ZeroRotator, FVector(0.052f));

	PressSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/SmartBasicInterfaces/Sound/SW_button-push.SW_button-push"));
	ApplyModuleVisuals();
}

void ABHButtonModule::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	ABHCharacter* Character = ActiveInteractor.Get();
	if (!Character || !CanInteract_Implementation(Character))
	{
		ActiveInteractor = nullptr;
		HoldProgress = 0.0f;
		SetActorTickEnabled(false);
		ApplyModuleVisuals();
		return;
	}

	HoldProgress = HoldSeconds > 0.0f ? FMath::Clamp(HoldProgress + DeltaSeconds / HoldSeconds, 0.0f, 1.0f) : 1.0f;
	ApplyModuleVisuals();
	if (HoldProgress >= 1.0f)
	{
		ActivateModule(Character);
		ActiveInteractor = nullptr;
		HoldProgress = 0.0f;
		SetActorTickEnabled(false);
	}
}

void ABHButtonModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHButtonModule, bActive);
	DOREPLIFETIME(ABHButtonModule, bConsumed);
	DOREPLIFETIME(ABHButtonModule, HoldProgress);
	DOREPLIFETIME(ABHButtonModule, CooldownEndServerTime);
}

void ABHButtonModule::ConfigureModule(FName NewModuleId, EBHButtonModuleMode NewMode, const FText& NewLabel, float NewHoldSeconds, float NewCooldownSeconds)
{
	ModuleId = NewModuleId.IsNone() ? FName(TEXT("Module")) : NewModuleId;
	Mode = NewMode;
	ModuleLabel = NewLabel.ToString().IsEmpty() ? FText::FromString(TEXT("Use Module")) : NewLabel;
	InteractionLabel = ModuleLabel;
	HoldSeconds = FMath::Max(0.0f, NewHoldSeconds);
	CooldownSeconds = FMath::Max(0.0f, NewCooldownSeconds);
	ApplyModuleVisuals();
}

void ABHButtonModule::AddLinkedReceiver(AActor* Receiver)
{
	if (Receiver)
	{
		LinkedReceivers.AddUnique(Receiver);
	}
}

bool ABHButtonModule::IsActive() const
{
	return bActive;
}

bool ABHButtonModule::IsConsumed() const
{
	return bConsumed;
}

float ABHButtonModule::GetHoldProgress() const
{
	return HoldProgress;
}

float ABHButtonModule::GetRemainingCooldownSeconds() const
{
	return FMath::Max(0.0f, CooldownEndServerTime - GetServerTimeSeconds());
}

bool ABHButtonModule::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	return BHPS
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& !bConsumed
		&& GetRemainingCooldownSeconds() <= 0.0f;
}

void ABHButtonModule::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (!HasAuthority() || !CanInteract_Implementation(Character))
	{
		return;
	}

	if (Mode == EBHButtonModuleMode::Hold)
	{
		ActiveInteractor = Character;
		HoldProgress = 0.0f;
		SetActorTickEnabled(true);
		ApplyModuleVisuals();
		ForceNetUpdate();
		return;
	}

	ActivateModule(Character);
}

void ABHButtonModule::EndInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority() && ActiveInteractor.Get() == Character)
	{
		ActiveInteractor = nullptr;
		HoldProgress = 0.0f;
		SetActorTickEnabled(false);
		ApplyModuleVisuals();
		ForceNetUpdate();
	}
}

FText ABHButtonModule::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	const float RemainingCooldown = GetRemainingCooldownSeconds();
	if (bConsumed)
	{
		return FText::FromString(FString::Printf(TEXT("%s Complete"), *ModuleLabel.ToString()));
	}
	if (RemainingCooldown > 0.0f)
	{
		return FText::FromString(FString::Printf(TEXT("%s (%ds)"), *ModuleLabel.ToString(), FMath::CeilToInt(RemainingCooldown)));
	}
	if (Mode == EBHButtonModuleMode::Hold && HoldProgress > 0.0f)
	{
		return FText::FromString(FString::Printf(TEXT("%s %d%%"), *ModuleLabel.ToString(), FMath::RoundToInt(HoldProgress * 100.0f)));
	}
	return ModuleLabel;
}

FBHInteractionPromptInfo ABHButtonModule::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = Mode == EBHButtonModuleMode::Hold ? HoldSeconds : 0.0f;
	Info.Progress = Mode == EBHButtonModuleMode::Hold ? HoldProgress : 0.0f;
	if (!Info.bCanInteract)
	{
		Info.DisabledReason = bConsumed
			? FText::FromString(TEXT("COMPLETE"))
			: (GetRemainingCooldownSeconds() > 0.0f ? FText::FromString(TEXT("COOLDOWN")) : FText::FromString(TEXT("LOCKED")));
	}
	return Info;
}

void ABHButtonModule::OnRep_ModuleState()
{
	ApplyModuleVisuals();
}

void ABHButtonModule::MulticastPlayPressSound_Implementation()
{
	if (PressSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PressSound, GetActorLocation(), 0.72f);
	}
}

void ABHButtonModule::ActivateModule(ABHCharacter* Character)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Mode == EBHButtonModuleMode::Toggle)
	{
		bActive = !bActive;
	}
	else
	{
		bActive = true;
	}

	const bool bBroadcastActive = bActive;
	if (Mode == EBHButtonModuleMode::Press)
	{
		bActive = false;
	}
	else if (Mode == EBHButtonModuleMode::OneShot)
	{
		bConsumed = true;
	}

	CooldownEndServerTime = GetServerTimeSeconds() + CooldownSeconds;
	MulticastPlayPressSound();

	OnModuleActivated.Broadcast(Character, ModuleId, bBroadcastActive);
	ReceiveModuleActivated(Character, ModuleId, bBroadcastActive);
	for (AActor* Receiver : LinkedReceivers)
	{
		if (Receiver && Receiver->GetClass()->ImplementsInterface(UBHModuleReceiverInterface::StaticClass()))
		{
			IBHModuleReceiverInterface::Execute_OnModuleActivated(Receiver, Character, ModuleId, bBroadcastActive);
		}
	}
	ApplyModuleVisuals();
	ForceNetUpdate();
}

void ABHButtonModule::ApplyModuleVisuals()
{
	const bool bCoolingDown = GetRemainingCooldownSeconds() > 0.0f;
	const bool bHolding = HoldProgress > 0.0f;
	const FLinearColor ButtonColor = bConsumed
		? FLinearColor(0.12f, 0.14f, 0.15f, 1.0f)
		: (bActive ? FLinearColor(0.12f, 0.86f, 0.48f, 1.0f) : (bCoolingDown ? FLinearColor(0.88f, 0.42f, 0.10f, 1.0f) : FLinearColor(0.84f, 0.11f, 0.08f, 1.0f)));
	const FLinearColor LightColor = bHolding
		? FLinearColor(0.30f + HoldProgress * 0.52f, 0.72f, 1.0f, 1.0f)
		: (bActive ? FLinearColor(0.12f, 0.96f, 0.48f, 1.0f) : (bCoolingDown ? FLinearColor(0.90f, 0.54f, 0.08f, 1.0f) : FLinearColor(0.55f, 0.08f, 0.06f, 1.0f)));

	if (ButtonMesh)
	{
		ButtonMesh->SetRelativeLocation(FVector(bActive || bHolding ? 25.5f : 29.0f, 0.0f, -4.0f));
	}
	BHPropVisuals::TintPart(ButtonMesh, ButtonColor, bActive ? 0.8f : 0.0f);
	BHPropVisuals::TintPart(ButtonRingMesh, bHolding ? FLinearColor(0.24f, 0.58f, 0.92f, 1.0f) : FLinearColor(0.08f, 0.09f, 0.10f, 1.0f), bHolding ? 1.3f + HoldProgress : 0.0f);
	BHPropVisuals::TintPart(StatusLightMesh, LightColor, bConsumed ? 0.0f : (bHolding ? 1.6f + HoldProgress * 1.4f : (bActive ? 2.2f : 0.7f)));
}

float ABHButtonModule::GetServerTimeSeconds() const
{
	if (!GetWorld())
	{
		return 0.0f;
	}
	const AGameStateBase* GameStateBase = GetWorld()->GetGameState();
	return GameStateBase ? GameStateBase->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

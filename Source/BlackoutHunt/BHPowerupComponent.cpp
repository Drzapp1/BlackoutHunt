#include "BHPowerupComponent.h"
#include "BHCharacter.h"
#include "BHGameState.h"
#include "BHNoiseDecoy.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPowerupLibrary.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UBHPowerupComponent::UBHPowerupComponent()
{
	SetIsReplicatedByDefault(true);
	StaminaBoostEndServerTime = 0.0f;
	SprintBurstEndServerTime = 0.0f;
	FlashlightBoostEndServerTime = 0.0f;
	QuestionHintEndServerTime = 0.0f;
	DoorRushEndServerTime = 0.0f;
}

void UBHPowerupComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBHPowerupComponent, StaminaBoostEndServerTime);
	DOREPLIFETIME(UBHPowerupComponent, SprintBurstEndServerTime);
	DOREPLIFETIME(UBHPowerupComponent, FlashlightBoostEndServerTime);
	DOREPLIFETIME(UBHPowerupComponent, QuestionHintEndServerTime);
	DOREPLIFETIME(UBHPowerupComponent, DoorRushEndServerTime);
}

bool UBHPowerupComponent::ServerUsePowerup(EBHPowerupType Type)
{
	ABHCharacter* Character = GetBHOwner();
	ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (!Character || !BHPS || !Character->HasAuthority() || !BHPS->IsAliveSurvivor())
	{
		return false;
	}

	FBHPowerupDefinition Definition;
	if (!FBHPowerupLibrary::GetDefinition(Type, Definition))
	{
		return false;
	}
	if (FBHPowerupLibrary::IsTeacherPowerup(Type))
	{
		return false;
	}

	const float Now = GetServerTime();
	if (BHPS->GetPowerupCooldownEnd(Type) > Now)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("%s cooling down."), *Definition.Name), 2.0f);
		}
		return false;
	}

	if (!BHPS->ConsumePowerupCharge(Type))
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("No %s charge owned."), *Definition.Name), 2.0f);
		}
		return false;
	}

	const float EndTime = Now + FMath::Max(0.0f, Definition.DurationSeconds);
	BHPS->SetPowerupCooldown(Type, Now + FMath::Max(0.0f, Definition.CooldownSeconds));

	switch (Type)
	{
	case EBHPowerupType::StaminaBoost:
		StaminaBoostEndServerTime = FMath::Max(StaminaBoostEndServerTime, EndTime);
		Character->RecoverStamina(35.0f);
		break;
	case EBHPowerupType::SprintBurst:
		SprintBurstEndServerTime = FMath::Max(SprintBurstEndServerTime, EndTime);
		Character->RecoverStamina(10.0f);
		break;
	case EBHPowerupType::FlashlightBoost:
		FlashlightBoostEndServerTime = FMath::Max(FlashlightBoostEndServerTime, EndTime);
		Character->RefillFlashlight(82.0f);
		break;
	case EBHPowerupType::QuestionHint:
		QuestionHintEndServerTime = FMath::Max(QuestionHintEndServerTime, EndTime);
		break;
	case EBHPowerupType::DecoySound:
		if (UWorld* World = GetWorld())
		{
			const FVector SpawnLocation = Character->GetActorLocation() + Character->GetActorForwardVector() * 120.0f + FVector(0.0f, 0.0f, 18.0f);
			World->SpawnActor<ABHNoiseDecoy>(SpawnLocation, Character->GetActorRotation());
		}
		break;
	case EBHPowerupType::DoorRush:
		DoorRushEndServerTime = FMath::Max(DoorRushEndServerTime, EndTime);
		break;
	case EBHPowerupType::TeamBeacon:
		break;
	case EBHPowerupType::AntiScareCharm:
		Character->AddFear(-28.0f);
		Character->AddDread(-32.0f);
		break;
	case EBHPowerupType::TeacherScanFocus:
	case EBHPowerupType::TeacherBlackoutSurge:
	case EBHPowerupType::TeacherPatrolIntel:
		return false;
	}

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
	{
		PC->ClientShowStatusMessage(FString::Printf(TEXT("%s used."), *Definition.Name), 2.5f);
	}
	return true;
}

float UBHPowerupComponent::GetSprintSpeedMultiplier() const
{
	float Multiplier = 1.0f;
	if (IsActiveUntil(SprintBurstEndServerTime))
	{
		Multiplier = FMath::Max(Multiplier, 1.18f);
	}
	if (HasActiveDoorRush())
	{
		Multiplier = FMath::Max(Multiplier, 1.10f);
	}
	return Multiplier;
}

float UBHPowerupComponent::GetStaminaRecoveryMultiplier() const
{
	float Multiplier = 1.0f;
	if (IsActiveUntil(StaminaBoostEndServerTime))
	{
		Multiplier *= 1.45f;
	}
	if (HasActiveDoorRush())
	{
		Multiplier *= 1.10f;
	}
	return Multiplier;
}

float UBHPowerupComponent::GetStaminaDrainMultiplier() const
{
	float Multiplier = 1.0f;
	if (IsActiveUntil(StaminaBoostEndServerTime))
	{
		Multiplier *= 0.84f;
	}
	if (HasActiveDoorRush())
	{
		Multiplier *= 0.92f;
	}
	return Multiplier;
}

float UBHPowerupComponent::GetFlashlightMultiplier() const
{
	return IsActiveUntil(FlashlightBoostEndServerTime) ? 1.36f : 1.0f;
}

bool UBHPowerupComponent::HasActiveQuestionHint() const
{
	return IsActiveUntil(QuestionHintEndServerTime);
}

bool UBHPowerupComponent::HasActiveDoorRush() const
{
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	return BHGS && BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive && IsActiveUntil(DoorRushEndServerTime);
}

float UBHPowerupComponent::GetQuestionHintEndServerTime() const
{
	return QuestionHintEndServerTime;
}

float UBHPowerupComponent::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

ABHCharacter* UBHPowerupComponent::GetBHOwner() const
{
	return Cast<ABHCharacter>(GetOwner());
}

bool UBHPowerupComponent::IsActiveUntil(float EndServerTime) const
{
	return EndServerTime > GetServerTime();
}

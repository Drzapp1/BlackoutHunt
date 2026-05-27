#include "BHHUD.h"
#include "BHCharacter.h"
#include "BHBreaker.h"
#include "BHExitGate.h"
#include "BHGameState.h"
#include "BHInteractableInterface.h"
#include "BHJumpscareMonster.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FString FormatClock(const int32 TotalSeconds)
	{
		const int32 ClampedSeconds = FMath::Max(0, TotalSeconds);
		return FString::Printf(TEXT("%02d:%02d"), ClampedSeconds / 60, ClampedSeconds % 60);
	}

	FString TrainPhaseLabel(EBHTrainPhase Phase)
	{
		switch (Phase)
		{
		case EBHTrainPhase::Arrival:
			return TEXT("ARRIVAL");
		case EBHTrainPhase::Recap:
			return TEXT("RECAP");
		case EBHTrainPhase::BonusQuestion:
			return TEXT("BONUS QUESTIONS");
		case EBHTrainPhase::Shop:
			return TEXT("SHOP");
		case EBHTrainPhase::StationStop:
			return TEXT("STATION STOP");
		case EBHTrainPhase::Departing:
			return TEXT("DEPARTING");
		case EBHTrainPhase::Inactive:
		default:
			return TEXT("TRAIN");
		}
	}

	FLinearColor MutedText()
	{
		return FLinearColor(0.58f, 0.66f, 0.66f, 0.92f);
	}

	FLinearColor MainText()
	{
		return FLinearColor(0.88f, 0.95f, 0.92f, 1.0f);
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= FMath::Clamp(Alpha, 0.0f, 1.0f);
		return Color;
	}

	bool IsAlivePathThreat(const ABHPlayerState* PlayerState)
	{
		return PlayerState
			&& PlayerState->LifeState == EBHPlayerLifeState::Alive
			&& (PlayerState->PlayerRole == EBHPlayerRole::Hunter
				|| PlayerState->PlayerRole == EBHPlayerRole::FakeHunter
				|| PlayerState->PlayerRole == EBHPlayerRole::Tester);
	}

	bool HasNearbyPathThreat(UWorld* World, const ABHCharacter* Character, float& OutThreatAlpha)
	{
		OutThreatAlpha = 0.0f;
		if (!World || !Character)
		{
			return false;
		}

		const ABHPlayerState* LocalPS = Character->GetBHPlayerState();
		if (!LocalPS || !LocalPS->IsAliveSurvivor())
		{
			return false;
		}

		FVector ViewLocation = Character->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
		FRotator ViewRotation = Character->GetActorRotation();
		Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);

		const FVector CharacterLocation = Character->GetActorLocation();
		constexpr float NearbyHunterRange = 1650.0f;
		constexpr float VisibleHunterRange = 2600.0f;
		constexpr float MonsterRange = 3200.0f;
		bool bFoundThreat = false;

		auto RegisterThreat = [&](float Distance, float Range)
		{
			bFoundThreat = true;
			OutThreatAlpha = FMath::Max(OutThreatAlpha, 1.0f - FMath::Clamp(Distance / FMath::Max(1.0f, Range), 0.0f, 1.0f));
		};

		auto HasLocalSightTo = [&](const ABHCharacter* OtherCharacter)
		{
			if (!OtherCharacter)
			{
				return false;
			}

			const FVector ThreatLocation = OtherCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDPathThreatLOS), false);
			Params.AddIgnoredActor(Character);
			Params.AddIgnoredActor(OtherCharacter);

			FHitResult Hit;
			return !World->LineTraceSingleByChannel(Hit, ViewLocation, ThreatLocation, ECC_Visibility, Params);
		};

		for (TActorIterator<ABHCharacter> It(World); It; ++It)
		{
			const ABHCharacter* OtherCharacter = *It;
			const ABHPlayerState* OtherPS = OtherCharacter ? OtherCharacter->GetBHPlayerState() : nullptr;
			if (!OtherCharacter || OtherCharacter == Character || !IsAlivePathThreat(OtherPS))
			{
				continue;
			}

			const float Distance = FVector::Dist2D(OtherCharacter->GetActorLocation(), CharacterLocation);
			const bool bVeryNear = Distance <= NearbyHunterRange;
			const bool bVisibleNear = Distance <= VisibleHunterRange && HasLocalSightTo(OtherCharacter);
			if (bVeryNear || bVisibleNear)
			{
				RegisterThreat(Distance, bVeryNear ? NearbyHunterRange : VisibleHunterRange);
			}
		}

		for (TActorIterator<ABHJumpscareMonster> It(World); It; ++It)
		{
			const ABHJumpscareMonster* Monster = *It;
			if (!Monster)
			{
				continue;
			}

			const float Distance = FVector::Dist2D(Monster->GetActorLocation(), CharacterLocation);
			if (Distance <= MonsterRange)
			{
				RegisterThreat(Distance, MonsterRange);
			}
		}

		if (bFoundThreat)
		{
			OutThreatAlpha = FMath::Clamp(OutThreatAlpha, 0.22f, 1.0f);
		}
		return bFoundThreat;
	}

	struct FBHTeacherProximityReadout
	{
		bool bFound = false;
		bool bLineOfSight = false;
		FVector TeacherLocation = FVector::ZeroVector;
		float DistanceCm = 0.0f;
		float ProximityPercent = 0.0f;
	};

	FBHTeacherProximityReadout FindTeacherProximity(UWorld* World, const ABHCharacter* Character)
	{
		FBHTeacherProximityReadout Readout;
		if (!World || !Character)
		{
			return Readout;
		}

		const ABHPlayerState* LocalPS = Character->GetBHPlayerState();
		if (!LocalPS || !LocalPS->IsAliveSurvivor())
		{
			return Readout;
		}

		FVector ViewLocation = Character->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
		FRotator ViewRotation = Character->GetActorRotation();
		Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);

		const FVector CharacterLocation = Character->GetActorLocation();
		constexpr float TeacherSignalRange = 6000.0f;
		float BestDistance = TeacherSignalRange;
		bool bBestLineOfSight = false;

		auto HasLineOfSightTo = [&](const ABHCharacter* OtherCharacter)
		{
			if (!OtherCharacter)
			{
				return false;
			}

			const FVector ThreatLocation = OtherCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDTeacherProximityLOS), false);
			Params.AddIgnoredActor(Character);
			Params.AddIgnoredActor(OtherCharacter);

			FHitResult Hit;
			return !World->LineTraceSingleByChannel(Hit, ViewLocation, ThreatLocation, ECC_Visibility, Params);
		};

		for (TActorIterator<ABHCharacter> It(World); It; ++It)
		{
			const ABHCharacter* OtherCharacter = *It;
			const ABHPlayerState* OtherPS = OtherCharacter ? OtherCharacter->GetBHPlayerState() : nullptr;
			if (!OtherCharacter || OtherCharacter == Character || !IsAlivePathThreat(OtherPS))
			{
				continue;
			}

			const float Distance = FVector::Dist2D(OtherCharacter->GetActorLocation(), CharacterLocation);
			if (Distance <= BestDistance)
			{
				BestDistance = Distance;
				bBestLineOfSight = HasLineOfSightTo(OtherCharacter);
				Readout.TeacherLocation = OtherCharacter->GetActorLocation();
				Readout.bFound = true;
			}
		}

		if (Readout.bFound)
		{
			const float BaseSignal = 1.0f - FMath::Clamp(BestDistance / TeacherSignalRange, 0.0f, 1.0f);
			Readout.bLineOfSight = bBestLineOfSight;
			Readout.DistanceCm = BestDistance;
			Readout.ProximityPercent = FMath::Clamp((BaseSignal + (bBestLineOfSight ? 0.12f : 0.0f)) * 100.0f, 4.0f, 100.0f);
		}
		return Readout;
	}
}

ABHHUD::ABHHUD()
{
	LastSeenPhase = EBHRoundPhase::Lobby;
	bHasSeenPhase = false;
	PhaseBannerEndTime = 0.0f;
	LastSeenPresencePulse = 0;
	PresencePulseEndTime = 0.0f;
	bHasVisibleHunterCue = false;
	LastVisibleHunterLocation = FVector::ZeroVector;
	LastVisibleHunterDistanceCm = 0.0f;
	VisibleHunterCueUntilTime = 0.0f;
	SmoothedVisibleHunterArrowX = 0.0f;
}

void ABHHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const APlayerController* PC = PlayerOwner;
	const ABHPlayerController* BHPC = PC ? Cast<ABHPlayerController>(PC) : nullptr;
	const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
	ABHCharacter* Character = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
	const bool bShowSurvivorWarnings = BHPS && BHPS->IsAliveSurvivor();

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (BHGS)
	{
		if (!bHasSeenPhase)
		{
			PhaseBannerEndTime = BHGS->RoundPhase == EBHRoundPhase::Lobby ? 0.0f : Now + 3.2f;
			LastSeenPhase = BHGS->RoundPhase;
			bHasSeenPhase = true;
		}
		else if (LastSeenPhase != BHGS->RoundPhase)
		{
			LastSeenPhase = BHGS->RoundPhase;
			PhaseBannerEndTime = Now + 3.6f;
		}

		if (LastSeenPresencePulse != BHGS->PresencePulse)
		{
			LastSeenPresencePulse = BHGS->PresencePulse;
			PresencePulseEndTime = Now + 0.75f;
		}
	}

	if (bShowSurvivorWarnings)
	{
		DrawHorrorOverlay(Character, BHGS);
	}

	const float SafePad = FMath::Max(18.0f, Canvas->ClipX * 0.014f);
	const float ReadoutW = FMath::Clamp(Canvas->ClipX * 0.38f, 280.0f, 560.0f);
	if (BHGS)
	{
		const FString TimerText = BHGS->bTestMode ? FString(TEXT("TEST LOOP")) : (BHGS->bPracticeMode ? FString(TEXT("PRACTICE")) : FString::Printf(TEXT("T-%s"), *FormatClock(BHGS->RemainingTime)));
		const FString ExitText = BHGS->bExitUnlocked ? FString(TEXT("EXIT OPEN")) : FString(TEXT("EXIT SHUT"));
		const FString ObjectiveText = BHGS->ObjectiveText.IsEmpty() ? FString(TEXT("Restore power and reach the exit.")) : BHGS->ObjectiveText;
		const FString ObjectiveLine = ObjectiveText.ToUpper();
		const FString BreakerReadout = BHGS->BreakersRequired > 0 ? FString::Printf(TEXT("%d/%d"), BHGS->BreakersCompleted, BHGS->BreakersRequired) : FString(TEXT("CLEAR"));
		const FString StationReadout = BHGS->SideObjectivesRequired > 0 ? FString::Printf(TEXT("%d/%d"), BHGS->SideObjectivesCompleted, BHGS->SideObjectivesRequired) : FString(TEXT("CLEAR"));
		const FLinearColor ExitColor = BHGS->bExitUnlocked ? FLinearColor(0.73f, 0.96f, 0.64f, 0.95f) : FLinearColor(0.96f, 0.24f, 0.16f, 0.95f);
		DrawHudText(FString::Printf(TEXT("%s / %s"), *TimerText, *ExitText), SafePad, SafePad, ExitColor, GEngine->GetSmallFont(), 0.88f);
		DrawWrappedHudText(ObjectiveLine, SafePad, SafePad + 22.0f, ReadoutW, FLinearColor(0.84f, 0.80f, 0.70f, 0.88f), GEngine->GetSmallFont(), 0.70f, 13.0f, 2);
		DrawHudText(FString::Printf(TEXT("PWR %s  TASK %s  PRES %.0f"), *BreakerReadout, *StationReadout, FMath::Clamp(BHGS->PresenceLevel, 0.0f, 100.0f)), SafePad, SafePad + 55.0f, FLinearColor(0.62f, 0.58f, 0.51f, 0.82f), GEngine->GetSmallFont(), 0.62f);
		if (BHGS->bRevisionMode)
		{
			const FString RevisionLine = BHGS->RevisionReviewTimeRemaining > 0
				? FString::Printf(TEXT("REVIEW %ds / %s"), BHGS->RevisionReviewTimeRemaining, *BHGS->RevisionReviewText)
				: FString::Printf(TEXT("WEAK %s / %s"), *FBHRevisionQuestionBank::TopicToString(BHGS->RevisionWeakTopic), *BHGS->PresenceText);
			DrawWrappedHudText(RevisionLine.ToUpper(), SafePad, SafePad + 73.0f, ReadoutW, FLinearColor(0.74f, 0.63f, 0.55f, 0.78f), GEngine->GetSmallFont(), 0.58f, 12.0f, 1);
		}
		if (BHGS->RoundPhase == EBHRoundPhase::Intermission)
		{
			const float ServerNow = BHGS->GetServerWorldTimeSeconds();
			const int32 Countdown = FMath::Max(0, FMath::CeilToInt(BHGS->TrainPhaseEndServerTime - ServerNow));
			const FString PointsText = BHPS ? FString::Printf(TEXT(" / POINTS %d"), BHPS->QuestionPoints) : TEXT("");
			const FString TrainLine = FString::Printf(TEXT("%s / %s / %s%s"), *TrainPhaseLabel(BHGS->TrainPhase), *FormatClock(Countdown), *BHGS->TrainDestinationName.ToUpper(), *PointsText);
			DrawWrappedHudText(TrainLine, SafePad, SafePad + 91.0f, ReadoutW, FLinearColor(0.48f, 0.90f, 0.82f, 0.88f), GEngine->GetSmallFont(), 0.64f, 13.0f, 2);
		}
		else if (BHGS->RoundPhase == EBHRoundPhase::FinalEscape)
		{
			const float ServerNow = BHGS->GetServerWorldTimeSeconds();
			const int32 Countdown = FMath::Max(0, FMath::CeilToInt(BHGS->FinalEscapeEndServerTime - ServerNow));
			const FString FinalLine = BHGS->FinalEscapeState == EBHFinalEscapeState::Cutscene
				? FString::Printf(TEXT("EVACUATION TRAIN UNLOCKING / CONTROL RETURNS IN %s"), *FormatClock(FMath::Max(0, FMath::CeilToInt(BHGS->FinalEscapeCutsceneEndServerTime - ServerNow))))
				: FString::Printf(TEXT("REACH THE EVACUATION TRAIN / DEPARTS IN %s"), *FormatClock(Countdown));
			DrawWrappedHudText(FinalLine, SafePad, SafePad + 91.0f, ReadoutW, FLinearColor(0.90f, 0.34f, 0.24f, 0.92f), GEngine->GetSmallFont(), 0.66f, 13.0f, 2);
		}
	}
	else
	{
		DrawHudText(TEXT("NO SIGNAL"), SafePad, SafePad, FLinearColor(0.96f, 0.24f, 0.16f, 0.92f), GEngine->GetSmallFont(), 0.90f);
		DrawHudText(TEXT("HOST OR JOIN"), SafePad, SafePad + 21.0f, FLinearColor(0.62f, 0.58f, 0.51f, 0.78f), GEngine->GetSmallFont(), 0.64f);
	}

	if (BHPS)
	{
		const UEnum* RoleEnum = StaticEnum<EBHPlayerRole>();
		const UEnum* LifeEnum = StaticEnum<EBHPlayerLifeState>();
		FString RoleName = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(BHPS->PlayerRole)) : TEXT("Unassigned");
		if (BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			RoleName = TEXT("Teacher");
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			RoleName = TEXT("Hall Monitor");
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			RoleName = TEXT("Tester");
		}
		const FString LifeName = LifeEnum ? LifeEnum->GetNameStringByValue(static_cast<int64>(BHPS->LifeState)) : TEXT("Alive");
		const FString ReadyText = (BHGS && BHGS->bTestMode) ? TEXT("TEST") : ((BHGS && BHGS->bPracticeMode) ? TEXT("LAB") : (BHPS->bReady ? TEXT("READY") : TEXT("NOT READY")));
		DrawRightAlignedText(RoleName.ToUpper(), Canvas->ClipX - SafePad, SafePad, FLinearColor(0.88f, 0.84f, 0.74f, 0.90f), GEngine->GetSmallFont(), 0.82f);
		DrawRightAlignedText(FString::Printf(TEXT("%s / %s / AV%02d"), *LifeName.ToUpper(), *ReadyText, BHPS->AvatarIndex + 1), Canvas->ClipX - SafePad, SafePad + 20.0f, FLinearColor(0.55f, 0.52f, 0.47f, 0.76f), GEngine->GetSmallFont(), 0.58f);
		if ((BHGS && BHGS->bTestMode) || BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			const float ShortcutW = FMath::Clamp(Canvas->ClipX * 0.34f, 310.0f, 540.0f);
			DrawWrappedHudText(TEXT("TEST KEYS  INS RES  HOME TRAIN  PGUP PHASE  END FINAL  PGDN ESCAPE  DEL RECAP"),
				Canvas->ClipX - SafePad - ShortcutW,
				SafePad + 42.0f,
				ShortcutW,
				FLinearColor(0.95f, 0.86f, 0.42f, 0.80f),
				GEngine->GetSmallFont(),
				0.50f,
				11.0f,
				2);
		}
	}

	if (Character)
	{
		const float MeterW = FMath::Clamp(Canvas->ClipX * 0.22f, 210.0f, 310.0f);
		const float VitalsY = Canvas->ClipY - SafePad - 132.0f;
		const FString VitalsTitle = Character->IsDetentionMarked()
			? FString::Printf(TEXT("MARKED %.0fs"), Character->GetDetentionMarkRemaining())
			: (Character->IsHiddenInLocker() ? FString(TEXT("CONCEALED")) : FString(TEXT("BODY")));
		DrawHudText(VitalsTitle.ToUpper(), SafePad, VitalsY - 19.0f, Character->IsDetentionMarked() ? FLinearColor(1.0f, 0.20f, 0.12f, 0.96f) : FLinearColor(0.76f, 0.72f, 0.64f, 0.84f), GEngine->GetSmallFont(), 0.66f);
		DrawProgressBar(TEXT("BATTERY"), Character->GetFlashlightBattery(), SafePad, VitalsY, MeterW, FLinearColor(0.80f, 0.82f, 0.70f, 0.88f));

		const FBHTeacherProximityReadout TeacherProximity = FindTeacherProximity(GetWorld(), Character);
		const FString TeacherText = TeacherProximity.bFound
			? FString::Printf(TEXT("%s %.0fm"), TeacherProximity.bLineOfSight ? TEXT("VISIBLE") : TEXT("NEAR"), TeacherProximity.DistanceCm / 100.0f)
			: FString(TEXT("CLEAR"));
		DrawProgressBar(TEXT("TEACHER"), TeacherProximity.ProximityPercent, SafePad, VitalsY + 32.0f, MeterW, FLinearColor(0.90f, 0.36f, 0.22f, 0.90f), TeacherText);
		DrawRawMeter(TEXT("STAM"), Character->GetStaminaPercent(), SafePad, VitalsY + 68.0f, MeterW, FLinearColor(0.75f, 0.83f, 0.54f, 0.88f), false);
		DrawRawMeter(TEXT("FEAR"), Character->GetFear(), SafePad, VitalsY + 86.0f, MeterW, FLinearColor(0.92f, 0.28f, 0.20f, 0.88f), true);
		DrawRawMeter(TEXT("DREAD"), Character->GetDread(), SafePad, VitalsY + 104.0f, MeterW, FLinearColor(0.84f, 0.18f, 0.14f, 0.90f), true);

		if (!bShowSurvivorWarnings)
		{
			bHasVisibleHunterCue = false;
			SmoothedVisibleHunterArrowX = 0.0f;
		}
		else if (TeacherProximity.bFound && TeacherProximity.bLineOfSight)
		{
			bHasVisibleHunterCue = true;
			LastVisibleHunterLocation = TeacherProximity.TeacherLocation;
			LastVisibleHunterDistanceCm = TeacherProximity.DistanceCm;
			VisibleHunterCueUntilTime = Now + 0.34f;
		}

		if (bHasVisibleHunterCue)
		{
			const float CueStrength = FMath::Clamp((VisibleHunterCueUntilTime - Now) / 0.34f, 0.0f, 1.0f);
			if (CueStrength > 0.02f)
			{
				DrawVisibleHunterArrow(Character, LastVisibleHunterLocation, LastVisibleHunterDistanceCm, CueStrength);
			}
			else
			{
				bHasVisibleHunterCue = false;
				SmoothedVisibleHunterArrowX = 0.0f;
			}
		}
	}

	float PathThreatAlpha = 0.0f;
	const bool bPathDetected = bShowSurvivorWarnings && Character && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && HasNearbyPathThreat(GetWorld(), Character, PathThreatAlpha);

	if (Character && BHGS && BHGS->RoundPhase != EBHRoundPhase::Lobby && BHPC && (BHPC->IsHudMapVisible() || bPathDetected))
	{
		const float MapW = FMath::Clamp(Canvas->ClipX * 0.26f, 280.0f, 380.0f);
		const float MapX = Canvas->ClipX - SafePad - MapW;
		const float MaxMapY = FMath::Max(SafePad + 44.0f, Canvas->ClipY - SafePad - 270.0f);
		const float MapY = FMath::Clamp(Canvas->ClipY * 0.17f, SafePad + 44.0f, MaxMapY);
		DrawHeatSensor(Character, BHGS, MapX, MapY);
	}

	const float WarningLevel = bShowSurvivorWarnings ? FMath::Max(Character ? Character->GetDread() : 0.0f, BHGS ? BHGS->PresenceLevel : 0.0f) : 0.0f;
	const float DangerAlpha = FMath::Clamp(WarningLevel / 100.0f, 0.0f, 1.0f);
	DrawCrosshair(DangerAlpha);
	DrawNearbyNameTags(Character);
	DrawInteractionPrompt(Character);

	if (bPathDetected)
	{
		const FString PulseText = TEXT("PATH DETECTED");
		const float PulseScale = FMath::Lerp(1.02f, 1.22f, PathThreatAlpha);
		const float PulseY = Canvas->ClipY * 0.53f;
		const FLinearColor PulseColor(1.0f, 0.18f, 0.12f, FMath::Lerp(0.80f, 0.98f, PathThreatAlpha));
		float PulseW = 0.0f;
		float PulseH = 0.0f;
		Canvas->TextSize(GEngine->GetSmallFont(), PulseText, PulseW, PulseH, PulseScale, PulseScale);
		const float PulseX = (Canvas->ClipX - PulseW) * 0.5f;
		DrawHudText(PulseText, PulseX + 1.0f, PulseY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), GEngine->GetSmallFont(), PulseScale);
		DrawHudText(PulseText, PulseX, PulseY, PulseColor, GEngine->GetSmallFont(), PulseScale);
		DrawLine(PulseX + PulseW * 0.10f, PulseY + PulseH + 4.0f, PulseX + PulseW * 0.90f, PulseY + PulseH + 5.0f, FLinearColor(1.0f, 0.10f, 0.05f, FMath::Lerp(0.45f, 0.85f, PathThreatAlpha)), 2.0f);
	}

	if (BHPC)
	{
		if (BHPC->HasActiveStatusMessage())
		{
			const FString& Status = BHPC->GetStatusMessage();
			const float StatusAlpha = BHPC->GetStatusMessageAlpha();
			float TextW = 0.0f;
			float TextH = 0.0f;
			Canvas->TextSize(GEngine->GetSmallFont(), Status, TextW, TextH);
			const float ToastW = FMath::Clamp(TextW + 48.0f, 260.0f, Canvas->ClipX * 0.62f);
			const float ToastX = (Canvas->ClipX - ToastW) * 0.5f;
			const bool bLongStatus = TextW > ToastW - 48.0f;
			const float ToastH = bLongStatus ? 66.0f : 48.0f;
			const float ToastY = Canvas->ClipY * 0.72f + (1.0f - StatusAlpha) * 10.0f;
			DrawPanel(ToastX, ToastY, ToastW, ToastH, WithAlpha(FLinearColor(0.020f, 0.020f, 0.018f, 0.84f), StatusAlpha), WithAlpha(FLinearColor(0.96f, 0.74f, 0.36f, 0.94f), StatusAlpha));
			DrawWrappedHudText(Status, ToastX + 24.0f, ToastY + 15.0f, ToastW - 48.0f, WithAlpha(FLinearColor(0.96f, 0.87f, 0.62f, 1.0f), StatusAlpha), GEngine->GetSmallFont(), 0.92f, 17.0f, 2);
		}
	}

	DrawPhaseBanner(BHGS, Character);
}

void ABHHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& FillColor, const FLinearColor& AccentColor)
{
	if (!Canvas || W <= 1.0f || H <= 1.0f)
	{
		return;
	}

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.42f), X + 6.0f, Y + 7.0f, W, H);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.26f), X + 2.0f, Y + 2.0f, W, H);
	DrawRect(FillColor, X, Y, W, H);

	const float HeaderH = FMath::Min(24.0f, H * 0.30f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, FillColor.A * 0.045f), X + 2.0f, Y + 1.0f, W - 4.0f, HeaderH);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.24f), X + 2.0f, Y + HeaderH, W - 4.0f, 1.0f);
	DrawRect(FLinearColor(0.84f, 0.96f, 0.93f, FillColor.A * 0.18f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.40f), X, Y + H - 1.0f, W, 1.0f);
	DrawRect(AccentColor, X, Y, 3.0f, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, AccentColor.A * 0.18f), X + 3.0f, Y, 16.0f, H);

	for (float ScanY = Y + HeaderH + 8.0f; ScanY < Y + H - 4.0f; ScanY += 9.0f)
	{
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.055f), X + 5.0f, ScanY, W - 10.0f, 1.0f);
	}

	DrawCornerBrackets(X + 5.0f, Y + 5.0f, W - 10.0f, H - 10.0f, FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, AccentColor.A * 0.75f), 12.0f, 1.25f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.52f), X + W - 9.0f, Y + 7.0f, 3.0f, 3.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FillColor.A * 0.52f), X + W - 9.0f, Y + H - 10.0f, 3.0f, 3.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, FillColor.A * 0.18f), X + W - 8.0f, Y + 8.0f, 1.0f, 1.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, FillColor.A * 0.18f), X + W - 8.0f, Y + H - 9.0f, 1.0f, 1.0f);
}

void ABHHUD::DrawCornerBrackets(float X, float Y, float W, float H, const FLinearColor& Color, float Length, float Thickness)
{
	if (!Canvas || W <= 1.0f || H <= 1.0f)
	{
		return;
	}

	const float ClampedLength = FMath::Clamp(Length, 4.0f, FMath::Min(W, H) * 0.45f);
	DrawLine(X, Y, X + ClampedLength, Y, Color, Thickness);
	DrawLine(X, Y, X, Y + ClampedLength, Color, Thickness);
	DrawLine(X + W, Y, X + W - ClampedLength, Y, Color, Thickness);
	DrawLine(X + W, Y, X + W, Y + ClampedLength, Color, Thickness);
	DrawLine(X, Y + H, X + ClampedLength, Y + H, Color, Thickness);
	DrawLine(X, Y + H, X, Y + H - ClampedLength, Color, Thickness);
	DrawLine(X + W, Y + H, X + W - ClampedLength, Y + H, Color, Thickness);
	DrawLine(X + W, Y + H, X + W, Y + H - ClampedLength, Color, Thickness);
}

void ABHHUD::DrawCircle(float CenterX, float CenterY, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	if (!Canvas || Radius <= 0.0f)
	{
		return;
	}

	const int32 ClampedSegments = FMath::Clamp(Segments, 8, 96);
	FVector2D Previous(CenterX + Radius, CenterY);
	for (int32 Index = 1; Index <= ClampedSegments; ++Index)
	{
		const float Angle = (static_cast<float>(Index) / static_cast<float>(ClampedSegments)) * 2.0f * PI;
		const FVector2D Current(CenterX + FMath::Cos(Angle) * Radius, CenterY + FMath::Sin(Angle) * Radius);
		DrawLine(Previous.X, Previous.Y, Current.X, Current.Y, Color, Thickness);
		Previous = Current;
	}
}

void ABHHUD::DrawKeyBox(const FString& Key, float X, float Y, float W, float H, const FLinearColor& AccentColor, bool bLit)
{
	if (!Canvas || !GEngine || Key.IsEmpty())
	{
		return;
	}

	const FLinearColor BackColor = bLit ? FLinearColor(0.045f, 0.058f, 0.060f, 0.92f) : FLinearColor(0.025f, 0.030f, 0.033f, 0.76f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.54f), X + 2.0f, Y + 2.0f, W, H);
	DrawRect(BackColor, X, Y, W, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.74f : 0.26f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.50f : 0.18f), X, Y + H - 1.0f, W, 1.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f), X, Y, 1.0f, H);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f), X + W - 1.0f, Y, 1.0f, H);

	float TextW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(GEngine->GetSmallFont(), Key, TextW, TextH, 0.76f, 0.76f);
	DrawHudText(Key, X + (W - TextW) * 0.5f, Y + (H - TextH) * 0.5f - 1.0f, bLit ? FLinearColor(0.88f, 1.0f, 0.96f, 1.0f) : FLinearColor(0.50f, 0.58f, 0.58f, 0.88f), GEngine->GetSmallFont(), 0.76f);
}

void ABHHUD::DrawStatusPill(const FString& Label, float X, float Y, float W, const FLinearColor& AccentColor, bool bLit)
{
	if (!Canvas || !GEngine || Label.IsEmpty())
	{
		return;
	}

	const float H = 21.0f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.34f), X + 2.0f, Y + 2.0f, W, H);
	DrawRect(FLinearColor(0.028f, 0.034f, 0.035f, 0.88f), X, Y, W, H);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.28f : 0.10f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.90f : 0.34f), X + 8.0f, Y + 7.0f, 7.0f, 7.0f);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, bLit ? 0.18f : 0.05f), X + 6.0f, Y + 5.0f, 11.0f, 11.0f);
	DrawWrappedHudText(Label, X + 24.0f, Y + 5.0f, W - 29.0f, bLit ? MainText() : FLinearColor(0.58f, 0.64f, 0.64f, 0.86f), GEngine->GetSmallFont(), 0.60f, 10.0f, 1);
}

void ABHHUD::DrawHudText(const FString& Text, float X, float Y, const FLinearColor& Color, const UFont* Font, float Scale) const
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}

	const UFont* DrawFont = Font ? Font : (GEngine ? GEngine->GetSmallFont() : nullptr);
	if (!DrawFont)
	{
		return;
	}

	Canvas->SetDrawColor(FLinearColor(0.0f, 0.0f, 0.0f, Color.A * 0.72f).ToFColor(true));
	Canvas->DrawText(DrawFont, Text, X + 1.0f, Y + 1.0f, Scale, Scale);
	Canvas->SetDrawColor(Color.ToFColor(true));
	Canvas->DrawText(DrawFont, Text, X, Y, Scale, Scale);
}

void ABHHUD::DrawRightAlignedText(const FString& Text, float RightX, float Y, const FLinearColor& Color, const UFont* Font, float Scale) const
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}

	const UFont* DrawFont = Font ? Font : (GEngine ? GEngine->GetSmallFont() : nullptr);
	if (!DrawFont)
	{
		return;
	}

	float TextW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(DrawFont, Text, TextW, TextH, Scale, Scale);
	DrawHudText(Text, RightX - TextW, Y, Color, DrawFont, Scale);
}

float ABHHUD::DrawWrappedHudText(const FString& Text, float X, float Y, float MaxWidth, const FLinearColor& Color, const UFont* Font, float Scale, float LineHeight, int32 MaxLines) const
{
	if (!Canvas || Text.IsEmpty() || MaxWidth <= 1.0f || MaxLines <= 0)
	{
		return 0.0f;
	}

	const UFont* DrawFont = Font ? Font : (GEngine ? GEngine->GetSmallFont() : nullptr);
	if (!DrawFont)
	{
		return 0.0f;
	}

	FString Sanitized = Text;
	Sanitized.ReplaceInline(TEXT("\r"), TEXT(" "));
	Sanitized.ReplaceInline(TEXT("\n"), TEXT(" "));

	TArray<FString> Words;
	Sanitized.ParseIntoArray(Words, TEXT(" "), true);
	if (Words.IsEmpty())
	{
		return 0.0f;
	}

	TArray<FString> Lines;
	FString CurrentLine;
	bool bTruncated = false;
	for (int32 WordIndex = 0; WordIndex < Words.Num(); ++WordIndex)
	{
		const FString Candidate = CurrentLine.IsEmpty() ? Words[WordIndex] : CurrentLine + TEXT(" ") + Words[WordIndex];
		float CandidateW = 0.0f;
		float CandidateH = 0.0f;
		Canvas->TextSize(DrawFont, Candidate, CandidateW, CandidateH, Scale, Scale);
		if (CandidateW <= MaxWidth || CurrentLine.IsEmpty())
		{
			CurrentLine = Candidate;
			continue;
		}

		Lines.Add(CurrentLine);
		CurrentLine = Words[WordIndex];
		if (Lines.Num() >= MaxLines)
		{
			bTruncated = true;
			break;
		}
	}

	if (!bTruncated && !CurrentLine.IsEmpty() && Lines.Num() < MaxLines)
	{
		Lines.Add(CurrentLine);
	}
	else if (!CurrentLine.IsEmpty() && Lines.Num() >= MaxLines)
	{
		bTruncated = true;
	}

	if (bTruncated && !Lines.IsEmpty() && !Lines.Last().EndsWith(TEXT("...")))
	{
		FString& LastLine = Lines.Last();
		while (LastLine.Len() > 3)
		{
			const FString Candidate = LastLine + TEXT("...");
			float CandidateW = 0.0f;
			float CandidateH = 0.0f;
			Canvas->TextSize(DrawFont, Candidate, CandidateW, CandidateH, Scale, Scale);
			if (CandidateW <= MaxWidth)
			{
				LastLine = Candidate;
				break;
			}
			LastLine = LastLine.LeftChop(1);
		}
	}

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		DrawHudText(Lines[LineIndex], X, Y + LineIndex * LineHeight, Color, DrawFont, Scale);
	}

	return Lines.Num() * LineHeight;
}

void ABHHUD::DrawProgressBar(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, const FString& ValueText)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	const float BarH = 9.0f;
	const float BarY = Y + 13.0f;
	const float FillW = W * (ClampedValue / 100.0f);
	const bool bTeacherSignal = Label.Contains(TEXT("TEACHER"));
	const bool bTeacherVisible = bTeacherSignal && ValueText.Contains(TEXT("VISIBLE"));
	const bool bWarnLow = (Label.Contains(TEXT("BATTERY")) || Label.Contains(TEXT("STAMINA"))) && ClampedValue <= 24.0f;
	const bool bWarnHigh = (Label.Contains(TEXT("FEAR")) || Label.Contains(TEXT("DREAD")) || Label.Contains(TEXT("PRESENCE")) || bTeacherSignal) && (bTeacherVisible || ClampedValue >= (bTeacherSignal ? 58.0f : 72.0f));
	const bool bWarning = bWarnLow || bWarnHigh;
	const FString RightText = ValueText.IsEmpty() ? FString::Printf(TEXT("%.0f%%"), ClampedValue) : ValueText;
	const FLinearColor ReadoutColor = bWarning ? FLinearColor(1.0f, 0.42f, 0.30f, 0.98f) : MutedText();
	DrawHudText(Label, X, Y - 4.0f, MutedText(), GEngine->GetSmallFont(), 0.66f);
	DrawRightAlignedText(RightText, X + W, Y - 4.0f, ReadoutColor, GEngine->GetSmallFont(), 0.66f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.42f), X, BarY + 1.0f, W, BarH);
	DrawRect(FLinearColor(0.020f, 0.026f, 0.028f, 0.94f), X, BarY, W, BarH);
	DrawRect(FLinearColor(0.82f, 0.95f, 0.90f, 0.10f), X, BarY, W, 1.0f);
	if (FillW > 0.5f)
	{
		const FLinearColor EffectiveFill = bWarning ? FLinearColor(1.0f, 0.28f, 0.18f, 0.96f) : FillColor;
		DrawRect(FLinearColor(EffectiveFill.R, EffectiveFill.G, EffectiveFill.B, EffectiveFill.A * 0.20f), X, BarY - 2.0f, FillW, BarH + 4.0f);
		DrawRect(EffectiveFill, X, BarY, FillW, BarH);
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, EffectiveFill.A * 0.28f), X, BarY, FillW, 1.0f);
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, EffectiveFill.A * 0.38f), X + FMath::Max(0.0f, FillW - 2.0f), BarY - 1.0f, 2.0f, BarH + 2.0f);
	}
	for (int32 Segment = 1; Segment < 10; ++Segment)
	{
		const float SegmentX = X + W * (static_cast<float>(Segment) / 10.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.34f), SegmentX, BarY, 1.0f, BarH);
	}
	if (bWarnLow || bWarnHigh)
	{
		const float Pulse = GetWorld() ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 7.0f) : 1.0f;
		DrawRect(FLinearColor(1.0f, 0.18f, 0.10f, 0.22f + Pulse * 0.20f), X, BarY - 2.0f, W, BarH + 4.0f);
	}
}

void ABHHUD::DrawVisibleHunterArrow(const ABHCharacter* Character, const FVector& HunterLocation, float DistanceCm, float CueStrength)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character)
	{
		return;
	}

	const float Strength = FMath::Clamp(CueStrength, 0.0f, 1.0f);
	if (Strength <= 0.01f)
	{
		return;
	}

	FVector2D ScreenPosition(Canvas->ClipX * 0.5f, 0.0f);
	const FVector MarkerLocation = HunterLocation + FVector(0.0f, 0.0f, 124.0f);
	const bool bProjectedOnScreen = PlayerOwner->ProjectWorldLocationToScreen(MarkerLocation, ScreenPosition, true)
		&& ScreenPosition.X >= 0.0f
		&& ScreenPosition.X <= Canvas->ClipX
		&& ScreenPosition.Y >= 0.0f
		&& ScreenPosition.Y <= Canvas->ClipY;
	if (!bProjectedOnScreen)
	{
		const FVector ToHunter = (HunterLocation - Character->GetActorLocation()).GetSafeNormal2D();
		const float Side = FVector::DotProduct(ToHunter, Character->GetActorRightVector());
		ScreenPosition.X = Canvas->ClipX * (0.5f + FMath::Clamp(Side, -1.0f, 1.0f) * 0.34f);
	}

	const float EdgePadX = FMath::Min(FMath::Clamp(Canvas->ClipX * 0.18f, 130.0f, 260.0f), Canvas->ClipX * 0.42f);
	const float TargetArrowX = FMath::Clamp(ScreenPosition.X, EdgePadX, Canvas->ClipX - EdgePadX);
	if (SmoothedVisibleHunterArrowX <= 0.0f || SmoothedVisibleHunterArrowX < EdgePadX || SmoothedVisibleHunterArrowX > Canvas->ClipX - EdgePadX || FMath::Abs(SmoothedVisibleHunterArrowX - TargetArrowX) > Canvas->ClipX * 0.36f)
	{
		SmoothedVisibleHunterArrowX = TargetArrowX;
	}
	else
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 1.0f / 60.0f;
		SmoothedVisibleHunterArrowX = FMath::FInterpTo(SmoothedVisibleHunterArrowX, TargetArrowX, DeltaSeconds, 14.0f);
	}
	const float ArrowX = SmoothedVisibleHunterArrowX;
	const float DistanceAlpha = 1.0f - FMath::Clamp(DistanceCm / 6000.0f, 0.0f, 1.0f);
	const float Pulse = GetWorld() ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.0f) : 1.0f;
	const float CueAlpha = FMath::Lerp(0.74f, 0.98f, DistanceAlpha) * Strength;
	const float ArrowY = FMath::Max(17.0f, Canvas->ClipY * 0.020f) + Pulse * 1.5f;
	const float ArrowH = FMath::Clamp(Canvas->ClipY * 0.025f, 17.0f, 26.0f);
	const float ArrowW = ArrowH * 0.62f;
	const float TailH = FMath::Clamp(Canvas->ClipY * 0.011f, 7.0f, 11.0f);

	const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.76f * Strength);
	const FLinearColor ArrowColor(1.0f, 0.11f, 0.05f, CueAlpha);
	const FLinearColor HotColor(1.0f, 0.32f, 0.18f, FMath::Clamp(CueAlpha + Pulse * 0.10f * Strength, 0.0f, 1.0f));

	if (bProjectedOnScreen && ScreenPosition.Y > Canvas->ClipY * 0.10f && ScreenPosition.Y < Canvas->ClipY * 0.88f)
	{
		const float BracketW = FMath::Lerp(20.0f, 34.0f, DistanceAlpha);
		const float BracketH = FMath::Lerp(24.0f, 44.0f, DistanceAlpha);
		const float MarkerX = FMath::Clamp(ScreenPosition.X - BracketW * 0.5f, 8.0f, Canvas->ClipX - BracketW - 8.0f);
		const float MarkerY = FMath::Clamp(ScreenPosition.Y - BracketH * 0.42f, Canvas->ClipY * 0.10f, Canvas->ClipY - BracketH - 18.0f);
		const float MarkerPulse = 0.5f + 0.5f * FMath::Sin((GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) * 6.5f);
		DrawCornerBrackets(MarkerX + 1.0f, MarkerY + 1.0f, BracketW, BracketH, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 8.0f, 2.4f);
		DrawCornerBrackets(MarkerX, MarkerY, BracketW, BracketH, FLinearColor(1.0f, 0.10f, 0.04f, CueAlpha), 8.0f, 1.8f);
		DrawCircle(ScreenPosition.X, ScreenPosition.Y, FMath::Lerp(6.0f, 10.0f, DistanceAlpha) + MarkerPulse * 1.5f, FLinearColor(1.0f, 0.12f, 0.05f, (0.18f + MarkerPulse * 0.12f) * Strength), 1.2f, 24);
		DrawLine(MarkerX + BracketW * 0.28f, MarkerY + BracketH + 4.0f, MarkerX + BracketW * 0.72f, MarkerY + BracketH + 4.0f, FLinearColor(1.0f, 0.16f, 0.08f, (0.42f + MarkerPulse * 0.20f) * Strength), 1.6f);
	}

	DrawRect(FLinearColor(1.0f, 0.04f, 0.02f, (0.12f + Pulse * 0.10f) * Strength), ArrowX - ArrowW * 2.1f, 0.0f, ArrowW * 4.2f, 3.0f);
	DrawLine(ArrowX - ArrowW * 1.65f, ArrowY - 2.0f, ArrowX - ArrowW * 0.65f, ArrowY - 2.0f, FLinearColor(1.0f, 0.08f, 0.04f, 0.30f * Strength), 1.5f);
	DrawLine(ArrowX + ArrowW * 0.65f, ArrowY - 2.0f, ArrowX + ArrowW * 1.65f, ArrowY - 2.0f, FLinearColor(1.0f, 0.08f, 0.04f, 0.30f * Strength), 1.5f);
	DrawLine(ArrowX + 1.0f, ArrowY + ArrowH + 1.0f, ArrowX - ArrowW + 1.0f, ArrowY + 1.0f, ShadowColor, 4.5f);
	DrawLine(ArrowX + 1.0f, ArrowY + ArrowH + 1.0f, ArrowX + ArrowW + 1.0f, ArrowY + 1.0f, ShadowColor, 4.5f);
	DrawLine(ArrowX + 1.0f, ArrowY + ArrowH + 1.0f, ArrowX + 1.0f, ArrowY + ArrowH + TailH + 1.0f, ShadowColor, 3.5f);
	DrawLine(ArrowX, ArrowY + ArrowH, ArrowX - ArrowW, ArrowY, ArrowColor, 3.0f);
	DrawLine(ArrowX, ArrowY + ArrowH, ArrowX + ArrowW, ArrowY, ArrowColor, 3.0f);
	DrawLine(ArrowX, ArrowY + ArrowH, ArrowX, ArrowY + ArrowH + TailH, HotColor, 2.0f);

	const FString Label = FString::Printf(TEXT("TEACHER VISIBLE %.0fm"), DistanceCm / 100.0f);
	float TextW = 0.0f;
	float TextH = 0.0f;
	const float TextScale = 0.58f;
	Canvas->TextSize(GEngine->GetSmallFont(), Label, TextW, TextH, TextScale, TextScale);
	const float TextMaxX = FMath::Max(12.0f, Canvas->ClipX - TextW - 12.0f);
	const float TextX = FMath::Clamp(ArrowX - TextW * 0.5f, 12.0f, TextMaxX);
	const float TextY = ArrowY + ArrowH + TailH + 5.0f;
	DrawHudText(Label, TextX + 1.0f, TextY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f * Strength), GEngine->GetSmallFont(), TextScale);
	DrawHudText(Label, TextX, TextY, FLinearColor(1.0f, 0.34f, 0.22f, CueAlpha), GEngine->GetSmallFont(), TextScale);
}

void ABHHUD::DrawRawMeter(const FString& Label, float Value, float X, float Y, float W, const FLinearColor& FillColor, bool bHighIsBad)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	const float LabelW = 36.0f;
	const float BarX = X + LabelW + 8.0f;
	const float BarW = FMath::Max(1.0f, W - LabelW - 8.0f);
	const float BarH = 9.0f;
	const float FillW = BarW * (ClampedValue / 100.0f);
	const bool bWarning = bHighIsBad ? ClampedValue >= 72.0f : ClampedValue <= 24.0f;
	const FLinearColor TextColor = bWarning ? FLinearColor(1.0f, 0.42f, 0.30f, 0.98f) : FLinearColor(0.62f, 0.70f, 0.68f, 0.88f);
	const FLinearColor EffectiveFill = bWarning ? FLinearColor(1.0f, 0.28f, 0.18f, 0.96f) : FillColor;

	DrawHudText(Label, X, Y - 2.0f, TextColor, GEngine->GetSmallFont(), 0.56f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.44f), BarX, Y + 1.0f, BarW, BarH);
	DrawRect(FLinearColor(0.016f, 0.020f, 0.022f, 0.90f), BarX, Y, BarW, BarH);
	DrawRect(FLinearColor(0.82f, 0.95f, 0.90f, 0.10f), BarX, Y, BarW, 1.0f);
	if (FillW > 0.5f)
	{
		DrawRect(FLinearColor(EffectiveFill.R, EffectiveFill.G, EffectiveFill.B, EffectiveFill.A * 0.18f), BarX, Y - 1.0f, FillW, BarH + 2.0f);
		DrawRect(EffectiveFill, BarX, Y, FillW, BarH);
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, EffectiveFill.A * 0.28f), BarX, Y, FillW, 1.0f);
	}
	for (int32 Segment = 1; Segment < 5; ++Segment)
	{
		const float SegmentX = BarX + BarW * (static_cast<float>(Segment) / 5.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.32f), SegmentX, Y, 1.0f, BarH);
	}
}

void ABHHUD::DrawCrosshair(float DangerAlpha)
{
	if (!Canvas)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const FLinearColor CalmColor(0.78f, 0.76f, 0.66f, 0.46f);
	const FLinearColor ThreatColor(0.96f, 0.16f, 0.10f, 0.84f);
	const float ClampedDanger = FMath::Clamp(DangerAlpha, 0.0f, 1.0f);
	const FLinearColor CrosshairColor = FLinearColor::LerpUsingHSV(CalmColor, ThreatColor, ClampedDanger);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const ABHPlayerController* BHPC = PlayerOwner ? Cast<ABHPlayerController>(PlayerOwner) : nullptr;
	const int32 Style = BHPC ? FMath::Clamp(BHPC->GetCrosshairStyle(), 0, 3) : 0;
	const float JitterX = (FMath::Sin(Now * 23.0f) + FMath::Sin(Now * 7.0f) * 0.35f) * ClampedDanger * 1.85f;
	const float JitterY = (FMath::Cos(Now * 19.0f) + FMath::Sin(Now * 11.0f) * 0.25f) * ClampedDanger * 1.55f;
	const float DrawCenterX = CenterX + JitterX;
	const float DrawCenterY = CenterY + JitterY;
	const float Tremor = FMath::Sin(Now * 37.0f) * ClampedDanger;
	const float Thickness = FMath::Lerp(0.85f, 1.45f, ClampedDanger);
	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.38f);

	auto ScratchLine = [&](float X1, float Y1, float X2, float Y2, float Offset)
	{
		DrawLine(X1 + 1.0f, Y1 + 1.0f, X2 + 1.0f, Y2 + 1.0f, Shadow, Thickness + 0.45f);
		DrawLine(X1, Y1, X2, Y2, CrosshairColor, Thickness);
		if (ClampedDanger > 0.58f)
		{
			DrawLine(X1 + Offset, Y1 - Offset, X2 - Offset * 0.35f, Y2 + Offset * 0.30f, FLinearColor(ThreatColor.R, ThreatColor.G, ThreatColor.B, 0.20f + ClampedDanger * 0.24f), 0.7f);
		}
	};

	switch (Style)
	{
	case 1:
	{
		const float Size = FMath::Lerp(2.0f, 3.5f, ClampedDanger);
		DrawRect(Shadow, DrawCenterX - Size + 1.0f, DrawCenterY - Size + 1.0f, Size * 2.0f, Size * 2.0f);
		DrawRect(CrosshairColor, DrawCenterX - Size * 0.5f, DrawCenterY - Size * 0.5f, Size, Size);
		ScratchLine(DrawCenterX - 8.0f, DrawCenterY + 8.0f + Tremor, DrawCenterX - 3.0f, DrawCenterY + 8.5f, 1.0f);
		break;
	}
	case 2:
	{
		const float Gap = FMath::Lerp(6.0f, 10.0f, ClampedDanger);
		const float Reach = FMath::Lerp(14.0f, 22.0f, ClampedDanger);
		ScratchLine(DrawCenterX - Reach, DrawCenterY - 1.0f, DrawCenterX - Gap, DrawCenterY + Tremor, 1.4f);
		ScratchLine(DrawCenterX + Gap, DrawCenterY - Tremor, DrawCenterX + Reach, DrawCenterY + 1.0f, 1.1f);
		ScratchLine(DrawCenterX - 1.0f, DrawCenterY - Reach, DrawCenterX + Tremor, DrawCenterY - Gap, 0.9f);
		ScratchLine(DrawCenterX + Tremor, DrawCenterY + Gap, DrawCenterX - 1.0f, DrawCenterY + Reach, 1.3f);
		break;
	}
	case 3:
		if (ClampedDanger > 0.28f)
		{
			ScratchLine(DrawCenterX - 5.0f, DrawCenterY, DrawCenterX + 5.0f, DrawCenterY + Tremor, 0.8f);
			ScratchLine(DrawCenterX, DrawCenterY - 5.0f, DrawCenterX + Tremor, DrawCenterY + 5.0f, 0.8f);
		}
		else
		{
			DrawRect(FLinearColor(CrosshairColor.R, CrosshairColor.G, CrosshairColor.B, 0.34f), DrawCenterX - 0.5f, DrawCenterY - 0.5f, 1.0f, 1.0f);
		}
		break;
	case 0:
	default:
	{
		const float Reach = FMath::Lerp(8.0f, 14.0f, ClampedDanger);
		ScratchLine(DrawCenterX - Reach, DrawCenterY - 1.0f, DrawCenterX - 2.0f, DrawCenterY + Tremor, 1.1f);
		ScratchLine(DrawCenterX + 2.0f, DrawCenterY - Tremor, DrawCenterX + Reach * 0.72f, DrawCenterY + 1.0f, 0.9f);
		ScratchLine(DrawCenterX + Tremor, DrawCenterY - Reach * 0.86f, DrawCenterX - 1.0f, DrawCenterY - 2.0f, 1.0f);
		DrawRect(FLinearColor(CrosshairColor.R, CrosshairColor.G, CrosshairColor.B, CrosshairColor.A * 0.62f), DrawCenterX - 1.0f, DrawCenterY - 1.0f, 2.0f, 2.0f);
		break;
	}
	}
}

void ABHHUD::DrawHorrorOverlay(const ABHCharacter* Character, const ABHGameState* GameState)
{
	if (!Canvas)
	{
		return;
	}

	const ABHPlayerState* LocalPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!LocalPS || !LocalPS->IsAliveSurvivor())
	{
		return;
	}

	const float FearAlpha = Character ? FMath::Clamp(Character->GetFear() / 100.0f, 0.0f, 1.0f) : 0.0f;
	const float DreadAlpha = Character ? FMath::Clamp(Character->GetDread() / 100.0f, 0.0f, 1.0f) : 0.0f;
	const float PresenceAlpha = GameState ? FMath::Clamp(GameState->PresenceLevel / 100.0f, 0.0f, 1.0f) : 0.0f;
	const ABHPlayerController* BHPC = Cast<ABHPlayerController>(PlayerOwner);
	const float HorrorFlashAlpha = BHPC ? BHPC->GetHorrorCueFlashAlpha() : 0.0f;
	const float OverlayAlpha = FMath::Clamp(FMath::Max(FMath::Max(FearAlpha, DreadAlpha), PresenceAlpha) * 0.34f, 0.0f, 0.34f);
	if (OverlayAlpha <= 0.01f && HorrorFlashAlpha <= 0.01f)
	{
		return;
	}

	if (OverlayAlpha > 0.01f)
	{
		const float EdgeW = FMath::Clamp(Canvas->ClipX * (0.055f + OverlayAlpha * 0.16f), 42.0f, 210.0f);
		const float EdgeH = FMath::Clamp(Canvas->ClipY * (0.050f + OverlayAlpha * 0.18f), 32.0f, 150.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha), 0.0f, 0.0f, Canvas->ClipX, EdgeH);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha), 0.0f, Canvas->ClipY - EdgeH, Canvas->ClipX, EdgeH);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha * 0.85f), 0.0f, 0.0f, EdgeW, Canvas->ClipY);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, OverlayAlpha * 0.85f), Canvas->ClipX - EdgeW, 0.0f, EdgeW, Canvas->ClipY);
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float PresencePulseAlpha = FMath::Clamp((PresencePulseEndTime - Now) / 0.75f, 0.0f, 1.0f);
	if (PresencePulseAlpha > 0.0f)
	{
		DrawRect(FLinearColor(0.95f, 0.12f, 0.05f, PresencePulseAlpha * 0.10f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	if (HorrorFlashAlpha > 0.01f && BHPC)
	{
		FLinearColor FlashColor = BHPC->GetHorrorCueFlashColor();
		FlashColor.A = FMath::Clamp(HorrorFlashAlpha * 0.34f, 0.0f, 0.34f);
		DrawRect(FlashColor, 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	if (PresenceAlpha >= 0.55f || DreadAlpha >= 0.65f)
	{
		const float PulseAlpha = FMath::Clamp(FMath::Max(PresenceAlpha, DreadAlpha) * 0.12f, 0.0f, 0.12f);
		DrawRect(FLinearColor(0.72f, 0.02f, 0.02f, PulseAlpha), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	const float ScanlineAlpha = FMath::Clamp(OverlayAlpha * 0.22f + PresencePulseAlpha * 0.035f, 0.0f, 0.09f);
	if (ScanlineAlpha > 0.01f)
	{
		const float Drift = FMath::Fmod(Now * 18.0f, 46.0f);
		for (float Y = 18.0f + Drift; Y < Canvas->ClipY; Y += 46.0f)
		{
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, ScanlineAlpha), 0.0f, Y, Canvas->ClipX, 1.0f);
		}
	}

	const float GrainAlpha = FMath::Clamp(OverlayAlpha * 0.32f + PresencePulseAlpha * 0.05f, 0.0f, 0.12f);
	if (GrainAlpha > 0.012f)
	{
		for (int32 Index = 0; Index < 18; ++Index)
		{
			const float SeedA = FMath::Frac(FMath::Abs(FMath::Sin(Index * 12.9898f + Now * 3.7f)) * 43758.5453f);
			const float SeedB = FMath::Frac(FMath::Abs(FMath::Sin(Index * 78.233f + Now * 2.1f)) * 16421.371f);
			const float SpeckW = 1.0f + FMath::Frac(SeedA * 17.0f) * 3.0f;
			const float SpeckH = 1.0f + FMath::Frac(SeedB * 11.0f) * 18.0f;
			DrawRect(FLinearColor(0.92f, 1.0f, 0.96f, GrainAlpha * (0.20f + SeedB * 0.55f)), SeedA * Canvas->ClipX, SeedB * Canvas->ClipY, SpeckW, SpeckH);
		}
	}
}

void ABHHUD::DrawHeatSensor(const ABHCharacter* Character, const ABHGameState* GameState, float X, float Y)
{
	if (!Canvas || !GEngine || !Character || !GameState || !GetWorld())
	{
		return;
	}

	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.26f, 280.0f, 380.0f);
	const float PanelH = FMath::Clamp(Canvas->ClipY * 0.38f, 250.0f, 360.0f);
	const float MapX = X + 12.0f;
	const float MapY = Y + 31.0f;
	const float MapW = PanelW - 24.0f;
	const float MapH = PanelH - 47.0f;
	const float CenterX = MapX + MapW * 0.5f;
	const float CenterY = MapY + MapH * 0.5f;
	const float MapRadius = FMath::Min(MapW, MapH) * 0.47f;
	const float SensorRange = 6500.0f;
	const float Now = GetWorld()->GetTimeSeconds();
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.52f), X + 4.0f, Y + 5.0f, PanelW, PanelH);
	DrawRect(FLinearColor(0.014f, 0.014f, 0.012f, 0.74f), X, Y, PanelW, PanelH);
	DrawLine(X, Y, X + PanelW * 0.72f, Y + 1.0f, FLinearColor(0.58f, 0.52f, 0.42f, 0.42f), 1.0f);
	DrawLine(X + PanelW, Y + 7.0f, X + PanelW - 1.0f, Y + PanelH, FLinearColor(0.58f, 0.16f, 0.10f, 0.38f), 1.0f);
	DrawLine(X + 9.0f, Y + PanelH, X + PanelW, Y + PanelH - 2.0f, FLinearColor(0.58f, 0.52f, 0.42f, 0.32f), 1.0f);
	DrawHudText(TEXT("MAP"), X + 12.0f, Y + 10.0f, FLinearColor(0.75f, 0.69f, 0.57f, 0.80f), GEngine->GetSmallFont(), 0.64f);
	DrawRightAlignedText(TEXT("65M"), X + PanelW - 13.0f, Y + 10.0f, FLinearColor(0.55f, 0.50f, 0.43f, 0.70f), GEngine->GetSmallFont(), 0.56f);
	DrawRect(FLinearColor(0.020f, 0.022f, 0.019f, 0.86f), MapX, MapY, MapW, MapH);
	DrawLine(CenterX, MapY + 6.0f, CenterX, MapY + MapH - 6.0f, FLinearColor(0.65f, 0.62f, 0.52f, 0.08f), 1.0f);
	DrawLine(MapX + 6.0f, CenterY, MapX + MapW - 6.0f, CenterY, FLinearColor(0.65f, 0.62f, 0.52f, 0.08f), 1.0f);
	DrawCircle(CenterX, CenterY, MapRadius * 0.48f, FLinearColor(0.65f, 0.62f, 0.52f, 0.08f), 1.0f, 36);
	DrawCircle(CenterX, CenterY, MapRadius, FLinearColor(0.65f, 0.62f, 0.52f, 0.12f), 1.0f, 44);
	const float SweepAngle = FMath::Fmod(Now * 1.55f, 2.0f * PI) - PI * 0.5f;
	for (int32 Trail = 0; Trail < 2; ++Trail)
	{
		const float TrailAngle = SweepAngle - Trail * 0.19f;
		const float TrailAlpha = 0.16f / static_cast<float>(Trail + 1);
		DrawLine(CenterX, CenterY, CenterX + FMath::Cos(TrailAngle) * MapRadius, CenterY + FMath::Sin(TrailAngle) * MapRadius, FLinearColor(0.80f, 0.12f, 0.08f, TrailAlpha), 1.0f);
	}

	auto ProjectLocation = [&](const FVector& WorldLocation)
	{
		const FVector Delta = WorldLocation - Character->GetActorLocation();
		const float LocalRight = FVector::DotProduct(Delta, Character->GetActorRightVector());
		const float LocalForward = FVector::DotProduct(Delta, Character->GetActorForwardVector());
		const float PX = CenterX + FMath::Clamp(LocalRight / SensorRange, -1.0f, 1.0f) * MapRadius;
		const float PY = CenterY - FMath::Clamp(LocalForward / SensorRange, -1.0f, 1.0f) * MapRadius;
		return FVector2D(PX, PY);
	};

	auto DrawDot = [&](const FVector& WorldLocation, const FLinearColor& Color, float Size)
	{
		const FVector2D P = ProjectLocation(WorldLocation);
		const float Pulse = 0.75f + FMath::Sin(Now * 4.0f + Size) * 0.25f;
		DrawRect(FLinearColor(Color.R, Color.G, Color.B, Color.A * 0.16f * Pulse), P.X - Size, P.Y - Size, Size * 2.0f, Size * 2.0f);
		DrawRect(Color, P.X - Size * 0.45f, P.Y - Size * 0.45f, Size * 0.9f, Size * 0.9f);
	};

	for (TActorIterator<ABHBreaker> It(GetWorld()); It; ++It)
	{
		const ABHBreaker* Breaker = *It;
		if (Breaker && Breaker->IsDirectorActive())
		{
			DrawDot(Breaker->GetActorLocation(), FLinearColor(0.26f, 0.95f, 0.82f, 0.96f), 5.0f);
		}
	}

	for (TActorIterator<ABHObjectiveStation> It(GetWorld()); It; ++It)
	{
		const ABHObjectiveStation* Station = *It;
		if (Station && Station->IsDirectorActive() && !Station->IsCompleted())
		{
			DrawDot(Station->GetActorLocation(), Station->IsQuestionSolved() ? FLinearColor(0.54f, 0.76f, 1.0f, 0.94f) : FLinearColor(1.0f, 0.62f, 0.22f, 0.98f), 5.0f);
		}
	}

	for (TActorIterator<ABHExitGate> It(GetWorld()); It; ++It)
	{
		const ABHExitGate* ExitGate = *It;
		if (ExitGate && ExitGate->IsDirectorActive())
		{
			DrawDot(ExitGate->GetActorLocation(), GameState->bExitUnlocked ? FLinearColor(0.40f, 1.0f, 0.62f, 1.0f) : FLinearColor(0.72f, 0.72f, 0.66f, 0.72f), 7.0f);
		}
	}

	for (TActorIterator<ABHJumpscareMonster> It(GetWorld()); It; ++It)
	{
		const ABHJumpscareMonster* Monster = *It;
		if (Monster)
		{
			DrawDot(Monster->GetActorLocation(), FLinearColor(1.0f, 0.05f, 0.02f, 1.0f), 9.0f);
		}
	}

	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* OtherCharacter = *It;
		const ABHPlayerState* OtherPS = OtherCharacter ? OtherCharacter->GetBHPlayerState() : nullptr;
		if (!OtherCharacter || !OtherPS || OtherCharacter == Character || OtherPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		const FLinearColor HeatColor = OtherPS->PlayerRole == EBHPlayerRole::Hunter
			? FLinearColor(1.0f, 0.14f, 0.08f, 0.98f)
			: (OtherPS->PlayerRole == EBHPlayerRole::FakeHunter ? FLinearColor(1.0f, 0.48f, 0.14f, 0.94f) : FLinearColor(0.42f, 0.94f, 0.56f, 0.86f));
		DrawDot(OtherCharacter->GetActorLocation(), HeatColor, 6.0f);
	}

	DrawLine(CenterX - 5.0f, CenterY, CenterX + 5.0f, CenterY, FLinearColor(0.82f, 0.78f, 0.66f, 0.90f), 1.2f);
	DrawLine(CenterX, CenterY - 7.0f, CenterX, CenterY + 5.0f, FLinearColor(0.82f, 0.78f, 0.66f, 0.90f), 1.2f);
	DrawRect(FLinearColor(0.26f, 0.78f, 0.68f, 0.76f), X + 12.0f, Y + PanelH - 13.0f, 4.0f, 4.0f);
	DrawRect(FLinearColor(0.86f, 0.50f, 0.18f, 0.80f), X + 42.0f, Y + PanelH - 13.0f, 4.0f, 4.0f);
	DrawRect(FLinearColor(0.92f, 0.08f, 0.04f, 0.86f), X + 72.0f, Y + PanelH - 13.0f, 4.0f, 4.0f);
}

void ABHHUD::DrawInteractionPrompt(ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character || !GetWorld())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDInteractPrompt), false, Character);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * 620.0f;
	TArray<FHitResult> Hits;
	if (!GetWorld()->LineTraceMultiByChannel(Hits, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		return;
	}

	AActor* Target = nullptr;
	float TargetDistanceCm = 0.0f;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (Candidate && Candidate->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
		{
			Target = Candidate;
			TargetDistanceCm = Hit.Distance;
			break;
		}

		if (Hit.bBlockingHit)
		{
			break;
		}
	}

	if (!Target || !Target->GetClass()->ImplementsInterface(UBHInteractableInterface::StaticClass()))
	{
		return;
	}

	const FText Label = IBHInteractableInterface::Execute_GetInteractionLabel(Target, Character);
	const bool bCanInteract = IBHInteractableInterface::Execute_CanInteract(Target, Character);
	const FString Prompt = Label.ToString().ToUpper();
	const FString DistanceText = TargetDistanceCm > 1.0f ? FString::Printf(TEXT(" / %.1fm"), TargetDistanceCm / 100.0f) : FString();
	const FString PromptLine = bCanInteract
		? FString::Printf(TEXT("E / %s%s"), *Prompt, *DistanceText)
		: FString::Printf(TEXT("NO / %s%s"), *Prompt, *DistanceText);

	float TextW = 0.0f;
	float TextH = 0.0f;
	const float Scale = bCanInteract ? 0.68f : 0.62f;
	Canvas->TextSize(GEngine->GetSmallFont(), PromptLine, TextW, TextH, Scale, Scale);

	const float PromptX = (Canvas->ClipX - TextW) * 0.5f;
	const float PromptY = Canvas->ClipY * 0.5f + 29.0f;
	const FLinearColor PromptColor = bCanInteract ? FLinearColor(0.82f, 0.78f, 0.66f, 0.88f) : FLinearColor(0.54f, 0.50f, 0.45f, 0.72f);
	DrawHudText(PromptLine, PromptX + 1.0f, PromptY + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.42f), GEngine->GetSmallFont(), Scale);
	DrawHudText(PromptLine, PromptX, PromptY, PromptColor, GEngine->GetSmallFont(), Scale);
	if (bCanInteract)
	{
		const float ScratchY = PromptY + TextH + 3.0f;
		DrawLine(PromptX + TextW * 0.18f, ScratchY, PromptX + TextW * 0.82f, ScratchY + 1.0f, FLinearColor(0.82f, 0.18f, 0.12f, 0.44f), 1.0f);
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bCanViewRevisionQuestion = BHPS
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& (BHPS->IsAliveSurvivor() || (BHGS && BHGS->bRevisionMode && BHPS->PlayerRole == EBHPlayerRole::FakeHunter));
	if (const ABHObjectiveStation* Station = Cast<ABHObjectiveStation>(Target); Station && bCanViewRevisionQuestion)
	{
		DrawQuestionPanel(Station);
	}
}

void ABHHUD::DrawNearbyNameTags(const ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !PlayerOwner || !Character || !GetWorld())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);

	constexpr float MaxNameTagDistance = 1500.0f;
	constexpr float MaxNameTagDistanceSq = MaxNameTagDistance * MaxNameTagDistance;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		const ABHCharacter* OtherCharacter = *It;
		if (!OtherCharacter || OtherCharacter == Character || OtherCharacter->IsHidden())
		{
			continue;
		}

		const ABHPlayerState* OtherPS = OtherCharacter->GetBHPlayerState();
		if (!OtherPS || OtherPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Character->GetActorLocation(), OtherCharacter->GetActorLocation());
		if (DistanceSq > MaxNameTagDistanceSq)
		{
			continue;
		}

		const FVector NameLocation = OtherCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BHHUDNameTagLOS), false);
		Params.AddIgnoredActor(Character);
		Params.AddIgnoredActor(OtherCharacter);

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, NameLocation, ECC_Visibility, Params))
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerOwner->ProjectWorldLocationToScreen(NameLocation, ScreenPosition, true))
		{
			continue;
		}
		if (ScreenPosition.X < 0.0f || ScreenPosition.X > Canvas->ClipX || ScreenPosition.Y < 0.0f || ScreenPosition.Y > Canvas->ClipY)
		{
			continue;
		}

		FString Label = OtherPS->GetPlayerName().IsEmpty() ? FString(TEXT("PLAYER")) : OtherPS->GetPlayerName().ToUpper();
		const bool bThreatRole = OtherPS->PlayerRole == EBHPlayerRole::Hunter || OtherPS->PlayerRole == EBHPlayerRole::FakeHunter;
		if (bThreatRole)
		{
			Label = TEXT("HUNTER");
		}

		const float DistanceAlpha = 1.0f - FMath::Clamp(FMath::Sqrt(DistanceSq) / MaxNameTagDistance, 0.0f, 1.0f);
		const float Alpha = FMath::Lerp(0.48f, 0.96f, DistanceAlpha);
		const float Scale = bThreatRole ? 0.98f : 0.90f;
		const FLinearColor TextColor = bThreatRole
			? FLinearColor(0.96f, 0.18f, 0.12f, Alpha)
			: FLinearColor(0.82f, 0.78f, 0.66f, Alpha);

		float TextW = 0.0f;
		float TextH = 0.0f;
		Canvas->TextSize(GEngine->GetSmallFont(), Label, TextW, TextH, Scale, Scale);
		const float X = ScreenPosition.X - TextW * 0.5f;
		const float Y = ScreenPosition.Y - TextH * 0.5f;
		DrawHudText(Label, X + 1.0f, Y + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, Alpha * 0.70f), GEngine->GetSmallFont(), Scale);
		DrawHudText(Label, X, Y, TextColor, GEngine->GetSmallFont(), Scale);
		DrawLine(X + TextW * 0.18f, Y + TextH + 3.0f, X + TextW * 0.82f, Y + TextH + 3.0f, FLinearColor(TextColor.R, TextColor.G, TextColor.B, Alpha * 0.58f), 2.0f);
	}
}

void ABHHUD::DrawEquipmentStrip(const ABHGameState* GameState)
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float SafePad = FMath::Max(22.0f, Canvas->ClipX * 0.018f);
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.44f, 430.0f, 680.0f);
	const float PanelH = 42.0f;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY - SafePad - PanelH;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.008f, 0.011f, 0.012f, 0.58f), FLinearColor(0.45f, 0.70f, 0.68f, 0.64f));

	const FString ModeText = !GameState ? FString(TEXT("OFFLINE")) : (GameState->bTestMode ? FString(TEXT("TEST LOOP")) : (GameState->bPracticeMode ? FString(TEXT("PRACTICE LAB")) : FString(TEXT("LIVE FEED"))));
	const FString PhaseText = GameState ? GameState->GetPhaseText().ToUpper() : FString(TEXT("NO SIGNAL"));
	const FString PresenceText = GameState ? FString::Printf(TEXT("PRESENCE %.0f%%"), FMath::Clamp(GameState->PresenceLevel, 0.0f, 100.0f)) : FString(TEXT("PRESENCE --"));
	const FString ExitText = GameState && GameState->bExitUnlocked ? FString(TEXT("EXIT OPEN")) : FString(TEXT("EXIT LOCKED"));
	const FLinearColor ModeColor = GameState && GameState->bTestMode ? FLinearColor(0.95f, 0.86f, 0.42f, 1.0f) : FLinearColor(0.48f, 0.86f, 0.78f, 1.0f);
	const FLinearColor PresenceColor = GameState && GameState->PresenceLevel >= 72.0f ? FLinearColor(1.0f, 0.28f, 0.18f, 1.0f) : FLinearColor(0.62f, 0.82f, 0.78f, 1.0f);
	const float Gap = 8.0f;
	const float PillY = PanelY + 11.0f;
	const float PillW = (PanelW - 38.0f - Gap * 3.0f) / 4.0f;
	DrawStatusPill(ModeText, PanelX + 15.0f, PillY, PillW, ModeColor, GameState != nullptr);
	DrawStatusPill(PhaseText, PanelX + 15.0f + (PillW + Gap), PillY, PillW, FLinearColor(0.66f, 0.78f, 0.92f, 1.0f), GameState != nullptr);
	DrawStatusPill(PresenceText, PanelX + 15.0f + (PillW + Gap) * 2.0f, PillY, PillW, PresenceColor, GameState != nullptr);
	DrawStatusPill(ExitText, PanelX + 15.0f + (PillW + Gap) * 3.0f, PillY, PillW, GameState && GameState->bExitUnlocked ? FLinearColor(0.36f, 1.0f, 0.68f, 1.0f) : FLinearColor(0.96f, 0.42f, 0.34f, 1.0f), GameState != nullptr);
}

void ABHHUD::DrawQuestionPanel(const ABHObjectiveStation* Station)
{
	if (!Canvas || !GEngine || !Station || !Station->IsDirectorActive() || Station->IsCompleted() || Station->IsQuestionSolved() || Station->GetQuestionChoiceCount() <= 0)
	{
		return;
	}

	const int32 ChoiceCount = FMath::Min(4, Station->GetQuestionChoiceCount());
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.60f, 560.0f, 920.0f);
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRevisionQuestion = BHGS && BHGS->bRevisionMode;
	const float DiagramH = bRevisionQuestion ? 118.0f : 0.0f;
	const float PanelH = 174.0f + DiagramH + ChoiceCount * 32.0f + (Station->GetQuestionFeedback().IsEmpty() ? 0.0f : 42.0f);
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float MaxPanelY = FMath::Max(120.0f, Canvas->ClipY - PanelH - 92.0f);
	const float PanelY = FMath::Min(FMath::Max(Canvas->ClipY * 0.58f, 120.0f), MaxPanelY);
	DrawPanel(PanelX, PanelY, PanelW, PanelH, FLinearColor(0.016f, 0.018f, 0.017f, 0.88f), FLinearColor(0.95f, 0.56f, 0.18f, 0.95f));

	FString Topic = Station->GetQuestionTopic();
	if (Topic.IsEmpty())
	{
		Topic = TEXT("Class");
	}
	const FString ProgressSuffix = bRevisionQuestion
		? FString::Printf(TEXT(" %d/%d"), FMath::Clamp(Station->GetRevisionQuestionsSolved() + 1, 1, FMath::Max(1, Station->GetRevisionQuestionsRequired())), FMath::Max(1, Station->GetRevisionQuestionsRequired()))
		: TEXT("");
	DrawHudText(FString::Printf(TEXT("%s CHECKPOINT%s"), *Topic.ToUpper(), *ProgressSuffix), PanelX + 22.0f, PanelY + 16.0f, FLinearColor(1.0f, 0.72f, 0.36f, 1.0f), GEngine->GetSmallFont(), 0.94f);
	if (bRevisionQuestion)
	{
		FString CounterText;
		if (Station->GetRevisionCounterType() == EBHRevisionCounterNodeType::PeerReview)
		{
			CounterText = TEXT(" | COUNTER: PEER REVIEW");
		}
		else if (Station->GetRevisionCounterType() == EBHRevisionCounterNodeType::DemonstrationTrap)
		{
			CounterText = TEXT(" | COUNTER: DEMO TRAP");
		}
		const FString Meta = FString::Printf(TEXT("%s | %s | %s%s"),
			*FBHRevisionQuestionBank::QuestionTypeToString(Station->GetQuestionType()),
			*FBHRevisionQuestionBank::DifficultyToString(Station->GetQuestionDifficulty()),
			*Station->GetQuestionSubtopic(),
			*CounterText);
		DrawRightAlignedText(Meta, PanelX + PanelW - 22.0f, PanelY + 16.0f, FLinearColor(0.78f, 0.86f, 0.94f, 1.0f), GEngine->GetSmallFont(), 0.76f);
	}
	DrawWrappedHudText(Station->GetQuestionPrompt(), PanelX + 22.0f, PanelY + 44.0f, PanelW - 44.0f, MainText(), GEngine->GetSmallFont(), 0.92f, 17.0f, 2);

	float ChoiceStartY = PanelY + 92.0f;
	if (bRevisionQuestion)
	{
		DrawRevisionDiagram(Station, PanelX + 24.0f, PanelY + 88.0f, PanelW - 48.0f, DiagramH - 10.0f);
		ChoiceStartY = PanelY + 92.0f + DiagramH;
		if (!Station->GetRevisionTeamSummary().IsEmpty())
		{
			DrawWrappedHudText(FString::Printf(TEXT("Answer team: %s"), *Station->GetRevisionTeamSummary()), PanelX + 28.0f, ChoiceStartY - 17.0f, PanelW - 56.0f, FLinearColor(0.76f, 0.92f, 0.98f, 1.0f), GEngine->GetSmallFont(), 0.72f, 12.0f, 1);
		}
	}

	for (int32 Index = 0; Index < ChoiceCount; ++Index)
	{
		const float ChoiceY = ChoiceStartY + Index * 32.0f;
		const FLinearColor RowColor = Index % 2 == 0 ? FLinearColor(0.045f, 0.052f, 0.050f, 0.80f) : FLinearColor(0.034f, 0.041f, 0.040f, 0.80f);
		DrawRect(RowColor, PanelX + 26.0f, ChoiceY - 5.0f, PanelW - 52.0f, 27.0f);
		DrawRect(FLinearColor(0.95f, 0.56f, 0.18f, 0.34f), PanelX + 26.0f, ChoiceY - 5.0f, 3.0f, 27.0f);
		DrawKeyBox(FString::Printf(TEXT("%d"), Index + 1), PanelX + 38.0f, ChoiceY - 2.0f, 26.0f, 21.0f, FLinearColor(0.95f, 0.56f, 0.18f, 0.94f), true);
		DrawWrappedHudText(Station->GetQuestionChoice(Index), PanelX + 76.0f, ChoiceY + 1.0f, PanelW - 116.0f, FLinearColor(0.88f, 0.92f, 0.88f, 1.0f), GEngine->GetSmallFont(), 0.82f, 14.0f, 1);
	}

	if (!Station->GetQuestionFeedback().IsEmpty())
	{
		const FLinearColor FeedbackColor = Station->IsQuestionFeedbackCorrect()
			? FLinearColor(0.42f, 1.0f, 0.62f, 1.0f)
			: FLinearColor(1.0f, 0.42f, 0.34f, 1.0f);
		DrawWrappedHudText(Station->GetQuestionFeedback(), PanelX + 22.0f, PanelY + PanelH - 38.0f, PanelW - 44.0f, FeedbackColor, GEngine->GetSmallFont(), 0.80f, 14.0f, 2);
	}
}

void ABHHUD::DrawRevisionDiagram(const ABHObjectiveStation* Station, float X, float Y, float W, float H)
{
	if (!Canvas || !GEngine || !Station)
	{
		return;
	}

	DrawRect(FLinearColor(0.025f, 0.032f, 0.036f, 0.88f), X, Y, W, H);
	DrawRect(FLinearColor(0.18f, 0.28f, 0.30f, 0.82f), X, Y, W, 1.0f);
	DrawRect(FLinearColor(0.18f, 0.28f, 0.30f, 0.58f), X, Y + H - 1.0f, W, 1.0f);
	for (float GridX = X + 42.0f; GridX < X + W - 12.0f; GridX += 48.0f)
	{
		DrawRect(FLinearColor(0.42f, 0.58f, 0.58f, 0.055f), GridX, Y + 6.0f, 1.0f, H - 12.0f);
	}
	for (float GridY = Y + 30.0f; GridY < Y + H - 8.0f; GridY += 28.0f)
	{
		DrawRect(FLinearColor(0.42f, 0.58f, 0.58f, 0.055f), X + 8.0f, GridY, W - 16.0f, 1.0f);
	}
	DrawCornerBrackets(X + 6.0f, Y + 6.0f, W - 12.0f, H - 12.0f, FLinearColor(0.48f, 0.92f, 0.86f, 0.28f), 10.0f, 1.0f);

	const FLinearColor LineColor(0.48f, 0.92f, 0.86f, 1.0f);
	const FLinearColor WarmColor(0.95f, 0.62f, 0.28f, 1.0f);
	const float MidY = Y + H * 0.52f;
	const float Left = X + 26.0f;
	const float Right = X + W - 26.0f;
	const float Top = Y + 16.0f;
	const float Bottom = Y + H - 18.0f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	switch (Station->GetQuestionDiagramType())
	{
	case EBHDiagramType::MotionGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom - 8.0f, X + W * 0.42f, Y + H * 0.38f, LineColor, 3.0f);
		DrawLine(X + W * 0.42f, Y + H * 0.38f, Right, Y + H * 0.26f, LineColor, 3.0f);
		DrawHudText(TEXT("gradient = velocity"), Left + 8.0f, Top + 4.0f, WarmColor, GEngine->GetSmallFont(), 0.72f);
		break;
	case EBHDiagramType::VelocityGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawRect(FLinearColor(0.30f, 0.60f, 0.78f, 0.24f), Left + 10.0f, MidY, W * 0.38f, Bottom - MidY);
		DrawLine(Left + 10.0f, MidY, X + W * 0.52f, MidY, LineColor, 3.0f);
		DrawLine(X + W * 0.52f, MidY, Right, Top + 8.0f, LineColor, 3.0f);
		DrawHudText(TEXT("area = distance"), Left + 18.0f, Bottom - 34.0f, WarmColor, GEngine->GetSmallFont(), 0.72f);
		break;
	case EBHDiagramType::ForceArrows:
		DrawLine(X + W * 0.50f, MidY, X + W * 0.26f, MidY, WarmColor, 4.0f);
		DrawLine(X + W * 0.50f, MidY, X + W * 0.74f, MidY, LineColor, 4.0f);
		DrawLine(X + W * 0.74f, MidY, X + W * 0.70f, MidY - 10.0f, LineColor, 3.0f);
		DrawLine(X + W * 0.74f, MidY, X + W * 0.70f, MidY + 10.0f, LineColor, 3.0f);
		DrawHudText(TEXT("resultant force"), X + W * 0.39f, Top + 8.0f, MainText(), GEngine->GetSmallFont(), 0.78f);
		break;
	case EBHDiagramType::SpringGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Right - 32.0f, Top + 18.0f, LineColor, 3.0f);
		DrawHudText(TEXT("gradient = k"), Left + 18.0f, Top + 6.0f, WarmColor, GEngine->GetSmallFont(), 0.76f);
		break;
	case EBHDiagramType::MomentBeam:
		DrawRect(FLinearColor(0.55f, 0.50f, 0.38f, 1.0f), Left, MidY, Right - Left, 8.0f);
		DrawLine(X + W * 0.48f, MidY + 8.0f, X + W * 0.48f - 14.0f, Bottom, WarmColor, 3.0f);
		DrawLine(X + W * 0.48f, MidY + 8.0f, X + W * 0.48f + 14.0f, Bottom, WarmColor, 3.0f);
		DrawLine(X + W * 0.78f, MidY - 30.0f, X + W * 0.78f, MidY, LineColor, 4.0f);
		DrawHudText(TEXT("moment = Fd"), Left + 14.0f, Top + 8.0f, MainText(), GEngine->GetSmallFont(), 0.78f);
		break;
	case EBHDiagramType::Circuit:
		DrawLine(Left, Top + 16.0f, Right, Top + 16.0f, LineColor, 2.0f);
		DrawLine(Right, Top + 16.0f, Right, Bottom - 10.0f, LineColor, 2.0f);
		DrawLine(Right, Bottom - 10.0f, Left, Bottom - 10.0f, LineColor, 2.0f);
		DrawLine(Left, Bottom - 10.0f, Left, Top + 16.0f, LineColor, 2.0f);
		DrawRect(WarmColor, Left + W * 0.36f, Top + 8.0f, 34.0f, 16.0f);
		DrawHudText(TEXT("A series | V parallel"), Left + 18.0f, MidY - 8.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		break;
	case EBHDiagramType::IVGraph:
		DrawLine(Left, Bottom, Right, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, Bottom, Right - 24.0f, Top + 12.0f, LineColor, 3.0f);
		DrawHudText(TEXT("straight: ohmic"), Left + 16.0f, Top + 8.0f, WarmColor, GEngine->GetSmallFont(), 0.74f);
		break;
	case EBHDiagramType::StaticCharge:
		DrawHudText(TEXT("+ + +"), Left + 24.0f, MidY - 8.0f, WarmColor, GEngine->GetSmallFont(), 1.0f);
		DrawHudText(TEXT("- - -"), Right - 92.0f, MidY - 8.0f, LineColor, GEngine->GetSmallFont(), 1.0f);
		DrawLine(X + W * 0.42f, MidY, X + W * 0.58f, MidY, MainText(), 3.0f);
		DrawHudText(TEXT("opposites attract"), X + W * 0.38f, Top + 8.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		break;
	case EBHDiagramType::Wave:
	{
		const int32 Segments = 36;
		FVector2D Prev(Left, MidY);
		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float T = static_cast<float>(Index) / Segments;
			const float PX = FMath::Lerp(Left, Right, T);
			const float PY = MidY + FMath::Sin(T * PI * 4.0f + Now * 2.0f) * H * 0.22f;
			DrawLine(Prev.X, Prev.Y, PX, PY, LineColor, 2.5f);
			Prev = FVector2D(PX, PY);
		}
		DrawHudText(TEXT("amplitude | wavelength"), Left + 14.0f, Top + 6.0f, WarmColor, GEngine->GetSmallFont(), 0.74f);
		break;
	}
	case EBHDiagramType::EMSpectrum:
	{
		const TCHAR* Labels[] = {TEXT("R"), TEXT("M"), TEXT("IR"), TEXT("VIS"), TEXT("UV"), TEXT("X"), TEXT("G")};
		const float SegmentW = (Right - Left) / 7.0f;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float SX = Left + SegmentW * Index;
			DrawRect(Index % 2 == 0 ? FLinearColor(0.22f, 0.32f, 0.42f, 1.0f) : FLinearColor(0.34f, 0.26f, 0.42f, 1.0f), SX, MidY - 16.0f, SegmentW - 3.0f, 32.0f);
			DrawHudText(Labels[Index], SX + 8.0f, MidY - 5.0f, MainText(), GEngine->GetSmallFont(), 0.72f);
		}
		DrawHudText(TEXT("long wavelength -> high frequency"), Left + 10.0f, Top + 4.0f, WarmColor, GEngine->GetSmallFont(), 0.68f);
		break;
	}
	case EBHDiagramType::RayDiagram:
		DrawLine(X + W * 0.50f, Top, X + W * 0.50f, Bottom, FLinearColor(0.50f, 0.56f, 0.58f, 1.0f), 1.5f);
		DrawLine(Left, MidY - 34.0f, X + W * 0.50f, MidY, WarmColor, 3.0f);
		DrawLine(X + W * 0.50f, MidY, Right, MidY + 22.0f, LineColor, 3.0f);
		DrawHudText(TEXT("normal | i = r / refraction"), Left + 8.0f, Top + 6.0f, MainText(), GEngine->GetSmallFont(), 0.72f);
		break;
	case EBHDiagramType::Sankey:
		DrawRect(LineColor, Left, MidY - 12.0f, W * 0.42f, 24.0f);
		DrawRect(FLinearColor(0.52f, 0.90f, 0.54f, 1.0f), X + W * 0.50f, MidY - 10.0f, W * 0.28f, 20.0f);
		DrawRect(WarmColor, X + W * 0.50f, MidY + 18.0f, W * 0.20f, 14.0f);
		DrawHudText(TEXT("input -> useful + wasted"), Left + 12.0f, Top + 6.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		break;
	case EBHDiagramType::EnergyChain:
	default:
		DrawRect(FLinearColor(0.26f, 0.42f, 0.34f, 1.0f), Left, MidY - 14.0f, 90.0f, 28.0f);
		DrawLine(Left + 95.0f, MidY, Left + 150.0f, MidY, LineColor, 3.0f);
		DrawRect(FLinearColor(0.35f, 0.34f, 0.52f, 1.0f), Left + 156.0f, MidY - 14.0f, 96.0f, 28.0f);
		DrawLine(Left + 258.0f, MidY, Left + 312.0f, MidY, LineColor, 3.0f);
		DrawRect(FLinearColor(0.46f, 0.31f, 0.28f, 1.0f), Left + 318.0f, MidY - 14.0f, 96.0f, 28.0f);
		DrawHudText(TEXT("store -> pathway -> store"), Left + 12.0f, Top + 6.0f, MainText(), GEngine->GetSmallFont(), 0.74f);
		break;
	}

	if (!Station->GetQuestionFormula().IsEmpty())
	{
		DrawRightAlignedText(FString::Printf(TEXT("Key idea: %s"), *Station->GetQuestionFormula()), X + W - 10.0f, Y + H - 18.0f, FLinearColor(0.95f, 0.84f, 0.45f, 1.0f), GEngine->GetSmallFont(), 0.66f);
	}
}

void ABHHUD::DrawPhaseBanner(const ABHGameState* GameState, const ABHCharacter* Character)
{
	if (!Canvas || !GEngine || !GameState || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	float Alpha = FMath::Clamp((PhaseBannerEndTime - Now) / 3.6f, 0.0f, 1.0f);
	if (GameState->RoundPhase == EBHRoundPhase::SurvivorsWin || GameState->RoundPhase == EBHRoundPhase::HunterWin)
	{
		Alpha = FMath::Max(Alpha, 0.86f);
	}

	if (Alpha <= 0.01f)
	{
		return;
	}

	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	FString Title = GameState->GetPhaseText().ToUpper();
	FString Subtitle = GameState->ObjectiveText;
	FLinearColor Accent(0.40f, 0.90f, 0.82f, 1.0f);

	switch (GameState->RoundPhase)
	{
	case EBHRoundPhase::Prep:
		Title = TEXT("PREP PHASE");
		Subtitle = TEXT("Route the objectives before the hunt begins.");
		Accent = FLinearColor(0.95f, 0.76f, 0.36f, 1.0f);
		break;
	case EBHRoundPhase::Hunt:
		Title = GameState->bTestMode ? TEXT("TEST ROUND") : (GameState->bPracticeMode ? TEXT("PRACTICE LAB") : (GameState->bRevisionMode ? TEXT("PHYSICS CLASSROOM") : TEXT("HUNT STARTED")));
		Subtitle = GameState->bTestMode ? TEXT("Tester role active. No timer, no minimum players, no forced round end.") : (GameState->bPracticeMode ? TEXT("Round end disabled. Test roles, tasks, and pressure.") : (GameState->bRevisionMode ? TEXT("Solve, correct, contribute, and escape the Physics Teacher.") : TEXT("Finish the objectives and reach the exit.")));
		Accent = FLinearColor(0.92f, 0.18f, 0.12f, 1.0f);
		break;
	case EBHRoundPhase::Intermission:
		Title = TEXT("SUBWAY INTERMISSION");
		Subtitle = GameState->TrainAnnouncement.IsEmpty() ? TEXT("Review the recap, answer bonus questions, buy upgrades, and board before departure.") : GameState->TrainAnnouncement;
		Accent = FLinearColor(0.30f, 0.92f, 0.82f, 1.0f);
		break;
	case EBHRoundPhase::FinalEscape:
		Title = TEXT("FINAL TRAIN");
		Subtitle = GameState->FinalEscapeState == EBHFinalEscapeState::Cutscene ? TEXT("Evacuation doors unlocking.") : TEXT("Reach any open subway door before departure.");
		Accent = FLinearColor(1.0f, 0.42f, 0.20f, 1.0f);
		break;
	case EBHRoundPhase::SurvivorsWin:
		Title = TEXT("SURVIVORS ESCAPED");
		Subtitle = TEXT("Round complete. Returning to lobby shortly.");
		Accent = FLinearColor(0.42f, 1.0f, 0.62f, 1.0f);
		break;
	case EBHRoundPhase::HunterWin:
		Title = TEXT("TEACHER WINS");
		Subtitle = TEXT("Round complete. Returning to lobby shortly.");
		Accent = FLinearColor(1.0f, 0.24f, 0.16f, 1.0f);
		break;
	case EBHRoundPhase::Lobby:
	default:
		Title = TEXT("LOBBY");
		Subtitle = TEXT("Ready up when everyone is connected.");
		break;
	}

	if (Character && Character->IsDetentionMarked() && GameState->RoundPhase == EBHRoundPhase::Hunt)
	{
		Subtitle = TEXT("Detention marked. Move carefully.");
	}

	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.48f, 420.0f, 720.0f);
	const float PanelH = 86.0f;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = Canvas->ClipY * 0.20f - (1.0f - SmoothAlpha) * 18.0f;
	DrawPanel(PanelX, PanelY, PanelW, PanelH, WithAlpha(FLinearColor(0.012f, 0.014f, 0.016f, 0.86f), SmoothAlpha), WithAlpha(Accent, SmoothAlpha));

	float TitleW = 0.0f;
	float TextH = 0.0f;
	Canvas->TextSize(GEngine->GetSmallFont(), Title, TitleW, TextH, 1.34f, 1.34f);
	DrawHudText(Title, PanelX + (PanelW - TitleW) * 0.5f, PanelY + 17.0f, WithAlpha(Accent, SmoothAlpha), GEngine->GetSmallFont(), 1.34f);

	DrawWrappedHudText(Subtitle, PanelX + 28.0f, PanelY + 52.0f, PanelW - 56.0f, WithAlpha(FLinearColor(0.82f, 0.90f, 0.88f, 1.0f), SmoothAlpha), GEngine->GetSmallFont(), 0.82f, 14.0f, 1);
}

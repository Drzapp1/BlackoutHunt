#include "BHTutorialDirector.h"

#include "BHBotController.h"
#include "BHBreaker.h"
#include "BHCharacter.h"
#include "BHCrawlSpaceVolume.h"
#include "BHExitGate.h"
#include "BHFlickerLight.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHLocker.h"
#include "BHObjectiveStation.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

ABHTutorialDirector::ABHTutorialDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ABHTutorialDirector::BeginPlay()
{
	Super::BeginPlay();

	// Server-only brain. Clients just receive the guidance status messages it broadcasts.
	if (!HasAuthority())
	{
		return;
	}

	// Defer a beat so the listen-server host's pawn and player state exist before the first lesson line.
	GetWorldTimerManager().SetTimer(ActivateTimerHandle, this, &ABHTutorialDirector::Activate, 1.0f, false);
}

void ABHTutorialDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivateTimerHandle);
		World->GetTimerManager().ClearTimer(EvalTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ABHTutorialDirector::Activate()
{
	if (bActivated || !HasAuthority() || !GetWorld())
	{
		return;
	}
	bActivated = true;

	// Which tutorial are we running, and is it the chained full course or a single selected lesson?
	if (const ABHGameMode* GameMode = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		Phase = GameMode->GetTutorialPhase();
		bChained = GameMode->IsTutorialChained();
	}

	// Cache the two question stations + exit gate (all phases end at the exit). Sort the stations by X so the
	// assignment is deterministic regardless of actor-iteration order: the westerly one is the primary.
	TArray<ABHObjectiveStation*> Stations;
	for (TActorIterator<ABHObjectiveStation> It(GetWorld()); It; ++It)
	{
		if (ABHObjectiveStation* Station = *It)
		{
			Stations.Add(Station);
		}
	}
	Stations.Sort([](const ABHObjectiveStation& A, const ABHObjectiveStation& B)
	{
		return A.GetActorLocation().X < B.GetActorLocation().X;
	});
	if (Stations.Num() > 0)
	{
		PracticeStation = Stations[0];
	}
	if (Stations.Num() > 1)
	{
		InteractiveStation = Stations[1];
	}
	for (TActorIterator<ABHExitGate> It(GetWorld()); It; ++It)
	{
		TutorialExitGate = *It;
		break;
	}
	TutorialBreaker = FindNearestBreaker(GetActorLocation());

	// Make the lone host immediately playable in this phase's role (Survivor / Hunter / FakeHunter): no
	// ready-up, no second player, live, Hunt phase.
	EnsurePlayablyConfigured();

	// Phase-specific setup + first step. The AI cast (student / teacher) is spawned at runtime, mirroring the
	// Survivor phase's scripted Teacher; the same baked map serves all three tutorials.
	switch (Phase)
	{
	case EBHTutorialPhase::Teacher:
		StudentBot = SpawnScriptedBot(EBHPlayerRole::Survivor, TEXT("Student"), FVector(3000.0f, 0.0f, 120.0f));
		EnterTeacherStep(ETeacherStep::Intro);
		break;
	case EBHTutorialPhase::Monitor:
		StudentBot = SpawnScriptedBot(EBHPlayerRole::Survivor, TEXT("Student"), FVector(3000.0f, 0.0f, 120.0f));
		TeacherBot = SpawnScriptedBot(EBHPlayerRole::Hunter, TEXT("Teacher"), GetWorld()->GetAuthGameMode<ABHGameMode>() ? GetWorld()->GetAuthGameMode<ABHGameMode>()->GetHunterSpawnLocation() : FVector(9400.0f, 0.0f, 120.0f));
		if (ABHObjectiveStation* Station = PracticeStation.Get())
		{
			Station->ConfigureTutorialVisualQuestion();
		}
		EnterMonitorStep(EMonitorStep::Intro);
		break;
	case EBHTutorialPhase::Survivor:
	default:
		// The Survivor lesson forces fixed visual + interactive questions onto the two stations.
		if (Stations.Num() > 0)
		{
			Stations[0]->ConfigureTutorialVisualQuestion();
		}
		if (Stations.Num() > 1)
		{
			Stations[1]->ConfigureTutorialInteractiveQuestion();
		}
		EnterStep(EStep::Intro);
		break;
	}
	GetWorldTimerManager().SetTimer(EvalTimerHandle, this, &ABHTutorialDirector::EvaluateStep, 0.5f, true);
}

float ABHTutorialDirector::StepElapsed() const
{
	const UWorld* World = GetWorld();
	return World ? (World->GetTimeSeconds() - StepStartServerTime) : 0.0f;
}

void ABHTutorialDirector::EnterStep(EStep NewStep)
{
	CurrentStep = NewStep;
	StepStartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	switch (NewStep)
	{
	case EStep::Intro:
		SetAllInputLocked(true);
		Broadcast(TEXT("Welcome to the Blackout Hunt tutorial. Watch for a moment - your controls unlock in a few seconds."), 5.0f);
		break;
	case EStep::Move:
		SetAllInputLocked(false);
		// Zero the WASD tally on entry so the prompt only clears once the student drives all four directions now.
		if (ABHCharacter* MoveSurvivor = FindTutorialSurvivor())
		{
			MoveSurvivor->ResetTutorialMovementMask();
			BroadcastMoveHint(MoveSurvivor->GetTutorialMovementMask());
		}
		else
		{
			BroadcastMoveHint(0);
		}
		NextMoveHintServerTime = StepStartServerTime + 3.0f;
		break;
	case EStep::Flashlight:
		SetAllInputLocked(false);
		Broadcast(TEXT("Good. Now press F to switch on your flashlight."), 12.0f);
		break;
	case EStep::Hide:
		bHideRegistered = false;
		Broadcast(TEXT("Nice. Head to the LOCKER (follow the marker) and press E to hide inside - this is how you avoid the Teacher."), 14.0f);
		break;
	case EStep::Crawl:
		Broadcast(TEXT("Good. Now the low CRAWL DUCT (follow the marker): press LEFT ALT to go prone, then crawl in. A standing Teacher can't follow."), 16.0f);
		break;
	case EStep::Breaker:
		// Cut the lights so the student has a real blackout to fix - this is the breaker lesson.
		SetTutorialLightsPowered(false);
		bTutorialBlackoutActive = true;
		Broadcast(TEXT("The power just cut out! Find the BREAKER (follow the marker) and hold E to repair it and bring the lights back."), 16.0f);
		break;
	case EStep::Question:
		// Re-assert the visual question in case the round director re-rolled it; only if still unsolved.
		if (ABHObjectiveStation* Station = PracticeStation.Get())
		{
			if (!Station->IsQuestionSolved())
			{
				Station->ConfigureTutorialVisualQuestion();
			}
		}
		Broadcast(TEXT("Now the lit STATION (follow the marker): hold E to work, then read the GRAPH and click your answer (or press 1-4)."), 16.0f);
		break;
	case EStep::Interact:
		// Re-assert the interactive question if the round director re-rolled it; only if still unsolved.
		if (ABHObjectiveStation* Station = InteractiveStation.Get())
		{
			if (!Station->IsQuestionSolved())
			{
				Station->ConfigureTutorialInteractiveQuestion();
			}
		}
		Broadcast(TEXT("One more: the SECOND station (follow the marker). Hold E, then DRAG each unit onto the right quantity and press Submit."), 18.0f);
		break;
	case EStep::Encounter:
		// A real chase: spawn the Teacher when it is time, play the "Teacher appears" reveal, then have it
		// actively pursue (driven each tick in EvaluateStep).
		SpawnScriptedTeacher();
		if (ABHBotController* Teacher = TeacherBot.Get())
		{
			if (const APawn* TeacherPawn = Teacher->GetPawn())
			{
				PlayTeacherRevealCutscene(TeacherPawn->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f));
			}
		}
		Broadcast(TEXT("The TEACHER is hunting you - RUN! Use lockers, the crawl duct, and corners to break line of sight, and get to the GREEN EXIT."), 8.0f);
		break;
	case EStep::Escape:
		Broadcast(TEXT("Keep moving - reach the GREEN EXIT door (follow the marker) to finish. Duck into a locker if the Teacher gets close."), 10.0f);
		break;
	case EStep::Complete:
		SetAllInputLocked(false);
		ClearBeats();
		Broadcast(bChained
			? TEXT("Survivor training complete! Now you'll learn to play as the TEACHER...")
			: TEXT("Survivor training complete - nice work! Returning to the menu..."), 6.0f);
		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(EvalTimerHandle);
			// Brief beat so the student reads the line, then ServerTravel to the next tutorial phase.
			GetWorldTimerManager().SetTimer(ActivateTimerHandle, this, &ABHTutorialDirector::FinishTutorialPhase, 3.0f, false);
		}
		break;
	default:
		break;
	}

	// Point the lone marker at the object this step needs (or clear it). Re-published each tick in
	// EvaluateStep so it keeps tracking the nearest locker/station as the student moves.
	PublishStepBeat();
}

void ABHTutorialDirector::EvaluateStep()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	// Keep the lone host in this phase's role every tick (a capture/revive or a late practice-mode pass could
	// otherwise leave them in a non-interactable state) and keep the single marker + scripted AI cast current.
	EnsurePlayablyConfigured();
	PublishStepBeat();
	DriveScriptedCast();

	// Capture-proof net: a tutorial has no real stakes, so if the scripted Teacher catches the human student we
	// put them straight back into play. (Only revives non-bots, so an AI student caught by the human Teacher in
	// the Teacher tutorial stays down - that IS the capture lesson completing.)
	ReviveIfCaught();

	// Teacher / Monitor phases run their own step machines; the Survivor machine continues below.
	if (Phase == EBHTutorialPhase::Teacher)
	{
		EvaluateTeacherStep();
		return;
	}
	if (Phase == EBHTutorialPhase::Monitor)
	{
		EvaluateMonitorStep();
		return;
	}

	ABHCharacter* Survivor = FindTutorialSurvivor();
	const float Elapsed = StepElapsed();

	switch (CurrentStep)
	{
	case EStep::Intro:
		if (Elapsed >= 5.0f)
		{
			EnterStep(EStep::Move);
		}
		break;
	case EStep::Move:
	{
		// Gate on the player actually driving all four directions; the WASD prompt stays pinned until then.
		const uint8 MoveMask = Survivor ? Survivor->GetTutorialMovementMask() : 0;
		const bool bPressedAll = Survivor && (MoveMask & ABHCharacter::TutorialMoveAllMask) == ABHCharacter::TutorialMoveAllMask;
		// A very generous fallback (a stuck gamepad / no keyboard) so a brand-new student can never hard-lock.
		if (bPressedAll || Elapsed >= 60.0f)
		{
			EnterStep(EStep::Flashlight);
			break;
		}
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextMoveHintServerTime)
		{
			BroadcastMoveHint(MoveMask);
			NextMoveHintServerTime = Now + 3.0f;
		}
		break;
	}
	case EStep::Flashlight:
		if ((Survivor && Survivor->IsFlashlightOn()) || Elapsed >= 14.0f)
		{
			EnterStep(EStep::Hide);
		}
		break;
	case EStep::Hide:
		// First register that the student actually hid; the next prompt must NOT fire while they are still
		// inside the locker (they can't see or act on it), so only advance once they have climbed back out.
		if (!bHideRegistered && Survivor && Survivor->IsHiddenInLocker())
		{
			bHideRegistered = true;
			Broadcast(TEXT("Good - you're hidden and the Teacher can't see you. When you're ready, press E to climb out."), 8.0f);
		}
		if ((bHideRegistered && Survivor && !Survivor->IsHiddenInLocker()) || Elapsed >= 28.0f)
		{
			EnterStep(EStep::Crawl);
		}
		break;
	case EStep::Crawl:
		// Advance once the student has actually gone low-profile by the duct, or on the generous timeout.
		if (SurvivorIsCrawlingNearDuct() || Elapsed >= 20.0f)
		{
			EnterStep(EStep::Breaker);
		}
		break;
	case EStep::Breaker:
	{
		const ABHBreaker* Breaker = TutorialBreaker.Get();
		const bool bRepaired = Breaker && Breaker->IsRepaired();
		// Restore the lights the moment the breaker is fixed (or on the generous timeout - never leave the
		// student stuck in the dark), then move on to the question.
		if (bRepaired || Elapsed >= 40.0f)
		{
			if (bTutorialBlackoutActive)
			{
				SetTutorialLightsPowered(true);
				bTutorialBlackoutActive = false;
			}
			if (bRepaired)
			{
				Broadcast(TEXT("Lights back on - nice work. That's how you keep the power up during a round."), 4.0f);
			}
			EnterStep(EStep::Question);
		}
		break;
	}
	case EStep::Question:
	{
		const ABHObjectiveStation* Station = PracticeStation.Get();
		if ((Station && (Station->IsQuestionSolved() || Station->IsCompleted())) || Elapsed >= 35.0f)
		{
			EnterStep(EStep::Interact);
		}
		break;
	}
	case EStep::Interact:
	{
		// If there is no second station (older bake), skip straight on rather than wait out the timeout.
		const ABHObjectiveStation* Station = InteractiveStation.Get();
		if (!Station || Station->IsQuestionSolved() || Station->IsCompleted() || Elapsed >= 40.0f)
		{
			EnterStep(EStep::Encounter);
		}
		break;
	}
	case EStep::Encounter:
		// Brief dramatic beat while the Teacher engages, then straight into the run-to-exit (the chase keeps
		// going through Escape via DriveTeacherChase, so it reads as one continuous pursuit).
		if (Elapsed >= 5.0f)
		{
			EnterStep(EStep::Escape);
		}
		break;
	case EStep::Escape:
	{
		bool bReachedExit = false;
		if (Survivor && TutorialExitGate.IsValid())
		{
			bReachedExit = FVector::Dist(Survivor->GetActorLocation(), TutorialExitGate->GetActorLocation()) < 450.0f;
		}
		// Generous fallback raised for the larger map so the longer run never times out mid-chase.
		if (bReachedExit || Elapsed >= 80.0f)
		{
			EnterStep(EStep::Complete);
		}
		break;
	}
	case EStep::Complete:
	default:
		break;
	}
}

void ABHTutorialDirector::Broadcast(const FString& Message, float DurationSeconds) const
{
	if (!GetWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(Message, DurationSeconds);
		}
	}
}

void ABHTutorialDirector::BroadcastMoveHint(uint8 MovementMask) const
{
	// Name the directions still un-pressed so the prompt reflects real progress and reads as a checklist.
	TArray<FString> Remaining;
	if ((MovementMask & ABHCharacter::TutorialMoveForwardBit) == 0) { Remaining.Add(TEXT("W")); }
	if ((MovementMask & ABHCharacter::TutorialMoveLeftBit) == 0) { Remaining.Add(TEXT("A")); }
	if ((MovementMask & ABHCharacter::TutorialMoveBackBit) == 0) { Remaining.Add(TEXT("S")); }
	if ((MovementMask & ABHCharacter::TutorialMoveRightBit) == 0) { Remaining.Add(TEXT("D")); }

	const FString Message = Remaining.Num() == 0
		? FString(TEXT("Great - that's all four directions. Moving on..."))
		: FString::Printf(TEXT("Use WASD to move and the mouse to look. Still need: %s"), *FString::Join(Remaining, TEXT(" ")));
	// Slightly longer than the 3s re-issue cadence so the line never blinks out between refreshes.
	Broadcast(Message, 3.5f);
}

void ABHTutorialDirector::SetAllInputLocked(bool bLocked) const
{
	if (!GetWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientSetJumpscareInputLocked(bLocked);
		}
	}
}

ABHCharacter* ABHTutorialDirector::FindTutorialSurvivor() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Character = *It;
		ABHPlayerState* PS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (PS && !PS->IsABot()
			&& (PS->PlayerRole == EBHPlayerRole::Survivor
				|| PS->PlayerRole == EBHPlayerRole::Tester
				|| PS->PlayerRole == EBHPlayerRole::Unassigned))
		{
			return Character;
		}
	}
	return nullptr;
}

ABHCharacter* ABHTutorialDirector::FindHumanPlayer() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Character = *It;
		const ABHPlayerState* PS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (PS && !PS->IsABot())
		{
			return Character;
		}
	}
	return nullptr;
}

ABHCharacter* ABHTutorialDirector::FindStudentCharacter() const
{
	const ABHBotController* Bot = StudentBot.Get();
	return Bot ? Cast<ABHCharacter>(Bot->GetPawn()) : nullptr;
}

void ABHTutorialDirector::DriveScriptedCast() const
{
	switch (Phase)
	{
	case EBHTutorialPhase::Survivor:
		if (CurrentStep == EStep::Encounter || CurrentStep == EStep::Escape)
		{
			DriveTeacherChase();
		}
		break;
	case EBHTutorialPhase::Teacher:
		// The human is the Hunter; keep the AI student lively so there's something to scan, chase and catch.
		DriveStudentBot();
		break;
	case EBHTutorialPhase::Monitor:
		// The AI student wanders; the AI teacher hunts it so the monitor has a live scene to mislead. If the
		// teacher ever catches the student, revive it (like the human revive) so the scene never goes dead -
		// the monitor lesson is ability/time gated, so this is purely to keep something to watch and mislead.
		if (ABHBotController* StudentCtrl = StudentBot.Get())
		{
			ABHPlayerState* StudentPS = StudentCtrl->GetPlayerState<ABHPlayerState>();
			if (StudentPS && StudentPS->LifeState == EBHPlayerLifeState::Captured)
			{
				StudentPS->SetLifeState(EBHPlayerLifeState::Alive);
				if (ABHGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
				{
					GM->RestartPlayer(StudentCtrl);
				}
			}
		}
		DriveStudentBot();
		if (ABHCharacter* Student = FindStudentCharacter())
		{
			DriveBotChase(TeacherBot.Get(), Student);
		}
		break;
	default:
		break;
	}
}

void ABHTutorialDirector::DriveStudentBot() const
{
	ABHBotController* Bot = StudentBot.Get();
	ABHCharacter* Student = FindStudentCharacter();
	if (!Bot || !Student)
	{
		return;
	}
	// Flee the nearest Hunter (the human Teacher, or the AI teacher in the Monitor phase) when it gets close;
	// otherwise wander to a station so the student is always on the move.
	ABHCharacter* Threat = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Other = *It;
		const ABHPlayerState* PS = Other ? Other->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!PS || (PS->PlayerRole != EBHPlayerRole::Hunter && PS->PlayerRole != EBHPlayerRole::Tester))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Student->GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Threat = Other;
		}
	}
	if (Threat && BestDistSq < (1500.0f * 1500.0f))
	{
		Bot->RunStateTreeIntent(EBHBotIntent::Flee, Threat, Student->GetActorLocation(), 200.0f);
	}
	else
	{
		Bot->RunStateTreeIntent(EBHBotIntent::WorkStation, nullptr, FVector::ZeroVector, 220.0f);
	}
}

void ABHTutorialDirector::DriveBotChase(ABHBotController* Bot, ABHCharacter* Target) const
{
	if (Bot && Target)
	{
		Bot->RunStateTreeIntent(EBHBotIntent::Chase, Target, Target->GetActorLocation(), 120.0f);
	}
}

void ABHTutorialDirector::FinishTutorialPhase() const
{
	if (ABHGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		GameMode->AdvanceTutorialPhase();
	}
}

void ABHTutorialDirector::EnsurePlayablyConfigured() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Hunt phase + practice mode is what the interaction/objective gates require; assert it so the tutorial
	// is interactable no matter how the map was entered (menu, PIE direct-open, or a stale lobby phase).
	if (ABHGameState* BHGS = World->GetGameState<ABHGameState>())
	{
		if (BHGS->RoundPhase != EBHRoundPhase::Hunt)
		{
			BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		}
		if (!BHGS->bPracticeMode)
		{
			BHGS->SetPracticeMode(true);
		}
		if (!BHGS->bTutorialMode)
		{
			// Lets client systems (adaptive-graphics prompt, horror vignette) soften for the tutorial.
			BHGS->SetTutorialMode(true);
		}
	}

	// Promote the lone human to this phase's role: Survivor (survivor tutorial), Hunter (teacher tutorial) or
	// FakeHunter (monitor tutorial). All three function in the practice sandbox (no ready-up / second player),
	// and the role unlocks the abilities each tutorial teaches. Re-asserted each tick so it never drifts.
	const EBHPlayerRole DesiredRole = Phase == EBHTutorialPhase::Teacher ? EBHPlayerRole::Hunter
		: (Phase == EBHTutorialPhase::Monitor ? EBHPlayerRole::FakeHunter : EBHPlayerRole::Survivor);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHPlayerState* PS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!PS || PS->IsABot())
		{
			continue;
		}
		if (PS->PlayerRole != DesiredRole)
		{
			PS->SetRole(DesiredRole);
			PS->SetDesiredRole(DesiredRole);
		}
		if (!PS->bReady)
		{
			PS->SetReady(true);
		}
		if (PS->LifeState != EBHPlayerLifeState::Alive)
		{
			PS->SetLifeState(EBHPlayerLifeState::Alive);
		}
	}
}

void ABHTutorialDirector::SetSingleBeat(const FString& Label, const FVector& Location) const
{
	UWorld* World = GetWorld();
	ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS)
	{
		return;
	}

	FBHObjectiveBeat Beat;
	Beat.BeatId = TEXT("TutorialNext");
	Beat.Label = Label;
	Beat.Location = Location;
	Beat.Radius = 9000.0f; // Always visible across the compact map so the student can never lose the marker.
	Beat.ExpireServerTime = 0.0f;
	Beat.bPrimary = true;
	Beat.bDanger = false;
	BHGS->SetObjectiveBeats(TArray<FBHObjectiveBeat>{ Beat });
}

void ABHTutorialDirector::ClearBeats() const
{
	UWorld* World = GetWorld();
	if (ABHGameState* BHGS = World ? World->GetGameState<ABHGameState>() : nullptr)
	{
		BHGS->SetObjectiveBeats(TArray<FBHObjectiveBeat>{});
	}
}

void ABHTutorialDirector::PublishStepBeat() const
{
	// Teacher / Monitor phases point the marker at their own targets (the AI student, a station, or the exit).
	if (Phase == EBHTutorialPhase::Teacher)
	{
		if (TeacherStep == ETeacherStep::Capture)
		{
			if (const ABHCharacter* Student = FindStudentCharacter())
			{
				SetSingleBeat(TEXT("Student"), Student->GetActorLocation());
				return;
			}
		}
		else if (TeacherStep == ETeacherStep::Exit)
		{
			if (const ABHExitGate* ExitGate = TutorialExitGate.Get())
			{
				SetSingleBeat(TEXT("Exit"), ExitGate->GetActorLocation());
				return;
			}
		}
		ClearBeats();
		return;
	}
	if (Phase == EBHTutorialPhase::Monitor)
	{
		if (MonitorStep == EMonitorStep::Unlock)
		{
			if (const ABHObjectiveStation* Station = PracticeStation.Get())
			{
				SetSingleBeat(TEXT("Task"), Station->GetActorLocation());
				return;
			}
		}
		else if (MonitorStep == EMonitorStep::Hint)
		{
			if (const ABHCharacter* Student = FindStudentCharacter())
			{
				SetSingleBeat(TEXT("Student"), Student->GetActorLocation());
				return;
			}
		}
		else if (MonitorStep == EMonitorStep::Exit)
		{
			if (const ABHExitGate* ExitGate = TutorialExitGate.Get())
			{
				SetSingleBeat(TEXT("Exit"), ExitGate->GetActorLocation());
				return;
			}
		}
		ClearBeats();
		return;
	}

	const ABHCharacter* Survivor = FindTutorialSurvivor();
	const FVector From = Survivor ? Survivor->GetActorLocation() : GetActorLocation();

	switch (CurrentStep)
	{
	case EStep::Hide:
		if (const ABHLocker* Locker = FindNearestLocker(From))
		{
			SetSingleBeat(TEXT("Hide"), Locker->GetActorLocation());
			return;
		}
		break;
	case EStep::Crawl:
		if (const ABHCrawlSpaceVolume* Crawl = FindNearestCrawlVolume(From))
		{
			SetSingleBeat(TEXT("Crawl"), Crawl->GetActorLocation());
			return;
		}
		break;
	case EStep::Breaker:
		if (const ABHBreaker* Breaker = TutorialBreaker.Get())
		{
			SetSingleBeat(TEXT("Power"), Breaker->GetActorLocation());
			return;
		}
		break;
	case EStep::Question:
		if (const ABHObjectiveStation* Station = PracticeStation.Get())
		{
			SetSingleBeat(TEXT("Task"), Station->GetActorLocation());
			return;
		}
		break;
	case EStep::Interact:
		if (const ABHObjectiveStation* Station = InteractiveStation.Get())
		{
			SetSingleBeat(TEXT("Task"), Station->GetActorLocation());
			return;
		}
		break;
	case EStep::Encounter:
	case EStep::Escape:
		// During the chase the goal is the exit - point there so the student runs for it while evading.
		if (const ABHExitGate* ExitGate = TutorialExitGate.Get())
		{
			SetSingleBeat(TEXT("Exit"), ExitGate->GetActorLocation());
			return;
		}
		break;
	default:
		break;
	}

	// Intro, Flashlight (a key press with no spatial target), Complete, or a missing target: no marker.
	ClearBeats();
}

ABHLocker* ABHTutorialDirector::FindNearestLocker(const FVector& From) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	ABHLocker* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHLocker> It(World); It; ++It)
	{
		ABHLocker* Locker = *It;
		if (!Locker)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(From, Locker->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Locker;
		}
	}
	return Best;
}

ABHCrawlSpaceVolume* ABHTutorialDirector::FindNearestCrawlVolume(const FVector& From) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	ABHCrawlSpaceVolume* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHCrawlSpaceVolume> It(World); It; ++It)
	{
		ABHCrawlSpaceVolume* Crawl = *It;
		if (!Crawl)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(From, Crawl->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Crawl;
		}
	}
	return Best;
}

ABHBreaker* ABHTutorialDirector::FindNearestBreaker(const FVector& From) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	ABHBreaker* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABHBreaker> It(World); It; ++It)
	{
		ABHBreaker* Breaker = *It;
		if (!Breaker)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(From, Breaker->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Breaker;
		}
	}
	return Best;
}

void ABHTutorialDirector::SetTutorialLightsPowered(bool bPowered) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Cut/restore every flicker light in the compact tutorial map so the blackout reads clearly and the
	// repair visibly brings the power back.
	for (TActorIterator<ABHFlickerLight> It(World); It; ++It)
	{
		if (ABHFlickerLight* Light = *It)
		{
			Light->SetPowered(bPowered);
		}
	}
}

void ABHTutorialDirector::DriveTeacherChase() const
{
	ABHBotController* Bot = TeacherBot.Get();
	ABHCharacter* Survivor = FindTutorialSurvivor();
	if (!Bot || !Survivor || !GetWorld())
	{
		return;
	}

	// Hold still for ~1s after the Teacher appears (during the reveal cutscene), then start moving.
	if (GetWorld()->GetTimeSeconds() - TeacherSpawnServerTime < 1.0f)
	{
		Bot->StopMovement();
		return;
	}

	// If the student has ducked into a locker, the Teacher loses sight of them - wander off on patrol instead
	// of camping the locker door. The next tick resumes the chase the moment they climb back out.
	if (Survivor->IsHiddenInLocker())
	{
		Bot->RunStateTreeIntent(EBHBotIntent::Patrol, nullptr, FVector::ZeroVector, 220.0f);
		return;
	}

	// Tight acceptance radius so the bot keeps closing on the student's live position - a real pursuit.
	Bot->RunStateTreeIntent(EBHBotIntent::Chase, Survivor, Survivor->GetActorLocation(), 100.0f);
}

void ABHTutorialDirector::PlayTeacherRevealCutscene(const FVector& TeacherFocusLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Reuse the horror-cue path (no bespoke cutscene system needed): snap the view to the Teacher, punch the
	// FOV in for a zoomed reveal, briefly lock the student in place, and slam a centred caption.
	FBHClientHorrorCue Cue;
	Cue.EventType = EBHScareEventType::Ambient;
	Cue.FocusLocation = TeacherFocusLocation;
	Cue.bSnapToFocus = true;
	Cue.FOVPunch = 16.0f;
	Cue.bLockInput = true;
	Cue.LockSeconds = 1.8f;
	Cue.DurationSeconds = 1.8f;
	Cue.ShakeIntensity = 0.28f;
	Cue.HitStopSeconds = 0.10f;
	Cue.Message = TEXT("THE TEACHER!");
	Cue.bDirectivePrompt = true;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientPlayHorrorCue(Cue);
		}
	}
}

bool ABHTutorialDirector::SurvivorIsCrawlingNearDuct() const
{
	const ABHCharacter* Survivor = FindTutorialSurvivor();
	if (!Survivor)
	{
		return false;
	}
	const EBHMovementSpecialState State = Survivor->GetMovementSpecialState();
	const bool bLowProfile = State == EBHMovementSpecialState::Prone
		|| State == EBHMovementSpecialState::Sliding
		|| State == EBHMovementSpecialState::Diving;
	if (!bLowProfile)
	{
		return false;
	}
	const ABHCrawlSpaceVolume* Crawl = FindNearestCrawlVolume(Survivor->GetActorLocation());
	if (!Crawl)
	{
		return true; // Low-profile and the lesson has no specific duct to gate against; accept it.
	}
	return FVector::DistSquared(Survivor->GetActorLocation(), Crawl->GetActorLocation()) < (600.0f * 600.0f);
}

bool ABHTutorialDirector::ReviveIfCaught() const
{
	ABHGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
	if (!GameMode)
	{
		return false;
	}

	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Character = *It;
		ABHPlayerState* PS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (PS && !PS->IsABot() && PS->LifeState == EBHPlayerLifeState::Captured)
		{
			AController* Ctrl = Character->GetController();
			PS->SetLifeState(EBHPlayerLifeState::Alive);
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(Ctrl))
			{
				PC->ClientShowStatusMessage(TEXT("Caught! No real penalty in the tutorial - you're back on your feet. Hide, then reach the exit."), 4.0f);
			}
			// RestartPlayer respawns a fresh pawn at a PlayerStart, guaranteeing the student is unstuck
			// even if the capture flow left the old pawn pinned. Iterator is now stale, so stop here.
			if (Ctrl)
			{
				GameMode->RestartPlayer(Ctrl);
			}
			return true;
		}
	}
	return false;
}

ABHBotController* ABHTutorialDirector::SpawnScriptedBot(EBHPlayerRole BotRole, const FString& BotName, const FVector& Spawn)
{
	UWorld* World = GetWorld();
	ABHGameMode* GameMode = World ? World->GetAuthGameMode<ABHGameMode>() : nullptr;
	if (!World || !GameMode)
	{
		return nullptr;
	}

	// Spawn one bot controller, mirroring the roster spawn path. Best-effort: any failure just means no live
	// bot this run; the lesson still completes on its timeout.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABHBotController* Bot = World->SpawnActor<ABHBotController>(ABHBotController::StaticClass(), Spawn, FRotator::ZeroRotator, SpawnParams);
	if (!Bot)
	{
		return nullptr;
	}
	if (!Bot->PlayerState)
	{
		Bot->InitPlayerState();
	}
	if (ABHPlayerState* BotPS = Bot->GetPlayerState<ABHPlayerState>())
	{
		BotPS->SetIsABot(true);
		BotPS->SetPlayerName(BotName);
		BotPS->SetRole(BotRole);
		BotPS->SetDesiredRole(BotRole);
		BotPS->SetReady(true);
		BotPS->SetLifeState(EBHPlayerLifeState::Alive);
		if (ABHGameState* BHGS = World->GetGameState<ABHGameState>())
		{
			if (!BHGS->PlayerArray.Contains(BotPS))
			{
				BHGS->AddPlayerState(BotPS);
			}
		}
	}
	GameMode->RestartPlayer(Bot);
	return Bot;
}

void ABHTutorialDirector::SpawnScriptedTeacher()
{
	if (bTeacherSpawned || TeacherBot.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	ABHGameMode* GameMode = World ? World->GetAuthGameMode<ABHGameMode>() : nullptr;
	if (!World || !GameMode)
	{
		return;
	}
	bTeacherSpawned = true;
	TeacherBot = SpawnScriptedBot(EBHPlayerRole::Hunter, TEXT("Teacher"), GameMode->GetHunterSpawnLocation());
	// Record the spawn time so DriveTeacherChase holds it still for ~1s (the reveal beat) before it moves.
	TeacherSpawnServerTime = World->GetTimeSeconds();
}

// ---- Teacher tutorial (player = Hunter): scan(Q) -> chase + capture(LMB) an AI student -> blackout(R) -> exit ----

void ABHTutorialDirector::EnterTeacherStep(ETeacherStep NewStep)
{
	TeacherStep = NewStep;
	StepStartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	switch (NewStep)
	{
	case ETeacherStep::Intro:
		SetAllInputLocked(true);
		Broadcast(TEXT("Now you're the TEACHER - hunt the student down before they escape. Watch a moment; your controls unlock shortly."), 5.0f);
		break;
	case ETeacherStep::Move:
		SetAllInputLocked(false);
		if (ABHCharacter* Human = FindHumanPlayer())
		{
			Human->ResetTutorialMovementMask();
			BroadcastMoveHint(Human->GetTutorialMovementMask());
		}
		else
		{
			BroadcastMoveHint(0);
		}
		NextMoveHintServerTime = StepStartServerTime + 3.0f;
		break;
	case ETeacherStep::Scan:
		Broadcast(TEXT("Press Q to SCAN - it pings the nearest student's heartbeat and points you their way."), 14.0f);
		break;
	case ETeacherStep::Capture:
		Broadcast(TEXT("Hunt the STUDENT (follow the marker). Get close and press LEFT MOUSE to swing and capture them."), 16.0f);
		break;
	case ETeacherStep::Blackout:
		Broadcast(TEXT("Press R for a BLACKOUT - it kills the lights so students lose their bearings."), 14.0f);
		break;
	case ETeacherStep::Exit:
		Broadcast(TEXT("Nice work, Teacher. Head to the GREEN EXIT (follow the marker) to continue to Monitor training."), 12.0f);
		break;
	case ETeacherStep::Done:
		SetAllInputLocked(false);
		ClearBeats();
		Broadcast(bChained
			? TEXT("Teacher training complete! Next: the HALL MONITOR...")
			: TEXT("Teacher training complete - nice work! Returning to the menu..."), 6.0f);
		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(EvalTimerHandle);
			GetWorldTimerManager().SetTimer(ActivateTimerHandle, this, &ABHTutorialDirector::FinishTutorialPhase, 3.0f, false);
		}
		break;
	default:
		break;
	}

	PublishStepBeat();
}

void ABHTutorialDirector::EvaluateTeacherStep()
{
	ABHCharacter* Human = FindHumanPlayer();
	const float Elapsed = StepElapsed();
	switch (TeacherStep)
	{
	case ETeacherStep::Intro:
		if (Elapsed >= 5.0f)
		{
			EnterTeacherStep(ETeacherStep::Move);
		}
		break;
	case ETeacherStep::Move:
	{
		const uint8 Mask = Human ? Human->GetTutorialMovementMask() : 0;
		if ((Human && (Mask & ABHCharacter::TutorialMoveAllMask) == ABHCharacter::TutorialMoveAllMask) || Elapsed >= 60.0f)
		{
			EnterTeacherStep(ETeacherStep::Scan);
			break;
		}
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextMoveHintServerTime)
		{
			BroadcastMoveHint(Mask);
			NextMoveHintServerTime = Now + 3.0f;
		}
		break;
	}
	case ETeacherStep::Scan:
		// Advance when the player actually scans (Q), or on the generous timeout.
		if ((Human && Human->GetLastScanTime() >= StepStartServerTime) || Elapsed >= 25.0f)
		{
			EnterTeacherStep(ETeacherStep::Capture);
		}
		break;
	case ETeacherStep::Capture:
	{
		// Advance when they swing the capture attack (the lesson), the student is downed, or timeout.
		bool bCaptured = false;
		if (const ABHCharacter* Student = FindStudentCharacter())
		{
			const ABHPlayerState* StudentPS = Student->GetPlayerState<ABHPlayerState>();
			bCaptured = StudentPS && StudentPS->LifeState == EBHPlayerLifeState::Captured;
		}
		const bool bSwung = Human && Human->IsTeacherCaptureAttackActive();
		if (bCaptured || bSwung || Elapsed >= 60.0f)
		{
			EnterTeacherStep(ETeacherStep::Blackout);
		}
		break;
	}
	case ETeacherStep::Blackout:
		// Advance when they trigger the blackout power (R), or on timeout.
		if ((Human && Human->GetLastHunterPowerTime() >= StepStartServerTime) || Elapsed >= 25.0f)
		{
			EnterTeacherStep(ETeacherStep::Exit);
		}
		break;
	case ETeacherStep::Exit:
	{
		bool bReachedExit = false;
		if (Human && TutorialExitGate.IsValid())
		{
			bReachedExit = FVector::Dist(Human->GetActorLocation(), TutorialExitGate->GetActorLocation()) < 450.0f;
		}
		if (bReachedExit || Elapsed >= 70.0f)
		{
			EnterTeacherStep(ETeacherStep::Done);
		}
		break;
	}
	case ETeacherStep::Done:
	default:
		break;
	}
}

// ---- Monitor tutorial (player = FakeHunter): unlock concept -> real hint(Q) -> false marker(R) -> trap(G) -> exit ----

void ABHTutorialDirector::EnterMonitorStep(EMonitorStep NewStep)
{
	MonitorStep = NewStep;
	StepStartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	switch (NewStep)
	{
	case EMonitorStep::Intro:
		SetAllInputLocked(true);
		Broadcast(TEXT("Last lesson: the HALL MONITOR. You can't catch anyone - you mislead the Teacher and slow the students. Watch a moment..."), 5.0f);
		break;
	case EMonitorStep::Move:
		SetAllInputLocked(false);
		if (ABHCharacter* Human = FindHumanPlayer())
		{
			Human->ResetTutorialMovementMask();
			BroadcastMoveHint(Human->GetTutorialMovementMask());
		}
		else
		{
			BroadcastMoveHint(0);
		}
		NextMoveHintServerTime = StepStartServerTime + 3.0f;
		break;
	case EMonitorStep::Unlock:
		Broadcast(TEXT("In a real match you'd answer at STATIONS to unlock your tools. Here they're ready - let's practise them."), 8.0f);
		break;
	case EMonitorStep::Hint:
		Broadcast(TEXT("Press Q to send a REAL hint - it tells the Teacher where a student actually is."), 16.0f);
		break;
	case EMonitorStep::Marker:
		Broadcast(TEXT("Press R to drop a FALSE corridor marker - it sends the Teacher chasing nothing."), 16.0f);
		break;
	case EMonitorStep::Trap:
		Broadcast(TEXT("Press G to set a TRAP - students must spot and dodge it or trip the alarm."), 16.0f);
		break;
	case EMonitorStep::Exit:
		Broadcast(TEXT("That's the whole toolkit! Head to the GREEN EXIT (follow the marker) to finish your training."), 12.0f);
		break;
	case EMonitorStep::Done:
		SetAllInputLocked(false);
		ClearBeats();
		Broadcast(TEXT("All training complete - you're ready for a real round! Returning to the menu..."), 6.0f);
		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(EvalTimerHandle);
			GetWorldTimerManager().SetTimer(ActivateTimerHandle, this, &ABHTutorialDirector::FinishTutorialPhase, 4.0f, false);
		}
		break;
	default:
		break;
	}

	PublishStepBeat();
}

void ABHTutorialDirector::EvaluateMonitorStep()
{
	ABHCharacter* Human = FindHumanPlayer();
	const float Elapsed = StepElapsed();
	switch (MonitorStep)
	{
	case EMonitorStep::Intro:
		if (Elapsed >= 5.0f)
		{
			EnterMonitorStep(EMonitorStep::Move);
		}
		break;
	case EMonitorStep::Move:
	{
		const uint8 Mask = Human ? Human->GetTutorialMovementMask() : 0;
		if ((Human && (Mask & ABHCharacter::TutorialMoveAllMask) == ABHCharacter::TutorialMoveAllMask) || Elapsed >= 60.0f)
		{
			EnterMonitorStep(EMonitorStep::Unlock);
			break;
		}
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextMoveHintServerTime)
		{
			BroadcastMoveHint(Mask);
			NextMoveHintServerTime = Now + 3.0f;
		}
		break;
	}
	case EMonitorStep::Unlock:
		// Informational only (tools aren't gated in the practice sandbox); advance on a short timer.
		if (Elapsed >= 8.0f)
		{
			EnterMonitorStep(EMonitorStep::Hint);
		}
		break;
	case EMonitorStep::Hint:
		if ((Human && Human->GetLastScanTime() >= StepStartServerTime) || Elapsed >= 25.0f)
		{
			EnterMonitorStep(EMonitorStep::Marker);
		}
		break;
	case EMonitorStep::Marker:
		if ((Human && Human->GetLastHunterPowerTime() >= StepStartServerTime) || Elapsed >= 25.0f)
		{
			EnterMonitorStep(EMonitorStep::Trap);
		}
		break;
	case EMonitorStep::Trap:
		if ((Human && Human->GetLastDecoyTime() >= StepStartServerTime) || Elapsed >= 25.0f)
		{
			EnterMonitorStep(EMonitorStep::Exit);
		}
		break;
	case EMonitorStep::Exit:
	{
		bool bReachedExit = false;
		if (Human && TutorialExitGate.IsValid())
		{
			bReachedExit = FVector::Dist(Human->GetActorLocation(), TutorialExitGate->GetActorLocation()) < 450.0f;
		}
		if (bReachedExit || Elapsed >= 70.0f)
		{
			EnterMonitorStep(EMonitorStep::Done);
		}
		break;
	}
	case EMonitorStep::Done:
	default:
		break;
	}
}

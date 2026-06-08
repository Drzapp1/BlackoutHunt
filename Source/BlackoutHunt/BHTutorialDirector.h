// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHTypes.h"
#include "BHTutorialDirector.generated.h"

class ABHCharacter;
class ABHObjectiveStation;
class ABHExitGate;
class ABHBotController;
class ABHLocker;
class ABHCrawlSpaceVolume;
class ABHBreaker;
class ABHAlarmTrap;

// Server-authoritative guided-tutorial driver. One instance is baked into the dedicated Tutorial map
// (spawned by ABHGameMode::BuildTutorialLevel during the authored-level export), so it simply exists when
// the Tutorial map loads and self-activates on the listen-server host. It runs one of three self-paced
// lessons, selected by the game mode's tutorial phase (?BHTutorialPhase=) and chained on reaching the exit:
//   * Survivor: move, flashlight, hide, crawl, breaker, two questions, a scripted Teacher chase, escape.
//   * Teacher:  play the Hunter - scan for heartbeats, learn student counterplay (noise decoys, lockers,
//               crawl ducts), chase + capture an AI student, black out the lights, then reach the exit.
//   * Monitor:  play the hall monitor - answer at a station to unlock tools, real hint, false marker, trap, exit.
//
// Design rules so a brand-new 15-16 year old can never get stuck:
//  * Every step has a generous timeout fallback, so the flow always advances even if a check never trips.
//  * The student is auto-revived if the scripted Teacher ever catches them (a tutorial has no real stakes),
//    so a fully-AI Hunter can loom and chase realistically without soft-locking the lesson.
//  * Guidance is delivered as client status messages; this adds NO new replicated state.
UCLASS()
class BLACKOUTHUNT_API ABHTutorialDirector : public AActor
{
	GENERATED_BODY()

public:
	ABHTutorialDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	// Internal ordered steps, one set per tutorial phase. Not UENUMs: server-internal only.
	enum class EStep : uint8       // Survivor phase
	{
		Intro,
		// Early-spawn the Teacher idle at the far Hunter spawn (it does NOT chase) so the whole teaching half has
		// stakes - a menace looming in the distance while the student learns. Inserted right after Intro.
		Loom,
		Move,
		Sprint,
		Flashlight,
		Hide,
		Crawl,
		Decoy,
		Breaker,
		Question,
		Interact,
		Encounter,
		Escape,
		Complete,
	};
	// True hunter loop: learn the READ tools (Scan, Noise) BEFORE the COMMIT tool (Axe), then a two-stage climax --
	// Hunt (close to ~600u of the fleeing student) then Capture (land the grab). No Move step: the Survivor lesson
	// already taught WASD and the Teacher Intro hands control straight to Sprint, so a Move step was unreachable.
	enum class ETeacherStep : uint8 { Intro, Sprint, Scan, Noise, Axe, Counterplay, Hunt, Capture, Blackout, Exit, Done };
	// No standalone Move step: the Survivor lesson already taught WASD, so Intro hands straight to Unlock.
	enum class EMonitorStep : uint8 { Intro, Unlock, Hint, Marker, Trap, Exit, Done };
	// Advanced-movement lesson (EBHTutorialPhase::Movement): each step teaches one link and auto-advances once the
	// player performs it (detected from the character's live state + the tutorial action latches), with a generous
	// timeout fallback so a stuck input can never hard-lock the lesson. Roll/Slide/Dive end the course.
	// Order is deliberate: the bare-key stances (Crouch=Ctrl, Prone=Alt) are taught BEFORE Sprint so a held Shift
	// never turns the tap into a Roll/Slide combo, and Jump comes after Sprint so the player is already standing.
	// After the core kit (through Dive) an optional advanced tail teaches the micro-techniques; every advanced step
	// has a long timeout fallback so a player who can't land the harder ones still finishes.
	enum class EMovementStep : uint8
	{
		Intro, Walk, Crouch, Prone, Sprint, Jump, BunnyHop, Roll, Slide, Dive,
		AdvancedIntro, QuietRoll, DropRoll, SlideStop, LockerRoll, FlowChain,
		Complete
	};

	// Defer activation a beat so the host pawn/player state exist, then run the step machine on a timer.
	void Activate();
	// Freeze controls the instant the map loads (re-asserted on a fast timer until Activate's intro takes over),
	// so the player can't run off during the ~1s before the lesson's intro lock would otherwise kick in.
	void EnforceIntroInputLock();
	void EnterStep(EStep NewStep);
	void EvaluateStep();
	// Per-phase step machines (Survivor uses EnterStep/EvaluateStep above; the timer dispatches by Phase).
	void EnterTeacherStep(ETeacherStep NewStep);
	void EvaluateTeacherStep();
	void EnterMonitorStep(EMonitorStep NewStep);
	void EvaluateMonitorStep();
	// Advanced-movement lesson step machine (EBHTutorialPhase::Movement).
	void EnterMovementStep(EMovementStep NewStep);
	void EvaluateMovementStep();
	// Spawn the Movement lesson's practice course (cover wall + low overhang, a drop ledge, and a doorway chase
	// dummy) in the clear western practice bay. Defined out-of-line in BHTutorialDirectorMovementCourse.cpp
	// (mirroring how BHGameModeEasterEgg.cpp splits BuildEasterEggRoom). Idempotent (bMovementCourseBuilt);
	// called once from Activate's Movement branch.
	void BuildMovementCourse();

	// Broadcast a guidance line to every player controller (solo in practice, but harmless for more).
	void Broadcast(const FString& Message, float DurationSeconds) const;
	// Lock/unlock movement+look on all clients (used for the brief intro beat).
	void SetAllInputLocked(bool bLocked) const;
	// The living tutorial survivor (the local host player, when playing as a Survivor). Re-resolved each tick.
	ABHCharacter* FindTutorialSurvivor() const;
	// The lone human player's pawn regardless of role (used by the Teacher/Monitor phases where the human is
	// the Hunter / hall monitor, not a survivor).
	ABHCharacter* FindHumanPlayer() const;
	// The AI "student" survivor bot's pawn (spawned for the Teacher/Monitor phases).
	ABHCharacter* FindStudentCharacter() const;

	// A real round needs two players to ready up; the solo tutorial can't. So the director makes the lone
	// host a live Survivor (ready, alive, Hunt phase) the moment it activates - the authentic role, which
	// satisfies the alive-survivor interaction gates AND the crawl-space role check, so every object is usable
	// solo. Re-asserted each tick because a captured-then-revived student keeps their role but must never drift.
	void EnsurePlayablyConfigured() const;
	// WASD lesson: re-issue the movement prompt (naming the directions still un-pressed) so it stays on screen
	// until the student has actually driven all four - it never fades out from under them prematurely.
	void BroadcastMoveHint(uint8 MovementMask) const;
	// Point the single objective marker at exactly one world location (the one object the current step needs),
	// or clear it. The generic many-beat planner is suppressed in tutorial mode (see PublishObjectiveBeats).
	void SetSingleBeat(const FString& Label, const FVector& Location) const;
	void ClearBeats() const;
	// Refresh the marker for the current step (re-resolves the locker/station so it tracks the nearest one).
	void PublishStepBeat() const;
	// Nearest locker / crawl volume / breaker to the student, used for the step markers + completion checks.
	ABHLocker* FindNearestLocker(const FVector& From) const;
	ABHCrawlSpaceVolume* FindNearestCrawlVolume(const FVector& From) const;
	ABHBreaker* FindNearestBreaker(const FVector& From) const;
	// True if the student is in a low-profile (prone/slide/dive) state near the taught crawl duct.
	bool SurvivorIsCrawlingNearDuct() const;
	// Breaker lesson: cut/restore the tutorial's flicker lights so a blackout the student can fix is taught.
	void SetTutorialLightsPowered(bool bPowered) const;
	// Force the exit gate into its GREEN "escape-ready" state for the run-to-exit beat. The Survivor lesson opens
	// it naturally by repairing the breaker, but the Teacher/Monitor never repair one - so without this the gate
	// stays amber/locked while the prompt tells them to reach the "GREEN EXIT".
	void SetTutorialExitUnlocked() const;
	// Encounter lesson (Survivor phase): every tick, push the Teacher bot a fresh Chase intent at the student's
	// live position so it is a real pursuit - but stand still for ~1s after spawn, and fall back to patrol
	// while the student is hidden in a locker (so it wanders off instead of camping the door).
	void DriveTeacherChase() const;
	// Hide step (staged scare): drive the loomed Teacher to the student's locker and let it "search" for ~2s, then
	// hand off to the DriveTeacherChase locker-retreat so it visibly gives up - proving locker capture-immunity.
	void DriveTeacherToLocker() const;
	// Decoy step (misdirection): pull the loomed Teacher toward the dropped decoy so the student SEES the trick
	// work; falls back to idling at the Hunter spawn before any decoy is dropped.
	void DriveTeacherToDecoy() const;
	// Phase-aware per-tick AI driving: Survivor -> teacher chase; Teacher -> the AI student wanders/flees the
	// human Hunter; Monitor -> the AI student wanders while an AI teacher patrols.
	void DriveScriptedCast();
	// Keep the AI student survivor lively: work a station when safe, flee when a Hunter is close.
	void DriveStudentBot() const;
	// Generic chase order for an arbitrary bot toward an arbitrary character (AI teacher -> AI student).
	void DriveBotChase(ABHBotController* Bot, ABHCharacter* Target) const;
	// Monitor phase per-tick AI driving, step-aware so the scene REACTS to each tool: Hint -> turn the Teacher onto
	// the student's live position; Marker -> steer the Teacher to the false-marker spot; Trap -> walk the student
	// across the spawned trap so it trips; otherwise the ambient chase. The hall-monitor tools only notify human
	// Teachers, so against the solo lesson's AI Teacher this scripting IS what makes them read true.
	void DriveMonitorCast() const;
	// Nearest still-alive hall-monitor alarm trap to a point (the one the player just dropped); used to drive the
	// student onto it during the Trap step. Returns null once it has been tripped (the trap destroys itself).
	ABHAlarmTrap* FindNearestAlarmTrap(const FVector& From) const;
	// A short "the Teacher appears" reveal the instant the bot spawns: snap the host's view to the Teacher,
	// punch the FOV in (zoom), briefly lock input, and slam a centred caption - built on the horror-cue path.
	void PlayTeacherRevealCutscene(const FVector& TeacherFocusLocation) const;
	// A tutorial has no real stakes: if the scripted Teacher catches the student, put them back on their
	// feet so the lesson keeps flowing. Returns true if a revive happened.
	bool ReviveIfCaught() const;
	// Shared bot spawner: spawn+possess one bot in the given role at the given spawn, registered with the
	// game state. Returns the controller (or null). SpawnScriptedTeacher / the Teacher+Monitor casts use it.
	ABHBotController* SpawnScriptedBot(EBHPlayerRole Role, const FString& BotName, const FVector& Spawn);
	// Best-effort: spawn one Hunter bot at the teacher spawn for the Survivor-phase encounter.
	void SpawnScriptedTeacher();
	// Teacher phase: put the human Teacher at the same Hunter spawn the AI Teacher uses in the Survivor lesson,
	// so picking up the Teacher tutorial reads as continuous with the chase the student just survived. Returns
	// true once the human pawn existed and was actually moved (retried each Intro tick until then).
	bool TeleportHumanToHunterSpawn() const;
	// Teacher phase (Noise step): drop a single demonstration decoy by the AI student so the Teacher sees exactly
	// one "Noise: decoy" ping. Autonomous bot decoys are suppressed in tutorials (they would spam over the prompt).
	void PlaceDemoDecoyNearStudent() const;
	// Teacher phase: drive the AI student on a fixed, always-moving SCRIPTED route (not the wander/flee decision
	// AI, which only moved when the Teacher was adjacent). It demonstrates hiding in a locker during Counterplay
	// and flees the human Teacher during Capture. Built on RunStateTreeIntent with explicit destinations.
	void DriveScriptedTutorialStudent();
	// Populate ScriptedWaypoints from the baked map's stations/breaker so the student has a deterministic loop.
	void BuildScriptedWaypoints();
	// Full-screen transition "snapshot": a titled card shown on every client for Seconds (covers the phase
	// hand-off so it reads as a loading screen, not a freeze). Drawn by ABHHUD from replicated PC state.
	void ShowTutorialCard(const FString& Title, const FString& Body, float Seconds) const;
	// When the student reaches the exit, hand off to the game mode to load the next tutorial phase (or menu).
	void FinishTutorialPhase() const;

	float StepElapsed() const;

	// Which tutorial this director instance is running (read from the game mode on Activate).
	EBHTutorialPhase Phase = EBHTutorialPhase::Survivor;
	// True when this is the full chained course (so completion hands off to the next tutorial); false when the
	// player picked a single tutorial (so completion returns to the menu). Read from the game mode on Activate.
	bool bChained = true;

	EStep CurrentStep = EStep::Intro;
	ETeacherStep TeacherStep = ETeacherStep::Intro;
	EMonitorStep MonitorStep = EMonitorStep::Intro;
	EMovementStep MovementStep = EMovementStep::Intro;
	// The current Movement-lesson prompt, re-broadcast on a cadence so the instruction stays on screen while the
	// player practises (the action steps don't navigate, so there is no marker to keep them oriented).
	FString CurrentMovementPrompt;
	float StepStartServerTime = 0.0f;
	// Next server time the WASD prompt is re-issued during the Move step (keeps it pinned until all four keys).
	float NextMoveHintServerTime = 0.0f;
	// Server time the scripted Teacher spawned; it holds still until ~1s later, then chases.
	float TeacherSpawnServerTime = 0.0f;
	bool bActivated = false;
	bool bTeacherSpawned = false;
	// Hide lesson: set once the student has actually hidden, so the next prompt only fires after they climb out.
	bool bHideRegistered = false;
	// Survivor phase: set once the loomed Teacher has been spawned during the Loom step (so the staged Hide scare +
	// Decoy misdirection have a live, idle Teacher to drive). Distinct from bTeacherSpawned, which gates the
	// one-time SpawnScriptedTeacher() call itself.
	bool bTeacherLoomed = false;
	// Hide lesson (staged scare): server time the student actually hid, so the loomed Teacher searches the locker
	// for ~2s before giving up (the DriveTeacherChase locker-retreat). 0 until they hide.
	float HideScareServerTime = 0.0f;
	// Encounter gate: throttle time for the "move!" re-nudge to a frozen player after the 5s floor.
	float EncounterNudgeServerTime = 0.0f;
	// Breaker lesson: true while the tutorial blackout is active (lights cut until the breaker is repaired). Reused
	// by the Teacher Blackout step to flag the forced demonstration blackout (restored on entering Exit).
	bool bTutorialBlackoutActive = false;
	// Teacher phase: set once the human Teacher has been moved to the Hunter spawn (continuity with the Survivor lesson).
	bool bTeacherStartPlaced = false;
	// Teacher phase (Blackout step): server time the forced tutorial blackout began, so the dark holds for a beat
	// before the run-to-exit instead of snapping back the instant the player taps R.
	float BlackoutDemoServerTime = 0.0f;
	// Teacher phase (Scan step): server time the player's scan fired, so the heartbeat readout stays on screen a
	// few seconds before the next prompt replaces it.
	float ScanDemoServerTime = 0.0f;
	// Monitor phase (Hint/Marker/Trap steps): server time the player fired the current tool. The step then holds a
	// few seconds showing a confirmation line before advancing - the hall-monitor tools only notify human Teachers,
	// so against the solo lesson's AI Teacher the key-press would otherwise look like it did nothing.
	float MonitorToolConfirmServerTime = 0.0f;
	// Monitor phase (Marker step): the world location the false corridor marker resolved to when R fired, captured so
	// the AI Teacher can be steered toward it for the whole confirmation hold (re-armed false each step entry).
	FVector MonitorMarkerTarget = FVector::ZeroVector;
	bool bMonitorMarkerActive = false;
	// Monitor phase (Hint step): true once the one-shot "the marker vanishes while the student is hidden" signpost
	// has fired, so it is shown at most once per Hint step.
	bool bMonitorLockerSignpostShown = false;
	// Teacher phase (Counterplay step): true once the one-shot "the student vanished into a locker" caption has
	// fired, so the staged locker-vanish demo is called out at most once. Re-armed each EnterTeacherStep.
	bool bTeacherLockerSignpostShown = false;
	// Teacher phase: deterministic patrol loop for the scripted student + the index it's currently heading to.
	TArray<FVector> ScriptedWaypoints;
	int32 ScriptedWaypointIndex = 0;

	TWeakObjectPtr<ABHObjectiveStation> PracticeStation;
	// Second, more easterly station that hosts the interactive drag-drop question (the Interact step).
	TWeakObjectPtr<ABHObjectiveStation> InteractiveStation;
	TWeakObjectPtr<ABHExitGate> TutorialExitGate;
	TWeakObjectPtr<ABHBotController> TeacherBot;
	// AI survivor "student" spawned for the Teacher/Monitor phases (the one the human hunts / misleads).
	TWeakObjectPtr<ABHBotController> StudentBot;
	TWeakObjectPtr<ABHBreaker> TutorialBreaker;

	// Movement-lesson practice course (built by BuildMovementCourse). World locations of each prop so the per-step
	// markers can point at the right one; bMovementCourseBuilt makes the spawn idempotent.
	bool bMovementCourseBuilt = false;
	FVector CoverWallLoc = FVector::ZeroVector;
	FVector OverhangLoc = FVector::ZeroVector;
	FVector DropLedgeLoc = FVector::ZeroVector;
	FVector ChaseDummyLoc = FVector::ZeroVector;
	// A practice locker spawned IN the bay for the LockerRoll step, so the marker doesn't pull the player two rooms
	// east to the nearest map locker. Zero until BuildMovementCourse spawns it.
	FVector PracticeLockerLoc = FVector::ZeroVector;

	FTimerHandle ActivateTimerHandle;
	FTimerHandle EvalTimerHandle;
	// Fast pre-activation timer that holds the intro input lock until the step machine starts.
	FTimerHandle IntroLockTimerHandle;
};

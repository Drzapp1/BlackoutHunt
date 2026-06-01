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

// Server-authoritative guided-tutorial driver. One instance is baked into the dedicated Tutorial map
// (spawned by ABHGameMode::BuildTutorialLevel during the authored-level export), so it simply exists when
// the Tutorial map loads and self-activates on the listen-server host. It runs one of three self-paced
// lessons, selected by the game mode's tutorial phase (?BHTutorialPhase=) and chained on reaching the exit:
//   * Survivor: move, flashlight, hide, crawl, breaker, two questions, a scripted Teacher chase, escape.
//   * Teacher:  play the Hunter - scan, chase + capture an AI student, blackout, then reach the exit.
//   * Monitor:  play the hall monitor - answer to unlock tools, real hint, false marker, trap, reach exit.
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
		Move,
		Flashlight,
		Hide,
		Crawl,
		Breaker,
		Question,
		Interact,
		Encounter,
		Escape,
		Complete,
	};
	enum class ETeacherStep : uint8 { Intro, Move, Scan, Capture, Blackout, Exit, Done };
	enum class EMonitorStep : uint8 { Intro, Move, Unlock, Hint, Marker, Trap, Exit, Done };

	// Defer activation a beat so the host pawn/player state exist, then run the step machine on a timer.
	void Activate();
	void EnterStep(EStep NewStep);
	void EvaluateStep();
	// Per-phase step machines (Survivor uses EnterStep/EvaluateStep above; the timer dispatches by Phase).
	void EnterTeacherStep(ETeacherStep NewStep);
	void EvaluateTeacherStep();
	void EnterMonitorStep(EMonitorStep NewStep);
	void EvaluateMonitorStep();

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
	// Encounter lesson (Survivor phase): every tick, push the Teacher bot a fresh Chase intent at the student's
	// live position so it is a real pursuit - but stand still for ~1s after spawn, and fall back to patrol
	// while the student is hidden in a locker (so it wanders off instead of camping the door).
	void DriveTeacherChase() const;
	// Phase-aware per-tick AI driving: Survivor -> teacher chase; Teacher -> the AI student wanders/flees the
	// human Hunter; Monitor -> the AI student wanders while an AI teacher patrols.
	void DriveScriptedCast() const;
	// Keep the AI student survivor lively: work a station when safe, flee when a Hunter is close.
	void DriveStudentBot() const;
	// Generic chase order for an arbitrary bot toward an arbitrary character (AI teacher -> AI student).
	void DriveBotChase(ABHBotController* Bot, ABHCharacter* Target) const;
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
	float StepStartServerTime = 0.0f;
	// Next server time the WASD prompt is re-issued during the Move step (keeps it pinned until all four keys).
	float NextMoveHintServerTime = 0.0f;
	// Server time the scripted Teacher spawned; it holds still until ~1s later, then chases.
	float TeacherSpawnServerTime = 0.0f;
	bool bActivated = false;
	bool bTeacherSpawned = false;
	// Hide lesson: set once the student has actually hidden, so the next prompt only fires after they climb out.
	bool bHideRegistered = false;
	// Breaker lesson: true while the tutorial blackout is active (lights cut until the breaker is repaired).
	bool bTutorialBlackoutActive = false;

	TWeakObjectPtr<ABHObjectiveStation> PracticeStation;
	// Second, more easterly station that hosts the interactive drag-drop question (the Interact step).
	TWeakObjectPtr<ABHObjectiveStation> InteractiveStation;
	TWeakObjectPtr<ABHExitGate> TutorialExitGate;
	TWeakObjectPtr<ABHBotController> TeacherBot;
	// AI survivor "student" spawned for the Teacher/Monitor phases (the one the human hunts / misleads).
	TWeakObjectPtr<ABHBotController> StudentBot;
	TWeakObjectPtr<ABHBreaker> TutorialBreaker;

	FTimerHandle ActivateTimerHandle;
	FTimerHandle EvalTimerHandle;
};

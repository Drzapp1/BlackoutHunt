#pragma once

#include "CoreMinimal.h"
#include "BHTypes.generated.h"

class AActor;
class AController;
class ABHCharacter;

UENUM(BlueprintType)
enum class EBHRoundPhase : uint8
{
	Lobby UMETA(DisplayName = "Lobby"),
	Prep UMETA(DisplayName = "Prep"),
	Hunt UMETA(DisplayName = "Hunt"),
	SurvivorsWin UMETA(DisplayName = "Survivors Win"),
	HunterWin UMETA(DisplayName = "Hunter Win")
};

UENUM(BlueprintType)
enum class EBHPlayerRole : uint8
{
	Unassigned UMETA(DisplayName = "Unassigned"),
	Hunter UMETA(DisplayName = "Hunter"),
	FakeHunter UMETA(DisplayName = "Fake Hunter"),
	Survivor UMETA(DisplayName = "Survivor"),
	Tester UMETA(DisplayName = "Tester"),
	Spectator UMETA(DisplayName = "Spectator")
};

UENUM(BlueprintType)
enum class EBHPlayerLifeState : uint8
{
	Alive UMETA(DisplayName = "Alive"),
	Captured UMETA(DisplayName = "Captured"),
	Escaped UMETA(DisplayName = "Escaped")
};

UENUM(BlueprintType)
enum class EBHRoundModifier : uint8
{
	None UMETA(DisplayName = "None"),
	LightsOut UMETA(DisplayName = "Lights Out"),
	LoudFooting UMETA(DisplayName = "Loud Footing"),
	JammedDoors UMETA(DisplayName = "Jammed Doors"),
	PanicSurge UMETA(DisplayName = "Panic Surge")
};

UENUM(BlueprintType)
enum class EBHFogPreset : uint8
{
	Light UMETA(DisplayName = "Light"),
	Heavy UMETA(DisplayName = "Heavy"),
	Extreme UMETA(DisplayName = "Extreme")
};

UENUM(BlueprintType)
enum class EBHBotDifficulty : uint8
{
	Easy UMETA(DisplayName = "Easy"),
	Normal UMETA(DisplayName = "Normal"),
	Hard UMETA(DisplayName = "Hard")
};

UENUM(BlueprintType)
enum class EBHBotPersonality : uint8
{
	Cautious UMETA(DisplayName = "Cautious"),
	Objective UMETA(DisplayName = "Objective"),
	Bold UMETA(DisplayName = "Bold"),
	Trickster UMETA(DisplayName = "Trickster"),
	Panicked UMETA(DisplayName = "Panicked"),
	Aggressive UMETA(DisplayName = "Aggressive"),
	Suspicious UMETA(DisplayName = "Suspicious"),
	Ambusher UMETA(DisplayName = "Ambusher")
};

UENUM(BlueprintType)
enum class EBHBotIntent : uint8
{
	None UMETA(DisplayName = "None"),
	Patrol UMETA(DisplayName = "Patrol"),
	AnswerStation UMETA(DisplayName = "Answer Station"),
	WorkStation UMETA(DisplayName = "Work Station"),
	RepairBreaker UMETA(DisplayName = "Repair Breaker"),
	Escape UMETA(DisplayName = "Escape"),
	Hide UMETA(DisplayName = "Hide"),
	Flee UMETA(DisplayName = "Flee"),
	Bait UMETA(DisplayName = "Bait"),
	Chase UMETA(DisplayName = "Chase"),
	InvestigateNoise UMETA(DisplayName = "Investigate Noise"),
	InvestigateLastSeen UMETA(DisplayName = "Investigate Last Seen"),
	SearchLocker UMETA(DisplayName = "Search Locker"),
	AmbushObjective UMETA(DisplayName = "Ambush Objective"),
	UseScan UMETA(DisplayName = "Use Scan"),
	UsePower UMETA(DisplayName = "Use Power"),
	DropTrap UMETA(DisplayName = "Drop Trap")
};

UENUM(BlueprintType)
enum class EBHBotStimulusType : uint8
{
	Sight UMETA(DisplayName = "Sight"),
	Noise UMETA(DisplayName = "Noise"),
	Locker UMETA(DisplayName = "Locker"),
	Objective UMETA(DisplayName = "Objective"),
	Trap UMETA(DisplayName = "Trap"),
	Capture UMETA(DisplayName = "Capture"),
	Escape UMETA(DisplayName = "Escape"),
	Unreachable UMETA(DisplayName = "Unreachable")
};

UENUM(BlueprintType)
enum class EBHObjectiveStationType : uint8
{
	Valve UMETA(DisplayName = "Valve"),
	Terminal UMETA(DisplayName = "Terminal"),
	Antenna UMETA(DisplayName = "Antenna"),
	Evidence UMETA(DisplayName = "Evidence")
};

UENUM(BlueprintType)
enum class EBHRevisionCounterNodeType : uint8
{
	None UMETA(DisplayName = "None"),
	PeerReview UMETA(DisplayName = "Peer Review"),
	DemonstrationTrap UMETA(DisplayName = "Demonstration Trap")
};

UENUM(BlueprintType)
enum class EBHRevisionMode : uint8
{
	None UMETA(DisplayName = "None"),
	PhysicsClassroom UMETA(DisplayName = "Physics Classroom")
};

UENUM(BlueprintType)
enum class EBHPhysicsTopic : uint8
{
	ForcesAndMotion UMETA(DisplayName = "Forces and Motion"),
	Electricity UMETA(DisplayName = "Electricity"),
	Waves UMETA(DisplayName = "Waves"),
	Energy UMETA(DisplayName = "Energy")
};

UENUM(BlueprintType)
enum class EBHQuestionDifficulty : uint8
{
	Easy UMETA(DisplayName = "Easy"),
	Medium UMETA(DisplayName = "Medium"),
	Hard UMETA(DisplayName = "Hard")
};

UENUM(BlueprintType)
enum class EBHQuestionType : uint8
{
	MultipleChoice UMETA(DisplayName = "Multiple Choice"),
	TrueFalse UMETA(DisplayName = "True/False"),
	Calculation UMETA(DisplayName = "Calculation"),
	FormulaFill UMETA(DisplayName = "Formula Fill"),
	GraphReading UMETA(DisplayName = "Graph Reading"),
	DragDropMatching UMETA(DisplayName = "Drag/Drop Matching"),
	Ordering UMETA(DisplayName = "Ordering")
};

UENUM(BlueprintType)
enum class EBHDiagramType : uint8
{
	None UMETA(DisplayName = "None"),
	MotionGraph UMETA(DisplayName = "Motion Graph"),
	VelocityGraph UMETA(DisplayName = "Velocity Graph"),
	ForceArrows UMETA(DisplayName = "Force Arrows"),
	SpringGraph UMETA(DisplayName = "Spring Graph"),
	MomentBeam UMETA(DisplayName = "Moment Beam"),
	Circuit UMETA(DisplayName = "Circuit"),
	IVGraph UMETA(DisplayName = "IV Graph"),
	StaticCharge UMETA(DisplayName = "Static Charge"),
	Wave UMETA(DisplayName = "Wave"),
	EMSpectrum UMETA(DisplayName = "EM Spectrum"),
	RayDiagram UMETA(DisplayName = "Ray Diagram"),
	Sankey UMETA(DisplayName = "Sankey"),
	EnergyChain UMETA(DisplayName = "Energy Chain")
};

UENUM(BlueprintType)
enum class EBHRevisionDifficultyMix : uint8
{
	Balanced UMETA(DisplayName = "Balanced"),
	Easy UMETA(DisplayName = "Easy"),
	Hard UMETA(DisplayName = "Hard"),
	Adaptive UMETA(DisplayName = "Adaptive")
};

USTRUCT(BlueprintType)
struct FBHRevisionAnswerPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	TArray<FString> Choices;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 CorrectChoiceIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Formula;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float NumericAnswer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float NumericTolerance = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHRevisionQuestion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHPhysicsTopic Topic = EBHPhysicsTopic::ForcesAndMotion;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString TopicName;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Subtopic;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHQuestionDifficulty Difficulty = EBHQuestionDifficulty::Easy;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHQuestionType Type = EBHQuestionType::MultipleChoice;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHDiagramType DiagramType = EBHDiagramType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Prompt;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FBHRevisionAnswerPayload Answer;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Hint;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString CorrectionPrompt;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float MasteryWeight = 1.0f;
};

USTRUCT(BlueprintType)
struct FBHPlayerRevisionStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 Attempts = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 CorrectAnswers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 CorrectionsCompleted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 ContributionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 HintCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float MasteryPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ForcesMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ElectricityMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float WavesMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float EnergyMastery = 0.0f;
};

USTRUCT(BlueprintType)
struct FBHClassRevisionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float ClassMasteryAverage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	float LowestStudentMastery = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 ActiveStudentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 StudentsBelowIndividualThreshold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	int32 StudentsWithoutContribution = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Blackout Hunt|Revision")
	EBHPhysicsTopic WeakTopic = EBHPhysicsTopic::ForcesAndMotion;
};

struct FBHBotStimulus
{
	EBHBotStimulusType Type = EBHBotStimulusType::Noise;
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<AActor> TargetActor;
	FVector Location = FVector::ZeroVector;
	float TimeSeconds = 0.0f;
	float Strength = 1.0f;
	FString Reason;
};

struct FBHBotMemory
{
	TWeakObjectPtr<ABHCharacter> LastSeenSurvivor;
	TWeakObjectPtr<ABHCharacter> LastSeenHunter;
	FVector LastSeenSurvivorLocation = FVector::ZeroVector;
	FVector LastSeenHunterLocation = FVector::ZeroVector;
	FVector LastHeardLocation = FVector::ZeroVector;
	float LastSeenSurvivorTime = -9999.0f;
	float LastSeenHunterTime = -9999.0f;
	float LastHeardTime = -9999.0f;
	float ThreatPressure = 0.0f;
	float ObjectivePressure = 0.0f;
	TArray<FBHBotStimulus> RecentStimuli;
};

struct FBHBotObjectiveClaim
{
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AController> Claimant;
	EBHBotIntent Intent = EBHBotIntent::None;
	float ExpireTimeSeconds = 0.0f;
};

struct FBHBotTargetCooldown
{
	TWeakObjectPtr<AController> Claimant;
	TWeakObjectPtr<AActor> Target;
	float ExpireTimeSeconds = 0.0f;
};

struct FBHBotDecisionCandidate
{
	EBHBotIntent Intent = EBHBotIntent::None;
	TWeakObjectPtr<AActor> Target;
	FVector Location = FVector::ZeroVector;
	float BaseScore = 0.0f;
	float Risk = 0.0f;
	float Urgency = 0.0f;
	float Distance = 0.0f;
	FString DebugLabel;
};

struct FBHBotPolicyFeatures
{
	EBHBotPersonality Personality = EBHBotPersonality::Objective;
	EBHBotDifficulty Difficulty = EBHBotDifficulty::Normal;
	EBHPlayerRole Role = EBHPlayerRole::Unassigned;
	float ThreatPressure = 0.0f;
	float ObjectivePressure = 0.0f;
	float TeamSpread = 0.0f;
	float TimeRemainingRatio = 1.0f;
	float DistanceToTarget = 0.0f;
	int32 VisibleEnemyCount = 0;
	int32 NearbyAllyCount = 0;
	int32 TargetClaimCount = 0;
};

struct FBHBotPolicyResult
{
	int32 ChosenIndex = INDEX_NONE;
	float Score = 0.0f;
	bool bUsedModel = false;
	FString DebugLabel;
};

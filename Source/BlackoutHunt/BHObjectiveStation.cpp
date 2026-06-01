#include "BHObjectiveStation.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Math/RandomStream.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

// Share of objective-station questions that become a "proper" drag matching/ordering task instead of
// multiple choice, in BOTH revision and standard Hunt modes. 0 = always MC (old behaviour), 1 = always
// drag. Tunable live; keyboard players can still answer a drag question by picking the correct
// arrangement string with 1-4, so this never strands non-mouse input.
static TAutoConsoleVariable<float> CVarBHDragQuestionFrequency(
	TEXT("bh.Questions.DragFrequency"), 0.4f,
	TEXT("Probability [0..1] an objective-station question is a drag matching/ordering question instead of multiple choice."),
	ECVF_Default);

// Mastery multiplier for answering a question via the interactive visual method (drag arrangement /
// clicking a diagram element) instead of picking a multiple-choice option. 1.0 = no bonus; 1.5 =
// visual answers build 50% more mastery for the same question. Clamped [1..2] on use.
static TAutoConsoleVariable<float> CVarBHVisualMasteryMultiplier(
	TEXT("bh.Questions.VisualMasteryMultiplier"), 1.5f,
	TEXT("Mastery multiplier for a visually/interactively answered question vs a multiple-choice pick (1..2)."),
	ECVF_Default);

namespace
{
	bool BHWantsDragQuestion()
	{
		const float Frequency = FMath::Clamp(CVarBHDragQuestionFrequency.GetValueOnGameThread(), 0.0f, 1.0f);
		if (Frequency <= 0.0f)
		{
			return false;
		}
		return FMath::FRand() < Frequency;
	}
}

namespace
{
float BHStationStressAlpha(float Value, float Threshold, float FullValue)
{
	return FMath::Clamp((Value - Threshold) / FMath::Max(1.0f, FullValue - Threshold), 0.0f, 1.0f);
}

float BHStationWorkerNerveMultiplier(const ABHCharacter* Character)
{
	if (!Character)
	{
		return 0.0f;
	}

	const float FearPenalty = BHStationStressAlpha(Character->GetFear(), 60.0f, 100.0f) * 0.18f;
	const float DreadPenalty = BHStationStressAlpha(Character->GetDread(), 55.0f, 100.0f) * 0.24f;
	return FMath::Clamp(1.0f - FearPenalty - DreadPenalty, 0.58f, 1.0f);
}

bool BHStationAllowsWarmup(const ABHGameState* GameState)
{
	return GameState
		&& GameState->RoundPhase == EBHRoundPhase::Prep
		&& !GameState->bPracticeMode
		&& !GameState->bTestMode;
}

struct FBHStationQuestion
{
	const TCHAR* Topic;
	const TCHAR* Prompt;
	const TCHAR* Choices[4];
	int32 CorrectIndex;
	const TCHAR* Hint;
};

static const FBHStationQuestion ValveQuestions[] = {
	{
		TEXT("Forces"),
		TEXT("What does a resultant force of zero mean?"),
		{TEXT("Balanced forces"), TEXT("The object has no mass"), TEXT("The object must stop instantly"), TEXT("Gravity is switched off")},
		0,
		TEXT("Zero resultant force means the pushes and pulls cancel out.")
	},
	{
		TEXT("Forces"),
		TEXT("Which equation links force, mass, and acceleration?"),
		{TEXT("F = ma"), TEXT("F = m / a"), TEXT("F = a / m"), TEXT("F = d / t")},
		0,
		TEXT("Newton's second law multiplies mass by acceleration.")
	},
	{
		TEXT("Pressure"),
		TEXT("A larger force on the same area does what to pressure?"),
		{TEXT("Increases it"), TEXT("Decreases it"), TEXT("Makes it zero"), TEXT("Only changes mass")},
		0,
		TEXT("Pressure rises when the same area carries more force.")
	},
	{
		TEXT("Motion"),
		TEXT("What does the gradient of a distance-time graph show?"),
		{TEXT("Speed"), TEXT("Mass"), TEXT("Pressure"), TEXT("Voltage")},
		0,
		TEXT("A steeper distance-time graph means greater speed.")
	},
	{
		TEXT("Forces"),
		TEXT("Which force opposes motion through air?"),
		{TEXT("Air resistance"), TEXT("Magnetism"), TEXT("Weight only"), TEXT("Upthrust only")},
		0,
		TEXT("Air resistance is drag from moving through air.")
	},
	{
		TEXT("Moments"),
		TEXT("What makes a door easier to turn?"),
		{TEXT("Pushing farther from the hinge"), TEXT("Pushing at the hinge"), TEXT("Using less force"), TEXT("Making the door lighter only")},
		0,
		TEXT("Moment equals force times perpendicular distance from the pivot.")
	},
	{
		TEXT("Pressure"),
		TEXT("Why do snow shoes stop you sinking as much?"),
		{TEXT("They spread force over a larger area"), TEXT("They remove your weight"), TEXT("They create frictionless ice"), TEXT("They lower gravity")},
		0,
		TEXT("Larger area means lower pressure for the same force.")
	}
};

static const FBHStationQuestion TerminalQuestions[] = {
	{
		TEXT("Electricity"),
		TEXT("State the equation linking voltage, current, and resistance."),
		{TEXT("V = IR"), TEXT("I = VR"), TEXT("R = VI"), TEXT("V = I / R")},
		0,
		TEXT("This is Ohm's law.")
	},
	{
		TEXT("Electricity"),
		TEXT("If resistance increases and voltage stays the same, current does what?"),
		{TEXT("Decreases"), TEXT("Increases"), TEXT("Becomes voltage"), TEXT("Stays exactly the same")},
		0,
		TEXT("Use V = IR, rearranged as I = V / R.")
	},
	{
		TEXT("Electricity"),
		TEXT("Which instrument measures current?"),
		{TEXT("Ammeter"), TEXT("Voltmeter"), TEXT("Thermometer"), TEXT("Balance")},
		0,
		TEXT("Current is measured in amps, so use an ammeter.")
	},
	{
		TEXT("Electricity"),
		TEXT("In a series circuit, current is..."),
		{TEXT("The same everywhere"), TEXT("Used up by each lamp"), TEXT("Highest at the battery only"), TEXT("Always zero")},
		0,
		TEXT("Series current has only one path, so it is the same through each component.")
	},
	{
		TEXT("Electricity"),
		TEXT("What happens to total resistance when resistors are added in series?"),
		{TEXT("It increases"), TEXT("It decreases"), TEXT("It becomes zero"), TEXT("It becomes current")},
		0,
		TEXT("Series resistances add together.")
	},
	{
		TEXT("Power"),
		TEXT("Which equation links power, current, and voltage?"),
		{TEXT("P = IV"), TEXT("P = I / V"), TEXT("V = P / I only never"), TEXT("I = PV")},
		0,
		TEXT("Electrical power is current multiplied by potential difference.")
	},
	{
		TEXT("Charge"),
		TEXT("Which equation links charge, current, and time?"),
		{TEXT("Q = It"), TEXT("Q = I / t"), TEXT("Q = Vt"), TEXT("Q = Rt")},
		0,
		TEXT("Charge flow equals current multiplied by time.")
	}
};

static const FBHStationQuestion AntennaQuestions[] = {
	{
		TEXT("Waves"),
		TEXT("What does frequency mean?"),
		{TEXT("Waves passing each second"), TEXT("Wave height"), TEXT("Wave color"), TEXT("Circuit length")},
		0,
		TEXT("Frequency counts complete waves per second.")
	},
	{
		TEXT("Waves"),
		TEXT("Which equation links wave speed, frequency, and wavelength?"),
		{TEXT("v = f x wavelength"), TEXT("v = f / wavelength"), TEXT("v = wavelength / f"), TEXT("v = f + wavelength")},
		0,
		TEXT("Wave speed equals frequency multiplied by wavelength.")
	},
	{
		TEXT("Waves"),
		TEXT("What is reflection?"),
		{TEXT("A wave bouncing off a surface"), TEXT("A wave disappearing"), TEXT("A circuit overheating"), TEXT("An object speeding up")},
		0,
		TEXT("A mirror reflects light because the wave bounces back.")
	},
	{
		TEXT("Waves"),
		TEXT("What does amplitude usually tell you about a sound wave?"),
		{TEXT("Loudness"), TEXT("Pitch only"), TEXT("Circuit resistance"), TEXT("Mass")},
		0,
		TEXT("Bigger amplitude means a louder sound.")
	},
	{
		TEXT("Waves"),
		TEXT("What happens to light when it enters glass at an angle?"),
		{TEXT("It refracts"), TEXT("It becomes charge"), TEXT("It stops forever"), TEXT("It loses all frequency")},
		0,
		TEXT("Refraction is a change in direction when speed changes between materials.")
	},
	{
		TEXT("EM Spectrum"),
		TEXT("Which wave is used for thermal imaging?"),
		{TEXT("Infrared"), TEXT("Gamma only"), TEXT("Radio only"), TEXT("Ultrasound")},
		0,
		TEXT("Infrared radiation is linked to thermal energy transfer.")
	},
	{
		TEXT("Waves"),
		TEXT("A shorter wavelength at the same speed means frequency is..."),
		{TEXT("Higher"), TEXT("Lower"), TEXT("Zero"), TEXT("Unchanged always")},
		0,
		TEXT("Use v = f x wavelength; if speed is fixed, shorter wavelength means higher frequency.")
	}
};

static const FBHStationQuestion EvidenceQuestions[] = {
	{
		TEXT("Energy"),
		TEXT("What does conservation of energy mean?"),
		{TEXT("Energy is transferred, not destroyed"), TEXT("Energy always disappears"), TEXT("Energy is always useful"), TEXT("Energy only exists in batteries")},
		0,
		TEXT("Energy changes store or pathway, but the total is conserved.")
	},
	{
		TEXT("Energy"),
		TEXT("Which energy store does a moving object have?"),
		{TEXT("Kinetic"), TEXT("Chemical"), TEXT("Nuclear"), TEXT("Elastic only")},
		0,
		TEXT("Moving objects have kinetic energy.")
	},
	{
		TEXT("Thermal Physics"),
		TEXT("Why does insulation reduce heating bills?"),
		{TEXT("It slows unwanted energy transfer"), TEXT("It creates energy"), TEXT("It removes all air"), TEXT("It makes roofs heavier")},
		0,
		TEXT("Insulation keeps useful thermal energy inside for longer.")
	},
	{
		TEXT("Energy"),
		TEXT("Which store increases when an object is lifted higher?"),
		{TEXT("Gravitational potential"), TEXT("Kinetic"), TEXT("Chemical only"), TEXT("Elastic only")},
		0,
		TEXT("Lifting increases gravitational potential energy.")
	},
	{
		TEXT("Energy"),
		TEXT("Which equation gives kinetic energy?"),
		{TEXT("0.5 x m x v squared"), TEXT("m x g x h"), TEXT("F x e"), TEXT("V x I")},
		0,
		TEXT("Kinetic energy depends on mass and the square of velocity.")
	},
	{
		TEXT("Thermal Physics"),
		TEXT("What transfers thermal energy through solids?"),
		{TEXT("Conduction"), TEXT("Evaporation only"), TEXT("Refraction"), TEXT("Magnetism")},
		0,
		TEXT("Conduction transfers energy through particle collisions in solids.")
	},
	{
		TEXT("Energy"),
		TEXT("What does efficiency compare?"),
		{TEXT("Useful output to total input"), TEXT("Mass to weight"), TEXT("Current to voltage"), TEXT("Speed to time")},
		0,
		TEXT("Efficiency is useful energy or power output divided by total input.")
	}
};

const FBHStationQuestion* QuestionBankForType(EBHObjectiveStationType StationType, int32& OutCount)
{
	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		OutCount = UE_ARRAY_COUNT(ValveQuestions);
		return ValveQuestions;
	case EBHObjectiveStationType::Terminal:
		OutCount = UE_ARRAY_COUNT(TerminalQuestions);
		return TerminalQuestions;
	case EBHObjectiveStationType::Antenna:
		OutCount = UE_ARRAY_COUNT(AntennaQuestions);
		return AntennaQuestions;
	case EBHObjectiveStationType::Evidence:
		OutCount = UE_ARRAY_COUNT(EvidenceQuestions);
		return EvidenceQuestions;
	default:
		OutCount = UE_ARRAY_COUNT(TerminalQuestions);
		return TerminalQuestions;
	}
}

bool BHUseImportedStationVisuals()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	if (!FParse::Param(CommandLine, TEXT("BHImportedStationVisuals")))
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const bool bAutomationRun = FString(CommandLine).Contains(TEXT("Automation RunTests"), ESearchCase::IgnoreCase);
	if (bAutomationRun)
	{
		return false;
	}
#endif

	return true;
}

UStaticMesh* ImportedStationMesh(const TCHAR* Path, UStaticMesh* Fallback)
{
	if (!Path || !Path[0] || !BHUseImportedStationVisuals())
	{
		return Fallback;
	}

	UStaticMesh* LoadedMesh = LoadObject<UStaticMesh>(nullptr, Path);
	return LoadedMesh ? LoadedMesh : Fallback;
}

UMaterialInterface* ImportedStationMaterial(const TCHAR* Path, UMaterialInterface* Fallback)
{
	if (!Path || !Path[0] || !BHUseImportedStationVisuals())
	{
		return Fallback;
	}

	UMaterialInterface* LoadedMaterial = LoadObject<UMaterialInterface>(nullptr, Path);
	return LoadedMaterial ? LoadedMaterial : Fallback;
}

void ConfigureImportedStationPart(
	UStaticMeshComponent* Component,
	const TCHAR* MeshPath,
	UStaticMesh* FallbackMesh,
	const TCHAR* MaterialPath,
	UMaterialInterface* FallbackMaterial,
	const FVector& RelativeLocation,
	const FRotator& RelativeRotation,
	const FVector& RelativeScale,
	bool bCollisionEnabled = false)
{
	BHPropVisuals::ConfigurePart(
		Component,
		ImportedStationMesh(MeshPath, FallbackMesh),
		ImportedStationMaterial(MaterialPath, FallbackMaterial),
		RelativeLocation,
		RelativeRotation,
		RelativeScale,
		bCollisionEnabled);
}
}

ABHObjectiveStation::ABHObjectiveStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
	StationType = EBHObjectiveStationType::Valve;
	WorkProgress = 0.0f;
	bCompleted = false;
	bDirectorActive = true;
	CorrectAnswerIndex = 0;
	bQuestionSolved = false;
	bQuestionFeedbackCorrect = false;
	QuestionType = EBHQuestionType::MultipleChoice;
	QuestionDifficulty = EBHQuestionDifficulty::Easy;
	QuestionDiagramType = EBHDiagramType::None;
	QuestionDiagram = FBHDiagramParams();
	QuestionNumericAnswer = 0.0f;
	QuestionNumericTolerance = 0.0f;
	RevisionQuestionsSolved = 0;
	RevisionQuestionsRequired = 1;
	RevisionQuestionStep = 0;
	RevisionCounterType = EBHRevisionCounterNodeType::None;
	bTeacherMirrorTrapNode = false;
	bRevisionReviewQuestion = false;
	bUseAdaptiveQuestionOverride = false;
	AdaptiveQuestionTopic = EBHPhysicsTopic::ForcesAndMotion;
	AdaptiveQuestionDifficulty = EBHQuestionDifficulty::Easy;
	PendingReviewQuestionId = TEXT("");
	WorkSeconds = 5.5f;
	LastNoiseTime = -999.0f;
	LastAnswerTime = -999.0f;
	InteractionLabel = FText::FromString(TEXT("Objective Station"));
	SetActorScale3D(FVector::OneVector);

	FixtureA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureA"));
	FixtureA->SetupAttachment(RootComponent);
	FixtureB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureB"));
	FixtureB->SetupAttachment(RootComponent);
	FixtureC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureC"));
	FixtureC->SetupAttachment(RootComponent);
	FixtureD = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureD"));
	FixtureD->SetupAttachment(RootComponent);
	FixtureE = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureE"));
	FixtureE->SetupAttachment(RootComponent);
	StatusLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(RootComponent);
	ProgressBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressBack"));
	ProgressBack->SetupAttachment(RootComponent);
	ProgressFill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressFill"));
	ProgressFill->SetupAttachment(RootComponent);

	ApplyStationVisuals();
}

void ABHObjectiveStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bDirectorActive || bCompleted || Workers.Num() == 0)
	{
		if (HasAuthority())
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (!BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !BHStationAllowsWarmup(BHGS)))
	{
		Workers.Empty();
		SetActorTickEnabled(false);
		return;
	}

	for (auto It = Workers.CreateIterator(); It; ++It)
	{
		// Drop workers who are gone OR no longer an alive survivor. MarkCaptured() hides the pawn and sets
		// bOutOfPlay but neither destroys it nor calls EndInteract, so an IsValid()-only prune let a student
		// captured mid-repair keep driving the objective to completion (an infection-mode flip to Hunter
		// would too). Only an alive survivor contributes work, mirroring the CanInteract entry gate.
		const ABHCharacter* Worker = It->Get();
		const ABHPlayerState* WorkerPS = Worker ? Worker->GetBHPlayerState() : nullptr;
		if (!WorkerPS || !WorkerPS->IsAliveSurvivor())
		{
			It.RemoveCurrent();
		}
	}

	if (Workers.Num() == 0)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float Rate = FMath::Max(0.1f, WorkSeconds);
	float WorkerRate = 0.0f;
	for (const TWeakObjectPtr<ABHCharacter>& Worker : Workers)
	{
		WorkerRate += BHStationWorkerNerveMultiplier(Worker.Get());
	}
	WorkProgress = FMath::Clamp(WorkProgress + DeltaSeconds * WorkerRate / Rate, 0.0f, 1.0f);
	ApplyStationVisuals();
	if (WorkProgress >= 1.0f)
	{
		CompleteObjective();
	}
}

void ABHObjectiveStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABHObjectiveStation, StationType);
	DOREPLIFETIME(ABHObjectiveStation, WorkProgress);
	DOREPLIFETIME(ABHObjectiveStation, bCompleted);
	DOREPLIFETIME(ABHObjectiveStation, bDirectorActive);
	DOREPLIFETIME(ABHObjectiveStation, QuestionTopic);
	DOREPLIFETIME(ABHObjectiveStation, QuestionPrompt);
	DOREPLIFETIME(ABHObjectiveStation, QuestionChoices);
	// Answer-bearing fields are kept server-only (COND_Never): replicating them let a modified
	// client read the correct answer off the wire and farm the economy. Bots read these directly
	// on the server via GetCorrectAnswerIndexForBot()/GetQuestionNumericAnswer(), so no client needs them.
	DOREPLIFETIME_CONDITION(ABHObjectiveStation, CorrectAnswerIndex, COND_Never);
	DOREPLIFETIME(ABHObjectiveStation, bQuestionSolved);
	DOREPLIFETIME(ABHObjectiveStation, QuestionHint);
	DOREPLIFETIME(ABHObjectiveStation, QuestionFeedback);
	DOREPLIFETIME(ABHObjectiveStation, bQuestionFeedbackCorrect);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionId);
	DOREPLIFETIME(ABHObjectiveStation, QuestionType);
	DOREPLIFETIME(ABHObjectiveStation, QuestionDifficulty);
	DOREPLIFETIME(ABHObjectiveStation, QuestionDiagramType);
	DOREPLIFETIME(ABHObjectiveStation, QuestionDiagram);
	DOREPLIFETIME(ABHObjectiveStation, InteractivePieces);
	DOREPLIFETIME(ABHObjectiveStation, InteractiveSlots);
	DOREPLIFETIME(ABHObjectiveStation, QuestionSubtopic);
	DOREPLIFETIME(ABHObjectiveStation, QuestionFormula);
	DOREPLIFETIME_CONDITION(ABHObjectiveStation, QuestionNumericAnswer, COND_Never);
	DOREPLIFETIME(ABHObjectiveStation, QuestionNumericTolerance);
	DOREPLIFETIME(ABHObjectiveStation, QuestionExplanation);
	DOREPLIFETIME(ABHObjectiveStation, RevisionTeamSummary);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionsSolved);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionsRequired);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionStep);
	DOREPLIFETIME(ABHObjectiveStation, RevisionCounterType);
	DOREPLIFETIME(ABHObjectiveStation, bTeacherMirrorTrapNode);
	DOREPLIFETIME(ABHObjectiveStation, bRevisionReviewQuestion);
}

bool ABHObjectiveStation::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRoleWarmup = BHStationAllowsWarmup(BHGS);
	if (bTeacherMirrorTrapNode)
	{
		return bDirectorActive && !bCompleted && BHPS && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && BHPS->IsAliveSurvivor();
	}
	const bool bQuestionReady = bQuestionSolved || QuestionChoices.Num() == 0;
	return bDirectorActive && !bCompleted && bQuestionReady && BHPS && BHGS && (BHGS->RoundPhase == EBHRoundPhase::Hunt || bRoleWarmup) && BHPS->IsAliveSurvivor();
}

void ABHObjectiveStation::BeginInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority() && bTeacherMirrorTrapNode && CanInteract_Implementation(Character))
	{
		bCompleted = true;
		WorkProgress = 1.0f;
		Workers.Empty();
		SetActorTickEnabled(false);
		QuestionFeedback = TEXT("Scare relay armed.");
		bQuestionFeedbackCorrect = true;
		ApplyStationVisuals();
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->ActivateStudentScareRelay(Character, this);
		}
		return;
	}

	if (HasAuthority() && CanInteract_Implementation(Character))
	{
		const bool bNewWorker = !Workers.Contains(Character);
		Workers.Add(Character);
		SetActorTickEnabled(true);
		const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
		if (bNewWorker && !BHStationAllowsWarmup(BHGS))
		{
			if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				const UEnum* StationEnum = StaticEnum<EBHObjectiveStationType>();
				const FString StationTypeName = StationEnum ? StationEnum->GetNameStringByValue(static_cast<int64>(StationType)) : TEXT("Station");
				BHGM->RecordPlaytestTelemetryMarker(TEXT("objective_work_started"), GetActorLocation(), FString::Printf(TEXT("%s progress=%.2f"), *StationTypeName, WorkProgress), Character->GetBHPlayerState());
			}
		}
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now - LastNoiseTime > 2.0f)
		{
			LastNoiseTime = Now;
			if (!BHStationAllowsWarmup(BHGS))
			{
				if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
				{
					BHGM->NotifyLoudNoise(GetActorLocation(), GetStationName().ToLower());
				}
			}
		}
	}
}

void ABHObjectiveStation::EndInteract_Implementation(ABHCharacter* Character)
{
	if (HasAuthority())
	{
		Workers.Remove(Character);
		if (Workers.Num() == 0)
		{
			SetActorTickEnabled(false);
		}
	}
}

FText ABHObjectiveStation::GetInteractionLabel_Implementation(ABHCharacter* Character) const
{
	if (!bDirectorActive)
	{
		return FText::FromString(FString::Printf(TEXT("Inactive %s"), *GetStationName()));
	}

	if (bCompleted)
	{
		return FText::FromString(FString::Printf(TEXT("%s Complete"), *GetStationName()));
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	if (bTeacherMirrorTrapNode)
	{
		return FText::FromString(BHPS && BHPS->IsAliveSurvivor() ? TEXT("Arm Scare Relay") : TEXT("Class Relay"));
	}
	if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		return FText::FromString(TEXT("Ready Up First"));
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bAliveMonitorCanAnswer = BHPS->PlayerRole == EBHPlayerRole::FakeHunter
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& BHGS
		&& BHGS->bRevisionMode
		&& !bQuestionSolved
		&& QuestionChoices.Num() > 0;
	if (!BHPS->IsAliveSurvivor() && !bAliveMonitorCanAnswer)
	{
		return FText::FromString(BHPS->PlayerRole == EBHPlayerRole::FakeHunter ? TEXT("Monitor Must Revise") : TEXT("Survivor Objective"));
	}

	if (BHStationAllowsWarmup(BHGS))
	{
		if (!bQuestionSolved && QuestionChoices.Num() > 0)
		{
			return FText::FromString(FString::Printf(TEXT("Warmup Answer %s: press 1-4"), *GetStationName()));
		}
		return FText::FromString(FString::Printf(TEXT("Warmup %s %s %d%%"), *GetActionVerb(), *GetStationName(), FMath::RoundToInt(WorkProgress * 100.0f)));
	}

	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return FText::FromString(TEXT("Objective During Hunt"));
	}

	if (!bQuestionSolved && QuestionChoices.Num() > 0)
	{
		const bool bRevisionQuestion = BHGS && BHGS->bRevisionMode;
		const FString ProgressText = bRevisionQuestion
			? FString::Printf(TEXT(" %d/%d"), FMath::Clamp(RevisionQuestionsSolved + 1, 1, FMath::Max(1, RevisionQuestionsRequired)), FMath::Max(1, RevisionQuestionsRequired))
			: TEXT("");
		return FText::FromString(FString::Printf(TEXT("Answer%s %s: press 1-4"), *ProgressText, *GetStationName()));
	}

	return FText::FromString(FString::Printf(TEXT("%s %s %d%%"), *GetActionVerb(), *GetStationName(), FMath::RoundToInt(WorkProgress * 100.0f)));
}

FBHInteractionPromptInfo ABHObjectiveStation::GetInteractionPromptInfo_Implementation(ABHCharacter* Character) const
{
	FBHInteractionPromptInfo Info;
	Info.bUsePromptInfo = true;
	Info.Label = GetInteractionLabel_Implementation(Character);
	Info.bCanInteract = CanInteract_Implementation(Character);
	Info.HoldSeconds = Info.bCanInteract ? WorkSeconds : 0.0f;
	Info.Progress = WorkProgress;

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	const bool bRoleWarmup = BHStationAllowsWarmup(BHGS);
	const bool bObjectivePhaseActive = BHGS && (BHGS->RoundPhase == EBHRoundPhase::Hunt || bRoleWarmup);
	Info.RiskText = FText::FromString(bRoleWarmup ? TEXT("WARMUP") : (!bQuestionSolved && QuestionChoices.Num() > 0 ? TEXT("ANSWER WITH 1-4") : TEXT("LISTENING")));
	Info.bDangerous = !bRoleWarmup;
	if (Info.bCanInteract)
	{
		return Info;
	}

	if (!bDirectorActive)
	{
		Info.DisabledReason = FText::FromString(TEXT("INACTIVE THIS ROUND"));
	}
	else if (bCompleted)
	{
		Info.DisabledReason = FText::FromString(TEXT("DONE"));
	}
	else if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
	{
		Info.DisabledReason = FText::FromString(TEXT("READY UP FIRST"));
	}
	else if (!bObjectivePhaseActive)
	{
		Info.DisabledReason = FText::FromString(TEXT("HUNT PHASE ONLY"));
	}
	else
	{
		const bool bAliveMonitorCanAnswer = BHPS->PlayerRole == EBHPlayerRole::FakeHunter
			&& BHPS->LifeState == EBHPlayerLifeState::Alive
			&& BHGS->bRevisionMode
			&& !bQuestionSolved
			&& QuestionChoices.Num() > 0;
		if (!BHPS->IsAliveSurvivor() && !bAliveMonitorCanAnswer)
		{
			Info.DisabledReason = FText::FromString(BHPS->PlayerRole == EBHPlayerRole::FakeHunter ? TEXT("CONTRIBUTE IN PHYSICS CLASSROOM") : TEXT("SURVIVOR OBJECTIVE"));
		}
		else if (!bQuestionSolved && QuestionChoices.Num() > 0)
		{
			Info.DisabledReason = FText::FromString(TEXT("ANSWER WITH 1-4 FIRST"));
		}
		else
		{
			Info.DisabledReason = FText::FromString(TEXT("LOCKED"));
		}
	}
	return Info;
}

void ABHObjectiveStation::Configure(EBHObjectiveStationType NewStationType)
{
	StationType = NewStationType;
	bTeacherMirrorTrapNode = false;
	ConfigureQuestion();
	ApplyStationVisuals();
}

void ABHObjectiveStation::ConfigureTutorialVisualQuestion()
{
	// A deliberately easy, fully self-contained diagram question: a straight, rising distance-time line
	// (MotionGraph ShapeVariant 1 = constant velocity). The HUD renders the graph above clickable choices,
	// so the student practises reading a diagram and answering it by pointing - the visual-question core.
	bRevisionReviewQuestion = false;
	InteractivePieces.Reset();
	InteractiveSlots.Reset();

	RevisionQuestionId = TEXT("");
	RevisionQuestionsSolved = 0;
	RevisionQuestionsRequired = 1;
	RevisionQuestionStep = 0;
	QuestionTopic = TEXT("Motion");
	QuestionSubtopic = TEXT("Distance-time graphs");
	QuestionType = EBHQuestionType::MultipleChoice;
	QuestionDifficulty = EBHQuestionDifficulty::Easy;

	QuestionDiagramType = EBHDiagramType::MotionGraph;
	FBHDiagramParams Diagram;
	Diagram.ShapeVariant = 1; // constant velocity (straight rising line)
	Diagram.XAxis = TEXT("Time");
	Diagram.YAxis = TEXT("Distance");
	QuestionDiagram = Diagram;

	QuestionPrompt = TEXT("Read the distance-time graph. How is this object moving?");
	QuestionChoices.Reset();
	QuestionChoices.Add(TEXT("At a steady speed"));
	QuestionChoices.Add(TEXT("Speeding up"));
	QuestionChoices.Add(TEXT("Slowing down"));
	QuestionChoices.Add(TEXT("Not moving at all"));
	CorrectAnswerIndex = 0;

	bQuestionSolved = false;
	QuestionHint = TEXT("A straight line that keeps rising means equal distance every second.");
	QuestionFormula = TEXT("");
	QuestionNumericAnswer = 0.0f;
	QuestionNumericTolerance = 0.0f;
	QuestionExplanation = TEXT("A straight, steadily rising distance-time line means constant (steady) speed.");
	QuestionFeedback = TEXT("");
	bQuestionFeedbackCorrect = false;
	RevisionTeamSummary = TEXT("");

	ApplyStationVisuals();
}

void ABHObjectiveStation::ConfigureTutorialInteractiveQuestion()
{
	// A hands-on drag-drop matching question: the student drags each unit onto the quantity it measures,
	// then presses Submit. The question-pointer system renders the pieces/slots/submit and grades it via
	// SubmitArrangement, so this teaches the interactive answer flow - distinct from clicking a fixed choice.
	bRevisionReviewQuestion = false;
	InteractivePieces.Reset();
	InteractiveSlots.Reset();

	RevisionQuestionId = TEXT("");
	RevisionQuestionsSolved = 0;
	RevisionQuestionsRequired = 1;
	RevisionQuestionStep = 0;
	QuestionTopic = TEXT("Units");
	QuestionSubtopic = TEXT("Quantities and units");
	QuestionType = EBHQuestionType::DragDropMatching;
	QuestionDifficulty = EBHQuestionDifficulty::Easy;
	QuestionDiagramType = EBHDiagramType::None;
	QuestionDiagram = FBHDiagramParams();

	QuestionPrompt = TEXT("Drag each unit onto the quantity it measures, then press Submit.");
	QuestionChoices.Reset();
	// QuestionChoices[CorrectAnswerIndex] is the canonical arrangement ("slot -> piece" pairs, ';'-separated);
	// the second entry is a decoy so the wrong-answer synthetic-MC mapping in SubmitArrangement stays in range.
	QuestionChoices.Add(TEXT("Speed -> m/s; Force -> N; Mass -> kg"));
	QuestionChoices.Add(TEXT("Speed -> kg; Force -> m/s; Mass -> N"));
	CorrectAnswerIndex = 0;

	// Populate the (deterministically shuffled) draggable pieces + drop slots from the correct arrangement.
	const FVector Location = GetActorLocation();
	const int32 Seed = FMath::Max(1, FMath::Abs(FMath::RoundToInt(Location.X * 0.17f + Location.Y * 0.11f)));
	BuildInteractiveArrangement(Seed);

	bQuestionSolved = false;
	QuestionHint = TEXT("Speed is metres per second, force is newtons, mass is kilograms.");
	QuestionFormula = TEXT("");
	QuestionNumericAnswer = 0.0f;
	QuestionNumericTolerance = 0.0f;
	QuestionExplanation = TEXT("Speed = m/s, Force = N (newtons), Mass = kg.");
	QuestionFeedback = TEXT("");
	bQuestionFeedbackCorrect = false;
	RevisionTeamSummary = TEXT("");

	ApplyStationVisuals();
}

void ABHObjectiveStation::ConfigureRevisionCounterNode(EBHRevisionCounterNodeType NewCounterType)
{
	RevisionCounterType = NewCounterType;
	if (RevisionCounterType == EBHRevisionCounterNodeType::PeerReview)
	{
		RevisionQuestionsRequired = FMath::Max(RevisionQuestionsRequired, 3);
	}
	else if (RevisionCounterType == EBHRevisionCounterNodeType::DemonstrationTrap)
	{
		RevisionQuestionsRequired = FMath::Max(RevisionQuestionsRequired, 2);
	}
}

void ABHObjectiveStation::ConfigureTeacherMirrorTrapNode()
{
	bTeacherMirrorTrapNode = true;
	RevisionCounterType = EBHRevisionCounterNodeType::None;
	bDirectorActive = true;
	bCompleted = false;
	bQuestionSolved = true;
	QuestionChoices.Reset();
	InteractivePieces.Reset();
	InteractiveSlots.Reset();
	QuestionTopic = TEXT("Class Relay");
	QuestionSubtopic = TEXT("Hidden module");
	QuestionPrompt = TEXT("A hidden relay the class can arm for a counter-scare.");
	QuestionHint = TEXT("");
	QuestionExplanation = TEXT("Hidden switch armed.");
	QuestionFeedback = TEXT("");
	RevisionTeamSummary = TEXT("");
	RevisionQuestionsSolved = 0;
	RevisionQuestionsRequired = 1;
	RevisionQuestionStep = 0;
	bUseAdaptiveQuestionOverride = false;
	WorkProgress = 0.0f;
	WorkSeconds = 1.0f;
	InteractionLabel = FText::FromString(TEXT("Class Scare Relay"));
	SetActorScale3D(FVector(0.58f, 0.58f, 0.58f));
	ApplyStationVisuals();
}

void ABHObjectiveStation::SetDirectorActive(bool bNewActive)
{
	if (bTeacherMirrorTrapNode)
	{
		bDirectorActive = bNewActive;
		return;
	}

	bDirectorActive = bNewActive;
	Workers.Empty();
	SetActorTickEnabled(false);
	WorkProgress = 0.0f;
	if (bDirectorActive)
	{
		bCompleted = false;
		RevisionQuestionsSolved = 0;
		RevisionQuestionStep = 0;
		RevisionQuestionsRequired = ResolveRevisionQuestionTarget();
		ConfigureQuestion();
		bQuestionSolved = QuestionChoices.Num() == 0;
		QuestionFeedback = TEXT("");
		bQuestionFeedbackCorrect = false;
		RevisionTeamVotes.Reset();
		RevisionTeamPlayerIds.Reset();
		RevisionTeamSummary = TEXT("");
		PendingCorrectionCharacters.Reset();
		bUseAdaptiveQuestionOverride = false;
	}
	else
	{
		QuestionFeedback = TEXT("");
		RevisionQuestionsSolved = 0;
		RevisionQuestionStep = 0;
		RevisionQuestionsRequired = 1;
		bUseAdaptiveQuestionOverride = false;
	}
	ApplyStationVisuals();
}

bool ABHObjectiveStation::SubmitAnswer(ABHCharacter* Character, int32 AnswerIndex, bool bVisualAnswer)
{
	if (!HasAuthority())
	{
		return false;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr;
	const bool bAliveSurvivor = BHPS && BHPS->IsAliveSurvivor();
	const bool bAliveMonitorCanAnswer = BHPS
		&& BHPS->PlayerRole == EBHPlayerRole::FakeHunter
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& BHGS
		&& BHGS->bRevisionMode;
	const bool bRoleWarmup = BHStationAllowsWarmup(BHGS);
	if (!bDirectorActive || bCompleted || !BHPS || (!bAliveSurvivor && !bAliveMonitorCanAnswer) || !BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !bRoleWarmup))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("That question is not available right now."), 2.75f);
		}
		return false;
	}

	if (bQuestionSolved)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(BHGS->bRevisionMode ? TEXT("Node questions solved. Hold E with classmates to finish the task.") : TEXT("Question solved. Hold E to finish the task."), 2.75f);
		}
		return true;
	}

	if (!QuestionChoices.IsValidIndex(AnswerIndex))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("Choose an answer with 1, 2, 3, or 4."), 2.75f);
		}
		return false;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastAnswerTime < 0.45f)
	{
		return false;
	}
	LastAnswerTime = Now;

	if (bRoleWarmup)
	{
		if (Character)
		{
			if (ABHPlayerState* WarmupPS = Character->GetPlayerState<ABHPlayerState>())
			{
				WarmupPS->MarkWarmupStep(EBHWarmupStep::Question);
			}
		}
		const bool bCorrect = AnswerIndex == CorrectAnswerIndex;
		if (bCorrect)
		{
			bQuestionSolved = true;
			QuestionFeedback = FString::Printf(TEXT("Warmup correct: %s Hold E to try the %s. The live Hunt reloads this node."), *QuestionExplanation, *GetStationName());
			bQuestionFeedbackCorrect = true;
			WorkProgress = FMath::Max(WorkProgress, 0.18f);
			if (Character)
			{
				Character->RecoverStamina(8.0f);
				Character->RefillFlashlight(4.0f);
			}
			if (PC)
			{
				PC->ClientShowStatusMessage(TEXT("Warmup correct. No report, XP, or mastery changes recorded."), 3.0f);
			}
		}
		else
		{
			QuestionFeedback = QuestionHint.IsEmpty()
				? TEXT("Warmup miss. Try another answer; no report data recorded.")
				: FString::Printf(TEXT("Warmup miss. Hint: %s No report data recorded."), *QuestionHint);
			bQuestionFeedbackCorrect = false;
			if (PC)
			{
				PC->ClientShowStatusMessage(TEXT("Warmup answer only. No detention mark or report entry recorded."), 3.0f);
			}
		}
		ApplyStationVisuals();
		return bCorrect;
	}

	// Firm-but-safe anti-gaming: after a wrong answer the station holds answer submission for a
	// few seconds so the student actually reads the correction instead of cycling 1-4. The hold
	// only delays input — the player is never pinned and can walk away and return.
	if (Now < CorrectionHoldUntil)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Read the correction first. Retry in %.0fs."), FMath::Max(0.0f, CorrectionHoldUntil - Now) + 0.5f), 2.0f);
		}
		return false;
	}

	int32 EvaluatedAnswerIndex = AnswerIndex;
	TArray<ABHCharacter*> RevisionParticipants;
	const bool bActiveRevisionMode = BHGS && BHGS->bRevisionMode;
	if (bActiveRevisionMode)
	{
		ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
		if (RevisionTeamPlayerIds.Num() == 0 && BHGM)
		{
			TSet<int32> NewTeam;
			FString TeamSummary;
			if (BHGM->BuildRevisionAnswerTeam(this, Character, NewTeam, TeamSummary))
			{
				RevisionTeamPlayerIds = NewTeam;
				RevisionTeamSummary = TeamSummary;
				QuestionFeedback = FString::Printf(TEXT("Answer team: %s. Vote with 1-4."), *RevisionTeamSummary);
				bQuestionFeedbackCorrect = true;
			}
		}

		const int32 PlayerId = BHPS ? BHPS->GetPlayerId() : INDEX_NONE;
		if (RevisionTeamPlayerIds.Num() > 0 && !RevisionTeamPlayerIds.Contains(PlayerId))
		{
			if (PC)
			{
				PC->ClientShowStatusMessage(FString::Printf(TEXT("Watch this one. Answer team: %s."), *RevisionTeamSummary), 3.0f);
			}
			return false;
		}

		RevisionTeamVotes.Add(PlayerId, AnswerIndex);
		int32 ChoiceVotes[4] = {0, 0, 0, 0};
		for (const TPair<int32, int32>& Vote : RevisionTeamVotes)
		{
			if (Vote.Value >= 0 && Vote.Value < 4)
			{
				++ChoiceVotes[Vote.Value];
			}
		}

		const int32 TeamSize = FMath::Max(1, RevisionTeamPlayerIds.Num());
		const int32 MajorityNeeded = FMath::Clamp((TeamSize / 2) + 1, 1, TeamSize);
		int32 BestChoice = INDEX_NONE;
		for (int32 ChoiceIndex = 0; ChoiceIndex < 4; ++ChoiceIndex)
		{
			if (ChoiceVotes[ChoiceIndex] >= MajorityNeeded)
			{
				BestChoice = ChoiceIndex;
				break;
			}
		}

		if (BestChoice == INDEX_NONE)
		{
			if (RevisionTeamVotes.Num() >= TeamSize)
			{
				RevisionTeamVotes.Reset();
				QuestionFeedback = FString::Printf(TEXT("No majority. Quick correction: %s Vote again."), *QuestionExplanation);
				bQuestionFeedbackCorrect = false;
				if (PC)
				{
					PC->ClientShowStatusMessage(TEXT("No majority. Discuss for ten seconds, then revote."), 3.25f);
				}
				if (BHGM)
				{
					BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("physics disagreement"));
				}
				return false;
			}

			QuestionFeedback = FString::Printf(TEXT("Vote recorded (%d/%d). Team: %s."), RevisionTeamVotes.Num(), TeamSize, *RevisionTeamSummary);
			bQuestionFeedbackCorrect = true;
			if (PC)
			{
				PC->ClientShowStatusMessage(QuestionFeedback, 2.0f);
			}
			return true;
		}

		EvaluatedAnswerIndex = BestChoice;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			ABHCharacter* VoterCharacter = It->Get() ? Cast<ABHCharacter>(It->Get()->GetPawn()) : nullptr;
			const ABHPlayerState* VoterPS = VoterCharacter ? VoterCharacter->GetPlayerState<ABHPlayerState>() : nullptr;
			if (VoterCharacter && VoterPS && RevisionTeamVotes.Contains(VoterPS->GetPlayerId()))
			{
				RevisionParticipants.Add(VoterCharacter);
			}
		}
		RevisionTeamVotes.Reset();
	}

	const bool bCorrect = EvaluatedAnswerIndex == CorrectAnswerIndex;
	const FString EvaluatedSelectedAnswer = QuestionChoices.IsValidIndex(EvaluatedAnswerIndex) ? QuestionChoices[EvaluatedAnswerIndex] : FString();
	return FinalizeRevisionAnswer(Character, PC, bCorrect, EvaluatedSelectedAnswer, RevisionParticipants, bActiveRevisionMode, bVisualAnswer);
}

// Shared post-evaluation path for both multiple-choice and typed-numeric answers.
// Records the result (using the actual chosen/typed answer text for distractor
// analytics), advances the revision node, applies feedback, and handles the wrong path.
bool ABHObjectiveStation::FinalizeRevisionAnswer(ABHCharacter* Character, ABHPlayerController* PC, bool bCorrect, const FString& EvaluatedSelectedAnswer, TArray<ABHCharacter*>& RevisionParticipants, bool bActiveRevisionMode, bool bVisualAnswer)
{
	if (bCorrect)
	{
		bool bRevisionNodeUnlocked = true;
		if (bActiveRevisionMode)
		{
			FBHRevisionQuestion RevisionQuestion;
			if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
			{
				if (!FBHRevisionQuestionBank::FindQuestion(RevisionQuestionId, RevisionQuestion))
				{
					RevisionQuestion.Id = RevisionQuestionId;
					RevisionQuestion.Topic = FBHRevisionQuestionBank::TopicForStationType(StationType);
					RevisionQuestion.TopicName = QuestionTopic;
					RevisionQuestion.Difficulty = QuestionDifficulty;
					RevisionQuestion.Type = QuestionType;
					RevisionQuestion.MasteryWeight = 1.0f;
				}
				if (RevisionParticipants.IsEmpty() && Character)
				{
					RevisionParticipants.Add(Character);
				}
				// A correct answer counts as a correction when the loaded question was itself a
				// re-surfaced review (a question the targeted student had previously missed).
				const bool bCorrection = bRevisionReviewQuestion;
				// Reward the interactive visual answer (drag arrangement / clicked diagram element) with
				// more mastery than a multiple-choice pick of the same question. RecordRevisionAnswer's
				// clamp ceiling is raised so this scaled weight is not clipped.
				if (bVisualAnswer)
				{
					RevisionQuestion.MasteryWeight *= FMath::Clamp(CVarBHVisualMasteryMultiplier.GetValueOnGameThread(), 1.0f, 2.0f);
				}
				for (ABHCharacter* Participant : RevisionParticipants)
				{
					BHGM->RecordRevisionAnswer(Participant, RevisionQuestion, true, bCorrection, EvaluatedSelectedAnswer, RevisionTeamSummary);
				}
				PendingCorrectionCharacters.Reset();
			}

			RevisionQuestionsRequired = FMath::Max(1, RevisionQuestionsRequired);
			RevisionQuestionsSolved = FMath::Clamp(RevisionQuestionsSolved + 1, 0, RevisionQuestionsRequired);
			bRevisionNodeUnlocked = RevisionQuestionsSolved >= RevisionQuestionsRequired;
			WorkProgress = FMath::Max(WorkProgress, bRevisionNodeUnlocked ? 0.72f : 0.12f + 0.48f * (static_cast<float>(RevisionQuestionsSolved) / static_cast<float>(RevisionQuestionsRequired)));
			RevisionTeamVotes.Reset();
			RevisionTeamPlayerIds.Reset();
			RevisionTeamSummary = TEXT("");
		}

		// A correct answer clears the anti-gaming hold/streak for this station.
		ConsecutiveWrongAtStation = 0;
		CorrectionHoldUntil = 0.0f;

		const FString ActionVerb = GetActionVerb().ToLower();
		if (Character)
		{
			Character->ClearDetentionMark();
			Character->AddFear(-12.0f);
			Character->AddDread(-18.0f);
			Character->RecoverStamina(12.0f);
			Character->RefillFlashlight(5.0f);
		}
		if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
		{
			BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("correct answer"));
		}

		if (bActiveRevisionMode && !bRevisionNodeUnlocked)
		{
			const int32 CompletedStep = RevisionQuestionsSolved;
			const int32 RequiredSteps = FMath::Max(1, RevisionQuestionsRequired);
			const FString CompletedExplanation = QuestionExplanation;
			QueueAdaptiveQuestionForParticipants(RevisionParticipants, true);
			++RevisionQuestionStep;
			ConfigureQuestion();
			QuestionFeedback = FString::Printf(TEXT("Correct %d/%d: %s Next team question loaded."), CompletedStep, RequiredSteps, *CompletedExplanation);
			bQuestionFeedbackCorrect = true;
			if (PC)
			{
				PC->ClientShowStatusMessage(FString::Printf(TEXT("Correct %d/%d. New team question loaded."), CompletedStep, RequiredSteps), 3.0f);
			}
			ApplyStationVisuals();
			return true;
		}

		bQuestionSolved = true;
		QuestionFeedback = bActiveRevisionMode
			? FString::Printf(TEXT("Node unlocked %d/%d: %s Now hold E to %s the %s."), RevisionQuestionsSolved, FMath::Max(1, RevisionQuestionsRequired), *QuestionExplanation, *ActionVerb, *GetStationName())
			: FString::Printf(TEXT("Correct. Now hold E to %s the %s."), *ActionVerb, *GetStationName());
		bQuestionFeedbackCorrect = true;
		if (bActiveRevisionMode && RevisionCounterType != EBHRevisionCounterNodeType::None)
		{
			if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
			{
				BHGM->TriggerTeacherCounterJumpscare(this, RevisionCounterType);
			}
			RevisionCounterType = EBHRevisionCounterNodeType::None;
		}
		if (PC)
		{
			PC->ClientShowStatusMessage(QuestionFeedback, 3.25f);
		}
		ApplyStationVisuals();
		return true;
	}

	// --- Wrong answer path ---
	// Capture the missed question's teaching text before any reload overwrites it.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FString MissedExplanation = QuestionExplanation;
	const FString MissedHint = QuestionHint;
	const FString MissedTopic = QuestionTopic;

	if (bActiveRevisionMode)
	{
		FBHRevisionQuestion RevisionQuestion;
		if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
		{
			if (!FBHRevisionQuestionBank::FindQuestion(RevisionQuestionId, RevisionQuestion))
			{
				RevisionQuestion.Id = RevisionQuestionId;
				RevisionQuestion.Topic = FBHRevisionQuestionBank::TopicForStationType(StationType);
				RevisionQuestion.TopicName = QuestionTopic;
				RevisionQuestion.Difficulty = QuestionDifficulty;
				RevisionQuestion.Type = QuestionType;
				RevisionQuestion.MasteryWeight = 1.0f;
			}
			if (RevisionParticipants.IsEmpty() && Character)
			{
				RevisionParticipants.Add(Character);
			}
			for (ABHCharacter* Participant : RevisionParticipants)
			{
				// RecordRevisionAnswer enqueues this question for spaced review and applies the
				// topic-mastery decay; the missed question resurfaces later, not right now.
				BHGM->RecordRevisionAnswer(Participant, RevisionQuestion, false, false, EvaluatedSelectedAnswer, RevisionTeamSummary);
			}
		}
	}

	// Shared wrong-answer pressure (retained); the detention mark scales on repeated misses.
	if (Character)
	{
		Character->AddFear(15.0f);
		Character->AddDread(22.0f);
		Character->ApplyDetentionMark(38.0f + 8.0f * static_cast<float>(FMath::Min(ConsecutiveWrongAtStation, 3)));
	}
	WorkProgress = FMath::Max(0.0f, WorkProgress - 0.18f);
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("wrong answer"));
	}

	// Escalating correction hold: each consecutive wrong answer extends the time the student must
	// spend reading the correction before answering again. Input-only; the player can walk away.
	++ConsecutiveWrongAtStation;
	const float HoldSeconds = FMath::Clamp(3.0f + 1.5f * static_cast<float>(ConsecutiveWrongAtStation - 1), 3.0f, 9.0f);
	CorrectionHoldUntil = Now + HoldSeconds;
	bQuestionFeedbackCorrect = false;

	if (bActiveRevisionMode)
	{
		// Anti-echo: never leave the just-revealed answer on screen to be re-entered. Load a
		// fresh, eased adaptive question; the missed one is already queued for spaced review.
		QueueAdaptiveQuestionForParticipants(RevisionParticipants, false);
		++RevisionQuestionStep;
		ConfigureQuestion();
		QuestionFeedback = FString::Printf(TEXT("Wrong. Correction (%s): %s Hint: %s A new question loaded — read the correction, then answer in %.0fs."), *MissedTopic, *MissedExplanation, *MissedHint, HoldSeconds);
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Wrong. Read the correction; a new question loaded. Retry in %.0fs."), HoldSeconds), 3.5f);
		}
	}
	else
	{
		QuestionFeedback = MissedHint.IsEmpty()
			? TEXT("Wrong. The room got quieter.")
			: FString::Printf(TEXT("Wrong. Hint: %s"), *MissedHint);
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("Wrong answer. The Teacher heard that."), 3.25f);
		}
	}
	ApplyStationVisuals();

	return false;
}

bool ABHObjectiveStation::SubmitNumericAnswer(ABHCharacter* Character, float Value)
{
	if (!HasAuthority())
	{
		return false;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr;
	const bool bAliveSurvivor = BHPS && BHPS->IsAliveSurvivor();
	const bool bAliveMonitorCanAnswer = BHPS
		&& BHPS->PlayerRole == EBHPlayerRole::FakeHunter
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& BHGS
		&& BHGS->bRevisionMode;
	const bool bRoleWarmup = BHStationAllowsWarmup(BHGS);
	if (!bDirectorActive || bCompleted || !BHPS || (!bAliveSurvivor && !bAliveMonitorCanAnswer) || !BHGS || (BHGS->RoundPhase != EBHRoundPhase::Hunt && !bRoleWarmup))
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("That question is not available right now."), 2.75f);
		}
		return false;
	}

	// Typed numeric entry only applies to Calculation questions; everything else uses 1-4.
	if (QuestionType != EBHQuestionType::Calculation)
	{
		return false;
	}

	if (bQuestionSolved)
	{
		return true;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastAnswerTime < 0.45f)
	{
		return false;
	}
	LastAnswerTime = Now;

	// Validate against the question's target value within tolerance. The bank authors a
	// tolerance (e.g. 0.1) for calculation questions; fall back to a tiny epsilon if absent.
	const float Tolerance = FMath::Max(QuestionNumericTolerance, 0.0001f);
	const bool bWithinTolerance = FMath::Abs(Value - QuestionNumericAnswer) <= Tolerance;
	const FString TypedAnswer = FString::Printf(TEXT("%g (typed)"), Value);

	if (bRoleWarmup)
	{
		if (bWithinTolerance)
		{
			bQuestionSolved = true;
			QuestionFeedback = FString::Printf(TEXT("Warmup correct: %s Hold E to try the %s. The live Hunt reloads this node."), *QuestionExplanation, *GetStationName());
			bQuestionFeedbackCorrect = true;
			WorkProgress = FMath::Max(WorkProgress, 0.18f);
			if (Character)
			{
				Character->RecoverStamina(8.0f);
				Character->RefillFlashlight(4.0f);
			}
			if (PC)
			{
				PC->ClientShowStatusMessage(TEXT("Warmup correct. No report, XP, or mastery changes recorded."), 3.0f);
			}
		}
		else
		{
			QuestionFeedback = QuestionHint.IsEmpty()
				? TEXT("Warmup miss. Type another value; no report data recorded.")
				: FString::Printf(TEXT("Warmup miss. Hint: %s No report data recorded."), *QuestionHint);
			bQuestionFeedbackCorrect = false;
			if (PC)
			{
				PC->ClientShowStatusMessage(TEXT("Warmup answer only. No detention mark or report entry recorded."), 3.0f);
			}
		}
		ApplyStationVisuals();
		return bWithinTolerance;
	}

	// Correction hold (see SubmitAnswer): block resubmission until the student has had a few
	// seconds with the correction after a wrong answer.
	if (Now < CorrectionHoldUntil)
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Read the correction first. Retry in %.0fs."), FMath::Max(0.0f, CorrectionHoldUntil - Now) + 0.5f), 2.0f);
		}
		return false;
	}

	// Numeric entry is individual (you type your own value), so credit the submitter
	// rather than a voting team; FinalizeRevisionAnswer adds the submitter when empty.
	TArray<ABHCharacter*> RevisionParticipants;
	return FinalizeRevisionAnswer(Character, PC, bWithinTolerance, TypedAnswer, RevisionParticipants, true);
}

void ABHObjectiveStation::BuildInteractiveArrangement(int32 Seed)
{
	InteractivePieces.Reset();
	InteractiveSlots.Reset();
	if (QuestionType != EBHQuestionType::DragDropMatching && QuestionType != EBHQuestionType::Ordering)
	{
		return;
	}
	if (!QuestionChoices.IsValidIndex(CorrectAnswerIndex))
	{
		return;
	}

	TArray<FString> Slots;
	TArray<FString> CanonicalPieces;
	if (!FBHRevisionQuestionBank::ParseArrangementChoice(QuestionChoices[CorrectAnswerIndex], QuestionType, Slots, CanonicalPieces))
	{
		return;
	}
	if (Slots.Num() < 2 || Slots.Num() != CanonicalPieces.Num())
	{
		return;
	}

	InteractiveSlots = Slots;

	// Shuffle the pieces so the tray never shows the answer order. Deterministic from Seed (the
	// station's LocationSeed) so the replicated snapshot is stable; no Rand()/Date use.
	TArray<int32> Perm;
	Perm.Reserve(CanonicalPieces.Num());
	for (int32 Index = 0; Index < CanonicalPieces.Num(); ++Index)
	{
		Perm.Add(Index);
	}
	FRandomStream Stream(Seed != 0 ? Seed : 1);
	for (int32 Index = Perm.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = Stream.RandRange(0, Index);
		Perm.Swap(Index, SwapIndex);
	}
	// Guard against the identity permutation, which would line each piece up under its correct slot
	// and hand over the answer; swapping the first two guarantees a non-trivial start for N >= 2.
	bool bIdentity = true;
	for (int32 Index = 0; Index < Perm.Num(); ++Index)
	{
		if (Perm[Index] != Index)
		{
			bIdentity = false;
			break;
		}
	}
	if (bIdentity && Perm.Num() >= 2)
	{
		Perm.Swap(0, 1);
	}

	InteractivePieces.Reserve(CanonicalPieces.Num());
	for (int32 Index = 0; Index < Perm.Num(); ++Index)
	{
		InteractivePieces.Add(CanonicalPieces[Perm[Index]]);
	}
}

bool ABHObjectiveStation::GradeArrangement(const TArray<int32>& SlotToPiece) const
{
	if (!QuestionChoices.IsValidIndex(CorrectAnswerIndex))
	{
		return false;
	}
	TArray<FString> Slots;
	TArray<FString> CanonicalPieces;
	if (!FBHRevisionQuestionBank::ParseArrangementChoice(QuestionChoices[CorrectAnswerIndex], QuestionType, Slots, CanonicalPieces))
	{
		return false;
	}
	if (CanonicalPieces.Num() != InteractiveSlots.Num() || SlotToPiece.Num() != InteractiveSlots.Num())
	{
		return false;
	}
	for (int32 Slot = 0; Slot < SlotToPiece.Num(); ++Slot)
	{
		const int32 PieceIndex = SlotToPiece[Slot];
		if (!InteractivePieces.IsValidIndex(PieceIndex))
		{
			return false;
		}
		// The shuffled piece dropped on this slot must equal the slot's canonically-correct piece.
		if (!InteractivePieces[PieceIndex].Equals(CanonicalPieces[Slot], ESearchCase::CaseSensitive))
		{
			return false;
		}
	}
	return true;
}

bool ABHObjectiveStation::SubmitArrangement(ABHCharacter* Character, const TArray<int32>& SlotToPiece)
{
	if (!HasAuthority())
	{
		return false;
	}
	// Only matching/ordering questions that actually built a tray accept arrangements.
	if (InteractiveSlots.Num() == 0 || (QuestionType != EBHQuestionType::DragDropMatching && QuestionType != EBHQuestionType::Ordering))
	{
		return false;
	}
	ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr;
	if (bQuestionSolved)
	{
		return true;
	}
	// Require a complete one-piece-per-slot assignment; reject malformed/partial client input.
	if (SlotToPiece.Num() != InteractiveSlots.Num())
	{
		if (PC)
		{
			PC->ClientShowStatusMessage(TEXT("Place a piece on every slot before submitting."), 2.0f);
		}
		return false;
	}
	TSet<int32> Used;
	for (const int32 PieceIndex : SlotToPiece)
	{
		if (!InteractivePieces.IsValidIndex(PieceIndex) || Used.Contains(PieceIndex))
		{
			return false;
		}
		Used.Add(PieceIndex);
	}

	const bool bCorrect = GradeArrangement(SlotToPiece);
	// Map onto a synthetic choice index so the whole SubmitAnswer pipeline (validity checks,
	// per-player throttle, team voting, anti-gaming correction hold, FinalizeRevisionAnswer and the
	// feedback/visuals) treats an arrangement exactly like a clicked multiple-choice answer.
	const int32 SyntheticIndex = bCorrect ? CorrectAnswerIndex : ((CorrectAnswerIndex == 0) ? 1 : 0);
	return SubmitAnswer(Character, SyntheticIndex, /*bVisualAnswer=*/true);
}

bool ABHObjectiveStation::IsDirectorActive() const
{
	return bDirectorActive;
}

bool ABHObjectiveStation::IsCompleted() const
{
	return bCompleted;
}

EBHObjectiveStationType ABHObjectiveStation::GetStationType() const
{
	return StationType;
}

FString ABHObjectiveStation::GetPhysicalTaskInstruction() const
{
	if (bTeacherMirrorTrapNode)
	{
		return TEXT("After finding the relay, hold E to arm the class counter-scare.");
	}

	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		return TEXT("After the question, hold E to balance the counterweight lock.");
	case EBHObjectiveStationType::Terminal:
		return TEXT("After the question, hold E to restore the circuit breaker.");
	case EBHObjectiveStationType::Antenna:
		return TEXT("After the question, hold E to tune the signal receiver.");
	case EBHObjectiveStationType::Evidence:
		return TEXT("After the question, hold E to charge the energy store.");
	default:
		return TEXT("After the question, hold E to finish the objective.");
	}
}

bool ABHObjectiveStation::IsQuestionSolved() const
{
	return bQuestionSolved || QuestionChoices.Num() == 0;
}

FString ABHObjectiveStation::GetQuestionTopic() const
{
	return QuestionTopic;
}

FString ABHObjectiveStation::GetQuestionPrompt() const
{
	return QuestionPrompt;
}

int32 ABHObjectiveStation::GetQuestionChoiceCount() const
{
	return QuestionChoices.Num();
}

int32 ABHObjectiveStation::GetCorrectAnswerIndexForBot() const
{
	return CorrectAnswerIndex;
}

FString ABHObjectiveStation::GetQuestionChoice(int32 ChoiceIndex) const
{
	return QuestionChoices.IsValidIndex(ChoiceIndex) ? QuestionChoices[ChoiceIndex] : FString();
}

FString ABHObjectiveStation::GetQuestionFeedback() const
{
	return QuestionFeedback;
}

bool ABHObjectiveStation::IsQuestionFeedbackCorrect() const
{
	return bQuestionFeedbackCorrect;
}

EBHQuestionType ABHObjectiveStation::GetQuestionType() const
{
	return QuestionType;
}

EBHQuestionDifficulty ABHObjectiveStation::GetQuestionDifficulty() const
{
	return QuestionDifficulty;
}

EBHDiagramType ABHObjectiveStation::GetQuestionDiagramType() const
{
	return QuestionDiagramType;
}

FString ABHObjectiveStation::GetQuestionSubtopic() const
{
	return QuestionSubtopic;
}

FString ABHObjectiveStation::GetQuestionFormula() const
{
	return QuestionFormula;
}

float ABHObjectiveStation::GetQuestionNumericAnswer() const
{
	return QuestionNumericAnswer;
}

float ABHObjectiveStation::GetQuestionNumericTolerance() const
{
	return QuestionNumericTolerance;
}

FString ABHObjectiveStation::GetQuestionExplanation() const
{
	return QuestionExplanation;
}

FString ABHObjectiveStation::GetRevisionTeamSummary() const
{
	return RevisionTeamSummary;
}

int32 ABHObjectiveStation::GetRevisionQuestionsSolved() const
{
	return RevisionQuestionsSolved;
}

int32 ABHObjectiveStation::GetRevisionQuestionsRequired() const
{
	return RevisionQuestionsRequired;
}

EBHRevisionCounterNodeType ABHObjectiveStation::GetRevisionCounterType() const
{
	return RevisionCounterType;
}

bool ABHObjectiveStation::IsTeacherMirrorTrapNode() const
{
	return bTeacherMirrorTrapNode;
}

bool ABHObjectiveStation::IsReviewQuestion() const
{
	return bRevisionReviewQuestion;
}

void ABHObjectiveStation::CompleteObjective()
{
	if (!bQuestionSolved && QuestionChoices.Num() > 0)
	{
		return;
	}

	bCompleted = true;
	WorkProgress = 1.0f;
	Workers.Empty();
	SetActorTickEnabled(false);
	ApplyStationVisuals();

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (BHStationAllowsWarmup(BHGS))
	{
		QuestionFeedback = FString::Printf(TEXT("Warmup %s complete. Live objective progress resets when Hunt starts."), *GetStationName());
		bQuestionFeedbackCorrect = true;
		return;
	}

	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyObjectiveStationCompleted(this);
	}
}

int32 ABHObjectiveStation::ResolveRevisionQuestionTarget() const
{
	if (const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		if (BHGM->IsRevisionMode())
		{
			return BHGM->GetRevisionQuestionTargetPerNode();
		}
	}
	return 1;
}

void ABHObjectiveStation::QueueAdaptiveQuestionForParticipants(const TArray<ABHCharacter*>& Participants, bool bLastAnswerCorrect)
{
	if (!HasAuthority() || Participants.IsEmpty())
	{
		return;
	}

	const ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr;
	if (!BHGM || !BHGM->IsRevisionMode())
	{
		return;
	}

	PendingReviewQuestionId.Reset();
	bUseAdaptiveQuestionOverride = false;

	// The station shows ONE shared question, so we must pick whose profile it targets. Always
	// serving the single weakest student starves everyone else of questions tuned to THEIR gaps,
	// so instead rotate the spotlight across the eligible participants by question step: over a
	// run of questions every individual periodically gets one calibrated to their own weakest
	// topic + mastery. A solo node (one participant) collapses to "always them".
	TArray<const ABHPlayerState*> Eligible;
	for (ABHCharacter* Participant : Participants)
	{
		const ABHPlayerState* ParticipantPS = Participant ? Participant->GetPlayerState<ABHPlayerState>() : nullptr;
		if (ParticipantPS && ABHGameMode::IsRevisionParticipantRole(ParticipantPS->PlayerRole))
		{
			Eligible.Add(ParticipantPS);
		}
	}
	if (Eligible.IsEmpty())
	{
		return;
	}

	// Stable order so the rotation is deterministic across questions regardless of array churn.
	Eligible.Sort([](const ABHPlayerState& A, const ABHPlayerState& B)
	{
		return A.GetPlayerId() < B.GetPlayerId();
	});

	// RevisionQuestionStep is still the current step here (callers increment it after this returns),
	// so each successive question advances the target by one participant.
	const int32 TargetIndex = FMath::Max(0, RevisionQuestionStep) % Eligible.Num();
	const ABHPlayerState* TargetPS = Eligible[TargetIndex];

	EBHPhysicsTopic PlanTopic = EBHPhysicsTopic::ForcesAndMotion;
	EBHQuestionDifficulty PlanDifficulty = EBHQuestionDifficulty::Easy;
	FString PlanReason;
	BHGM->GetAdaptiveRevisionPlan(TargetPS, bLastAnswerCorrect, PlanTopic, PlanDifficulty, PlanReason);
	AdaptiveQuestionTopic = PlanTopic;
	AdaptiveQuestionDifficulty = PlanDifficulty;
	// Re-test the targeted player on a question they previously missed (spaced repetition).
	PendingReviewQuestionId = TargetPS->PeekRevisionReview();
	bUseAdaptiveQuestionOverride = true;
}

void ABHObjectiveStation::ApplyRevisionQuestion(const FBHRevisionQuestion& Selected, int32 Seed)
{
	RevisionQuestionId = Selected.Id;
	QuestionTopic = Selected.TopicName;
	QuestionSubtopic = Selected.Subtopic;
	QuestionType = Selected.Type;
	QuestionDifficulty = Selected.Difficulty;
	QuestionDiagramType = Selected.DiagramType;
	QuestionDiagram = Selected.Diagram;
	QuestionPrompt = Selected.Prompt;
	QuestionChoices.Reset();
	CorrectAnswerIndex = 0;
	const int32 ChoiceCount = Selected.Answer.Choices.Num();
	if (ChoiceCount > 0)
	{
		const int32 ChoiceRotation = Seed % ChoiceCount;
		bool bFoundCorrect = false;
		for (int32 Index = 0; Index < ChoiceCount; ++Index)
		{
			const int32 SourceIndex = (Index + ChoiceCount - ChoiceRotation) % ChoiceCount;
			QuestionChoices.Add(Selected.Answer.Choices[SourceIndex]);
			if (SourceIndex == Selected.Answer.CorrectChoiceIndex)
			{
				CorrectAnswerIndex = Index;
				bFoundCorrect = true;
			}
		}
		if (!bFoundCorrect)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BHObjectiveStation] Question '%s' has CorrectChoiceIndex %d out of range [0,%d); clamping."), *Selected.Id, Selected.Answer.CorrectChoiceIndex, ChoiceCount);
			const int32 ClampedSource = FMath::Clamp(Selected.Answer.CorrectChoiceIndex, 0, ChoiceCount - 1);
			CorrectAnswerIndex = (ClampedSource + ChoiceRotation) % ChoiceCount;
		}
	}
	// Build the interactive drag tray for matching/ordering questions (no-op otherwise).
	BuildInteractiveArrangement(Seed);
	bQuestionSolved = false;
	QuestionHint = Selected.Hint;
	QuestionFormula = Selected.Answer.Formula;
	QuestionNumericAnswer = Selected.Answer.NumericAnswer;
	QuestionNumericTolerance = Selected.Answer.NumericTolerance;
	QuestionExplanation = Selected.Explanation;
	QuestionFeedback = TEXT("");
	bQuestionFeedbackCorrect = false;
	RevisionTeamVotes.Reset();
	RevisionTeamPlayerIds.Reset();
	RevisionTeamSummary = TEXT("");
	PendingCorrectionCharacters.Reset();
}

void ABHObjectiveStation::ConfigureQuestion()
{
	bRevisionReviewQuestion = false;
	// Cleared up front so a stale matching/ordering tray never lingers onto a non-arrangement
	// question; rebuilt below only when a DragDropMatching/Ordering revision question loads.
	InteractivePieces.Reset();
	InteractiveSlots.Reset();
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		if (BHGM->IsRevisionMode())
		{
			RevisionQuestionsRequired = FMath::Max(1, RevisionQuestionsRequired);
			FBHRevisionQuestion Selected;
			const FVector Location = GetActorLocation();
			const int32 LocationSeed = FMath::Abs(FMath::RoundToInt(Location.X * 0.13f + Location.Y * 0.07f + static_cast<int32>(StationType) * 131.0f + RevisionQuestionStep * 911.0f + FMath::RandRange(0, 100000)));

			// Spaced repetition: if the targeted participant has a previously missed
			// question queued, re-ask that exact question before normal selection.
			bool bSelected = false;
			bool bReviewSelected = false;
			if (!PendingReviewQuestionId.IsEmpty() && FBHRevisionQuestionBank::FindQuestion(PendingReviewQuestionId, Selected))
			{
				bSelected = true;
				bReviewSelected = true;
			}
			else
			{
				const EBHPhysicsTopic Topic = bUseAdaptiveQuestionOverride ? AdaptiveQuestionTopic : FBHRevisionQuestionBank::TopicForStationType(StationType);
				const EBHQuestionDifficulty PreferredDifficulty = bUseAdaptiveQuestionOverride ? AdaptiveQuestionDifficulty : EBHQuestionDifficulty::Medium;
				// Bias a share of nodes toward "proper" drag matching/ordering questions; otherwise (or if
				// the topic has no drag question) fall back to the normal difficulty/weakness-adaptive pick.
				if (BHWantsDragQuestion() && FBHRevisionQuestionBank::SelectDragQuestion(Topic, PreferredDifficulty, LocationSeed, Selected))
				{
					bSelected = true;
				}
				else
				{
					bSelected = bUseAdaptiveQuestionOverride
						? FBHRevisionQuestionBank::SelectQuestionByDifficulty(Topic, AdaptiveQuestionDifficulty, LocationSeed, Selected)
						: FBHRevisionQuestionBank::SelectQuestion(Topic, BHGM->GetRevisionDifficultyMix(), LocationSeed, BHGM->GetRevisionWeakTopics(), Selected);
				}
			}
			bRevisionReviewQuestion = bReviewSelected;
			PendingReviewQuestionId.Reset();
			bUseAdaptiveQuestionOverride = false;
			if (bSelected)
			{
				RevisionQuestionId = Selected.Id;
				QuestionTopic = Selected.TopicName;
				QuestionSubtopic = Selected.Subtopic;
				QuestionType = Selected.Type;
				QuestionDifficulty = Selected.Difficulty;
				QuestionDiagramType = Selected.DiagramType;
				QuestionDiagram = Selected.Diagram;
				QuestionPrompt = Selected.Prompt;
				QuestionChoices.Reset();
				CorrectAnswerIndex = 0;
				// Guard the modulo: the data model permits choice-less (numeric-only) questions, and only
				// the strict question-bank validator currently keeps Choices at 4 here. A future content
				// or validator change must not turn this into a modulo-by-zero host crash.
				const int32 ChoiceCount = Selected.Answer.Choices.Num();
				if (ChoiceCount > 0)
				{
					const int32 ChoiceRotation = LocationSeed % ChoiceCount;
					bool bFoundCorrect = false;
					for (int32 Index = 0; Index < ChoiceCount; ++Index)
					{
						const int32 SourceIndex = (Index + ChoiceCount - ChoiceRotation) % ChoiceCount;
						QuestionChoices.Add(Selected.Answer.Choices[SourceIndex]);
						if (SourceIndex == Selected.Answer.CorrectChoiceIndex)
						{
							CorrectAnswerIndex = Index;
							bFoundCorrect = true;
						}
					}
					// Defensive: the question-bank validator keeps CorrectChoiceIndex in range, but a future
					// content/validator change must not silently mark shuffled choice 0 as correct. If the
					// source index never matched, warn and clamp to a valid (rotated) index instead of trusting 0.
					if (!bFoundCorrect)
					{
						UE_LOG(LogTemp, Warning, TEXT("[BHObjectiveStation] Question '%s' has CorrectChoiceIndex %d out of range [0,%d); clamping."), *Selected.Id, Selected.Answer.CorrectChoiceIndex, ChoiceCount);
						const int32 ClampedSource = FMath::Clamp(Selected.Answer.CorrectChoiceIndex, 0, ChoiceCount - 1);
						CorrectAnswerIndex = (ClampedSource + ChoiceRotation) % ChoiceCount;
					}
				}
				// Build the interactive drag tray for matching/ordering questions (no-op otherwise).
				BuildInteractiveArrangement(LocationSeed);
				bQuestionSolved = false;
				QuestionHint = Selected.Hint;
				QuestionFormula = Selected.Answer.Formula;
				QuestionNumericAnswer = Selected.Answer.NumericAnswer;
				QuestionNumericTolerance = Selected.Answer.NumericTolerance;
				QuestionExplanation = Selected.Explanation;
				QuestionFeedback = TEXT("");
				bQuestionFeedbackCorrect = false;
				RevisionTeamVotes.Reset();
				RevisionTeamPlayerIds.Reset();
				RevisionTeamSummary = TEXT("");
				PendingCorrectionCharacters.Reset();
				return;
			}
		}
	}

	// Proper (drag) questions in standard Hunt too: with some probability pull a matching/ordering
	// question from the full bank for this station's topic instead of the fixed multiple-choice set.
	// (Revision mode already biased its own pick above and returned.)
	if (BHWantsDragQuestion())
	{
		const FVector DragLocation = GetActorLocation();
		const int32 DragSeed = FMath::Abs(FMath::RoundToInt(DragLocation.X * 0.13f + DragLocation.Y * 0.07f + static_cast<int32>(StationType) * 71.0f));
		FBHRevisionQuestion DragSelected;
		if (FBHRevisionQuestionBank::SelectDragQuestion(FBHRevisionQuestionBank::TopicForStationType(StationType), EBHQuestionDifficulty::Easy, DragSeed, DragSelected))
		{
			RevisionQuestionsSolved = 0;
			RevisionQuestionsRequired = 1;
			RevisionQuestionStep = 0;
			ApplyRevisionQuestion(DragSelected, DragSeed);
			return;
		}
	}

	int32 QuestionCount = 0;
	const FBHStationQuestion* Bank = QuestionBankForType(StationType, QuestionCount);
	if (!Bank || QuestionCount <= 0)
	{
		RevisionQuestionId = TEXT("");
		RevisionQuestionsSolved = 0;
		RevisionQuestionsRequired = 1;
		RevisionQuestionStep = 0;
		QuestionTopic = TEXT("");
		QuestionSubtopic = TEXT("");
		QuestionType = EBHQuestionType::MultipleChoice;
		QuestionDifficulty = EBHQuestionDifficulty::Easy;
		QuestionDiagramType = EBHDiagramType::None;
		QuestionDiagram = FBHDiagramParams();
		QuestionPrompt = TEXT("");
		QuestionChoices.Reset();
		CorrectAnswerIndex = 0;
		bQuestionSolved = true;
		QuestionHint = TEXT("");
		QuestionFormula = TEXT("");
		QuestionNumericAnswer = 0.0f;
		QuestionNumericTolerance = 0.0f;
		QuestionExplanation = TEXT("");
		QuestionFeedback = TEXT("");
		bQuestionFeedbackCorrect = false;
		return;
	}

	const FVector Location = GetActorLocation();
	const int32 LocationSeed = FMath::Abs(FMath::RoundToInt(Location.X * 0.13f + Location.Y * 0.07f + static_cast<int32>(StationType) * 31.0f));
	const FBHStationQuestion& Selected = Bank[LocationSeed % QuestionCount];
	RevisionQuestionId = TEXT("");
	RevisionQuestionsSolved = 0;
	RevisionQuestionsRequired = 1;
	RevisionQuestionStep = 0;
	QuestionTopic = Selected.Topic;
	QuestionSubtopic = TEXT("");
	QuestionType = EBHQuestionType::MultipleChoice;
	QuestionDifficulty = EBHQuestionDifficulty::Easy;
	// Standard-play questions carry no numeric diagram params, but we still pick a
	// topic-appropriate diagram type so a relevant (generic) schematic renders at the
	// node. The data-driven labels are reserved for revision-mode questions above.
	switch (StationType)
	{
	case EBHObjectiveStationType::Terminal:
		QuestionDiagramType = EBHDiagramType::Circuit;
		break;
	case EBHObjectiveStationType::Antenna:
		QuestionDiagramType = EBHDiagramType::Wave;
		break;
	case EBHObjectiveStationType::Evidence:
		QuestionDiagramType = EBHDiagramType::EnergyChain;
		break;
	case EBHObjectiveStationType::Valve:
	default:
		QuestionDiagramType = EBHDiagramType::ForceArrows;
		break;
	}
	QuestionDiagram = FBHDiagramParams();
	QuestionPrompt = Selected.Prompt;
	QuestionChoices.Reset();
	const int32 StandardChoiceCount = UE_ARRAY_COUNT(Selected.Choices);
	const int32 ChoiceRotation = LocationSeed % StandardChoiceCount;
	CorrectAnswerIndex = 0;
	bool bFoundStandardCorrect = false;
	for (int32 Index = 0; Index < StandardChoiceCount; ++Index)
	{
		const int32 SourceIndex = (Index + StandardChoiceCount - ChoiceRotation) % StandardChoiceCount;
		QuestionChoices.Add(Selected.Choices[SourceIndex]);
		if (SourceIndex == Selected.CorrectIndex)
		{
			CorrectAnswerIndex = Index;
			bFoundStandardCorrect = true;
		}
	}
	// Defensive: never silently mark shuffled choice 0 as correct if CorrectIndex falls out of the
	// fixed choice array; warn and clamp to a valid (rotated) index instead.
	if (!bFoundStandardCorrect)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BHObjectiveStation] Standard question '%s' has CorrectIndex %d out of range [0,%d); clamping."), Selected.Prompt, Selected.CorrectIndex, StandardChoiceCount);
		const int32 ClampedSource = FMath::Clamp(Selected.CorrectIndex, 0, StandardChoiceCount - 1);
		CorrectAnswerIndex = (ClampedSource + ChoiceRotation) % StandardChoiceCount;
	}
	bQuestionSolved = false;
	QuestionHint = Selected.Hint;
	QuestionFormula = TEXT("");
	QuestionNumericAnswer = 0.0f;
	QuestionNumericTolerance = 0.0f;
	QuestionExplanation = Selected.Hint;
	QuestionFeedback = TEXT("");
	bQuestionFeedbackCorrect = false;
	RevisionTeamSummary = TEXT("");
}

FString ABHObjectiveStation::GetActionVerb() const
{
	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		return TEXT("Balance");
	case EBHObjectiveStationType::Terminal:
		return TEXT("Restore");
	case EBHObjectiveStationType::Antenna:
		return TEXT("Tune");
	case EBHObjectiveStationType::Evidence:
		return TEXT("Charge");
	default:
		return TEXT("Use");
	}
}

FString ABHObjectiveStation::GetStationName() const
{
	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		return TEXT("Counterweight Lock");
	case EBHObjectiveStationType::Terminal:
		return TEXT("Circuit Breaker");
	case EBHObjectiveStationType::Antenna:
		return TEXT("Signal Receiver");
	case EBHObjectiveStationType::Evidence:
		return TEXT("Energy Store");
	default:
		return TEXT("Station");
	}
}

void ABHObjectiveStation::OnRep_StationVisuals()
{
	ApplyStationVisuals();
}

void ABHObjectiveStation::ApplyStationVisuals()
{
	if (!Mesh)
	{
		return;
	}

	const TArray<UStaticMeshComponent*> Fixtures = { FixtureA, FixtureB, FixtureC, FixtureD, FixtureE };
	for (UStaticMeshComponent* Fixture : Fixtures)
	{
		BHPropVisuals::SetPartVisible(Fixture, false);
	}

	UMaterialInterface* BodyMaterial = BHPropVisuals::PaintedMetalMaterial();
	FLinearColor AccentColor(0.20f, 0.24f, 0.28f, 1.0f);
	if (bTeacherMirrorTrapNode)
	{
		BodyMaterial = BHPropVisuals::RustedMetalMaterial();
		AccentColor = FLinearColor(0.72f, 0.12f, 0.88f, 1.0f);
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -45.0f), FRotator::ZeroRotator, FVector(0.48f, 0.36f, 0.42f), true);
		ConfigureImportedStationPart(FixtureA, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_powerbtn.SM_powerbtn"), BHPropVisuals::SphereMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_PowerBTN.MI_PowerBTN"), BHPropVisuals::BasicMaterial(), FVector(34.0f, 0.0f, -12.0f), FRotator::ZeroRotator, FVector(0.34f));
		ConfigureImportedStationPart(FixtureB, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_panel.SM_panel"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_Panel.MI_Panel"), BHPropVisuals::PaintedMetalMaterial(), FVector(18.0f, 0.0f, -32.0f), FRotator::ZeroRotator, FVector(0.42f));
		BHPropVisuals::ConfigurePart(FixtureC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(37.0f, 0.0f, 12.0f), FRotator::ZeroRotator, FVector(0.014f, 0.32f, 0.035f));
	}
	else
	{
		switch (StationType)
		{
		case EBHObjectiveStationType::Valve:
			BodyMaterial = BHPropVisuals::RustedMetalMaterial();
			AccentColor = FLinearColor(0.66f, 0.16f, 0.08f, 1.0f);
			BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -48.0f), FRotator::ZeroRotator, FVector(0.82f, 0.56f, 0.62f), true);
			ConfigureImportedStationPart(FixtureA, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_pressureplate.SM_pressureplate"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_PressurePlate.MI_PressurePlate"), BHPropVisuals::DiamondPlateMaterial(), FVector(22.0f, 0.0f, -7.0f), FRotator::ZeroRotator, FVector(0.72f));
			ConfigureImportedStationPart(FixtureB, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_leverbase.SM_leverbase"), BHPropVisuals::CylinderMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_Lever.MI_Lever"), BHPropVisuals::RustedMetalMaterial(), FVector(40.0f, -25.0f, -3.0f), FRotator::ZeroRotator, FVector(0.68f));
			ConfigureImportedStationPart(FixtureC, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_lever.SM_lever"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_Lever.MI_Lever"), BHPropVisuals::RustedMetalMaterial(), FVector(40.0f, -25.0f, 22.0f), FRotator(0.0f, 0.0f, -28.0f), FVector(0.68f));
			BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(-38.0f, 26.0f, 10.0f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.44f));
			BHPropVisuals::ConfigurePart(FixtureE, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(-12.0f, 0.0f, 42.0f), FRotator(0.0f, 0.0f, -6.0f), FVector(0.76f, 0.060f, 0.055f));
			break;
		case EBHObjectiveStationType::Terminal:
			BodyMaterial = BHPropVisuals::PaintedMetalMaterial();
			AccentColor = FLinearColor(0.08f, 0.76f, 0.92f, 1.0f);
			BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -32.0f), FRotator::ZeroRotator, FVector(0.88f, 0.38f, 1.25f), true);
			ConfigureImportedStationPart(FixtureA, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_panel.SM_panel"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_Panel.MI_Panel"), BHPropVisuals::PaintedMetalMaterial(), FVector(46.0f, 0.0f, 5.0f), FRotator::ZeroRotator, FVector(0.62f));
			ConfigureImportedStationPart(FixtureB, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_powerbtn.SM_powerbtn"), BHPropVisuals::SphereMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_PowerBTN.MI_PowerBTN"), BHPropVisuals::BasicMaterial(), FVector(49.0f, -22.0f, 26.0f), FRotator::ZeroRotator, FVector(0.30f));
			ConfigureImportedStationPart(FixtureC, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_smallbattery.SM_smallbattery"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_SmallBattery.MI_SmallBattery"), BHPropVisuals::PaintedMetalMaterial(), FVector(-16.0f, 0.0f, 24.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.62f));
			BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(51.0f, 0.0f, -31.0f), FRotator::ZeroRotator, FVector(0.016f, 0.30f, 0.050f));
			BHPropVisuals::ConfigurePart(FixtureE, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(52.0f, 0.0f, -12.0f), FRotator::ZeroRotator, FVector(0.014f, 0.052f, 0.34f));
			break;
		case EBHObjectiveStationType::Antenna:
			BodyMaterial = BHPropVisuals::PaintedMetalMaterial();
			AccentColor = FLinearColor(0.60f, 0.84f, 0.92f, 1.0f);
			BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CylinderMesh(), BodyMaterial, FVector(0.0f, 0.0f, -8.0f), FRotator::ZeroRotator, FVector(0.075f, 0.075f, 1.72f), true);
			ConfigureImportedStationPart(FixtureA, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_panel.SM_panel"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel1.MI_ScreenPanel1"), BHPropVisuals::DiamondPlateMaterial(), FVector(0.0f, 0.0f, -92.0f), FRotator::ZeroRotator, FVector(0.54f));
			BHPropVisuals::ConfigurePart(FixtureB, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(34.0f, 0.0f, 42.0f), FRotator(0.0f, 68.0f, 0.0f), FVector(0.30f, 0.30f, 0.026f));
			BHPropVisuals::ConfigurePart(FixtureC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 62.0f), FRotator::ZeroRotator, FVector(0.05f, 0.68f, 0.04f));
			BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 84.0f), FRotator::ZeroRotator, FVector(0.09f));
			ConfigureImportedStationPart(FixtureE, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_powerbtnring.SM_powerbtnring"), BHPropVisuals::CylinderMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_PowerBTN.MI_PowerBTN"), BHPropVisuals::BasicMaterial(), FVector(30.0f, 0.0f, 18.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.40f));
			break;
		case EBHObjectiveStationType::Evidence:
			BodyMaterial = BHPropVisuals::RustedMetalMaterial();
			AccentColor = FLinearColor(0.96f, 0.46f, 0.12f, 1.0f);
			BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -52.0f), FRotator::ZeroRotator, FVector(0.82f, 0.60f, 0.74f), true);
			ConfigureImportedStationPart(FixtureA, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_generatorFront.SM_generatorFront"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_GeneratorFront.MI_GeneratorFront"), BHPropVisuals::PaintedMetalMaterial(), FVector(32.0f, 0.0f, -16.0f), FRotator::ZeroRotator, FVector(0.54f));
			ConfigureImportedStationPart(FixtureB, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_generatorSection.SM_generatorSection"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_GeneratorSection.MI_GeneratorSection"), BHPropVisuals::PaintedMetalMaterial(), FVector(0.0f, 0.0f, -16.0f), FRotator::ZeroRotator, FVector(0.54f));
			ConfigureImportedStationPart(FixtureC, TEXT("/Game/SmartBasicInterfaces/Meshes/SM_smallbattery.SM_smallbattery"), BHPropVisuals::CubeMesh(), TEXT("/Game/SmartBasicInterfaces/Materials/MI_SmallBattery.MI_SmallBattery"), BHPropVisuals::BasicMaterial(), FVector(0.0f, 28.0f, 18.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.64f));
			BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(37.0f, -18.0f, 12.0f), FRotator::ZeroRotator, FVector(0.11f, 0.07f, 0.18f));
			BHPropVisuals::ConfigurePart(FixtureE, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(43.0f, 0.0f, -24.0f), FRotator::ZeroRotator, FVector(0.018f, 0.24f, 0.08f));
			break;
		default:
			BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -35.0f), FRotator::ZeroRotator, FVector(0.75f, 0.55f, 1.0f), true);
			break;
		}
	}

	for (UStaticMeshComponent* Fixture : Fixtures)
	{
		if (Fixture && Fixture->GetStaticMesh())
		{
			BHPropVisuals::SetPartVisible(Fixture, true);
		}
	}

	if (!bDirectorActive)
	{
		AccentColor = FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
	}
	else if (!bQuestionSolved && QuestionChoices.Num() > 0)
	{
		AccentColor = FLinearColor(0.86f, 0.50f, 0.10f, 1.0f);
	}
	if (bCompleted)
	{
		AccentColor = FLinearColor(0.10f, 0.72f, 0.34f, 1.0f);
	}

	const FLinearColor DarkInset(0.016f, 0.018f, 0.020f, 1.0f);
	const FLinearColor PaperColor(0.82f, 0.78f, 0.64f, 1.0f);
	const FLinearColor IndicatorColor = bDirectorActive ? AccentColor : FLinearColor(0.025f, 0.025f, 0.025f, 1.0f);
	const float IndicatorEmissive = bDirectorActive ? 2.0f : 0.0f;

	if (bTeacherMirrorTrapNode)
	{
		BHPropVisuals::TintPart(FixtureA, AccentColor, IndicatorEmissive);
		BHPropVisuals::TintPart(FixtureB, DarkInset);
		BHPropVisuals::TintPart(FixtureC, AccentColor, IndicatorEmissive);
	}
	else
	{
		switch (StationType)
		{
		case EBHObjectiveStationType::Valve:
			BHPropVisuals::TintPart(FixtureA, FLinearColor(0.20f, 0.18f, 0.15f, 1.0f));
			BHPropVisuals::TintPart(FixtureB, FLinearColor(0.16f, 0.08f, 0.04f, 1.0f));
			BHPropVisuals::TintPart(FixtureC, FLinearColor(0.16f, 0.08f, 0.04f, 1.0f));
			BHPropVisuals::TintPart(FixtureD, AccentColor, bDirectorActive ? 0.7f : 0.0f);
			BHPropVisuals::TintPart(FixtureE, FLinearColor(0.10f, 0.08f, 0.06f, 1.0f));
			break;
		case EBHObjectiveStationType::Terminal:
			BHPropVisuals::TintPart(FixtureA, AccentColor, IndicatorEmissive * 0.65f);
			BHPropVisuals::TintPart(FixtureB, AccentColor, IndicatorEmissive);
			BHPropVisuals::TintPart(FixtureC, FLinearColor(0.12f, 0.82f, 0.42f, 1.0f), bDirectorActive ? 1.2f : 0.0f);
			BHPropVisuals::TintPart(FixtureD, DarkInset);
			BHPropVisuals::TintPart(FixtureE, AccentColor, bDirectorActive ? 1.3f : 0.0f);
			break;
		case EBHObjectiveStationType::Antenna:
			BHPropVisuals::TintPart(FixtureB, AccentColor, bDirectorActive ? 1.0f : 0.0f);
			BHPropVisuals::TintPart(FixtureC, FLinearColor(0.12f, 0.13f, 0.13f, 1.0f));
			BHPropVisuals::TintPart(FixtureD, AccentColor, IndicatorEmissive);
			BHPropVisuals::TintPart(FixtureE, AccentColor, bDirectorActive ? 1.4f : 0.0f);
			break;
		case EBHObjectiveStationType::Evidence:
			BHPropVisuals::TintPart(FixtureA, PaperColor);
			BHPropVisuals::TintPart(FixtureB, FLinearColor(0.40f, 0.34f, 0.28f, 1.0f));
			BHPropVisuals::TintPart(FixtureC, FLinearColor(0.32f, 1.0f, 0.58f, 1.0f), bDirectorActive ? 1.4f : 0.0f);
			BHPropVisuals::TintPart(FixtureD, AccentColor, bDirectorActive ? 2.6f : 0.0f);
			BHPropVisuals::TintPart(FixtureE, FLinearColor(0.88f, 0.62f, 0.12f, 1.0f));
			break;
		default:
			break;
		}
	}

	BHPropVisuals::ConfigurePart(StatusLight, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(52.0f, -25.0f, 36.0f), FRotator::ZeroRotator, FVector(0.060f));
	BHPropVisuals::ConfigurePart(ProgressBack, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(52.0f, 0.0f, -72.0f), FRotator::ZeroRotator, FVector(0.018f, 0.35f, 0.035f));
	const float FillProgress = FMath::Clamp(WorkProgress, 0.0f, 1.0f);
	const float FillScaleY = FMath::Max(0.012f, 0.33f * FillProgress);
	BHPropVisuals::ConfigurePart(ProgressFill, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(53.5f, -16.5f + FillProgress * 16.5f, -72.0f), FRotator::ZeroRotator, FVector(0.020f, FillScaleY, 0.045f));
	BHPropVisuals::SetPartVisible(ProgressFill, bDirectorActive && !bCompleted && FillProgress > 0.01f);
	BHPropVisuals::TintPart(StatusLight, IndicatorColor, IndicatorEmissive);
	BHPropVisuals::TintPart(ProgressBack, DarkInset);
	BHPropVisuals::TintPart(ProgressFill, bCompleted ? FLinearColor(0.10f, 0.76f, 0.34f, 1.0f) : AccentColor, bDirectorActive ? 1.4f : 0.0f);
}

#include "BHObjectiveStation.h"
#include "BHPropVisuals.h"
#include "BHCharacter.h"
#include "BHGameMode.h"
#include "BHGameState.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHRevisionQuestionBank.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
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
	RevisionQuestionsSolved = 0;
	RevisionQuestionsRequired = 1;
	RevisionQuestionStep = 0;
	RevisionCounterType = EBHRevisionCounterNodeType::None;
	bTeacherMirrorTrapNode = false;
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
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		Workers.Empty();
		SetActorTickEnabled(false);
		return;
	}

	for (auto It = Workers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
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
	WorkProgress = FMath::Clamp(WorkProgress + DeltaSeconds * Workers.Num() / Rate, 0.0f, 1.0f);
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
	DOREPLIFETIME(ABHObjectiveStation, CorrectAnswerIndex);
	DOREPLIFETIME(ABHObjectiveStation, bQuestionSolved);
	DOREPLIFETIME(ABHObjectiveStation, QuestionHint);
	DOREPLIFETIME(ABHObjectiveStation, QuestionFeedback);
	DOREPLIFETIME(ABHObjectiveStation, bQuestionFeedbackCorrect);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionId);
	DOREPLIFETIME(ABHObjectiveStation, QuestionType);
	DOREPLIFETIME(ABHObjectiveStation, QuestionDifficulty);
	DOREPLIFETIME(ABHObjectiveStation, QuestionDiagramType);
	DOREPLIFETIME(ABHObjectiveStation, QuestionSubtopic);
	DOREPLIFETIME(ABHObjectiveStation, QuestionFormula);
	DOREPLIFETIME(ABHObjectiveStation, QuestionExplanation);
	DOREPLIFETIME(ABHObjectiveStation, RevisionTeamSummary);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionsSolved);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionsRequired);
	DOREPLIFETIME(ABHObjectiveStation, RevisionQuestionStep);
	DOREPLIFETIME(ABHObjectiveStation, RevisionCounterType);
	DOREPLIFETIME(ABHObjectiveStation, bTeacherMirrorTrapNode);
}

bool ABHObjectiveStation::CanInteract_Implementation(ABHCharacter* Character) const
{
	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	if (bTeacherMirrorTrapNode)
	{
		return bDirectorActive && !bCompleted && BHPS && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && BHPS->IsAliveSurvivor();
	}
	const bool bQuestionReady = bQuestionSolved || QuestionChoices.Num() == 0;
	return bDirectorActive && !bCompleted && bQuestionReady && BHPS && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && BHPS->IsAliveSurvivor();
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
		Workers.Add(Character);
		SetActorTickEnabled(true);
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Now - LastNoiseTime > 2.0f)
		{
			LastNoiseTime = Now;
			if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
			{
				BHGM->NotifyLoudNoise(GetActorLocation(), GetStationName().ToLower());
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

	if (!BHPS->IsAliveSurvivor())
	{
		return FText::FromString(TEXT("Survivor Objective"));
	}

	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
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

void ABHObjectiveStation::Configure(EBHObjectiveStationType NewStationType)
{
	StationType = NewStationType;
	bTeacherMirrorTrapNode = false;
	ConfigureQuestion();
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
	}
	else
	{
		QuestionFeedback = TEXT("");
		RevisionQuestionsSolved = 0;
		RevisionQuestionStep = 0;
		RevisionQuestionsRequired = 1;
	}
	ApplyStationVisuals();
}

bool ABHObjectiveStation::SubmitAnswer(ABHCharacter* Character, int32 AnswerIndex)
{
	if (!HasAuthority())
	{
		return false;
	}

	const ABHPlayerState* BHPS = Character ? Character->GetBHPlayerState() : nullptr;
	const ABHGameState* BHGS = GetWorld() ? GetWorld()->GetGameState<ABHGameState>() : nullptr;
	ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr;
	if (!bDirectorActive || bCompleted || !BHPS || !BHPS->IsAliveSurvivor() || !BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
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
				for (ABHCharacter* Participant : RevisionParticipants)
				{
					const bool bCorrection = PendingCorrectionCharacters.Contains(Participant);
					BHGM->RecordRevisionAnswer(Participant, RevisionQuestion, true, bCorrection);
					PendingCorrectionCharacters.Remove(Participant);
				}
			}

			RevisionQuestionsRequired = FMath::Max(1, RevisionQuestionsRequired);
			RevisionQuestionsSolved = FMath::Clamp(RevisionQuestionsSolved + 1, 0, RevisionQuestionsRequired);
			bRevisionNodeUnlocked = RevisionQuestionsSolved >= RevisionQuestionsRequired;
			WorkProgress = FMath::Max(WorkProgress, bRevisionNodeUnlocked ? 0.72f : 0.12f + 0.48f * (static_cast<float>(RevisionQuestionsSolved) / static_cast<float>(RevisionQuestionsRequired)));
			RevisionTeamVotes.Reset();
			RevisionTeamPlayerIds.Reset();
			RevisionTeamSummary = TEXT("");
		}

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
				BHGM->RecordRevisionAnswer(Participant, RevisionQuestion, false, false);
				PendingCorrectionCharacters.Add(Participant);
			}
		}
	}

	QuestionFeedback = bActiveRevisionMode
		? FString::Printf(TEXT("Wrong. Hint: %s Correction: %s"), *QuestionHint, *QuestionExplanation)
		: (QuestionHint.IsEmpty()
			? TEXT("Wrong. The room got quieter.")
			: FString::Printf(TEXT("Wrong. Hint: %s"), *QuestionHint));
	bQuestionFeedbackCorrect = false;
	WorkProgress = FMath::Max(0.0f, WorkProgress - 0.18f);
	if (Character)
	{
		Character->AddFear(15.0f);
		Character->AddDread(22.0f);
		Character->ApplyDetentionMark(38.0f);
	}
	if (PC)
	{
		PC->ClientShowStatusMessage(TEXT("Wrong answer. The Teacher heard that."), 3.25f);
	}
	if (ABHGameMode* BHGM = GetWorld()->GetAuthGameMode<ABHGameMode>())
	{
		BHGM->NotifyLoudNoise(GetActorLocation(), TEXT("wrong answer"));
	}

	return false;
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

void ABHObjectiveStation::ConfigureQuestion()
{
	if (ABHGameMode* BHGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABHGameMode>() : nullptr)
	{
		if (BHGM->IsRevisionMode())
		{
			RevisionQuestionsRequired = FMath::Max(1, RevisionQuestionsRequired);
			const EBHPhysicsTopic Topic = FBHRevisionQuestionBank::TopicForStationType(StationType);
			FBHRevisionQuestion Selected;
			const FVector Location = GetActorLocation();
			const int32 LocationSeed = FMath::Abs(FMath::RoundToInt(Location.X * 0.13f + Location.Y * 0.07f + static_cast<int32>(StationType) * 131.0f + RevisionQuestionStep * 911.0f + FMath::RandRange(0, 100000)));
			if (FBHRevisionQuestionBank::SelectQuestion(Topic, BHGM->GetRevisionDifficultyMix(), LocationSeed, BHGM->GetRevisionWeakTopics(), Selected))
			{
				RevisionQuestionId = Selected.Id;
				QuestionTopic = Selected.TopicName;
				QuestionSubtopic = Selected.Subtopic;
				QuestionType = Selected.Type;
				QuestionDifficulty = Selected.Difficulty;
				QuestionDiagramType = Selected.DiagramType;
				QuestionPrompt = Selected.Prompt;
				QuestionChoices.Reset();
				const int32 ChoiceRotation = LocationSeed % Selected.Answer.Choices.Num();
				CorrectAnswerIndex = 0;
				for (int32 Index = 0; Index < Selected.Answer.Choices.Num(); ++Index)
				{
					const int32 SourceIndex = (Index + Selected.Answer.Choices.Num() - ChoiceRotation) % Selected.Answer.Choices.Num();
					QuestionChoices.Add(Selected.Answer.Choices[SourceIndex]);
					if (SourceIndex == Selected.Answer.CorrectChoiceIndex)
					{
						CorrectAnswerIndex = Index;
					}
				}
				bQuestionSolved = false;
				QuestionHint = Selected.Hint;
				QuestionFormula = Selected.Answer.Formula;
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
		QuestionPrompt = TEXT("");
		QuestionChoices.Reset();
		CorrectAnswerIndex = 0;
		bQuestionSolved = true;
		QuestionHint = TEXT("");
		QuestionFormula = TEXT("");
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
	QuestionDiagramType = EBHDiagramType::None;
	QuestionPrompt = Selected.Prompt;
	QuestionChoices.Reset();
	const int32 ChoiceRotation = LocationSeed % UE_ARRAY_COUNT(Selected.Choices);
	CorrectAnswerIndex = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Selected.Choices); ++Index)
	{
		const int32 SourceIndex = (Index + UE_ARRAY_COUNT(Selected.Choices) - ChoiceRotation) % UE_ARRAY_COUNT(Selected.Choices);
		QuestionChoices.Add(Selected.Choices[SourceIndex]);
		if (SourceIndex == Selected.CorrectIndex)
		{
			CorrectAnswerIndex = Index;
		}
	}
	bQuestionSolved = false;
	QuestionHint = Selected.Hint;
	QuestionFormula = TEXT("");
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
		return TEXT("Turn");
	case EBHObjectiveStationType::Terminal:
		return TEXT("Hack");
	case EBHObjectiveStationType::Antenna:
		return TEXT("Tune");
	case EBHObjectiveStationType::Evidence:
		return TEXT("Burn");
	default:
		return TEXT("Use");
	}
}

FString ABHObjectiveStation::GetStationName() const
{
	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		return TEXT("Valve");
	case EBHObjectiveStationType::Terminal:
		return TEXT("Terminal");
	case EBHObjectiveStationType::Antenna:
		return TEXT("Antenna");
	case EBHObjectiveStationType::Evidence:
		return TEXT("Evidence");
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
	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		BodyMaterial = BHPropVisuals::RustedMetalMaterial();
		AccentColor = FLinearColor(0.56f, 0.10f, 0.06f, 1.0f);
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CylinderMesh(), BodyMaterial, FVector(0.0f, 0.0f, -36.0f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 1.18f), true);
		BHPropVisuals::ConfigurePart(FixtureA, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(38.0f, 0.0f, 25.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.34f, 0.34f, 0.035f));
		BHPropVisuals::ConfigurePart(FixtureB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(41.5f, 0.0f, 25.0f), FRotator::ZeroRotator, FVector(0.022f, 0.58f, 0.035f));
		BHPropVisuals::ConfigurePart(FixtureC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(41.5f, 0.0f, 25.0f), FRotator::ZeroRotator, FVector(0.022f, 0.035f, 0.58f));
		BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::CylinderMesh(), BHPropVisuals::RustedMetalMaterial(), FVector(0.0f, 0.0f, 24.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(0.11f, 0.11f, 0.64f));
		break;
	case EBHObjectiveStationType::Terminal:
		BodyMaterial = BHPropVisuals::PaintedMetalMaterial();
		AccentColor = FLinearColor(0.08f, 0.76f, 0.92f, 1.0f);
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -32.0f), FRotator::ZeroRotator, FVector(0.88f, 0.36f, 1.25f), true);
		BHPropVisuals::ConfigurePart(FixtureA, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(47.0f, 0.0f, 6.0f), FRotator::ZeroRotator, FVector(0.020f, 0.24f, 0.30f));
		BHPropVisuals::ConfigurePart(FixtureB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(48.0f, 0.0f, -31.0f), FRotator::ZeroRotator, FVector(0.018f, 0.24f, 0.13f));
		BHPropVisuals::ConfigurePart(FixtureC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(50.0f, -12.0f, -31.0f), FRotator::ZeroRotator, FVector(0.016f, 0.045f, 0.040f));
		BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(50.0f, 0.0f, -31.0f), FRotator::ZeroRotator, FVector(0.016f, 0.045f, 0.040f));
		BHPropVisuals::ConfigurePart(FixtureE, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(50.0f, 12.0f, -31.0f), FRotator::ZeroRotator, FVector(0.016f, 0.045f, 0.040f));
		break;
	case EBHObjectiveStationType::Antenna:
		BodyMaterial = BHPropVisuals::PaintedMetalMaterial();
		AccentColor = FLinearColor(0.74f, 0.78f, 0.52f, 1.0f);
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CylinderMesh(), BodyMaterial, FVector(0.0f, 0.0f, -8.0f), FRotator::ZeroRotator, FVector(0.075f, 0.075f, 1.72f), true);
		BHPropVisuals::ConfigurePart(FixtureA, BHPropVisuals::CubeMesh(), BHPropVisuals::DiamondPlateMaterial(), FVector(0.0f, 0.0f, -92.0f), FRotator::ZeroRotator, FVector(0.54f, 0.54f, 0.13f));
		BHPropVisuals::ConfigurePart(FixtureB, BHPropVisuals::CylinderMesh(), BHPropVisuals::BasicMaterial(), FVector(34.0f, 0.0f, 42.0f), FRotator(0.0f, 68.0f, 0.0f), FVector(0.26f, 0.26f, 0.030f));
		BHPropVisuals::ConfigurePart(FixtureC, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 62.0f), FRotator::ZeroRotator, FVector(0.05f, 0.62f, 0.04f));
		BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 0.0f, 84.0f), FRotator::ZeroRotator, FVector(0.09f));
		break;
	case EBHObjectiveStationType::Evidence:
		BodyMaterial = BHPropVisuals::RustedMetalMaterial();
		AccentColor = FLinearColor(0.92f, 0.28f, 0.08f, 1.0f);
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -52.0f), FRotator::ZeroRotator, FVector(0.78f, 0.58f, 0.72f), true);
		BHPropVisuals::ConfigurePart(FixtureA, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, -9.0f, -8.0f), FRotator(0.0f, 0.0f, -4.0f), FVector(0.54f, 0.20f, 0.035f));
		BHPropVisuals::ConfigurePart(FixtureB, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(0.0f, 13.0f, -4.0f), FRotator(0.0f, 0.0f, 5.0f), FVector(0.50f, 0.20f, 0.035f));
		BHPropVisuals::ConfigurePart(FixtureC, BHPropVisuals::SphereMesh(), BHPropVisuals::BasicMaterial(), FVector(35.0f, 0.0f, -4.0f), FRotator::ZeroRotator, FVector(0.11f, 0.07f, 0.18f));
		BHPropVisuals::ConfigurePart(FixtureD, BHPropVisuals::CubeMesh(), BHPropVisuals::BasicMaterial(), FVector(42.0f, 0.0f, -24.0f), FRotator::ZeroRotator, FVector(0.018f, 0.20f, 0.08f));
		break;
	default:
		BHPropVisuals::ConfigurePart(Mesh, BHPropVisuals::CubeMesh(), BodyMaterial, FVector(0.0f, 0.0f, -35.0f), FRotator::ZeroRotator, FVector(0.75f, 0.55f, 1.0f), true);
		break;
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

	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		BHPropVisuals::TintPart(FixtureA, AccentColor);
		BHPropVisuals::TintPart(FixtureB, FLinearColor(0.12f, 0.04f, 0.035f, 1.0f));
		BHPropVisuals::TintPart(FixtureC, FLinearColor(0.12f, 0.04f, 0.035f, 1.0f));
		break;
	case EBHObjectiveStationType::Terminal:
		BHPropVisuals::TintPart(FixtureA, AccentColor, IndicatorEmissive);
		BHPropVisuals::TintPart(FixtureB, DarkInset);
		BHPropVisuals::TintPart(FixtureC, FLinearColor(0.05f, 0.06f, 0.065f, 1.0f));
		BHPropVisuals::TintPart(FixtureD, FLinearColor(0.05f, 0.06f, 0.065f, 1.0f));
		BHPropVisuals::TintPart(FixtureE, FLinearColor(0.05f, 0.06f, 0.065f, 1.0f));
		break;
	case EBHObjectiveStationType::Antenna:
		BHPropVisuals::TintPart(FixtureB, AccentColor);
		BHPropVisuals::TintPart(FixtureC, FLinearColor(0.12f, 0.13f, 0.13f, 1.0f));
		BHPropVisuals::TintPart(FixtureD, AccentColor, IndicatorEmissive);
		break;
	case EBHObjectiveStationType::Evidence:
		BHPropVisuals::TintPart(FixtureA, PaperColor);
		BHPropVisuals::TintPart(FixtureB, PaperColor * 0.88f);
		BHPropVisuals::TintPart(FixtureC, AccentColor, bDirectorActive ? 2.6f : 0.0f);
		BHPropVisuals::TintPart(FixtureD, FLinearColor(0.88f, 0.62f, 0.12f, 1.0f));
		break;
	default:
		break;
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

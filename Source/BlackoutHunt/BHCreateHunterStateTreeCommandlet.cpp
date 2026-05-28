#include "BHCreateHunterStateTreeCommandlet.h"

#if WITH_EDITOR
#include "BHBotController.h"
#include "BHBotStateTreeNodes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeFactory.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BHCreateHunterStateTreeCommandlet)

UBHCreateHunterStateTreeCommandlet::UBHCreateHunterStateTreeCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Creates or refreshes BlackoutHunt's hunter StateTree proof asset.");
	HelpUsage = TEXT("BlackoutHuntEditor-Cmd.exe BlackoutHunt.uproject -run=BHCreateHunterStateTree");
}

int32 UBHCreateHunterStateTreeCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogTemp, Error, TEXT("BHCreateHunterStateTree requires an editor target."));
	return 1;
#else
	constexpr TCHAR PackagePath[] = TEXT("/Game/BlackoutHunt/AI/ST_BH_HunterAtmosphere");
	constexpr TCHAR AssetName[] = TEXT("ST_BH_HunterAtmosphere");

	UPackage* Package = CreatePackage(PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create package %s"), PackagePath);
		return 1;
	}

	UStateTree* StateTree = FindObject<UStateTree>(Package, AssetName);
	if (!StateTree)
	{
		UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();
		Factory->SetSchemaClass(UStateTreeAIComponentSchema::StaticClass());
		StateTree = Cast<UStateTree>(Factory->FactoryCreateNew(
			UStateTree::StaticClass(),
			Package,
			FName(AssetName),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn));
		if (!StateTree)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create StateTree asset %s"), PackagePath);
			return 1;
		}
		FAssetRegistryModule::AssetCreated(StateTree);
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (!EditorData)
	{
		UE_LOG(LogTemp, Error, TEXT("StateTree %s has no editor data."), PackagePath);
		return 1;
	}

	EditorData->Modify();
	EditorData->Evaluators.Reset();
	EditorData->GlobalTasks.Reset();
	EditorData->SubTrees.Reset();

	UStateTreeState& Root = EditorData->AddRootState();
	Root.Name = TEXT("Root");
	Root.Description = TEXT("BlackoutHunt proof StateTree for the hunter atmosphere pass.");
	Root.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
	Root.TasksCompletion = EStateTreeTaskCompletionType::All;

	auto AddDebugTask = [](UStateTreeState& State, const TCHAR* StateName)
	{
		TStateTreeEditorNode<FBHStateTreeDebugTask>& DebugTask = State.AddTask<FBHStateTreeDebugTask>();
		DebugTask.GetInstanceData().StateName = StateName;
	};

	auto AddStimulusEnterCondition = [](UStateTreeState& State, EBHBotStimulusType Type, float MaxAgeSeconds)
	{
		TStateTreeEditorNode<FBHStateTreeHasStimulusCondition>& Condition = State.AddEnterCondition<FBHStateTreeHasStimulusCondition>();
		Condition.GetInstanceData().StimulusType = Type;
		Condition.GetInstanceData().MaxAgeSeconds = MaxAgeSeconds;
	};

	auto AddStimulusTransition = [](UStateTreeState& FromState, const UStateTreeState& ToState, EBHBotStimulusType Type, float MaxAgeSeconds, EStateTreeTransitionPriority Priority)
	{
		FStateTreeTransition& Transition = FromState.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &ToState);
		Transition.Priority = Priority;
		TStateTreeEditorNode<FBHStateTreeHasStimulusCondition>& Condition = Transition.AddCondition<FBHStateTreeHasStimulusCondition>();
		Condition.GetInstanceData().StimulusType = Type;
		Condition.GetInstanceData().MaxAgeSeconds = MaxAgeSeconds;
	};

	auto AddFallbackPatrolTransition = [](UStateTreeState& FromState, const UStateTreeState& PatrolState)
	{
		FStateTreeTransition& Transition = FromState.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &PatrolState);
		Transition.Priority = EStateTreeTransitionPriority::Low;
	};

	auto AddIntentState = [&Root, &AddDebugTask](const TCHAR* StateName, EBHBotIntent Intent, float AcceptanceRadius)
	{
		UStateTreeState& State = Root.AddChildState(FName(StateName));
		State.SelectionBehavior = EStateTreeStateSelectionBehavior::TryEnterState;
		State.TasksCompletion = EStateTreeTaskCompletionType::All;
		State.bHasCustomTickRate = true;
		State.CustomTickRate = 0.25f;
		AddDebugTask(State, StateName);
		TStateTreeEditorNode<FBHStateTreeBotIntentTask>& IntentTask = State.AddTask<FBHStateTreeBotIntentTask>();
		IntentTask.GetInstanceData().Intent = Intent;
		IntentTask.GetInstanceData().AcceptanceRadius = AcceptanceRadius;
		IntentTask.GetInstanceData().bFinishImmediately = false;
		return &State;
	};

	UStateTreeState& Patrol = Root.AddChildState(TEXT("Patrol"));
	Patrol.SelectionBehavior = EStateTreeStateSelectionBehavior::TryEnterState;
	Patrol.TasksCompletion = EStateTreeTaskCompletionType::All;
	Patrol.bHasCustomTickRate = true;
	Patrol.CustomTickRate = 0.25f;
	AddDebugTask(Patrol, TEXT("Patrol"));
	TStateTreeEditorNode<FBHStateTreeBotPatrolTask>& PatrolTask = Patrol.AddTask<FBHStateTreeBotPatrolTask>();
	PatrolTask.GetInstanceData().AcceptanceRadius = 220.0f;

	UStateTreeState& Idle = Root.AddChildState(TEXT("Idle"));
	Idle.SelectionBehavior = EStateTreeStateSelectionBehavior::None;
	Idle.TasksCompletion = EStateTreeTaskCompletionType::All;
	AddDebugTask(Idle, TEXT("Idle"));

	UStateTreeState* InvestigateNoise = AddIntentState(TEXT("InvestigateNoise"), EBHBotIntent::InvestigateNoise, 190.0f);
	AddStimulusEnterCondition(*InvestigateNoise, EBHBotStimulusType::Noise, 8.0f);

	UStateTreeState* StalkLastSeen = AddIntentState(TEXT("StalkLastSeen"), EBHBotIntent::InvestigateLastSeen, 190.0f);
	AddStimulusEnterCondition(*StalkLastSeen, EBHBotStimulusType::Sight, 10.0f);

	UStateTreeState* ChaseVisibleSurvivor = AddIntentState(TEXT("ChaseVisibleSurvivor"), EBHBotIntent::Chase, 145.0f);
	AddStimulusEnterCondition(*ChaseVisibleSurvivor, EBHBotStimulusType::Sight, 1.25f);

	UStateTreeState* SearchLocker = AddIntentState(TEXT("SearchLocker"), EBHBotIntent::SearchLocker, 230.0f);
	AddStimulusEnterCondition(*SearchLocker, EBHBotStimulusType::Locker, 10.0f);

	UStateTreeState* AmbushObjective = AddIntentState(TEXT("AmbushObjective"), EBHBotIntent::AmbushObjective, 260.0f);
	AddStimulusEnterCondition(*AmbushObjective, EBHBotStimulusType::Objective, 10.0f);

	UStateTreeState& UsePower = Root.AddChildState(TEXT("UsePower"));
	UsePower.SelectionBehavior = EStateTreeStateSelectionBehavior::TryEnterState;
	UsePower.TasksCompletion = EStateTreeTaskCompletionType::All;
	AddDebugTask(UsePower, TEXT("UsePower"));
	TStateTreeEditorNode<FBHStateTreeBotActionTask>& PowerAction = UsePower.AddTask<FBHStateTreeBotActionTask>();
	PowerAction.GetInstanceData().Action = EBHStateTreeBotAction::Power;
	UsePower.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Patrol).Priority = EStateTreeTransitionPriority::Normal;

	UStateTreeState* RecoverFromStuck = AddIntentState(TEXT("RecoverFromStuck"), EBHBotIntent::Patrol, 260.0f);
	AddStimulusEnterCondition(*RecoverFromStuck, EBHBotStimulusType::Unreachable, 4.0f);

	TArray<UStateTreeState*> SwitchingStates;
	SwitchingStates.Append({ &Patrol, &Idle, InvestigateNoise, StalkLastSeen, ChaseVisibleSurvivor, SearchLocker, AmbushObjective, &UsePower, RecoverFromStuck });
	for (UStateTreeState* State : SwitchingStates)
	{
		if (!State)
		{
			continue;
		}
		AddStimulusTransition(*State, *ChaseVisibleSurvivor, EBHBotStimulusType::Sight, 1.25f, EStateTreeTransitionPriority::Critical);
		AddStimulusTransition(*State, *RecoverFromStuck, EBHBotStimulusType::Unreachable, 4.0f, EStateTreeTransitionPriority::High);
		AddStimulusTransition(*State, *SearchLocker, EBHBotStimulusType::Locker, 10.0f, EStateTreeTransitionPriority::High);
		AddStimulusTransition(*State, *InvestigateNoise, EBHBotStimulusType::Noise, 8.0f, EStateTreeTransitionPriority::High);
		AddStimulusTransition(*State, *StalkLastSeen, EBHBotStimulusType::Sight, 10.0f, EStateTreeTransitionPriority::Medium);
		AddStimulusTransition(*State, *AmbushObjective, EBHBotStimulusType::Objective, 10.0f, EStateTreeTransitionPriority::Normal);
		if (State != &Patrol)
		{
			AddFallbackPatrolTransition(*State, Patrol);
		}
	}

	UStateTreeEditingSubsystem::ValidateStateTree(StateTree);

	FStateTreeCompilerLog Log;
	const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(StateTree, Log);
	if (!bCompiled)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to compile %s"), PackagePath);
		return 1;
	}

	Package->MarkPackageDirty();
	StateTree->MarkPackageDirty();

	const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFileName), true);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.Error = GError;
	const bool bSaved = UPackage::SavePackage(Package, StateTree, *PackageFileName, SaveArgs);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save %s"), *PackageFileName);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("Created hunter StateTree proof asset: %s"), PackagePath);
	return 0;
#endif
}

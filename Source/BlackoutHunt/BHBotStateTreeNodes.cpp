#include "BHBotStateTreeNodes.h"

#include "BHBotController.h"
#include "AIController.h"
#include "Components/ActorComponent.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BHBotStateTreeNodes)

namespace
{
ABHBotController* BHGetBotController(AAIController* Controller, FStateTreeExecutionContext& Context)
{
	if (ABHBotController* BoundController = Cast<ABHBotController>(Controller))
	{
		return BoundController;
	}
	if (ABHBotController* OwnerController = Cast<ABHBotController>(Context.GetOwner()))
	{
		return OwnerController;
	}
	if (const UActorComponent* OwnerComponent = Cast<UActorComponent>(Context.GetOwner()))
	{
		return Cast<ABHBotController>(OwnerComponent->GetOwner());
	}
	return nullptr;
}

EBHBotIntent BHActionToIntent(EBHStateTreeBotAction Action)
{
	switch (Action)
	{
	case EBHStateTreeBotAction::Power:
		return EBHBotIntent::UsePower;
	case EBHStateTreeBotAction::TryCapture:
		return EBHBotIntent::Chase;
	case EBHStateTreeBotAction::DropTrap:
		return EBHBotIntent::DropTrap;
	case EBHStateTreeBotAction::Scan:
	default:
		return EBHBotIntent::UseScan;
	}
}
}

FBHStateTreeBotIntentTask::FBHStateTreeBotIntentTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
}

EStateTreeRunStatus FBHStateTreeBotIntentTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABHBotController* Controller = BHGetBotController(InstanceData.AIController, Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	const bool bAccepted = Controller->RunStateTreeIntent(InstanceData.Intent, InstanceData.TargetActor, InstanceData.Location, InstanceData.AcceptanceRadius);
	return bAccepted ? (InstanceData.bFinishImmediately ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running) : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FBHStateTreeBotIntentTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABHBotController* Controller = BHGetBotController(InstanceData.AIController, Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	const bool bAccepted = Controller->RunStateTreeIntent(InstanceData.Intent, InstanceData.TargetActor, InstanceData.Location, InstanceData.AcceptanceRadius);
	return bAccepted ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

#if WITH_EDITOR
FText FBHStateTreeBotIntentTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	const UEnum* IntentEnum = StaticEnum<EBHBotIntent>();
	const FString IntentName = IntentEnum && InstanceData
		? IntentEnum->GetNameStringByValue(static_cast<int64>(InstanceData->Intent))
		: TEXT("Intent");
	return FText::FromString(FString::Printf(TEXT("Run BH %s"), *IntentName));
}
#endif

FBHStateTreeBotPatrolTask::FBHStateTreeBotPatrolTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
}

EStateTreeRunStatus FBHStateTreeBotPatrolTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABHBotController* Controller = BHGetBotController(InstanceData.AIController, Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}
	return Controller->RunStateTreeIntent(EBHBotIntent::Patrol, nullptr, FVector::ZeroVector, InstanceData.AcceptanceRadius)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FBHStateTreeBotPatrolTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABHBotController* Controller = BHGetBotController(InstanceData.AIController, Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}
	return Controller->RunStateTreeIntent(EBHBotIntent::Patrol, nullptr, FVector::ZeroVector, InstanceData.AcceptanceRadius)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

FBHStateTreeBotActionTask::FBHStateTreeBotActionTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FBHStateTreeBotActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABHBotController* Controller = BHGetBotController(InstanceData.AIController, Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	return Controller->RunStateTreeIntent(BHActionToIntent(InstanceData.Action), InstanceData.TargetActor, InstanceData.Location, 180.0f)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

bool FBHStateTreeHasStimulusCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABHBotController* Controller = BHGetBotController(InstanceData.AIController, Context);
	if (!Controller)
	{
		return false;
	}

	FVector UnusedLocation = FVector::ZeroVector;
	return Controller->HasRecentStateTreeStimulus(InstanceData.StimulusType, InstanceData.MaxAgeSeconds, UnusedLocation);
}

FBHStateTreeDebugTask::FBHStateTreeDebugTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FBHStateTreeDebugTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABHBotController* Controller = BHGetBotController(InstanceData.AIController, Context);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	Controller->ReportStateTreeDebugState(InstanceData.StateName);
	return EStateTreeRunStatus::Succeeded;
}

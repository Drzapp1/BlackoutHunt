#pragma once

#include "CoreMinimal.h"
#include "BHTypes.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "BHBotStateTreeNodes.generated.h"

class AAIController;
class AActor;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;

USTRUCT(BlueprintType)
struct FBHStateTreeBotIntentTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBHBotIntent Intent = EBHBotIntent::Patrol;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AcceptanceRadius = 220.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bFinishImmediately = false;
};

USTRUCT(meta = (DisplayName = "Run BH Bot Intent", Category = "Blackout Hunt|AI"))
struct BLACKOUTHUNT_API FBHStateTreeBotIntentTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBHStateTreeBotIntentTaskInstanceData;

	FBHStateTreeBotIntentTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT(BlueprintType)
struct FBHStateTreeBotPatrolTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AcceptanceRadius = 220.0f;
};

USTRUCT(meta = (DisplayName = "Choose BH Patrol", Category = "Blackout Hunt|AI"))
struct BLACKOUTHUNT_API FBHStateTreeBotPatrolTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBHStateTreeBotPatrolTaskInstanceData;

	FBHStateTreeBotPatrolTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

UENUM(BlueprintType)
enum class EBHStateTreeBotAction : uint8
{
	Scan UMETA(DisplayName = "Scan"),
	Power UMETA(DisplayName = "Power"),
	TryCapture UMETA(DisplayName = "Try Capture"),
	DropTrap UMETA(DisplayName = "Drop Trap")
};

USTRUCT(BlueprintType)
struct FBHStateTreeBotActionTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBHStateTreeBotAction Action = EBHStateTreeBotAction::Scan;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Location = FVector::ZeroVector;
};

USTRUCT(meta = (DisplayName = "Run BH Bot Action", Category = "Blackout Hunt|AI"))
struct BLACKOUTHUNT_API FBHStateTreeBotActionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBHStateTreeBotActionTaskInstanceData;

	FBHStateTreeBotActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(BlueprintType)
struct FBHStateTreeHasStimulusConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBHBotStimulusType StimulusType = EBHBotStimulusType::Noise;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxAgeSeconds = 8.0f;
};

USTRUCT(meta = (DisplayName = "Has BH Bot Stimulus", Category = "Blackout Hunt|AI"))
struct BLACKOUTHUNT_API FBHStateTreeHasStimulusCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBHStateTreeHasStimulusConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT(BlueprintType)
struct FBHStateTreeDebugTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FString StateName;
};

USTRUCT(meta = (DisplayName = "Report BH Bot Debug State", Category = "Blackout Hunt|AI"))
struct BLACKOUTHUNT_API FBHStateTreeDebugTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBHStateTreeDebugTaskInstanceData;

	FBHStateTreeDebugTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

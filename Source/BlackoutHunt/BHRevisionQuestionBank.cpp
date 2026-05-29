#include "BHRevisionQuestionBank.h"

#include "Containers/Set.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include <initializer_list>

namespace
{
// Optional diagram parameters authored alongside a spec so the HUD can render the
// question's actual values. Default-initialized members let the ~430 existing rows that
// omit it compile unchanged (trailing aggregate init).
struct FRevisionDiagram
{
	float ValueA = 0.0f;
	float ValueB = 0.0f;
	float ValueC = 0.0f;
	float ValueD = 0.0f;
	const TCHAR* LabelA = nullptr;
	const TCHAR* LabelB = nullptr;
	const TCHAR* LabelC = nullptr;
	const TCHAR* LabelD = nullptr;
	const TCHAR* XAxis = nullptr;
	const TCHAR* YAxis = nullptr;
	float AngleOrShape = 0.0f;
	int32 ShapeVariant = 0;
	const TCHAR* ImageSoftPath = nullptr;
};

struct FRevisionSpec
{
	const TCHAR* Subtopic;
	const TCHAR* Prompt;
	const TCHAR* Choices[4];
	int32 CorrectIndex;
	const TCHAR* Hint;
	const TCHAR* CorrectionPrompt;
	const TCHAR* Explanation;
	EBHDiagramType DiagramType;
	const TCHAR* Formula;
	float NumericAnswer;
	float NumericTolerance;
	FRevisionDiagram Diagram = {};
};

TArray<FString> MakeChoices(std::initializer_list<const TCHAR*> Values)
{
	TArray<FString> Result;
	for (const TCHAR* Value : Values)
	{
		Result.Add(Value ? FString(Value) : FString());
	}
	return Result;
}

EBHQuestionDifficulty DifficultyFor(EBHQuestionType Type, int32 Index)
{
	switch (Type)
	{
	case EBHQuestionType::MultipleChoice:
		return Index < 5 ? EBHQuestionDifficulty::Easy : (Index < 9 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
	case EBHQuestionType::TrueFalse:
		return Index < 3 ? EBHQuestionDifficulty::Easy : (Index == 3 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
	case EBHQuestionType::Calculation:
		return Index < 4 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard;
	case EBHQuestionType::FormulaFill:
		return Index == 0 ? EBHQuestionDifficulty::Easy : (Index < 4 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
	case EBHQuestionType::GraphReading:
		// Indices 0-3 keep the original 1E/2M/1H shape; indices 4-9 are the new visual
		// questions added per build-half as 2 Easy / 2 Medium / 2 Hard.
		if (Index <= 3)
		{
			return Index == 0 ? EBHQuestionDifficulty::Easy : (Index < 3 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
		}
		return Index < 6 ? EBHQuestionDifficulty::Easy : (Index < 8 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
	case EBHQuestionType::DragDropMatching:
	case EBHQuestionType::Ordering:
		return Index == 0 ? EBHQuestionDifficulty::Easy : (Index == 1 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
	default:
		return EBHQuestionDifficulty::Easy;
	}
}

float MasteryWeightFor(EBHQuestionDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EBHQuestionDifficulty::Hard:
		return 1.5f;
	case EBHQuestionDifficulty::Medium:
		return 1.2f;
	case EBHQuestionDifficulty::Easy:
	default:
		return 1.0f;
	}
}

const TCHAR* TypeCode(EBHQuestionType Type)
{
	switch (Type)
	{
	case EBHQuestionType::MultipleChoice:
		return TEXT("mc");
	case EBHQuestionType::TrueFalse:
		return TEXT("tf");
	case EBHQuestionType::Calculation:
		return TEXT("calc");
	case EBHQuestionType::FormulaFill:
		return TEXT("skill");
	case EBHQuestionType::GraphReading:
		return TEXT("graph");
	case EBHQuestionType::DragDropMatching:
		return TEXT("match");
	case EBHQuestionType::Ordering:
		return TEXT("order");
	default:
		return TEXT("q");
	}
}

void AddSpecs(TArray<FBHRevisionQuestion>& Bank, EBHPhysicsTopic Topic, const TCHAR* Prefix, const TCHAR* TopicName, EBHQuestionType Type, const FRevisionSpec* Specs, int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FRevisionSpec& Spec = Specs[Index];
		FBHRevisionQuestion Question;
		Question.Id = FString::Printf(TEXT("%s_%s_%02d"), Prefix, TypeCode(Type), Index + 1);
		Question.Topic = Topic;
		Question.TopicName = TopicName;
		Question.Subtopic = Spec.Subtopic;
		Question.Difficulty = DifficultyFor(Type, Index);
		Question.Type = Type;
		Question.DiagramType = Spec.DiagramType;
		Question.Prompt = Spec.Prompt;
		Question.Answer.Choices = MakeChoices({Spec.Choices[0], Spec.Choices[1], Spec.Choices[2], Spec.Choices[3]});
		Question.Answer.CorrectChoiceIndex = FMath::Clamp(Spec.CorrectIndex, 0, Question.Answer.Choices.Num() - 1);
		Question.Answer.Formula = Spec.Formula ? FString(Spec.Formula) : FString();
		Question.Answer.NumericAnswer = Spec.NumericAnswer;
		Question.Answer.NumericTolerance = FMath::Max(0.0f, Spec.NumericTolerance);
		Question.Diagram.ValueA = Spec.Diagram.ValueA;
		Question.Diagram.ValueB = Spec.Diagram.ValueB;
		Question.Diagram.ValueC = Spec.Diagram.ValueC;
		Question.Diagram.ValueD = Spec.Diagram.ValueD;
		Question.Diagram.LabelA = Spec.Diagram.LabelA ? FString(Spec.Diagram.LabelA) : FString();
		Question.Diagram.LabelB = Spec.Diagram.LabelB ? FString(Spec.Diagram.LabelB) : FString();
		Question.Diagram.LabelC = Spec.Diagram.LabelC ? FString(Spec.Diagram.LabelC) : FString();
		Question.Diagram.LabelD = Spec.Diagram.LabelD ? FString(Spec.Diagram.LabelD) : FString();
		Question.Diagram.XAxis = Spec.Diagram.XAxis ? FString(Spec.Diagram.XAxis) : FString();
		Question.Diagram.YAxis = Spec.Diagram.YAxis ? FString(Spec.Diagram.YAxis) : FString();
		Question.Diagram.AngleOrShape = Spec.Diagram.AngleOrShape;
		Question.Diagram.ShapeVariant = Spec.Diagram.ShapeVariant;
		Question.Diagram.ImageSoftPath = Spec.Diagram.ImageSoftPath ? FString(Spec.Diagram.ImageSoftPath) : FString();
		Question.Hint = Spec.Hint;
		Question.CorrectionPrompt = Spec.CorrectionPrompt;
		Question.Explanation = Spec.Explanation;
		Question.MasteryWeight = MasteryWeightFor(Question.Difficulty);
		Bank.Add(Question);
	}
}

void BuildForces(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Speed"), TEXT("A student sprints from one locked classroom to another. What is speed?"), {TEXT("Distance travelled per unit time"), TEXT("Force per unit area"), TEXT("Energy transferred per second"), TEXT("Mass times velocity")}, 0, TEXT("Speed compares distance with time."), TEXT("Say the definition, then identify the units."), TEXT("Speed is distance travelled per unit time and is measured in m/s."), EBHDiagramType::MotionGraph, TEXT("speed = distance / time"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m"), .ShapeVariant=1}},
		{TEXT("Velocity"), TEXT("Which statement makes velocity different from speed?"), {TEXT("It includes direction"), TEXT("It uses kilograms"), TEXT("It cannot be negative"), TEXT("It is always constant")}, 0, TEXT("Velocity is a vector."), TEXT("Compare scalar and vector quantities."), TEXT("Velocity is speed in a given direction, so it has magnitude and direction."), EBHDiagramType::ForceArrows, TEXT("v = displacement / time"), 0.0f, 0.0f, {.ValueA=1.0f, .LabelA=TEXT("velocity")}},
		{TEXT("Acceleration"), TEXT("The Teacher speeds up down a corridor. Which equation gives acceleration?"), {TEXT("a = (v - u) / t"), TEXT("a = vt"), TEXT("a = m / F"), TEXT("a = d / t")}, 0, TEXT("Acceleration is change in velocity per second."), TEXT("Write the initial and final velocity symbols before choosing."), TEXT("Acceleration is change in velocity divided by time taken."), EBHDiagramType::VelocityGraph, TEXT("a = (v - u) / t"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=2}},
		{TEXT("Vectors"), TEXT("Which card should go in the vector tray?"), {TEXT("Force"), TEXT("Time"), TEXT("Energy"), TEXT("Distance")}, 0, TEXT("Vectors need direction."), TEXT("Name the magnitude and the direction."), TEXT("Force is a vector because it has magnitude and direction."), EBHDiagramType::ForceArrows, TEXT("vector = magnitude + direction"), 0.0f, 0.0f, {.ValueA=1.0f, .LabelA=TEXT("vector")}},
		{TEXT("Resultant force"), TEXT("Two students push a desk east with 30 N and 20 N. What is the resultant?"), {TEXT("50 N east"), TEXT("10 N east"), TEXT("50 N west"), TEXT("0 N")}, 0, TEXT("Forces in the same direction add."), TEXT("Draw both arrows in the same direction."), TEXT("30 N + 20 N = 50 N east."), EBHDiagramType::ForceArrows, TEXT("resultant = sum of forces"), 50.0f, 0.0f, {.ValueA=30.0f, .ValueB=20.0f, .LabelA=TEXT("30 N"), .LabelB=TEXT("20 N")}},
		{TEXT("Newton's first law"), TEXT("A trolley keeps rolling at constant velocity. What does that tell you?"), {TEXT("Resultant force is zero"), TEXT("There must be a forward resultant force"), TEXT("Its weight is zero"), TEXT("Friction is the only force")}, 0, TEXT("Constant velocity means no acceleration."), TEXT("Link constant velocity to resultant force."), TEXT("Newton's first law says an object stays at constant velocity unless acted on by a resultant force."), EBHDiagramType::ForceArrows, TEXT("resultant force = 0"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("push"), .LabelB=TEXT("friction")}},
		{TEXT("Weight"), TEXT("Where does weight act through for a balanced object diagram?"), {TEXT("Centre of gravity"), TEXT("Largest surface"), TEXT("Direction of motion"), TEXT("Front edge only")}, 0, TEXT("Weight is a gravitational force."), TEXT("Mark the point where the weight arrow starts."), TEXT("Weight acts through the centre of gravity."), EBHDiagramType::ForceArrows, TEXT("W = mg"), 0.0f, 0.0f, {.LabelC=TEXT("weight"), .LabelD=TEXT("contact"), .ShapeVariant=1}},
		{TEXT("Hooke's law"), TEXT("On a straight force-extension graph, what does the gradient represent?"), {TEXT("Spring constant"), TEXT("Momentum"), TEXT("Weight"), TEXT("Terminal speed")}, 0, TEXT("F = kx, so F divided by x gives k."), TEXT("Use gradient = rise/run on the graph."), TEXT("The gradient of a force-extension graph is the spring constant k."), EBHDiagramType::SpringGraph, TEXT("F = kx"), 0.0f, 0.0f, {.XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Terminal velocity"), TEXT("A falling prop reaches terminal velocity. Which forces are balanced?"), {TEXT("Weight and air resistance"), TEXT("Mass and speed"), TEXT("Friction and time"), TEXT("Momentum and distance")}, 0, TEXT("At terminal velocity the resultant force is zero."), TEXT("Draw the downward and upward force arrows equal length."), TEXT("At terminal velocity, weight equals air resistance so acceleration is zero."), EBHDiagramType::ForceArrows, TEXT("resultant force = 0"), 0.0f, 0.0f, {.LabelC=TEXT("weight"), .LabelD=TEXT("air resist"), .ShapeVariant=1}},
		{TEXT("Momentum"), TEXT("In a closed collision, what remains the same if no external resultant force acts?"), {TEXT("Total momentum"), TEXT("Each object's speed"), TEXT("Each object's kinetic energy"), TEXT("The impact time")}, 0, TEXT("This is conservation of momentum."), TEXT("Compare total before with total after."), TEXT("Total momentum before equals total momentum after in a closed system."), EBHDiagramType::ForceArrows, TEXT("p = mv"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("p before"), .LabelB=TEXT("p after")}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Displacement-time graphs"), TEXT("True or false: a horizontal displacement-time graph means the object is stationary."), {TEXT("True"), TEXT("False"), TEXT("Only during braking"), TEXT("Only if mass is zero")}, 0, TEXT("Gradient gives velocity."), TEXT("Point to the graph gradient."), TEXT("Horizontal gradient is zero, so velocity is zero."), EBHDiagramType::MotionGraph, TEXT("gradient = velocity"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Velocity-time graphs"), TEXT("True or false: the area under a velocity-time graph gives distance travelled."), {TEXT("True"), TEXT("False"), TEXT("Only for springs"), TEXT("Only for waves")}, 0, TEXT("Area represents velocity multiplied by time."), TEXT("Shade the area and name its unit."), TEXT("Velocity times time gives distance, so area under the graph is distance."), EBHDiagramType::VelocityGraph, TEXT("distance = area under v-t graph"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s")}},
		{TEXT("Mass and weight"), TEXT("True or false: mass and weight are the same quantity."), {TEXT("False"), TEXT("True"), TEXT("Only on Earth"), TEXT("Only in free fall")}, 0, TEXT("Mass is kg; weight is N."), TEXT("State both units before revoting."), TEXT("Mass is amount of matter in kg; weight is gravitational force in N."), EBHDiagramType::ForceArrows, TEXT("W = mg"), 0.0f, 0.0f, {.LabelC=TEXT("weight"), .LabelD=TEXT("contact"), .ShapeVariant=1}},
		{TEXT("Stopping distance"), TEXT("True or false: tiredness can increase thinking distance."), {TEXT("True"), TEXT("False"), TEXT("Only braking distance"), TEXT("Only vehicle mass")}, 0, TEXT("Thinking distance depends on reaction time."), TEXT("Separate thinking distance from braking distance."), TEXT("Tiredness slows reaction time, increasing thinking distance."), EBHDiagramType::MotionGraph, TEXT("stopping = thinking + braking"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Newton's third law"), TEXT("True or false: Newton's third-law forces act on the same object."), {TEXT("False"), TEXT("True"), TEXT("Only for gravity"), TEXT("Only in collisions")}, 0, TEXT("Action-reaction pairs act on different objects."), TEXT("Name both objects in the force pair."), TEXT("Equal and opposite forces act on different objects, not the same one."), EBHDiagramType::ForceArrows, TEXT("action = reaction on different objects"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("action"), .LabelB=TEXT("reaction")}}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Average speed"), TEXT("A student covers 120 m in 30 s. What is the average speed?"), {TEXT("4 m/s"), TEXT("0.25 m/s"), TEXT("90 m/s"), TEXT("150 m/s")}, 0, TEXT("Use total distance divided by total time."), TEXT("Write speed = distance / time and substitute."), TEXT("120 / 30 = 4 m/s."), EBHDiagramType::MotionGraph, TEXT("average speed = distance / time"), 4.0f, 0.1f, {.LabelA=TEXT("120 m"), .LabelB=TEXT("30 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("d / m"), .ShapeVariant=1}},
		{TEXT("Acceleration"), TEXT("A trolley speeds up from 4 m/s to 20 m/s in 8 s. Find acceleration."), {TEXT("2 m/s^2"), TEXT("3 m/s^2"), TEXT("4 m/s^2"), TEXT("16 m/s^2")}, 0, TEXT("Subtract initial velocity from final velocity."), TEXT("Show (20 - 4) / 8."), TEXT("a = (20 - 4) / 8 = 2 m/s^2."), EBHDiagramType::VelocityGraph, TEXT("a = (v - u) / t"), 2.0f, 0.1f, {.LabelA=TEXT("4 to 20 m/s"), .LabelB=TEXT("8 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=2}},
		{TEXT("Newton's second law"), TEXT("A 3 kg book trolley accelerates at 2 m/s^2. What force acts?"), {TEXT("6 N"), TEXT("1.5 N"), TEXT("5 N"), TEXT("9 N")}, 0, TEXT("Use F = ma."), TEXT("Multiply mass by acceleration."), TEXT("F = 3 x 2 = 6 N."), EBHDiagramType::ForceArrows, TEXT("F = ma"), 6.0f, 0.1f, {.LabelA=TEXT("3 kg"), .LabelB=TEXT("2 m/s2")}},
		{TEXT("Weight"), TEXT("A 6 kg backpack is on Earth. Use g = 10 N/kg. What is its weight?"), {TEXT("60 N"), TEXT("0.6 N"), TEXT("16 N"), TEXT("6 N")}, 0, TEXT("Weight equals mass times gravitational field strength."), TEXT("Substitute into W = mg."), TEXT("W = 6 x 10 = 60 N."), EBHDiagramType::ForceArrows, TEXT("W = mg"), 60.0f, 0.1f, {.LabelC=TEXT("6 kg"), .LabelD=TEXT("g 10 N/kg"), .ShapeVariant=1}},
		{TEXT("Motion equation"), TEXT("A cart starts from rest and accelerates at 2 m/s^2 for 9 m. What is v^2?"), {TEXT("36 m^2/s^2"), TEXT("18 m^2/s^2"), TEXT("11 m^2/s^2"), TEXT("4.5 m^2/s^2")}, 0, TEXT("Use v^2 = u^2 + 2as and u = 0."), TEXT("Show 0 + 2 x 2 x 9."), TEXT("v^2 = 0 + 2 x 2 x 9 = 36."), EBHDiagramType::VelocityGraph, TEXT("v^2 = u^2 + 2as"), 36.0f, 0.1f, {.LabelA=TEXT("u 0"), .LabelB=TEXT("2 m/s2, 9 m"), .XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=2}},
		{TEXT("Hooke's law"), TEXT("A spring has k = 40 N/m and extension 0.20 m. What force is needed?"), {TEXT("8 N"), TEXT("40.2 N"), TEXT("200 N"), TEXT("0.005 N")}, 0, TEXT("Use F = kx."), TEXT("Multiply 40 by 0.20."), TEXT("F = 40 x 0.20 = 8 N."), EBHDiagramType::SpringGraph, TEXT("F = kx"), 8.0f, 0.1f, {.LabelA=TEXT("k 40 N/m"), .LabelB=TEXT("x 0.20 m"), .XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Moments"), TEXT("A 12 N force acts 0.50 m from a pivot. What is the moment?"), {TEXT("6 Nm"), TEXT("24 Nm"), TEXT("12.5 Nm"), TEXT("0.042 Nm")}, 0, TEXT("Moment equals force times perpendicular distance."), TEXT("Show force x distance from pivot."), TEXT("moment = 12 x 0.50 = 6 Nm."), EBHDiagramType::MomentBeam, TEXT("moment = Fd"), 6.0f, 0.1f, {.ValueC=0.50f, .ValueD=12.0f, .LabelC=TEXT("0.50 m"), .LabelD=TEXT("12 N")}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Motion investigation"), TEXT("A trolley's displacement-time graph is curved and gets steeper each second. Which IGCSE conclusion is best?"), {TEXT("The trolley is accelerating because velocity is increasing"), TEXT("The trolley is stationary because the line is not straight"), TEXT("The trolley has constant velocity because displacement increases"), TEXT("The graph shows force, not motion")}, 0, TEXT("Gradient on a displacement-time graph gives velocity."), TEXT("Explain how the changing gradient proves the velocity changes."), TEXT("A steeper gradient means greater velocity; if the gradient increases, the trolley is accelerating."), EBHDiagramType::MotionGraph, TEXT("exam skill: interpret changing graph gradient"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m"), .ShapeVariant=2}},
		{TEXT("Resultant force"), TEXT("A box moves at constant speed along the floor while being pushed. Which answer would earn the explanation mark?"), {TEXT("Push force equals friction, so resultant force is zero"), TEXT("The push force is larger because the box is moving"), TEXT("There is no friction because speed is constant"), TEXT("Weight is larger than normal contact force")}, 0, TEXT("Constant speed means no acceleration."), TEXT("Use Newton's first law and name the balanced horizontal forces."), TEXT("Constant velocity means zero resultant force; the forward push is balanced by friction."), EBHDiagramType::ForceArrows, TEXT("exam skill: link constant velocity to balanced forces"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("push"), .LabelB=TEXT("friction")}},
		{TEXT("Free-fall reasoning"), TEXT("A student says a heavier ball falls faster because it has more weight. What is the best correction for an IGCSE answer?"), {TEXT("Both have the same acceleration if air resistance is negligible"), TEXT("The heavier ball has no air resistance"), TEXT("The lighter ball has no weight"), TEXT("Mass is the same as speed")}, 0, TEXT("Separate weight from acceleration due to gravity."), TEXT("Mention negligible air resistance and the same gravitational acceleration."), TEXT("With negligible air resistance, objects near Earth accelerate at the same rate even if their weights differ."), EBHDiagramType::ForceArrows, TEXT("exam skill: challenge a misconception"), 0.0f, 0.0f, {.LabelC=TEXT("weight"), .LabelD=TEXT("air resist"), .ShapeVariant=1}},
		{TEXT("Hooke's law practical"), TEXT("In a spring experiment, the last two points stop lying on the straight line. What should the student write?"), {TEXT("The limit of proportionality has been exceeded"), TEXT("The spring constant has become zero from the start"), TEXT("The extension readings must be ignored because all graphs curve"), TEXT("Mass and weight are the same quantity")}, 0, TEXT("Hooke's law only applies in the straight-line region."), TEXT("Name the limit and describe what changed on the graph."), TEXT("Once the force-extension graph curves, extension is no longer directly proportional to force; the limit of proportionality has been exceeded."), EBHDiagramType::SpringGraph, TEXT("exam skill: evaluate practical graph evidence"), 0.0f, 0.0f, {.XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Momentum safety"), TEXT("Two identical crash trolleys stop from the same speed. One has a soft bumper that increases collision time. Why is the force smaller?"), {TEXT("Same momentum change occurs over a longer time"), TEXT("The trolley has no momentum before impact"), TEXT("Increasing time increases acceleration"), TEXT("The bumper removes the trolley's mass")}, 0, TEXT("Use impulse: force depends on change in momentum per time."), TEXT("State that momentum change is similar but time is larger."), TEXT("For the same change in momentum, increasing the impact time reduces the average force."), EBHDiagramType::ForceArrows, TEXT("exam skill: explain safety using momentum change"), 0.0f, 0.0f, {.ValueA=1.0f, .LabelA=TEXT("impact F")}},
		{TEXT("Moments practical"), TEXT("A metre rule balances when clockwise and anticlockwise moments are equal. Which improvement gives a more reliable result?"), {TEXT("Read distances from the pivot to the line of action of each force"), TEXT("Measure from the table edge instead of the pivot"), TEXT("Ignore the rule's own weight in every setup"), TEXT("Use masses but never convert them to weights")}, 0, TEXT("Moments use perpendicular distance from the pivot."), TEXT("Name the line of action and the pivot."), TEXT("Reliable moment calculations need the perpendicular distance from the pivot to each force's line of action."), EBHDiagramType::MomentBeam, TEXT("exam skill: use perpendicular distance correctly"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueC=1.0f, .LabelA=TEXT("d1"), .LabelB=TEXT("F1"), .LabelC=TEXT("d2"), .LabelD=TEXT("F2")}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Displacement-time graph"), TEXT("On the projected displacement-time graph, what does the gradient show?"), {TEXT("Velocity"), TEXT("Weight"), TEXT("Energy"), TEXT("Mass")}, 0, TEXT("Gradient is change in displacement over time."), TEXT("Draw a tangent or rise/run triangle."), TEXT("The gradient of a displacement-time graph is velocity."), EBHDiagramType::MotionGraph, TEXT("gradient = velocity"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Velocity-time graph"), TEXT("A velocity-time graph has a horizontal section at 5 m/s for 4 s. What distance is that section?"), {TEXT("20 m"), TEXT("9 m"), TEXT("1.25 m"), TEXT("5 m")}, 0, TEXT("Area under the graph is distance."), TEXT("Use rectangle area: velocity x time."), TEXT("5 x 4 = 20 m."), EBHDiagramType::VelocityGraph, TEXT("distance = area under v-t graph"), 20.0f, 0.1f, {.LabelA=TEXT("5 m/s"), .LabelB=TEXT("4 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=1}},
		{TEXT("Force-extension graph"), TEXT("A straight force-extension graph rises 10 N over 0.25 m. What is k?"), {TEXT("40 N/m"), TEXT("2.5 N/m"), TEXT("10.25 N/m"), TEXT("0.025 N/m")}, 0, TEXT("Spring constant is gradient."), TEXT("Divide force by extension."), TEXT("k = 10 / 0.25 = 40 N/m."), EBHDiagramType::SpringGraph, TEXT("k = F / x"), 40.0f, 0.1f, {.LabelA=TEXT("10 N"), .LabelB=TEXT("0.25 m"), .XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Terminal velocity graph"), TEXT("On a speed-time graph for a falling object, what does a flat final section mean?"), {TEXT("Terminal velocity"), TEXT("Increasing acceleration"), TEXT("Negative mass"), TEXT("No air resistance")}, 0, TEXT("A flat speed-time graph means constant speed."), TEXT("Link constant speed to balanced forces."), TEXT("The object has reached terminal velocity: resultant force and acceleration are zero."), EBHDiagramType::VelocityGraph, TEXT("resultant force = 0"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=4}},
		{TEXT("Leverage"), TEXT("On the crowbar diagram the load sits 0.10 m from the pivot and your hands push 0.90 m from the pivot. Why is the load easier to lift?"), {TEXT("Your force acts over a larger distance, giving a bigger moment"), TEXT("Your force is larger than the load"), TEXT("The load loses its weight"), TEXT("The pivot removes friction")}, 0, TEXT("moment = force x distance from the pivot."), TEXT("Compare the two distances shown on the bar."), TEXT("The effort acts 0.90 m from the pivot while the load is only 0.10 m away, so the same force gives a much larger turning moment."), EBHDiagramType::MomentBeam, TEXT("moment = F x d"), 0.0f, 0.0f, {.ValueA=0.10f, .ValueC=0.90f, .LabelA=TEXT("load 0.10 m"), .LabelC=TEXT("effort 0.90 m")}},
		{TEXT("Resultant force"), TEXT("The free-body arrows show two people pushing a stalled car the same way with 250 N and 150 N. What resultant force is drawn?"), {TEXT("400 N forward"), TEXT("100 N forward"), TEXT("250 N forward"), TEXT("37500 N forward")}, 0, TEXT("Forces in the same direction add."), TEXT("Add the two arrow values."), TEXT("250 N + 150 N = 400 N in the direction of the push."), EBHDiagramType::ForceArrows, TEXT("resultant = sum of forces"), 400.0f, 0.1f, {.ValueA=250.0f, .ValueB=150.0f, .LabelA=TEXT("250 N"), .LabelB=TEXT("150 N")}},
		{TEXT("Velocity-time area"), TEXT("The velocity-time graph rises from 0 to 8 m/s over 5 s, with the region beneath the line shaded. What does the shaded area represent?"), {TEXT("Distance travelled"), TEXT("Acceleration"), TEXT("Force"), TEXT("Mass")}, 0, TEXT("Area under a velocity-time graph."), TEXT("Multiply velocity by time and check the units."), TEXT("The shaded area under a velocity-time graph equals the distance travelled."), EBHDiagramType::VelocityGraph, TEXT("distance = area under v-t graph"), 0.0f, 0.0f, {.ValueA=8.0f, .ValueB=5.0f, .LabelA=TEXT("8 m/s"), .LabelB=TEXT("5 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("v / m per s")}},
		{TEXT("Balancing moments"), TEXT("On the balanced beam a 300 N child sits 1.5 m left of the pivot and a 450 N child sits on the right. How far from the pivot is the 450 N child?"), {TEXT("1.0 m"), TEXT("2.25 m"), TEXT("0.5 m"), TEXT("6.75 m")}, 0, TEXT("Balanced: clockwise moment equals anticlockwise moment."), TEXT("Set 300 x 1.5 equal to 450 x d."), TEXT("300 x 1.5 = 450 Nm, so d = 450 / 450 = 1.0 m."), EBHDiagramType::MomentBeam, TEXT("F1 d1 = F2 d2"), 1.0f, 0.1f, {.ValueA=1.5f, .ValueB=300.0f, .ValueD=450.0f, .LabelA=TEXT("1.5 m"), .LabelB=TEXT("300 N"), .LabelD=TEXT("450 N")}},
		{TEXT("Spring constant"), TEXT("Two force-extension lines are plotted: spring P is steeper than spring Q. What does P's steeper line tell you?"), {TEXT("P has the larger spring constant"), TEXT("P has the smaller spring constant"), TEXT("P stores no energy"), TEXT("P has reached its limit")}, 0, TEXT("Gradient of a force-extension graph is the spring constant k."), TEXT("Steeper gradient means more force per metre."), TEXT("A steeper force-extension graph has the larger gradient, so spring P has the larger spring constant."), EBHDiagramType::SpringGraph, TEXT("k = gradient of F-x"), 0.0f, 0.0f, {.LabelA=TEXT("P steeper"), .XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Free-body diagram"), TEXT("A skydiver's free-body diagram shows weight 700 N downward and air resistance 500 N upward. What is the resultant force and the motion?"), {TEXT("200 N downward, still accelerating"), TEXT("200 N upward, slowing to a stop"), TEXT("0 N, terminal velocity reached"), TEXT("1200 N downward, free fall")}, 0, TEXT("Subtract the opposing forces."), TEXT("Find the difference and its direction."), TEXT("700 N - 500 N = 200 N downward, so the skydiver still accelerates downward and is not yet at terminal velocity."), EBHDiagramType::ForceArrows, TEXT("resultant = weight - drag"), 200.0f, 0.1f, {.ValueA=700.0f, .ValueB=500.0f, .LabelA=TEXT("W 700 N"), .LabelB=TEXT("drag 500 N")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Vectors and scalars"), TEXT("Choose the correct matching set for the sorting lockers."), {TEXT("Vector: force; Scalar: time"), TEXT("Vector: energy; Scalar: velocity"), TEXT("Vector: distance; Scalar: acceleration"), TEXT("Vector: speed; Scalar: force")}, 0, TEXT("Vectors have direction; scalars do not."), TEXT("Sort one quantity at a time."), TEXT("Force is a vector; time is a scalar."), EBHDiagramType::ForceArrows, TEXT("vector = magnitude + direction"), 0.0f, 0.0f, {.ValueA=1.0f, .LabelA=TEXT("vector")}},
		{TEXT("Stopping distances"), TEXT("Match the factor to the distance it mainly affects."), {TEXT("Tiredness -> thinking; icy road -> braking"), TEXT("Tiredness -> braking; icy road -> thinking"), TEXT("Mass -> thinking only; distraction -> braking only"), TEXT("Reaction time -> braking only; tyres -> thinking only")}, 0, TEXT("Reaction affects thinking; road grip affects braking."), TEXT("Split the journey into before braking and during braking."), TEXT("Tiredness worsens reaction time, while icy roads increase braking distance."), EBHDiagramType::MotionGraph, TEXT("stopping = thinking + braking"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Springs"), TEXT("Match each spring graph label correctly."), {TEXT("Straight section -> Hooke's law; bend -> beyond limit"), TEXT("Straight section -> fracture; bend -> balanced force"), TEXT("Gradient -> extension; x-axis -> spring constant"), TEXT("Origin -> terminal velocity; bend -> velocity")}, 0, TEXT("Hooke's law gives a straight line."), TEXT("Look for proportional force and extension."), TEXT("A straight force-extension graph obeys Hooke's law until the limit of proportionality."), EBHDiagramType::SpringGraph, TEXT("F = kx"), 0.0f, 0.0f, {.XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Newton's laws"), TEXT("Match the law to the example."), {TEXT("First: constant velocity; Second: F = ma"), TEXT("First: action-reaction; Second: no resultant force"), TEXT("Third: F = ma; Second: balanced forces"), TEXT("First: spring extension; Third: current split")}, 0, TEXT("First law is about no resultant force; second law is F = ma."), TEXT("Say each law in one sentence before choosing."), TEXT("Newton's first law links zero resultant force with constant velocity; the second law is F = ma."), EBHDiagramType::ForceArrows, TEXT("F = ma"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("force"), .LabelB=TEXT("reaction")}}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Terminal velocity"), TEXT("Order the falling object story."), {TEXT("Weight acts -> speed rises -> air resistance rises -> forces balance"), TEXT("Forces balance -> speed rises -> air resistance disappears -> weight acts"), TEXT("Air resistance starts largest -> speed zero -> acceleration rises -> balance breaks"), TEXT("Weight disappears -> speed falls -> air resistance rises -> terminal velocity")}, 0, TEXT("Air resistance grows as speed grows."), TEXT("Start with weight before much air resistance."), TEXT("At first weight is greater; speed and air resistance increase; finally forces balance."), EBHDiagramType::ForceArrows, TEXT("resultant force decreases to zero"), 0.0f, 0.0f, {.LabelC=TEXT("weight"), .LabelD=TEXT("air resist"), .ShapeVariant=1}},
		{TEXT("Stopping distance"), TEXT("Order the stopping distance chain."), {TEXT("See hazard -> react -> brakes act -> vehicle stops"), TEXT("Brakes act -> see hazard -> react -> vehicle stops"), TEXT("React -> vehicle stops -> see hazard -> brakes act"), TEXT("Vehicle stops -> react -> brakes act -> see hazard")}, 0, TEXT("Thinking happens before braking."), TEXT("Put reaction before brake force."), TEXT("Stopping distance is thinking distance followed by braking distance."), EBHDiagramType::MotionGraph, TEXT("stopping = thinking + braking"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Acceleration calculation"), TEXT("Order the method to find acceleration from u, v, and t."), {TEXT("Find v - u -> divide by t -> add m/s^2"), TEXT("Divide by t -> find v - u -> add kg"), TEXT("Multiply u by v -> divide by force -> add N"), TEXT("Find distance -> divide by mass -> add m/s")}, 0, TEXT("Acceleration is change in velocity per time."), TEXT("Calculate change in velocity first."), TEXT("Use a = (v - u) / t, with units m/s^2."), EBHDiagramType::VelocityGraph, TEXT("a = (v - u) / t"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s")}},
		{TEXT("Momentum collision"), TEXT("Order a conservation of momentum answer."), {TEXT("Write total before -> write total after -> set equal -> solve"), TEXT("Solve -> ignore direction -> write mass only -> set equal"), TEXT("Write force first -> choose graph -> divide by voltage -> solve"), TEXT("Set speeds to zero -> add weights -> solve")}, 0, TEXT("Use totals before and after."), TEXT("Keep direction signs consistent."), TEXT("In a closed system, total momentum before equals total momentum after."), EBHDiagramType::ForceArrows, TEXT("total p before = total p after"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("p before"), .LabelB=TEXT("p after")}}
	};

	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces"), TEXT("Forces and Motion"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces"), TEXT("Forces and Motion"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces"), TEXT("Forces and Motion"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces"), TEXT("Forces and Motion"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces"), TEXT("Forces and Motion"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces"), TEXT("Forces and Motion"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces"), TEXT("Forces and Motion"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

void BuildElectricity(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Current"), TEXT("What does current measure in the haunted lab circuit?"), {TEXT("Rate of flow of charge"), TEXT("Energy per charge"), TEXT("Charge stored in a wire"), TEXT("Resistance per metre")}, 0, TEXT("Current is charge per second."), TEXT("State the link between charge and time."), TEXT("Current is the rate of flow of charge and is measured in amperes."), EBHDiagramType::Circuit, TEXT("I = Q / t"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("I = ?"), .ShapeVariant=0}},
		{TEXT("Potential difference"), TEXT("What is potential difference?"), {TEXT("Work done per unit charge"), TEXT("Charge per unit time"), TEXT("Mass per unit volume"), TEXT("Distance per unit time")}, 0, TEXT("Voltage transfers energy to each coulomb."), TEXT("Use the unit J/C."), TEXT("Potential difference is work done or energy transferred per unit charge."), EBHDiagramType::Circuit, TEXT("V = E / Q"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("V = ?"), .ShapeVariant=0}},
		{TEXT("Resistance"), TEXT("Which equation defines resistance?"), {TEXT("R = V / I"), TEXT("R = VI"), TEXT("R = I / V"), TEXT("R = Q / t")}, 0, TEXT("Rearrange V = IR."), TEXT("Resistance is voltage divided by current."), TEXT("R = V / I, measured in ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Meters"), TEXT("Where should an ammeter be connected?"), {TEXT("In series"), TEXT("In parallel"), TEXT("Across the battery only"), TEXT("Outside the circuit")}, 0, TEXT("An ammeter measures current through a component."), TEXT("It must be in the path of the charge."), TEXT("An ammeter is connected in series."), EBHDiagramType::Circuit, TEXT("ammeter in series"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("ammeter A"), .ShapeVariant=0}},
		{TEXT("Parallel circuits"), TEXT("In a parallel lighting circuit, what is the same across each branch?"), {TEXT("Potential difference"), TEXT("Current in every branch"), TEXT("Resistance of every branch"), TEXT("Fuse rating")}, 0, TEXT("Parallel branches share the supply voltage."), TEXT("Compare branch voltage, not total current."), TEXT("Potential difference is the same across each parallel branch."), EBHDiagramType::Circuit, TEXT("parallel p.d. is same"), 0.0f, 0.0f, {.LabelA=TEXT("branch 1"), .LabelC=TEXT("branch 2"), .ShapeVariant=2}},
		{TEXT("Series circuits"), TEXT("In a series circuit, what happens to total resistance when another resistor is added?"), {TEXT("It increases"), TEXT("It decreases to zero"), TEXT("It always halves"), TEXT("It becomes current")}, 0, TEXT("Series resistances add."), TEXT("Add R1 + R2 + ..."), TEXT("Total resistance in series is the sum of each resistance."), EBHDiagramType::Circuit, TEXT("RT = R1 + R2 + ..."), 0.0f, 0.0f, {.LabelA=TEXT("R1"), .LabelC=TEXT("R2 added"), .ShapeVariant=1}},
		{TEXT("Components"), TEXT("A thermistor warms up near the boiler. What happens to its resistance?"), {TEXT("It decreases"), TEXT("It increases"), TEXT("It becomes infinite"), TEXT("It becomes a fuse")}, 0, TEXT("Thermistor resistance falls with temperature."), TEXT("Remember hot thermistor means easier current path."), TEXT("A thermistor's resistance decreases as temperature increases."), EBHDiagramType::IVGraph, TEXT("thermistor: hot -> lower R"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=1}},
		{TEXT("Safety"), TEXT("Which fuse rating is best for a device that normally uses 4 A?"), {TEXT("5 A"), TEXT("3 A"), TEXT("13 A"), TEXT("0 A")}, 0, TEXT("Choose slightly above normal current."), TEXT("Avoid a rating below normal current."), TEXT("A fuse should be rated slightly above normal current, so 5 A is best."), EBHDiagramType::Circuit, TEXT("fuse rating just above normal current"), 5.0f, 0.0f, {.LabelA=TEXT("4 A normal"), .LabelB=TEXT("fuse = ?"), .ShapeVariant=0}},
		{TEXT("Static electricity"), TEXT("When an insulator gains electrons, what charge does it have?"), {TEXT("Negative"), TEXT("Positive"), TEXT("Neutral forever"), TEXT("Alternating")}, 0, TEXT("Electrons are negative."), TEXT("Track whether electrons are added or removed."), TEXT("Gaining electrons gives an object a negative charge."), EBHDiagramType::StaticCharge, TEXT("electron charge = -1"), 0.0f, 0.0f, {.LabelA=TEXT("gains e-")}},
		{TEXT("Earthing"), TEXT("Why does an earth wire help protect a metal-cased appliance?"), {TEXT("It provides a low-resistance fault path"), TEXT("It stores extra voltage"), TEXT("It makes current zero in normal use"), TEXT("It replaces the live wire")}, 0, TEXT("Fault current goes safely to Earth."), TEXT("Explain how the fuse then disconnects."), TEXT("The earth wire provides a low-resistance path so a large fault current blows the fuse."), EBHDiagramType::Circuit, TEXT("earth wire -> large fault current -> fuse blows"), 0.0f, 0.0f, {.LabelA=TEXT("metal case"), .LabelB=TEXT("earth wire"), .ShapeVariant=0}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Current direction"), TEXT("True or false: conventional current is opposite to electron flow."), {TEXT("True"), TEXT("False"), TEXT("Only in batteries"), TEXT("Only in AC")}, 0, TEXT("Conventional current is positive-charge direction."), TEXT("Draw electron flow, then conventional current."), TEXT("Electrons are negative, so electron flow is opposite conventional current."), EBHDiagramType::Circuit, TEXT("conventional current opposite electron flow"), 0.0f, 0.0f, {.LabelA=TEXT("cell"), .LabelB=TEXT("e- flow"), .ShapeVariant=0}},
		{TEXT("Junctions"), TEXT("True or false: current is conserved at a junction."), {TEXT("True"), TEXT("False"), TEXT("Only in series"), TEXT("Only with lamps")}, 0, TEXT("Charge does not disappear at a junction."), TEXT("Add currents entering and leaving."), TEXT("Total current entering a junction equals total current leaving."), EBHDiagramType::Circuit, TEXT("current in = current out"), 0.0f, 0.0f, {.LabelA=TEXT("junction"), .LabelB=TEXT("in / out"), .ShapeVariant=2}},
		{TEXT("Voltmeters"), TEXT("True or false: a voltmeter is connected in series."), {TEXT("False"), TEXT("True"), TEXT("Only for cells"), TEXT("Only for fuses")}, 0, TEXT("A voltmeter compares energy difference across a component."), TEXT("Place it across the component."), TEXT("A voltmeter is connected in parallel."), EBHDiagramType::Circuit, TEXT("voltmeter in parallel"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("voltmeter V"), .ShapeVariant=0}},
		{TEXT("Power"), TEXT("True or false: P = IV can be used for electrical power."), {TEXT("True"), TEXT("False"), TEXT("Only for sound"), TEXT("Only for springs")}, 0, TEXT("Power is current times potential difference."), TEXT("Check units: A x V = W."), TEXT("Electrical power is P = IV."), EBHDiagramType::Circuit, TEXT("P = IV"), 0.0f, 0.0f, {.LabelA=TEXT("device"), .LabelB=TEXT("P = ?"), .ShapeVariant=0}},
		{TEXT("Double insulation"), TEXT("True or false: a double-insulated plastic appliance must have an earth wire."), {TEXT("False"), TEXT("True"), TEXT("Only if small"), TEXT("Only if DC")}, 0, TEXT("Double insulation prevents touching live metal."), TEXT("Explain why no exposed metal case needs earthing."), TEXT("Double-insulated appliances do not need an earth wire."), EBHDiagramType::Circuit, TEXT("double insulation -> no earth wire needed"), 0.0f, 0.0f, {.LabelA=TEXT("plastic case"), .LabelB=TEXT("live + neutral"), .ShapeVariant=0}}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Current"), TEXT("18 C of charge flows in 6 s through the door lock. What is current?"), {TEXT("3 A"), TEXT("12 A"), TEXT("108 A"), TEXT("0.33 A")}, 0, TEXT("Use I = Q / t."), TEXT("Divide charge by time."), TEXT("I = 18 / 6 = 3 A."), EBHDiagramType::Circuit, TEXT("I = Q / t"), 3.0f, 0.1f, {.LabelA=TEXT("Q = 18 C"), .LabelB=TEXT("t = 6 s"), .ShapeVariant=0}},
		{TEXT("Potential difference"), TEXT("A cell transfers 24 J to 6 C. What is the potential difference?"), {TEXT("4 V"), TEXT("18 V"), TEXT("144 V"), TEXT("0.25 V")}, 0, TEXT("Use V = E / Q."), TEXT("Divide energy by charge."), TEXT("V = 24 / 6 = 4 V."), EBHDiagramType::Circuit, TEXT("V = E / Q"), 4.0f, 0.1f, {.LabelA=TEXT("E = 24 J"), .LabelB=TEXT("Q = 6 C"), .ShapeVariant=0}},
		{TEXT("Resistance"), TEXT("A lamp has 12 V across it and current 3 A. What is its resistance?"), {TEXT("4 ohms"), TEXT("36 ohms"), TEXT("15 ohms"), TEXT("0.25 ohms")}, 0, TEXT("Use R = V / I."), TEXT("Divide voltage by current."), TEXT("R = 12 / 3 = 4 ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 4.0f, 0.1f, {.ValueA=12.0f, .ValueB=3.0f, .ValueC=15.0f, .ValueD=4.0f, .LabelA=TEXT("(12 V, 3 A)"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Power"), TEXT("A heater uses 2 A from a 230 V supply. What is its power?"), {TEXT("460 W"), TEXT("115 W"), TEXT("232 W"), TEXT("228 W")}, 0, TEXT("Use P = IV."), TEXT("Multiply current by voltage."), TEXT("P = 2 x 230 = 460 W."), EBHDiagramType::Circuit, TEXT("P = IV"), 460.0f, 1.0f, {.ValueB=230.0f, .LabelA=TEXT("I = 2 A"), .LabelB=TEXT("V = 230 V"), .ShapeVariant=0}},
		{TEXT("Series resistance"), TEXT("Three resistors of 2 ohms, 5 ohms, and 8 ohms are in series. What is total resistance?"), {TEXT("15 ohms"), TEXT("10 ohms"), TEXT("80 ohms"), TEXT("1.5 ohms")}, 0, TEXT("Series resistances add directly."), TEXT("Add all three values."), TEXT("RT = 2 + 5 + 8 = 15 ohms."), EBHDiagramType::Circuit, TEXT("RT = R1 + R2 + R3"), 15.0f, 0.1f, {.ValueA=2.0f, .ValueC=8.0f, .LabelA=TEXT("2 ohm"), .LabelC=TEXT("8 ohm"), .ShapeVariant=3}},
		{TEXT("Energy transferred"), TEXT("A 5 A device at 12 V runs for 10 s. How much energy is transferred?"), {TEXT("600 J"), TEXT("27 J"), TEXT("60 J"), TEXT("120 J")}, 0, TEXT("Use E = IVt."), TEXT("Multiply current, voltage, and time."), TEXT("E = 5 x 12 x 10 = 600 J."), EBHDiagramType::Circuit, TEXT("E = IVt"), 600.0f, 1.0f, {.ValueB=12.0f, .LabelA=TEXT("I = 5 A"), .LabelB=TEXT("V = 12 V"), .LabelC=TEXT("t = 10 s"), .ShapeVariant=0}},
		{TEXT("Power from resistance"), TEXT("A 3 A current passes through a 4 ohm resistor. Use P = I^2R. What is power?"), {TEXT("36 W"), TEXT("12 W"), TEXT("7 W"), TEXT("48 W")}, 0, TEXT("Square the current first."), TEXT("Show 3^2 x 4."), TEXT("P = 3^2 x 4 = 36 W."), EBHDiagramType::IVGraph, TEXT("P = I^2R"), 36.0f, 0.1f, {.LabelA=TEXT("I = 3 A"), .LabelB=TEXT("R = 4 ohm"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Circuit practical"), TEXT("A student measures the I-V graph of a resistor. Which method is most suitable?"), {TEXT("Vary the p.d., record V and I pairs, and keep temperature as constant as possible"), TEXT("Use one reading only because resistance never changes"), TEXT("Connect the voltmeter in series and the ammeter in parallel"), TEXT("Heat the resistor deliberately for every reading")}, 0, TEXT("An I-V investigation needs several paired readings."), TEXT("Mention meter placement and controlling temperature."), TEXT("A good I-V experiment varies p.d., measures current and voltage correctly, and controls temperature so resistance is meaningful."), EBHDiagramType::IVGraph, TEXT("exam skill: plan an I-V investigation"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Junction reasoning"), TEXT("At a junction, 0.60 A enters and two branches carry 0.20 A and 0.40 A. What conclusion is best?"), {TEXT("The data supports conservation of charge"), TEXT("Charge is used up in the first branch"), TEXT("The 0.40 A branch must have the larger p.d. because it has more current"), TEXT("The circuit cannot be parallel")}, 0, TEXT("Current entering a junction equals current leaving."), TEXT("Add the branch currents and compare with the input."), TEXT("0.20 A + 0.40 A = 0.60 A, so the readings support conservation of charge at the junction."), EBHDiagramType::Circuit, TEXT("exam skill: use junction evidence"), 0.0f, 0.0f, {.LabelA=TEXT("in 0.60 A"), .LabelB=TEXT("br 0.20 A"), .LabelC=TEXT("br 0.40 A"), .ShapeVariant=2}},
		{TEXT("Component behaviour"), TEXT("A filament lamp's I-V graph curves and becomes less steep at higher voltage. What is the IGCSE explanation?"), {TEXT("The filament heats up, increasing its resistance"), TEXT("The lamp becomes an ohmic conductor at high temperature"), TEXT("The current has stopped flowing"), TEXT("The voltmeter is connected in series")}, 0, TEXT("A hot filament has more resistance."), TEXT("Link temperature rise to resistance rise."), TEXT("As the filament gets hotter, metal ion vibrations increase, causing greater resistance and a curved I-V graph."), EBHDiagramType::IVGraph, TEXT("exam skill: explain non-ohmic behaviour"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=1}},
		{TEXT("Mains safety"), TEXT("A metal-cased heater has a live wire touching the case. Which explanation earns full credit?"), {TEXT("The earth wire gives a low-resistance path, causing a large current that blows the fuse"), TEXT("The earth wire stores the charge until the switch is opened"), TEXT("The fuse stops the case becoming live before any current flows"), TEXT("The neutral wire becomes an insulator")}, 0, TEXT("Explain both the earth wire and the fuse."), TEXT("Use low resistance, large current, and disconnection."), TEXT("The earth wire provides a low-resistance fault path; the large current melts the fuse and disconnects the supply."), EBHDiagramType::Circuit, TEXT("exam skill: explain fault protection"), 0.0f, 0.0f, {.LabelA=TEXT("metal case"), .LabelB=TEXT("earth wire"), .ShapeVariant=0}},
		{TEXT("Static hazards"), TEXT("A tanker is earthed before fuel is transferred. Why?"), {TEXT("To prevent charge build-up causing a spark near flammable vapour"), TEXT("To increase the fuel's potential difference"), TEXT("To make the fuel flow as conventional current"), TEXT("To make the tanker positively charged")}, 0, TEXT("Moving fuel can cause static charge."), TEXT("Link charge build-up, spark, and ignition risk."), TEXT("Earthing lets charge flow away, reducing the risk of a spark igniting fuel vapour."), EBHDiagramType::StaticCharge, TEXT("exam skill: apply electrostatic safety"), 0.0f, 0.0f, {.LabelA=TEXT("earth tanker")}},
		{TEXT("Domestic electricity"), TEXT("A 900 W kettle is used on a 230 V supply. A student chooses a 3 A fuse. What is the best evaluation?"), {TEXT("Unsuitable, because the normal current is about 3.9 A so the fuse may melt in normal use"), TEXT("Suitable, because lower fuse ratings always make appliances safer"), TEXT("Unsuitable, because the fuse rating must equal the voltage"), TEXT("Suitable, because kettles do not use current")}, 0, TEXT("Estimate current using power divided by voltage."), TEXT("Compare normal current with the fuse rating."), TEXT("I = P / V = 900 / 230, about 3.9 A; a 3 A fuse is below normal current, so it is unsuitable."), EBHDiagramType::Circuit, TEXT("exam skill: evaluate a fuse choice"), 0.0f, 0.0f, {.ValueB=230.0f, .LabelA=TEXT("900 W"), .LabelB=TEXT("230 V"), .LabelC=TEXT("3 A fuse"), .ShapeVariant=0}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Circuit diagrams"), TEXT("In the projected circuit, which meter placement is correct?"), {TEXT("Ammeter series, voltmeter parallel"), TEXT("Ammeter parallel, voltmeter series"), TEXT("Both in series"), TEXT("Both outside the circuit")}, 0, TEXT("Current through, voltage across."), TEXT("Trace the current path and branch across component."), TEXT("Ammeters go in series; voltmeters go in parallel."), EBHDiagramType::Circuit, TEXT("A series, V parallel"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("A and V"), .ShapeVariant=0}},
		{TEXT("IV graphs"), TEXT("Which IV graph shape shows an ohmic conductor at constant temperature?"), {TEXT("Straight line through origin"), TEXT("Curve flattening at high current"), TEXT("Vertical line only"), TEXT("Random spikes")}, 0, TEXT("Ohmic means current proportional to voltage."), TEXT("Proportional relationships are straight through origin."), TEXT("An ohmic conductor has a straight-line IV graph through the origin."), EBHDiagramType::IVGraph, TEXT("I proportional to V"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Filament lamps"), TEXT("On the lamp IV graph, why does the curve get shallower at high current?"), {TEXT("Resistance increases as temperature increases"), TEXT("Resistance becomes zero"), TEXT("Voltage disappears"), TEXT("Charge stops existing")}, 0, TEXT("A hot filament has higher resistance."), TEXT("Link heating to resistance."), TEXT("The filament heats up, so resistance increases and the IV graph curves."), EBHDiagramType::IVGraph, TEXT("hot filament -> higher R"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=1}},
		{TEXT("Parallel current"), TEXT("A junction shows 6 A entering, with 2 A in one branch. What current is in the other branch?"), {TEXT("4 A"), TEXT("8 A"), TEXT("12 A"), TEXT("3 A")}, 0, TEXT("Current entering equals current leaving."), TEXT("Subtract the known branch current from total."), TEXT("6 A = 2 A + 4 A, so the other branch is 4 A."), EBHDiagramType::Circuit, TEXT("current in = current out"), 4.0f, 0.1f, {.ValueA=6.0f, .ValueB=2.0f, .LabelA=TEXT("in 6 A"), .LabelB=TEXT("branch 2 A"), .ShapeVariant=2}},
		{TEXT("Ohm's law from circuit"), TEXT("The circuit diagram shows a 12 V supply driving current through a single 6 ohm resistor. What does the ammeter read?"), {TEXT("2 A"), TEXT("18 A"), TEXT("72 A"), TEXT("0.5 A")}, 0, TEXT("Use I = V / R."), TEXT("Divide the supply voltage by the resistance shown."), TEXT("I = V / R = 12 / 6 = 2 A."), EBHDiagramType::Circuit, TEXT("I = V / R"), 2.0f, 0.1f, {.ValueA=6.0f, .ValueB=12.0f, .LabelA=TEXT("R = 6 ohm"), .LabelB=TEXT("V = 12 V")}},
		{TEXT("Meter placement"), TEXT("In the circuit diagram, where must the ammeter (A) and voltmeter (V) go to read the lamp correctly?"), {TEXT("Ammeter in series, voltmeter across the lamp"), TEXT("Ammeter across the lamp, voltmeter in series"), TEXT("Both across the lamp"), TEXT("Both in series")}, 0, TEXT("Current through, voltage across."), TEXT("Trace the single current path, then branch across the lamp."), TEXT("An ammeter goes in series; a voltmeter goes in parallel across the lamp."), EBHDiagramType::Circuit, TEXT("A series, V parallel"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("A in series")}},
		{TEXT("Resistance from IV graph"), TEXT("The IV graph is a straight line through the origin passing through the marked point (12 V, 2 A). What is the resistance?"), {TEXT("6 ohm"), TEXT("24 ohm"), TEXT("0.17 ohm"), TEXT("10 ohm")}, 0, TEXT("R = V / I at any point on the line."), TEXT("Read the marked point, then divide V by I."), TEXT("R = V / I = 12 / 2 = 6 ohm."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 6.0f, 0.1f, {.ValueA=12.0f, .ValueB=2.0f, .ValueC=15.0f, .ValueD=3.0f, .LabelA=TEXT("(12 V, 2 A)"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A")}},
		{TEXT("Junction current"), TEXT("The junction in the diagram has 5 A flowing in, splitting into two branches; one branch shows 2 A. What is the current in the other branch?"), {TEXT("3 A"), TEXT("7 A"), TEXT("10 A"), TEXT("2.5 A")}, 0, TEXT("Current in equals current out."), TEXT("Subtract the known branch from the total."), TEXT("5 A - 2 A = 3 A in the other branch."), EBHDiagramType::Circuit, TEXT("current in = current out"), 3.0f, 0.1f, {.ValueA=5.0f, .ValueB=2.0f, .LabelA=TEXT("in 5 A"), .LabelB=TEXT("branch 2 A")}},
		{TEXT("Comparing IV gradients"), TEXT("On one IV graph with V on the x-axis and I on the y-axis, conductor X has a steeper line than conductor Y. Which statement is correct?"), {TEXT("X has lower resistance than Y"), TEXT("X has higher resistance than Y"), TEXT("X carries no current"), TEXT("Y is a perfect insulator")}, 0, TEXT("Gradient is I / V = 1 / R."), TEXT("Steeper means more current per volt."), TEXT("A steeper I-V line means more current per volt, so a smaller resistance: X has lower resistance."), EBHDiagramType::IVGraph, TEXT("gradient = 1 / R"), 0.0f, 0.0f, {.LabelA=TEXT("X steeper"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A")}},
		{TEXT("Series voltage share"), TEXT("The series circuit diagram shows a 12 V supply across two equal resistors, R1 = R2 = 4 ohm. What is the voltage across R1?"), {TEXT("6 V"), TEXT("12 V"), TEXT("3 V"), TEXT("48 V")}, 0, TEXT("Equal series resistors share the supply equally."), TEXT("Find total R and current, or split the supply equally."), TEXT("Equal resistors in series share the 12 V equally, so 6 V across each."), EBHDiagramType::Circuit, TEXT("series p.d. shares with R"), 6.0f, 0.1f, {.ValueA=4.0f, .ValueB=12.0f, .ValueC=4.0f, .LabelA=TEXT("R1 = 4 ohm"), .LabelB=TEXT("V = 12 V"), .LabelC=TEXT("R2 = 4 ohm")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Series and parallel"), TEXT("Choose the correct matching pair."), {TEXT("Series: same current; Parallel: same p.d."), TEXT("Series: same p.d.; Parallel: same current"), TEXT("Series: broken branch still works; Parallel: one loop only"), TEXT("Series: lower total resistance; Parallel: always one component")}, 0, TEXT("Series has one path; parallel has branches."), TEXT("Trace the paths in the circuit."), TEXT("Series circuits have the same current; parallel branches have the same p.d."), EBHDiagramType::Circuit, TEXT("series I same, parallel V same"), 0.0f, 0.0f},
		{TEXT("Hazards"), TEXT("Match each mains hazard to the danger."), {TEXT("Damaged insulation -> shock; thin cable high current -> fire"), TEXT("Damaged insulation -> lower bill; thin cable -> cooling"), TEXT("Damp room -> perfect insulation; live wire -> no risk"), TEXT("Fuse -> shock source; earth -> live supply")}, 0, TEXT("Expose live metal or overheating can harm people."), TEXT("Connect each hazard to shock or fire."), TEXT("Damaged insulation can cause shock; overheating cables can cause fire."), EBHDiagramType::Circuit, TEXT("fault hazards: shock/fire"), 0.0f, 0.0f, {.LabelA=TEXT("insulation"), .LabelB=TEXT("cable"), .ShapeVariant=0}},
		{TEXT("Components"), TEXT("Match the component behaviour."), {TEXT("Thermistor hot -> lower R; LDR bright -> lower R"), TEXT("Thermistor hot -> higher R; LDR bright -> higher R"), TEXT("Fuse hot -> lower rating; lamp cool -> no resistance"), TEXT("Cell bright -> lower p.d.; switch open -> more current")}, 0, TEXT("Both thermistor and LDR can lower resistance when stimulated."), TEXT("Remember heat for thermistor, light for LDR."), TEXT("Thermistor resistance decreases as temperature increases; LDR resistance decreases as light intensity increases."), EBHDiagramType::IVGraph, TEXT("thermistor hot lower R, LDR bright lower R"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=1}},
		{TEXT("Static uses"), TEXT("Match the static effect to its use."), {TEXT("Charged ink droplets -> inkjet printer; toner attraction -> photocopier"), TEXT("Charged ink -> fuse; toner -> voltmeter"), TEXT("Balloon wall -> circuit breaker; lightning -> LDR"), TEXT("Electron loss -> AC; proton flow -> DC")}, 0, TEXT("Printers and copiers both use charged particles."), TEXT("Name which charged material moves."), TEXT("Inkjet printers deflect charged droplets; photocopiers attract toner to charged image areas."), EBHDiagramType::StaticCharge, TEXT("static charge attracts/deflects"), 0.0f, 0.0f, {.LabelA=TEXT("charged ink")}}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Earthing"), TEXT("Order the fault protection sequence."), {TEXT("Live touches case -> earth current large -> fuse melts -> supply disconnects"), TEXT("Fuse melts -> live touches case -> current stops -> earth wire heats first"), TEXT("Earth wire removed -> fuse melts -> voltage rises -> current falls"), TEXT("Case becomes plastic -> live wire grows -> fuse rating rises -> shock")}, 0, TEXT("A fault current must happen before the fuse melts."), TEXT("Start with live wire touching the metal case."), TEXT("The earth wire carries a large current, causing the fuse to melt and disconnect the supply."), EBHDiagramType::Circuit, TEXT("fault -> earth current -> fuse blows"), 0.0f, 0.0f, {.LabelA=TEXT("metal case"), .LabelB=TEXT("earth wire"), .ShapeVariant=0}},
		{TEXT("Static charging"), TEXT("Order charging an insulator by rubbing."), {TEXT("Rub surfaces -> electrons transfer -> one gains electrons -> it becomes negative"), TEXT("Protons move -> object loses neutrons -> it becomes AC -> sparks stop"), TEXT("Neutrons transfer -> both become positive -> electrons vanish -> no charge"), TEXT("Current alternates -> fuse melts -> object becomes neutral -> rubbing starts")}, 0, TEXT("Static charging usually moves electrons."), TEXT("Track the object gaining electrons."), TEXT("Rubbing transfers electrons; the object gaining electrons becomes negative."), EBHDiagramType::StaticCharge, TEXT("gain electrons -> negative"), 0.0f, 0.0f, {.LabelA=TEXT("rub +/-")}},
		{TEXT("Circuit calculation"), TEXT("Order a resistance calculation from V and I."), {TEXT("Write R = V / I -> substitute V and I -> divide -> add ohms"), TEXT("Write I = Q / t -> multiply by voltage -> add N -> divide"), TEXT("Add V and I -> square answer -> add amps -> draw graph"), TEXT("Choose fuse -> subtract charge -> add volts -> divide by time")}, 0, TEXT("Resistance is voltage divided by current."), TEXT("Do not multiply V and I for resistance."), TEXT("Use R = V / I and give the unit ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Circuit breaker"), TEXT("Order how a circuit breaker protects the classroom supply."), {TEXT("Current too high -> electromagnet pulls switch -> circuit opens -> breaker can reset"), TEXT("Switch opens -> current too high -> electromagnet disappears -> breaker melts"), TEXT("Fuse warms -> plastic case charges -> circuit opens -> voltage doubles"), TEXT("Earth wire glows -> current lowers -> breaker stores charge -> switch closes")}, 0, TEXT("A circuit breaker uses an electromagnet."), TEXT("Start with excessive current."), TEXT("High current strengthens the electromagnet, opening the switch; the breaker can be reset."), EBHDiagramType::Circuit, TEXT("high current -> electromagnet switch opens"), 0.0f, 0.0f, {.LabelA=TEXT("electromagnet"), .LabelB=TEXT("switch"), .ShapeVariant=0}}
	};

	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity"), TEXT("Electricity"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity"), TEXT("Electricity"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity"), TEXT("Electricity"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity"), TEXT("Electricity"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity"), TEXT("Electricity"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity"), TEXT("Electricity"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity"), TEXT("Electricity"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

void BuildWaves(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Wave transfer"), TEXT("What do waves transfer across the dark hall?"), {TEXT("Energy and information without matter transfer"), TEXT("Only matter"), TEXT("Only mass"), TEXT("Only charge")}, 0, TEXT("Particles oscillate about fixed positions."), TEXT("State what moves and what does not."), TEXT("Waves transfer energy and information without transferring matter overall."), EBHDiagramType::Wave, TEXT("wave transfers energy"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("energy"), .LabelB=TEXT("no matter")}},
		{TEXT("Transverse waves"), TEXT("Which description fits a transverse wave?"), {TEXT("Vibrations at right angles to travel"), TEXT("Vibrations parallel to travel"), TEXT("No oscillations"), TEXT("Only compressions")}, 0, TEXT("Light is transverse."), TEXT("Compare vibration direction with wave direction."), TEXT("In transverse waves, vibrations are perpendicular to the direction of travel."), EBHDiagramType::Wave, TEXT("transverse: perpendicular vibrations"), 0.0f, 0.0f, {.ValueA=0.30f, .ValueB=3.0f, .LabelA=TEXT("vibration"), .LabelB=TEXT("travel dir")}},
		{TEXT("Longitudinal waves"), TEXT("Which feature belongs to longitudinal waves?"), {TEXT("Compressions and rarefactions"), TEXT("Crests and troughs only"), TEXT("No frequency"), TEXT("No wavelength")}, 0, TEXT("Sound is longitudinal."), TEXT("Look for squashed and spread-out regions."), TEXT("Longitudinal waves have compressions and rarefactions."), EBHDiagramType::Wave, TEXT("longitudinal: parallel vibrations"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("compression"), .LabelB=TEXT("rarefaction")}},
		{TEXT("Frequency"), TEXT("What does frequency mean?"), {TEXT("Number of waves passing per second"), TEXT("Maximum displacement"), TEXT("Distance between mirrors"), TEXT("Energy wasted")}, 0, TEXT("Frequency is measured in hertz."), TEXT("Count complete waves each second."), TEXT("Frequency is the number of waves passing a point per second."), EBHDiagramType::Wave, TEXT("f = 1 / T"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("waves/s"), .LabelB=TEXT("f in Hz")}},
		{TEXT("EM spectrum"), TEXT("Which EM wave has the longest wavelength in the standard order?"), {TEXT("Radio"), TEXT("Gamma"), TEXT("X-rays"), TEXT("Ultraviolet")}, 0, TEXT("The order starts with radio."), TEXT("Say radio, microwave, infrared..."), TEXT("Radio waves have the longest wavelength and lowest frequency in the EM spectrum list."), EBHDiagramType::EMSpectrum, TEXT("radio -> microwave -> infrared -> visible -> ultraviolet -> X-rays -> gamma"), 0.0f, 0.0f, {.LabelA=TEXT("longest"), .LabelB=TEXT("shortest"), .ShapeVariant=0}},
		{TEXT("Doppler effect"), TEXT("If a siren source moves toward you, what happens to observed pitch?"), {TEXT("It increases"), TEXT("It decreases"), TEXT("It becomes zero"), TEXT("It changes to DC")}, 0, TEXT("Wavefronts bunch together in front."), TEXT("Shorter wavelength means higher frequency."), TEXT("When a source approaches, observed frequency and pitch increase."), EBHDiagramType::Wave, TEXT("approaching source -> higher f"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("approaching"), .LabelB=TEXT("observer")}},
		{TEXT("Reflection"), TEXT("A ray hits a mirror at 35 degrees to the normal. What is the angle of reflection?"), {TEXT("35 degrees"), TEXT("55 degrees"), TEXT("70 degrees"), TEXT("0 degrees")}, 0, TEXT("Angle of incidence equals angle of reflection."), TEXT("Angles are measured from the normal."), TEXT("The law of reflection gives 35 degrees."), EBHDiagramType::RayDiagram, TEXT("i = r"), 35.0f, 0.1f, {.LabelA=TEXT("i 35 deg"), .AngleOrShape=35.0f}},
		{TEXT("Refraction"), TEXT("Light enters a more optically dense medium at an angle. Which way does it bend?"), {TEXT("Towards the normal"), TEXT("Away from the normal"), TEXT("It never bends"), TEXT("Into a circle")}, 0, TEXT("Speed decreases in the denser medium."), TEXT("Draw the normal first."), TEXT("Entering a more optically dense medium makes light bend towards the normal."), EBHDiagramType::RayDiagram, TEXT("denser -> slower -> towards normal"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("air->dense")}},
		{TEXT("Sound"), TEXT("What wave type is sound in air?"), {TEXT("Longitudinal"), TEXT("Transverse"), TEXT("Electrostatic"), TEXT("Nuclear")}, 0, TEXT("Air particles vibrate parallel to the direction of travel."), TEXT("Look for compressions and rarefactions."), TEXT("Sound in air is a longitudinal wave."), EBHDiagramType::Wave, TEXT("sound is longitudinal"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("air vibrate"), .LabelB=TEXT("in air")}},
		{TEXT("Total internal reflection"), TEXT("Which condition is required for total internal reflection?"), {TEXT("More dense to less dense and i greater than c"), TEXT("Less dense to more dense and i less than c"), TEXT("Any angle in any medium"), TEXT("Only radio waves in air")}, 0, TEXT("TIR needs the critical angle condition."), TEXT("Name both medium direction and angle condition."), TEXT("TIR occurs from more to less optically dense medium when incidence angle exceeds critical angle."), EBHDiagramType::RayDiagram, TEXT("TIR: i > c, dense to less dense"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("dense->less")}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Reflection"), TEXT("True or false: reflection changes wave frequency."), {TEXT("False"), TEXT("True"), TEXT("Only for mirrors"), TEXT("Only for sound")}, 0, TEXT("Reflection changes direction, not frequency."), TEXT("List what stays unchanged."), TEXT("Frequency, wavelength, and speed are unchanged during reflection in the same medium."), EBHDiagramType::RayDiagram, TEXT("reflection: i = r"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("mirror")}},
		{TEXT("Refraction"), TEXT("True or false: frequency stays the same during refraction."), {TEXT("True"), TEXT("False"), TEXT("Only in glass"), TEXT("Only in air")}, 0, TEXT("Speed and wavelength change, frequency stays fixed."), TEXT("Separate source frequency from medium speed."), TEXT("During refraction, frequency stays the same while speed and wavelength change."), EBHDiagramType::RayDiagram, TEXT("frequency unchanged in refraction"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("refraction")}},
		{TEXT("Light"), TEXT("True or false: light is transverse."), {TEXT("True"), TEXT("False"), TEXT("Only red light"), TEXT("Only sound")}, 0, TEXT("Light is an electromagnetic wave."), TEXT("Recall EM waves are transverse."), TEXT("Light is a transverse wave."), EBHDiagramType::Wave, TEXT("light is transverse"), 0.0f, 0.0f, {.ValueA=0.30f, .ValueB=3.0f, .LabelA=TEXT("light"), .LabelB=TEXT("vibration")}},
		{TEXT("EM hazards"), TEXT("True or false: X-rays and gamma rays are ionising and can increase cancer risk."), {TEXT("True"), TEXT("False"), TEXT("Only radio waves ionise"), TEXT("Only visible light ionises")}, 0, TEXT("High-frequency EM waves are more hazardous."), TEXT("Link ionising radiation to mutations."), TEXT("X-rays and gamma rays are ionising and can cause mutations and cancer risk."), EBHDiagramType::EMSpectrum, TEXT("X-rays/gamma: ionising"), 0.0f, 0.0f, {.LabelA=TEXT("X-ray/gamma"), .LabelB=TEXT("ionising"), .ShapeVariant=6}},
		{TEXT("Critical angle"), TEXT("True or false: n = 1 / sin c for the critical angle from glass to air."), {TEXT("True"), TEXT("False"), TEXT("Only for sound"), TEXT("Only for mirrors")}, 0, TEXT("This is the IGCSE critical angle relationship."), TEXT("Use c as the critical angle."), TEXT("For a material to air, refractive index n = 1 / sin c."), EBHDiagramType::RayDiagram, TEXT("n = 1 / sin c"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("glass->air")}}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Wave speed"), TEXT("A wave has frequency 5 Hz and wavelength 3 m. What is its speed?"), {TEXT("15 m/s"), TEXT("8 m/s"), TEXT("1.67 m/s"), TEXT("0.6 m/s")}, 0, TEXT("Use v = f lambda."), TEXT("Multiply frequency by wavelength."), TEXT("v = 5 x 3 = 15 m/s."), EBHDiagramType::Wave, TEXT("v = f lambda"), 15.0f, 0.1f, {.ValueA=0.30f, .ValueB=3.0f, .LabelA=TEXT("f 5 Hz"), .LabelB=TEXT("lambda 3 m")}},
		{TEXT("Period"), TEXT("A sound has frequency 4 Hz. What is its period?"), {TEXT("0.25 s"), TEXT("4 s"), TEXT("8 s"), TEXT("16 s")}, 0, TEXT("Use T = 1 / f."), TEXT("Find one divided by frequency."), TEXT("T = 1 / 4 = 0.25 s."), EBHDiagramType::Wave, TEXT("f = 1 / T"), 0.25f, 0.01f, {.ValueA=0.25f, .ValueB=4.0f, .LabelA=TEXT("f 4 Hz"), .LabelB=TEXT("period = ?")}},
		{TEXT("Echo speed"), TEXT("An echo returns in 0.50 s from a wall 85 m away. What distance did sound travel?"), {TEXT("170 m"), TEXT("85 m"), TEXT("42.5 m"), TEXT("0.5 m")}, 0, TEXT("Echo distance is there and back."), TEXT("Double the one-way distance."), TEXT("Sound travels to the wall and back: 2 x 85 = 170 m."), EBHDiagramType::Wave, TEXT("echo distance = 2 x one-way distance"), 170.0f, 0.1f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("wall 85 m"), .LabelB=TEXT("t 0.50 s")}},
		{TEXT("Reflection angle"), TEXT("A ray's angle of incidence is 42 degrees. What is the angle of reflection?"), {TEXT("42 degrees"), TEXT("48 degrees"), TEXT("84 degrees"), TEXT("21 degrees")}, 0, TEXT("Use i = r."), TEXT("Angles are from the normal."), TEXT("The angle of reflection equals 42 degrees."), EBHDiagramType::RayDiagram, TEXT("i = r"), 42.0f, 0.1f, {.LabelA=TEXT("i 42 deg"), .AngleOrShape=42.0f}},
		{TEXT("Wave speed"), TEXT("A radio wave has wavelength 2 m and speed 300,000,000 m/s. What is frequency?"), {TEXT("150,000,000 Hz"), TEXT("600,000,000 Hz"), TEXT("299,999,998 Hz"), TEXT("2 Hz")}, 0, TEXT("Rearrange v = f lambda to f = v / lambda."), TEXT("Divide speed by wavelength."), TEXT("f = 300,000,000 / 2 = 150,000,000 Hz."), EBHDiagramType::EMSpectrum, TEXT("f = v / lambda"), 150000000.0f, 1000.0f, {.LabelA=TEXT("lambda 2 m"), .LabelB=TEXT("v 3e8 m/s"), .ShapeVariant=1}},
		{TEXT("Snell's law"), TEXT("n1 sin i = n2 sin r. If n1=1.0, sin i=0.6, and n2=1.5, what is sin r?"), {TEXT("0.4"), TEXT("0.9"), TEXT("2.5"), TEXT("0.6")}, 0, TEXT("Divide n1 sin i by n2."), TEXT("Show 1.0 x 0.6 / 1.5."), TEXT("sin r = 0.6 / 1.5 = 0.4."), EBHDiagramType::RayDiagram, TEXT("n1 sin i = n2 sin r"), 0.4f, 0.01f, {.LabelA=TEXT("n1 1.0 n2 1.5"), .LabelB=TEXT("sin i 0.6")}},
		{TEXT("Critical angle"), TEXT("A glass block has critical angle 30 degrees. Use sin 30 = 0.5. What is n?"), {TEXT("2.0"), TEXT("0.5"), TEXT("30"), TEXT("1.5")}, 0, TEXT("Use n = 1 / sin c."), TEXT("Divide one by 0.5."), TEXT("n = 1 / 0.5 = 2.0."), EBHDiagramType::RayDiagram, TEXT("n = 1 / sin c"), 2.0f, 0.01f, {.LabelA=TEXT("c 30 deg"), .LabelB=TEXT("sin c 0.5"), .AngleOrShape=30.0f}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Wave investigation"), TEXT("A ripple tank photo shows 6 wavefront gaps covering 0.30 m. The vibrator frequency is 20 Hz. What is the best method?"), {TEXT("Find wavelength from spacing, then multiply by frequency"), TEXT("Divide frequency by the full 0.30 m without finding wavelength"), TEXT("Use amplitude instead of wavelength"), TEXT("Ignore frequency because the photo already gives speed")}, 0, TEXT("Use distance across several wavelengths to reduce percentage uncertainty."), TEXT("Find one wavelength first, then use wave speed."), TEXT("Measure several wavelengths, divide to get one wavelength, then use v = f lambda."), EBHDiagramType::Wave, TEXT("exam skill: process ripple-tank data"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=6.0f, .LabelA=TEXT("0.30 m / 6"), .LabelB=TEXT("f 20 Hz")}},
		{TEXT("Sound experiment"), TEXT("In an echo experiment, the measured time is for the sound to travel to the wall and back. What mistake must be avoided?"), {TEXT("Using the one-way wall distance instead of the total sound distance"), TEXT("Using seconds instead of minutes"), TEXT("Measuring the wall distance with a ruler"), TEXT("Repeating the timing")}, 0, TEXT("An echo makes a round trip."), TEXT("Double the wall distance before using speed = distance / time."), TEXT("The sound travels to the wall and back, so the total distance is twice the wall distance."), EBHDiagramType::Wave, TEXT("exam skill: avoid echo distance error"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("to wall"), .LabelB=TEXT("and back")}},
		{TEXT("Refraction explanation"), TEXT("A ray bends towards the normal when it enters glass from air. Which explanation is most complete?"), {TEXT("It slows down in glass; frequency stays the same, so wavelength decreases"), TEXT("Its frequency increases and wavelength stays the same"), TEXT("Its speed increases because glass is denser"), TEXT("It reflects before entering the glass")}, 0, TEXT("Frequency is fixed by the source."), TEXT("Link denser medium, lower speed, unchanged frequency, and shorter wavelength."), TEXT("In glass the wave speed is lower; frequency is unchanged, so wavelength decreases and the ray bends towards the normal."), EBHDiagramType::RayDiagram, TEXT("exam skill: explain refraction, not just name it"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("air->glass")}},
		{TEXT("Critical angle reasoning"), TEXT("A ray in glass meets the glass-air boundary at an angle greater than the critical angle. What happens?"), {TEXT("Total internal reflection occurs"), TEXT("It refracts out along the normal"), TEXT("It stops at the boundary"), TEXT("Its frequency becomes zero")}, 0, TEXT("TIR happens from more dense to less dense above the critical angle."), TEXT("State both the medium direction and angle condition."), TEXT("When travelling from glass to air with incidence angle greater than the critical angle, the ray is totally internally reflected."), EBHDiagramType::RayDiagram, TEXT("exam skill: apply the TIR conditions"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("glass->air")}},
		{TEXT("EM spectrum evaluation"), TEXT("A hospital uses X-rays for imaging but limits exposure time. What is the best IGCSE reason?"), {TEXT("X-rays are ionising, so dose should be kept as low as practical"), TEXT("X-rays are longitudinal sound waves"), TEXT("X-rays have the longest wavelength in the EM spectrum"), TEXT("X-rays cannot pass through soft tissue")}, 0, TEXT("High-frequency EM radiation can ionise cells."), TEXT("Link ionisation to cell damage and dose control."), TEXT("X-rays are ionising and can damage living cells, so exposure is limited while still getting a useful image."), EBHDiagramType::EMSpectrum, TEXT("exam skill: balance use and hazard"), 0.0f, 0.0f, {.LabelA=TEXT("X-ray"), .LabelB=TEXT("imaging"), .ShapeVariant=6}},
		{TEXT("Doppler evidence"), TEXT("A moving ambulance passes a student. The pitch is higher as it approaches and lower as it moves away. What causes this?"), {TEXT("Wavefronts are compressed in front and spread out behind"), TEXT("The sound changes from longitudinal to transverse"), TEXT("The ambulance changes the speed of sound in air"), TEXT("The siren emits gamma rays when moving")}, 0, TEXT("Observed frequency changes because wavefront spacing changes."), TEXT("Use shorter wavelength in front and longer wavelength behind."), TEXT("Motion compresses wavefronts ahead of the source, increasing observed frequency; behind it, wavefronts spread out and frequency decreases."), EBHDiagramType::Wave, TEXT("exam skill: explain Doppler observations"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=4.0f, .LabelA=TEXT("approaches"), .LabelB=TEXT("moves away")}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Wave labels"), TEXT("On the wave sketch, what is amplitude?"), {TEXT("Distance from equilibrium to maximum displacement"), TEXT("Distance between two wavefronts only"), TEXT("Number of waves per second"), TEXT("Speed times time")}, 0, TEXT("Amplitude is wave height from the middle line."), TEXT("Do not measure crest to trough."), TEXT("Amplitude is the distance from equilibrium to maximum displacement."), EBHDiagramType::Wave, TEXT("amplitude = max displacement from equilibrium"), 0.0f, 0.0f, {.ValueA=0.35f, .ValueB=2.0f, .LabelA=TEXT("amplitude"), .LabelB=TEXT("equilibrium")}},
		{TEXT("Oscilloscope"), TEXT("Which oscilloscope trace has the louder sound?"), {TEXT("The trace with greater amplitude"), TEXT("The trace with greater period"), TEXT("The trace with lower frequency"), TEXT("The trace with a flat line")}, 0, TEXT("Loudness is linked to amplitude."), TEXT("Compare wave height."), TEXT("Greater amplitude means louder sound."), EBHDiagramType::Wave, TEXT("greater amplitude -> louder"), 0.0f, 0.0f, {.ValueA=0.35f, .ValueB=3.0f, .LabelA=TEXT("amplitude"), .LabelB=TEXT("loudness")}},
		{TEXT("Pitch"), TEXT("Which trace has higher pitch?"), {TEXT("More cycles per second"), TEXT("Lower amplitude only"), TEXT("Longer period only"), TEXT("No wavefronts")}, 0, TEXT("Pitch is linked to frequency."), TEXT("Count cycles in the same time window."), TEXT("Higher frequency means higher pitch."), EBHDiagramType::Wave, TEXT("higher f -> higher pitch"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=4.0f, .LabelA=TEXT("cycles/s"), .LabelB=TEXT("pitch")}},
		{TEXT("TIR ray diagram"), TEXT("On the ray diagram, what happens when i is greater than the critical angle?"), {TEXT("Total internal reflection"), TEXT("Refraction along the normal"), TEXT("Absorption only"), TEXT("Frequency becomes zero")}, 0, TEXT("The ray stays inside the denser material."), TEXT("Check the direction is dense to less dense."), TEXT("If i is greater than c, total internal reflection occurs."), EBHDiagramType::RayDiagram, TEXT("i > c -> TIR"), 0.0f, 0.0f, {.LabelA=TEXT("i > c"), .AngleOrShape=55.0f}},
		{TEXT("Reading amplitude"), TEXT("The wave on the screen oscillates between +4 cm and -4 cm about the middle line. What is its amplitude?"), {TEXT("4 cm"), TEXT("8 cm"), TEXT("2 cm"), TEXT("0 cm")}, 0, TEXT("Amplitude is measured from the middle line to a crest."), TEXT("Do not use the crest-to-trough distance."), TEXT("Amplitude is from equilibrium to the crest, so 4 cm; crest-to-trough is 8 cm."), EBHDiagramType::Wave, TEXT("amplitude = max displacement"), 4.0f, 0.1f, {.ValueA=0.30f, .ValueB=3.0f, .LabelA=TEXT("crest"), .LabelB=TEXT("trough")}},
		{TEXT("Law of reflection"), TEXT("A ray hits a plane mirror at 30 deg to the normal, as shown. What is the angle of reflection?"), {TEXT("30 deg"), TEXT("60 deg"), TEXT("90 deg"), TEXT("15 deg")}, 0, TEXT("Angle of incidence equals angle of reflection."), TEXT("Both angles are measured from the normal."), TEXT("By the law of reflection, the angle of reflection equals the angle of incidence: 30 deg."), EBHDiagramType::RayDiagram, TEXT("angle i = angle r"), 30.0f, 0.1f, {.LabelA=TEXT("normal"), .AngleOrShape=30.0f}},
		{TEXT("Frequency from a trace"), TEXT("An oscilloscope trace shows 4 complete waves in 0.02 s. What is the frequency?"), {TEXT("200 Hz"), TEXT("0.005 Hz"), TEXT("80 Hz"), TEXT("4 Hz")}, 0, TEXT("f = number of cycles / time."), TEXT("Divide the cycle count by the time shown."), TEXT("f = 4 / 0.02 = 200 Hz."), EBHDiagramType::Wave, TEXT("f = cycles / time"), 200.0f, 1.0f, {.ValueA=0.25f, .ValueB=4.0f, .LabelA=TEXT("4 cycles"), .LabelB=TEXT("0.02 s")}},
		{TEXT("Refraction at a boundary"), TEXT("The ray diagram shows light passing from air into glass at 40 deg to the normal. Which way does the refracted ray bend?"), {TEXT("Towards the normal"), TEXT("Away from the normal"), TEXT("Straight back out"), TEXT("Along the surface")}, 0, TEXT("Entering a denser medium slows light down."), TEXT("Slower medium means it bends toward the normal."), TEXT("Going from air into denser glass, the ray slows and bends towards the normal."), EBHDiagramType::RayDiagram, TEXT("denser -> bend toward normal"), 0.0f, 0.0f, {.LabelA=TEXT("i = 40 deg air->glass"), .AngleOrShape=40.0f}},
		{TEXT("Wave equation"), TEXT("The displacement-distance graph shows a wavelength of 0.5 m and the source vibrates at 200 Hz. What is the wave speed?"), {TEXT("100 m/s"), TEXT("400 m/s"), TEXT("0.0025 m/s"), TEXT("200.5 m/s")}, 0, TEXT("Use v = f x lambda."), TEXT("Multiply frequency by wavelength."), TEXT("v = f x lambda = 200 x 0.5 = 100 m/s."), EBHDiagramType::Wave, TEXT("v = f x lambda"), 100.0f, 1.0f, {.ValueA=0.28f, .ValueB=2.0f, .LabelA=TEXT("lambda = 0.5 m"), .LabelB=TEXT("f = 200 Hz")}},
		{TEXT("EM spectrum use"), TEXT("On the EM spectrum strip, a wave sits just beyond the red end of visible light at a longer wavelength. Which part is it, and a use?"), {TEXT("Infrared - thermal imaging"), TEXT("Ultraviolet - sun beds"), TEXT("Gamma - sterilising"), TEXT("X-ray - bone scans")}, 0, TEXT("Longer than visible red is infrared."), TEXT("Move from visible towards longer wavelength."), TEXT("Just beyond red is infrared, used for thermal imaging and remote controls."), EBHDiagramType::EMSpectrum, TEXT("IR: longer than red"), 0.0f, 0.0f, {.LabelA=TEXT("beyond red"), .LabelB=TEXT("longer lambda")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Wave type"), TEXT("Choose the correct matching pair."), {TEXT("Light -> transverse; sound -> longitudinal"), TEXT("Light -> longitudinal; sound -> transverse"), TEXT("Radio -> longitudinal; sound -> transverse"), TEXT("Water only -> EM; gamma -> sound")}, 0, TEXT("All EM waves are transverse; sound is longitudinal."), TEXT("Match examples to vibration direction."), TEXT("Light is transverse; sound is longitudinal."), EBHDiagramType::Wave, TEXT("light transverse, sound longitudinal"), 0.0f, 0.0f},
		{TEXT("EM uses"), TEXT("Match the EM wave to a use."), {TEXT("Microwave -> satellite; X-ray -> medical imaging"), TEXT("Gamma -> TV remote; radio -> sterilising food"), TEXT("Infrared -> security scan bones; ultraviolet -> cooking food"), TEXT("Visible -> ionising cancer treatment; microwave -> fluorescent lamp")}, 0, TEXT("Microwaves communicate with satellites; X-rays image bones."), TEXT("Separate uses from hazards."), TEXT("Microwaves are used for satellite transmissions; X-rays for medical imaging."), EBHDiagramType::EMSpectrum, TEXT("EM uses by frequency"), 0.0f, 0.0f, {.LabelA=TEXT("wave"), .LabelB=TEXT("use"), .ShapeVariant=0}},
		{TEXT("EM hazards"), TEXT("Match the hazard correctly."), {TEXT("Ultraviolet -> skin cancer risk; microwave -> internal heating"), TEXT("Radio -> ionising mutations; visible -> deep tissue heating"), TEXT("Infrared -> no hazard; gamma -> harmless photos"), TEXT("Microwave -> sunburn only; X-rays -> skin warming only")}, 0, TEXT("High-energy waves can ionise; microwaves heat water-containing tissue."), TEXT("Put hazard beside the wave."), TEXT("Ultraviolet increases skin cancer risk; microwaves can cause internal heating."), EBHDiagramType::EMSpectrum, TEXT("hazards depend on frequency/energy"), 0.0f, 0.0f, {.LabelA=TEXT("wave"), .LabelB=TEXT("hazard"), .ShapeVariant=0}},
		{TEXT("Refraction"), TEXT("Match the medium change to bending direction."), {TEXT("Denser -> towards normal; less dense -> away from normal"), TEXT("Denser -> away; less dense -> towards"), TEXT("Both -> along boundary"), TEXT("Both -> no change in wavelength")}, 0, TEXT("Slower in denser means towards normal."), TEXT("Draw the normal before deciding."), TEXT("Light bends towards the normal entering a denser medium and away entering a less dense medium."), EBHDiagramType::RayDiagram, TEXT("denser towards, less dense away"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("medium")}}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("EM spectrum"), TEXT("Order the EM spectrum from longest wavelength to shortest."), {TEXT("Radio -> microwave -> infrared -> visible -> ultraviolet -> X-rays -> gamma"), TEXT("Gamma -> X-rays -> ultraviolet -> visible -> infrared -> microwave -> radio"), TEXT("Visible -> radio -> gamma -> microwave -> infrared -> X-rays -> ultraviolet"), TEXT("Radio -> visible -> infrared -> gamma -> microwave -> ultraviolet -> X-rays")}, 0, TEXT("Start with radio and end with gamma."), TEXT("Use the standard EM spectrum order."), TEXT("The standard order is radio, microwave, infrared, visible, ultraviolet, X-rays, gamma."), EBHDiagramType::EMSpectrum, TEXT("radio to gamma"), 0.0f, 0.0f, {.LabelA=TEXT("longest"), .LabelB=TEXT("shortest"), .ShapeVariant=0}},
		{TEXT("Visible colours"), TEXT("Order visible light from longest wavelength to shortest."), {TEXT("Red -> orange -> yellow -> green -> blue -> indigo -> violet"), TEXT("Violet -> indigo -> blue -> green -> yellow -> orange -> red"), TEXT("Green -> red -> blue -> violet -> orange -> yellow -> indigo"), TEXT("Red -> violet -> orange -> indigo -> yellow -> blue -> green")}, 0, TEXT("Use ROYGBIV."), TEXT("Red is longest in visible light."), TEXT("Visible colours from longest to shortest wavelength are ROYGBIV."), EBHDiagramType::EMSpectrum, TEXT("ROYGBIV"), 0.0f, 0.0f, {.LabelA=TEXT("longest"), .LabelB=TEXT("shortest"), .ShapeVariant=4}},
		{TEXT("Doppler effect"), TEXT("Order the approaching-source explanation."), {TEXT("Source approaches -> wavefronts bunch -> wavelength shorter -> frequency higher"), TEXT("Frequency higher -> source approaches -> wavelength longer -> wavefronts spread"), TEXT("Wavefronts spread -> source approaches -> frequency lower -> pitch higher"), TEXT("Source stops -> wavelength zero -> frequency zero -> pitch high")}, 0, TEXT("Approaching source bunches wavefronts."), TEXT("Short wavelength means high frequency for same wave speed."), TEXT("Approach bunches wavefronts, reducing wavelength and increasing observed frequency."), EBHDiagramType::Wave, TEXT("approach -> higher observed f"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=4.0f, .LabelA=TEXT("approaching"), .LabelB=TEXT("wavefronts")}},
		{TEXT("Optical fibre"), TEXT("Order how an optical fibre carries a light signal."), {TEXT("Light enters core -> hits boundary above c -> TIR repeats -> signal travels"), TEXT("Light enters air -> frequency stops -> TIR breaks -> signal disappears"), TEXT("Cladding absorbs light -> core heats -> signal becomes sound -> exits"), TEXT("Ray bends out -> angle lowers -> charge builds -> printer fires")}, 0, TEXT("Optical fibres use repeated total internal reflection."), TEXT("Keep the ray inside the glass core."), TEXT("Light repeatedly undergoes total internal reflection inside the fibre."), EBHDiagramType::RayDiagram, TEXT("optical fibre uses TIR"), 0.0f, 0.0f, {.LabelA=TEXT("core"), .LabelB=TEXT("TIR repeats")}}
	};

	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves"), TEXT("Waves"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves"), TEXT("Waves"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves"), TEXT("Waves"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves"), TEXT("Waves"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves"), TEXT("Waves"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves"), TEXT("Waves"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves"), TEXT("Waves"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

void BuildEnergy(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Energy stores"), TEXT("Which store does a moving student have while escaping?"), {TEXT("Kinetic"), TEXT("Nuclear"), TEXT("Electrostatic only"), TEXT("Magnetic only")}, 0, TEXT("Movement means kinetic energy."), TEXT("Name the store linked to motion."), TEXT("A moving object has kinetic energy."), EBHDiagramType::EnergyChain, TEXT("Ek = 1/2 mv^2"), 0.0f, 0.0f, {.LabelA=TEXT("moving"), .LabelB=TEXT("store = ?")}},
		{TEXT("Energy pathways"), TEXT("Which pathway transfers energy when a force moves a desk?"), {TEXT("Mechanically"), TEXT("By heating only"), TEXT("By radiation only"), TEXT("Nuclearly")}, 0, TEXT("A force moving an object is mechanical work."), TEXT("Link force and distance."), TEXT("Energy is transferred mechanically when a force moves an object."), EBHDiagramType::EnergyChain, TEXT("work done = energy transferred"), 0.0f, 0.0f, {.LabelA=TEXT("force"), .LabelB=TEXT("moves desk"), .LabelC=TEXT("pathway = ?")}},
		{TEXT("Conservation"), TEXT("What does conservation of energy mean?"), {TEXT("Total energy before equals total energy after"), TEXT("Energy is always useful"), TEXT("Energy can be destroyed by friction"), TEXT("Only batteries store energy")}, 0, TEXT("Energy changes store or pathway."), TEXT("Separate useful and wasted transfers."), TEXT("Energy is conserved; total energy before equals total energy after."), EBHDiagramType::Sankey, TEXT("energy in = energy out"), 0.0f, 0.0f, {.LabelA=TEXT("energy in"), .LabelB=TEXT("useful out"), .LabelC=TEXT("wasted out")}},
		{TEXT("Efficiency"), TEXT("What do wider arrows show on a Sankey diagram?"), {TEXT("More energy"), TEXT("Less time"), TEXT("More mass"), TEXT("Higher current only")}, 0, TEXT("Sankey arrow width is proportional to energy."), TEXT("Compare useful and wasted arrows."), TEXT("Wider arrows represent larger energy transfers."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("useful"), .LabelC=TEXT("wasted")}},
		{TEXT("Conduction"), TEXT("Why are metals usually good thermal conductors?"), {TEXT("They have free electrons"), TEXT("They have no particles"), TEXT("They trap all infrared"), TEXT("They are always black")}, 0, TEXT("Free electrons transfer energy by collisions."), TEXT("Mention vibrating particles and free electrons."), TEXT("Metals conduct well because free electrons transfer thermal energy through collisions."), EBHDiagramType::EnergyChain, TEXT("metals: free electrons transfer thermal energy"), 0.0f, 0.0f, {.LabelA=TEXT("hot end"), .LabelB=TEXT("metal rod"), .LabelC=TEXT("cool end")}},
		{TEXT("Convection"), TEXT("What happens to heated fluid in a convection current?"), {TEXT("It expands, becomes less dense, and rises"), TEXT("It contracts, becomes denser, and rises"), TEXT("It freezes immediately"), TEXT("It emits gamma rays")}, 0, TEXT("Warm fluid rises."), TEXT("Use density changes to explain motion."), TEXT("Heated fluid expands, becomes less dense, and rises."), EBHDiagramType::EnergyChain, TEXT("heated fluid rises"), 0.0f, 0.0f, {.LabelA=TEXT("heated fluid"), .LabelB=TEXT("density = ?"), .LabelC=TEXT("motion = ?")}},
		{TEXT("Radiation"), TEXT("Which surface is best at absorbing and emitting infrared?"), {TEXT("Black and dull"), TEXT("White and shiny"), TEXT("Transparent and cold"), TEXT("Silver and polished only")}, 0, TEXT("Black dull surfaces are best absorbers and emitters."), TEXT("Shiny white surfaces reflect infrared."), TEXT("Black dull surfaces are the best absorbers and emitters of infrared radiation."), EBHDiagramType::EnergyChain, TEXT("black dull absorbs/emits best"), 0.0f, 0.0f, {.LabelA=TEXT("surface"), .LabelB=TEXT("infrared")}},
		{TEXT("Renewables"), TEXT("Which resource is renewable?"), {TEXT("Wind"), TEXT("Coal"), TEXT("Oil"), TEXT("Natural gas")}, 0, TEXT("Renewable resources are replenished quickly."), TEXT("Ask whether it will run out on human timescales."), TEXT("Wind is renewable because it is replenished as it is used."), EBHDiagramType::EnergyChain, TEXT("renewable = replenished"), 0.0f, 0.0f, {.LabelA=TEXT("resource"), .LabelB=TEXT("renewable?")}},
		{TEXT("Nuclear"), TEXT("What is a main disadvantage of nuclear power?"), {TEXT("Radioactive waste must be stored safely"), TEXT("It releases no useful energy"), TEXT("It can only work at night"), TEXT("It burns coal directly")}, 0, TEXT("Nuclear power produces long-lived radioactive waste."), TEXT("Balance high output against waste."), TEXT("Nuclear power uses small fuel amounts but produces radioactive waste needing safe storage."), EBHDiagramType::EnergyChain, TEXT("nuclear -> radioactive waste"), 0.0f, 0.0f, {.LabelA=TEXT("nuclear fuel"), .LabelB=TEXT("power"), .LabelC=TEXT("waste = ?")}},
		{TEXT("Generation chains"), TEXT("What is the main useful energy transfer in a solar cell?"), {TEXT("Light energy directly to electrical energy"), TEXT("Chemical to kinetic to sound only"), TEXT("Nuclear to thermal to light"), TEXT("Electrical to gravitational")}, 0, TEXT("Solar cells do not need a turbine."), TEXT("Name input and output stores."), TEXT("A solar cell transfers light energy directly to electrical energy."), EBHDiagramType::EnergyChain, TEXT("solar cell: light -> electrical"), 0.0f, 0.0f, {.LabelA=TEXT("solar cell"), .LabelB=TEXT("input = ?"), .LabelC=TEXT("output = ?")}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Conservation"), TEXT("True or false: wasted energy is destroyed."), {TEXT("False"), TEXT("True"), TEXT("Only in motors"), TEXT("Only in lamps")}, 0, TEXT("Wasted energy is dissipated to surroundings."), TEXT("Use conservation of energy."), TEXT("Wasted energy is transferred to less useful stores, usually thermal, not destroyed."), EBHDiagramType::Sankey, TEXT("energy conserved"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("useful"), .LabelC=TEXT("wasted")}},
		{TEXT("Radiation"), TEXT("True or false: infrared radiation can transfer energy through a vacuum."), {TEXT("True"), TEXT("False"), TEXT("Only through solids"), TEXT("Only by convection")}, 0, TEXT("Radiation does not need a medium."), TEXT("Think of heat from the Sun."), TEXT("Infrared radiation can travel through a vacuum."), EBHDiagramType::EnergyChain, TEXT("radiation needs no medium"), 0.0f, 0.0f, {.LabelA=TEXT("infrared"), .LabelB=TEXT("vacuum")}},
		{TEXT("Power"), TEXT("True or false: power is the rate of energy transfer."), {TEXT("True"), TEXT("False"), TEXT("Only force"), TEXT("Only distance")}, 0, TEXT("Power is energy per time."), TEXT("Use watts as joules per second."), TEXT("Power is the rate of energy transfer or rate of doing work."), EBHDiagramType::EnergyChain, TEXT("P = W / t"), 0.0f, 0.0f, {.LabelA=TEXT("energy"), .LabelB=TEXT("time")}},
		{TEXT("Resources"), TEXT("True or false: fossil fuels release greenhouse gases that contribute to global warming."), {TEXT("True"), TEXT("False"), TEXT("Only wind turbines do"), TEXT("Only solar cells do")}, 0, TEXT("Coal, oil, and gas are fossil fuels."), TEXT("Link combustion to carbon dioxide."), TEXT("Burning fossil fuels releases greenhouse gases that contribute to global warming."), EBHDiagramType::EnergyChain, TEXT("fossil fuels -> greenhouse gases"), 0.0f, 0.0f, {.LabelA=TEXT("fossil fuel"), .LabelB=TEXT("burning"), .LabelC=TEXT("gases = ?")}},
		{TEXT("Convection"), TEXT("True or false: convection is thermal transfer mainly in fluids."), {TEXT("True"), TEXT("False"), TEXT("Only in solids"), TEXT("Only by light")}, 0, TEXT("Fluids are liquids and gases."), TEXT("Use rising warm fluid and sinking cool fluid."), TEXT("Convection transfers thermal energy in fluids by circulation."), EBHDiagramType::EnergyChain, TEXT("convection in fluids"), 0.0f, 0.0f, {.LabelA=TEXT("fluid"), .LabelB=TEXT("thermal")}}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Efficiency"), TEXT("A generator gets 500 J input and gives 350 J useful output. What is efficiency?"), {TEXT("70%"), TEXT("35%"), TEXT("150%"), TEXT("850%")}, 0, TEXT("Useful output divided by total input times 100."), TEXT("Show 350 / 500 x 100."), TEXT("Efficiency = 350 / 500 x 100 = 70%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 70.0f, 0.5f, {.ValueA=500.0f, .ValueB=350.0f, .LabelA=TEXT("in 500 J"), .LabelB=TEXT("useful 350 J")}},
		{TEXT("Work done"), TEXT("A 20 N force moves a crate 3 m. What work is done?"), {TEXT("60 J"), TEXT("6.7 J"), TEXT("23 J"), TEXT("17 J")}, 0, TEXT("Use W = Fd."), TEXT("Multiply force by distance."), TEXT("W = 20 x 3 = 60 J."), EBHDiagramType::EnergyChain, TEXT("W = Fd"), 60.0f, 0.1f, {.LabelA=TEXT("20 N"), .LabelB=TEXT("3 m")}},
		{TEXT("Kinetic energy"), TEXT("A 2 kg object moves at 4 m/s. What is its kinetic energy?"), {TEXT("16 J"), TEXT("8 J"), TEXT("32 J"), TEXT("4 J")}, 0, TEXT("Use Ek = 1/2 mv^2."), TEXT("Square the velocity first."), TEXT("Ek = 0.5 x 2 x 4^2 = 16 J."), EBHDiagramType::EnergyChain, TEXT("Ek = 1/2 mv^2"), 16.0f, 0.1f, {.LabelA=TEXT("2 kg"), .LabelB=TEXT("4 m/s")}},
		{TEXT("GPE"), TEXT("A 3 kg box is lifted 2 m. Use g = 10 N/kg. What GPE is gained?"), {TEXT("60 J"), TEXT("15 J"), TEXT("6 J"), TEXT("30 J")}, 0, TEXT("Use Ep = mgh."), TEXT("Multiply mass, g, and height."), TEXT("Ep = 3 x 10 x 2 = 60 J."), EBHDiagramType::EnergyChain, TEXT("Ep = mgh"), 60.0f, 0.1f, {.LabelA=TEXT("3 kg"), .LabelB=TEXT("2 m"), .LabelC=TEXT("g=10 N/kg")}},
		{TEXT("Power"), TEXT("A machine transfers 900 J in 30 s. What is its power?"), {TEXT("30 W"), TEXT("27000 W"), TEXT("930 W"), TEXT("0.033 W")}, 0, TEXT("Use P = W / t."), TEXT("Divide energy transferred by time."), TEXT("P = 900 / 30 = 30 W."), EBHDiagramType::EnergyChain, TEXT("P = W / t"), 30.0f, 0.1f, {.LabelA=TEXT("900 J"), .LabelB=TEXT("30 s")}},
		{TEXT("Sankey"), TEXT("A lamp takes 100 J. 25 J is useful light. How much is wasted?"), {TEXT("75 J"), TEXT("125 J"), TEXT("25 J"), TEXT("4 J")}, 0, TEXT("Input = useful + wasted."), TEXT("Subtract useful from total input."), TEXT("Wasted energy = 100 - 25 = 75 J."), EBHDiagramType::Sankey, TEXT("input = useful + wasted"), 75.0f, 0.1f, {.ValueA=100.0f, .ValueB=25.0f, .LabelA=TEXT("in 100 J"), .LabelB=TEXT("useful 25 J"), .LabelC=TEXT("wasted = ?")}},
		{TEXT("Power and work"), TEXT("A student does 240 J of work in 12 s lifting equipment. What is power?"), {TEXT("20 W"), TEXT("2880 W"), TEXT("252 W"), TEXT("0.05 W")}, 0, TEXT("Power is work divided by time."), TEXT("Show 240 / 12."), TEXT("P = 240 / 12 = 20 W."), EBHDiagramType::EnergyChain, TEXT("P = W / t"), 20.0f, 0.1f, {.LabelA=TEXT("240 J"), .LabelB=TEXT("12 s")}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Sankey evaluation"), TEXT("A motor has a wide wasted thermal arrow and a narrow useful kinetic arrow. Which IGCSE conclusion is best?"), {TEXT("The motor is inefficient because most input energy is dissipated as thermal energy"), TEXT("The motor is efficient because wasted arrows are always ignored"), TEXT("Energy has been destroyed in the motor"), TEXT("The useful output is larger because the arrow is narrower")}, 0, TEXT("Sankey arrow width represents energy."), TEXT("Compare useful output with wasted output and use the word dissipated."), TEXT("A wide wasted arrow means much of the input energy is dissipated, so the motor has low efficiency."), EBHDiagramType::Sankey, TEXT("exam skill: interpret a Sankey diagram"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("narrow useful"), .LabelC=TEXT("wide wasted")}},
		{TEXT("Energy stores"), TEXT("A toy car rolls down a ramp and speeds up. Which energy account is most complete?"), {TEXT("Gravitational potential energy decreases while kinetic energy and thermal energy increase"), TEXT("Kinetic energy changes directly into nuclear energy"), TEXT("Energy is created because the car speeds up"), TEXT("Only elastic energy changes")}, 0, TEXT("Track the store before and after the ramp."), TEXT("Include the useful kinetic increase and wasted thermal transfer."), TEXT("As height decreases, gravitational potential energy is transferred mainly to kinetic energy, with some dissipated thermally by friction."), EBHDiagramType::EnergyChain, TEXT("exam skill: describe stores and dissipation"), 0.0f, 0.0f, {.LabelA=TEXT("on ramp"), .LabelB=TEXT("rolling"), .LabelC=TEXT("speeds up")}},
		{TEXT("Insulation practical"), TEXT("Two identical cans of hot water are tested; one has a lid and insulation. Which is the best control variable?"), {TEXT("Same starting temperature and same volume of water"), TEXT("Different room temperatures"), TEXT("Different can sizes"), TEXT("Different thermometers and no repeats")}, 0, TEXT("A fair test changes only the insulation."), TEXT("Name variables that must be kept the same."), TEXT("To compare insulation fairly, keep the water volume, starting temperature, container size, and surroundings the same."), EBHDiagramType::EnergyChain, TEXT("exam skill: plan a fair thermal experiment"), 0.0f, 0.0f, {.LabelA=TEXT("hot water"), .LabelB=TEXT("can + lid"), .LabelC=TEXT("surroundings")}},
		{TEXT("Specific heat capacity"), TEXT("A metal block is heated electrically. Which data must be measured to find its specific heat capacity?"), {TEXT("Mass, energy supplied, and temperature rise"), TEXT("Only final temperature"), TEXT("Colour, surface area, and room brightness"), TEXT("Current only")}, 0, TEXT("Use energy transferred, mass, and change in temperature."), TEXT("Think of E = mc delta T but focus on measurements."), TEXT("Specific heat capacity is found from energy supplied divided by mass and temperature rise."), EBHDiagramType::EnergyChain, TEXT("exam skill: identify required measurements"), 0.0f, 0.0f, {.LabelA=TEXT("electrical"), .LabelB=TEXT("metal block"), .LabelC=TEXT("warms")}},
		{TEXT("Resource evaluation"), TEXT("A village chooses between diesel generators and wind turbines. Which is the strongest physics comparison?"), {TEXT("Wind is renewable and has no fuel cost, but output varies with weather"), TEXT("Diesel is renewable because it can be delivered by truck"), TEXT("Wind always gives the same power output"), TEXT("Diesel produces no greenhouse gases")}, 0, TEXT("Compare reliability, renewability, and emissions."), TEXT("Give one advantage and one limitation."), TEXT("Wind is renewable and low-emission in use, but its power output is variable; diesel is reliable but non-renewable and emits greenhouse gases."), EBHDiagramType::EnergyChain, TEXT("exam skill: evaluate energy resources"), 0.0f, 0.0f, {.LabelA=TEXT("diesel"), .LabelB=TEXT("vs"), .LabelC=TEXT("wind")}},
		{TEXT("Power station explanation"), TEXT("In a fossil-fuel power station, where is energy transferred to the generator?"), {TEXT("Steam turns a turbine, and the turbine drives the generator"), TEXT("Coal changes directly into electrical energy in the wires"), TEXT("The cooling tower produces the current"), TEXT("The transformer burns fuel to make steam")}, 0, TEXT("Follow fuel, boiler, steam, turbine, generator."), TEXT("Name the turbine before the generator."), TEXT("Fuel heats water to make steam; steam turns a turbine, and the turbine drives the generator to produce electrical energy."), EBHDiagramType::EnergyChain, TEXT("exam skill: explain an energy transfer chain"), 0.0f, 0.0f, {.LabelA=TEXT("fuel"), .LabelB=TEXT("steam"), .LabelC=TEXT("order = ?")}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Sankey diagram"), TEXT("On the Sankey display, which arrow is useful energy?"), {TEXT("The labelled output doing the intended job"), TEXT("Only the widest wasted arrow"), TEXT("The input arrow after it splits"), TEXT("Any arrow pointing downward")}, 0, TEXT("Useful means what the device is meant to produce."), TEXT("Compare useful output with wasted output."), TEXT("The useful arrow is the output energy transferred for the intended purpose."), EBHDiagramType::Sankey, TEXT("useful output / total input"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("useful"), .LabelC=TEXT("wasted")}},
		{TEXT("Sankey calculation"), TEXT("The Sankey input is 200 J and wasted arrow is 80 J. What is useful output?"), {TEXT("120 J"), TEXT("280 J"), TEXT("80 J"), TEXT("40 J")}, 0, TEXT("Input equals useful plus wasted."), TEXT("Subtract wasted from input."), TEXT("Useful output = 200 - 80 = 120 J."), EBHDiagramType::Sankey, TEXT("input = useful + wasted"), 120.0f, 0.1f, {.ValueA=200.0f, .ValueC=80.0f, .LabelA=TEXT("in 200 J"), .LabelB=TEXT("useful = ?"), .LabelC=TEXT("wasted 80 J")}},
		{TEXT("Heating graph"), TEXT("Which surface would give the strongest infrared emission in the display?"), {TEXT("Black dull large hot surface"), TEXT("White shiny small cool surface"), TEXT("Transparent cold surface"), TEXT("Silver polished cold surface")}, 0, TEXT("Hotter, larger, black dull surfaces emit more infrared."), TEXT("Check colour, texture, temperature, and area."), TEXT("Black dull, hot, large surfaces are strongest emitters."), EBHDiagramType::EnergyChain, TEXT("emission increases with temperature and area"), 0.0f, 0.0f, {.LabelA=TEXT("surface"), .LabelB=TEXT("infrared out")}},
		{TEXT("Efficiency bar"), TEXT("A Sankey shows 80 J useful from 100 J input. Which efficiency bar is correct?"), {TEXT("80%"), TEXT("20%"), TEXT("125%"), TEXT("180%")}, 0, TEXT("Useful divided by input times 100."), TEXT("Show 80 / 100 x 100."), TEXT("Efficiency = 80%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 80.0f, 0.1f, {.ValueA=100.0f, .ValueB=80.0f, .LabelA=TEXT("in 100 J"), .LabelB=TEXT("useful 80 J")}},
		{TEXT("Reading a Sankey"), TEXT("The Sankey diagram shows 100 J input splitting into a wide light arrow and a thin heat arrow. Which is the useful output for a lamp?"), {TEXT("The light arrow"), TEXT("The heat arrow"), TEXT("The input arrow"), TEXT("The widest arrow always")}, 0, TEXT("Useful is the energy you want from the device."), TEXT("Match the output to the device's job."), TEXT("For a lamp the useful output is light; the heat is wasted."), EBHDiagramType::Sankey, TEXT("useful = intended output"), 0.0f, 0.0f, {.ValueA=100.0f, .LabelA=TEXT("in 100 J"), .LabelB=TEXT("light = useful"), .LabelC=TEXT("heat = wasted")}},
		{TEXT("Energy transfer chain"), TEXT("The diagram shows a battery torch as boxes: chemical -> ? -> light + heat. What goes in the middle box?"), {TEXT("Electrical"), TEXT("Nuclear"), TEXT("Elastic"), TEXT("Sound")}, 0, TEXT("A battery drives an electric current."), TEXT("Name the store the current carries."), TEXT("Chemical energy in the cell becomes electrical energy, then light and wasted heat."), EBHDiagramType::EnergyChain, TEXT("chemical -> electrical -> light"), 0.0f, 0.0f, {.LabelA=TEXT("chemical"), .LabelB=TEXT("?"), .LabelC=TEXT("light+heat")}},
		{TEXT("Sankey calculation"), TEXT("The Sankey diagram shows 250 J input with a 150 J wasted heat arrow. What useful energy does the output arrow carry?"), {TEXT("100 J"), TEXT("400 J"), TEXT("150 J"), TEXT("1.67 J")}, 0, TEXT("Input = useful + wasted."), TEXT("Subtract wasted from the input."), TEXT("Useful = 250 - 150 = 100 J."), EBHDiagramType::Sankey, TEXT("input = useful + wasted"), 100.0f, 0.1f, {.ValueA=250.0f, .ValueC=150.0f, .LabelA=TEXT("in 250 J"), .LabelB=TEXT("useful = ?"), .LabelC=TEXT("wasted 150 J")}},
		{TEXT("Efficiency from Sankey"), TEXT("The Sankey diagram shows 400 J input and a 300 J useful output arrow. What is the efficiency?"), {TEXT("75%"), TEXT("133%"), TEXT("100 J"), TEXT("25 J")}, 0, TEXT("efficiency = useful / total x 100%."), TEXT("Divide useful by input, then x100."), TEXT("efficiency = 300 / 400 x 100% = 75%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 75.0f, 0.1f, {.ValueA=400.0f, .ValueB=300.0f, .LabelA=TEXT("in 400 J"), .LabelB=TEXT("useful 300 J")}},
		{TEXT("Energy in a chain"), TEXT("The diagram shows a falling diver as gravitational store -> kinetic store. Ignoring air resistance, a 600 J drop in gravitational energy gives how much kinetic energy at the water?"), {TEXT("600 J"), TEXT("0 J"), TEXT("300 J"), TEXT("1200 J")}, 0, TEXT("Energy is conserved along the chain."), TEXT("With no losses, the transfer is complete."), TEXT("With no air resistance, all 600 J of the gravitational store transfers to 600 J of kinetic store."), EBHDiagramType::EnergyChain, TEXT("GPE -> KE (conserved)"), 600.0f, 1.0f, {.LabelA=TEXT("GPE store"), .LabelB=TEXT("KE = ?")}},
		{TEXT("Comparing Sankeys"), TEXT("Two Sankey diagrams use the same input. Device A's useful arrow is wider than device B's. What does that show?"), {TEXT("Device A is more efficient"), TEXT("Device B is more efficient"), TEXT("Both waste the same energy"), TEXT("Device A has no input")}, 0, TEXT("A wider useful arrow means more useful energy per input."), TEXT("Compare the useful arrow widths for equal input."), TEXT("For the same input, the wider useful output means device A transfers more usefully, so it is more efficient."), EBHDiagramType::Sankey, TEXT("wider useful -> more efficient"), 0.0f, 0.0f, {.LabelA=TEXT("A useful wider"), .LabelB=TEXT("same input")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Stores"), TEXT("Choose the correct matching pair of energy stores."), {TEXT("Raised book -> gravitational; stretched band -> elastic"), TEXT("Raised book -> nuclear; stretched band -> thermal"), TEXT("Moving cart -> chemical; battery -> kinetic"), TEXT("Hot pan -> magnetic; fuel -> electrostatic")}, 0, TEXT("Height links to gravitational; stretching links to elastic."), TEXT("Name the store from what changed."), TEXT("A raised book has gravitational potential energy; a stretched band has elastic energy."), EBHDiagramType::EnergyChain, TEXT("energy stores"), 0.0f, 0.0f, {.LabelA=TEXT("raised book"), .LabelB=TEXT("stretched band"), .LabelC=TEXT("store = ?")}},
		{TEXT("Pathways"), TEXT("Match the transfer pathway to the example."), {TEXT("Electrical: lamp circuit; radiation: infrared from heater"), TEXT("Mechanical: current in wire; heating: light through vacuum"), TEXT("Radiation: push on desk; electrical: hot air rising"), TEXT("Convection: work done by force; nuclear: sound wave")}, 0, TEXT("Electrical uses current; radiation uses waves."), TEXT("Do not confuse convection with radiation."), TEXT("A lamp circuit transfers energy electrically; a heater can transfer energy by infrared radiation."), EBHDiagramType::EnergyChain, TEXT("transfer pathways"), 0.0f, 0.0f, {.LabelA=TEXT("lamp circuit"), .LabelB=TEXT("heater IR"), .LabelC=TEXT("pathway = ?")}},
		{TEXT("Resources"), TEXT("Match each resource to renewable/non-renewable."), {TEXT("Wind -> renewable; gas -> non-renewable"), TEXT("Coal -> renewable; solar -> non-renewable"), TEXT("Nuclear fuel -> renewable; oil -> renewable"), TEXT("Tides -> non-renewable; geothermal -> non-renewable")}, 0, TEXT("Fossil fuels run out; wind is replenished."), TEXT("Sort by whether it is replaced as quickly as used."), TEXT("Wind is renewable; natural gas is non-renewable."), EBHDiagramType::EnergyChain, TEXT("renewable vs non-renewable"), 0.0f, 0.0f, {.LabelA=TEXT("wind"), .LabelB=TEXT("gas"), .LabelC=TEXT("type = ?")}},
		{TEXT("Thermal transfer"), TEXT("Match each thermal transfer correctly."), {TEXT("Conduction -> solids; convection -> fluids"), TEXT("Conduction -> vacuum only; convection -> metals only"), TEXT("Radiation -> needs particles; convection -> no medium"), TEXT("Conduction -> gamma rays; radiation -> liquid circulation")}, 0, TEXT("Convection needs fluid movement."), TEXT("Use particle collisions for conduction."), TEXT("Conduction mainly transfers through solids; convection transfers through fluids."), EBHDiagramType::EnergyChain, TEXT("conduction solids, convection fluids"), 0.0f, 0.0f, {.LabelA=TEXT("conduction"), .LabelB=TEXT("convection"), .LabelC=TEXT("medium = ?")}}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Convection"), TEXT("Order a convection current."), {TEXT("Fluid heats -> expands -> density decreases -> rises -> cool fluid sinks"), TEXT("Cool fluid rises -> density decreases -> heats -> contracts -> sinks"), TEXT("Fluid freezes -> radiation stops -> density zero -> rises"), TEXT("Particles leave solid -> current flows -> fuse melts -> fluid rises")}, 0, TEXT("Warm fluid becomes less dense."), TEXT("Start with heating."), TEXT("Heated fluid expands, becomes less dense and rises; cooler denser fluid sinks."), EBHDiagramType::EnergyChain, TEXT("heated fluid rises"), 0.0f, 0.0f, {.LabelA=TEXT("heated fluid"), .LabelB=TEXT("order = ?")}},
		{TEXT("Conduction in metals"), TEXT("Order thermal conduction in a metal rod."), {TEXT("Hot end particles vibrate more -> free electrons gain energy -> collisions transfer energy -> cooler end warms"), TEXT("Cool end emits gamma -> electrons stop -> hot end freezes -> rod warms"), TEXT("Current splits -> fuse blows -> particles vanish -> energy destroyed"), TEXT("Air rises -> particles leave metal -> rod becomes vacuum -> warms")}, 0, TEXT("Metals have free electrons."), TEXT("Use vibrations and electron collisions."), TEXT("In metals, vibrating particles and free electrons transfer energy by collisions."), EBHDiagramType::EnergyChain, TEXT("metal conduction uses free electrons"), 0.0f, 0.0f, {.LabelA=TEXT("hot end"), .LabelB=TEXT("metal rod"), .LabelC=TEXT("cool end")}},
		{TEXT("Power station"), TEXT("Order a fossil-fuel power station transfer chain."), {TEXT("Chemical store -> thermal transfer -> turbine kinetic -> generator electrical"), TEXT("Electrical -> chemical -> turbine nuclear -> thermal"), TEXT("Kinetic water -> static charge -> elastic cable -> nuclear"), TEXT("Light -> chemical coal -> magnetic heater -> gravitational")}, 0, TEXT("Fuel heats water; steam turns turbine."), TEXT("End with generator electrical output."), TEXT("Fossil fuel chemical energy heats water, producing kinetic turbine energy and electrical energy."), EBHDiagramType::EnergyChain, TEXT("chemical -> thermal -> kinetic -> electrical"), 0.0f, 0.0f, {.LabelA=TEXT("fuel"), .LabelB=TEXT("order = ?"), .LabelC=TEXT("electrical")}},
		{TEXT("Efficiency method"), TEXT("Order an efficiency calculation."), {TEXT("Identify useful output -> identify total input -> divide -> multiply by 100"), TEXT("Multiply by 100 -> identify wasted only -> divide by time -> add force"), TEXT("Find mass -> square speed -> divide by voltage -> multiply by 100"), TEXT("Read current -> add charge -> subtract force -> choose resource")}, 0, TEXT("Efficiency compares useful output with total input."), TEXT("Percentage comes at the end."), TEXT("Efficiency = useful output / total input x 100%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 0.0f, 0.0f}
	};

	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy"), TEXT("Energy"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy"), TEXT("Energy"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy"), TEXT("Energy"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy"), TEXT("Energy"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy"), TEXT("Energy"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy"), TEXT("Energy"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy"), TEXT("Energy"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

void BuildForcesExtension(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Distance-time graphs"), TEXT("A distance-time line becomes steeper after 5 s. What has happened to the speed?"), {TEXT("It has increased"), TEXT("It has become zero"), TEXT("It has changed direction only"), TEXT("It has become mass")}, 0, TEXT("Gradient on a distance-time graph is speed."), TEXT("Compare the gradients before and after 5 s."), TEXT("A steeper distance-time gradient means a greater speed."), EBHDiagramType::MotionGraph, TEXT("speed = gradient"), 0.0f, 0.0f, {.LabelA=TEXT("after 5 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Average velocity"), TEXT("Which pair must be used to calculate average velocity?"), {TEXT("Displacement and time"), TEXT("Distance and mass"), TEXT("Force and area"), TEXT("Energy and power")}, 0, TEXT("Velocity uses displacement, not total distance."), TEXT("State the direction as well as the size."), TEXT("Average velocity is displacement divided by time, with a direction."), EBHDiagramType::MotionGraph, TEXT("velocity = displacement / time"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Balanced forces"), TEXT("A crate is stationary on the floor. What is true about the vertical forces?"), {TEXT("Weight equals normal contact force"), TEXT("Weight is larger than normal contact force"), TEXT("There is no weight"), TEXT("Friction equals mass")}, 0, TEXT("Stationary means zero resultant force."), TEXT("Draw equal upward and downward arrows."), TEXT("For no vertical acceleration, the upward normal contact force balances weight."), EBHDiagramType::ForceArrows, TEXT("resultant force = 0"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelC=TEXT("weight"), .LabelD=TEXT("normal"), .ShapeVariant=1}},
		{TEXT("Friction"), TEXT("Which way does friction act on a desk sliding east?"), {TEXT("West, opposite the motion"), TEXT("East, with the motion"), TEXT("Up, away from Earth"), TEXT("Only through the centre of gravity")}, 0, TEXT("Friction opposes relative motion."), TEXT("Draw the motion arrow first, then the friction arrow."), TEXT("Friction acts opposite the direction of sliding, so it acts west."), EBHDiagramType::ForceArrows, TEXT("friction opposes motion"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("moves east"), .LabelB=TEXT("friction")}},
		{TEXT("Pressure"), TEXT("Which equation calculates pressure from a force on an area?"), {TEXT("pressure = force / area"), TEXT("pressure = mass x speed"), TEXT("pressure = work / time"), TEXT("pressure = extension / force")}, 0, TEXT("Pressure spreads a force over an area."), TEXT("Check the unit: N/m^2."), TEXT("Pressure is force divided by area."), EBHDiagramType::ForceArrows, TEXT("p = F / A"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("force"), .LabelB=TEXT("area")}},
		{TEXT("Moments"), TEXT("A longer spanner makes a bolt easier to turn because it increases what?"), {TEXT("Perpendicular distance from the pivot"), TEXT("The mass of the bolt"), TEXT("The gravitational field strength"), TEXT("The stopping distance")}, 0, TEXT("Moment depends on force and distance from pivot."), TEXT("Name the pivot and the line of action."), TEXT("Increasing perpendicular distance increases the turning moment for the same force."), EBHDiagramType::MomentBeam, TEXT("moment = Fd"), 0.0f, 0.0f, {.LabelA=TEXT("distance"), .LabelB=TEXT("force")}},
		{TEXT("Stopping distance"), TEXT("Which statement is correct for stopping distance?"), {TEXT("It is thinking distance plus braking distance"), TEXT("It is braking distance minus reaction time"), TEXT("It is only the length of the car"), TEXT("It is independent of speed")}, 0, TEXT("There are two stages before the vehicle stops."), TEXT("Name what happens before and after the brakes are applied."), TEXT("Stopping distance is thinking distance plus braking distance."), EBHDiagramType::MotionGraph, TEXT("stopping = thinking + braking"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Inertia"), TEXT("Which object has the greatest inertia?"), {TEXT("The object with the greatest mass"), TEXT("The object with the greatest temperature"), TEXT("The object with the smallest volume"), TEXT("The object with no velocity")}, 0, TEXT("Inertia is resistance to changing motion."), TEXT("Link inertia to mass."), TEXT("Greater mass means greater inertia, so motion is harder to change."), EBHDiagramType::ForceArrows, TEXT("inertia increases with mass"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("mass"), .LabelB=TEXT("inertia")}},
		{TEXT("Momentum"), TEXT("Which unit is suitable for momentum?"), {TEXT("kg m/s"), TEXT("N/m^2"), TEXT("J/s"), TEXT("m/s^2")}, 0, TEXT("Momentum is mass times velocity."), TEXT("Multiply kg by m/s."), TEXT("p = mv, so the unit is kg m/s."), EBHDiagramType::ForceArrows, TEXT("p = mv"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("mass"), .LabelB=TEXT("velocity")}},
		{TEXT("Circular motion"), TEXT("A student runs round a circular path at constant speed. Why is the velocity changing?"), {TEXT("The direction is changing"), TEXT("The mass is changing"), TEXT("The speed must be zero"), TEXT("There is no resultant force")}, 0, TEXT("Velocity is a vector."), TEXT("Check whether direction is part of velocity."), TEXT("Velocity changes whenever direction changes, even if speed stays constant."), EBHDiagramType::ForceArrows, TEXT("velocity includes direction"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("speed"), .LabelB=TEXT("direction")}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Acceleration"), TEXT("True or false: acceleration can be negative when an object slows down."), {TEXT("True"), TEXT("False"), TEXT("Only if mass is zero"), TEXT("Only in space")}, 0, TEXT("Acceleration is a change in velocity."), TEXT("Think about deceleration as acceleration in the opposite direction."), TEXT("Negative acceleration can represent slowing down in the chosen positive direction."), EBHDiagramType::VelocityGraph, TEXT("a = change in velocity / time"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=3}},
		{TEXT("Scalars"), TEXT("True or false: speed is a scalar quantity."), {TEXT("True"), TEXT("False"), TEXT("Only at constant speed"), TEXT("Only with direction")}, 0, TEXT("Scalars have magnitude only."), TEXT("Compare speed with velocity."), TEXT("Speed has size but no direction, so it is scalar."), EBHDiagramType::MotionGraph, TEXT("scalar = magnitude only"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Mass and weight"), TEXT("True or false: a student's mass changes when they move from Earth to the Moon."), {TEXT("False"), TEXT("True"), TEXT("Only if they run"), TEXT("Only if they are stationary")}, 0, TEXT("Mass is amount of matter."), TEXT("Weight changes with gravitational field strength."), TEXT("Mass stays the same; weight changes because gravitational field strength changes."), EBHDiagramType::ForceArrows, TEXT("W = mg"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("mass"), .LabelB=TEXT("weight")}},
		{TEXT("Resultant force"), TEXT("True or false: an unbalanced resultant force causes acceleration."), {TEXT("True"), TEXT("False"), TEXT("Only for springs"), TEXT("Only for light")}, 0, TEXT("Use Newton's second law."), TEXT("Link resultant force to change in velocity."), TEXT("A non-zero resultant force causes acceleration."), EBHDiagramType::ForceArrows, TEXT("F = ma"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("resultant"), .LabelB=TEXT("motion")}},
		{TEXT("Hooke's law"), TEXT("True or false: Hooke's law applies after the spring has passed its limit of proportionality."), {TEXT("False"), TEXT("True"), TEXT("Only for rubber bands"), TEXT("Only for small masses after the limit")}, 0, TEXT("The straight-line region matters."), TEXT("Find where the force-extension graph stops being linear."), TEXT("Hooke's law applies only while extension is proportional to force."), EBHDiagramType::SpringGraph, TEXT("F = kx before the limit"), 0.0f, 0.0f, {.XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Average speed"), TEXT("A student walks 150 m in 25 s. What is the average speed?"), {TEXT("6 m/s"), TEXT("0.17 m/s"), TEXT("125 m/s"), TEXT("175 m/s")}, 0, TEXT("Use distance divided by time."), TEXT("Show 150 / 25."), TEXT("Average speed = 150 / 25 = 6 m/s."), EBHDiagramType::MotionGraph, TEXT("speed = distance / time"), 6.0f, 0.1f, {.LabelA=TEXT("150 m"), .LabelB=TEXT("25 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Distance"), TEXT("A trolley moves at 12 m/s for 8 s. How far does it travel?"), {TEXT("96 m"), TEXT("20 m"), TEXT("1.5 m"), TEXT("4 m")}, 0, TEXT("Rearrange speed = distance / time."), TEXT("Multiply speed by time."), TEXT("Distance = 12 x 8 = 96 m."), EBHDiagramType::MotionGraph, TEXT("distance = speed x time"), 96.0f, 0.1f, {.LabelA=TEXT("12 m/s"), .LabelB=TEXT("8 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Acceleration"), TEXT("Velocity increases from 6 m/s to 18 m/s in 4 s. What is acceleration?"), {TEXT("3 m/s^2"), TEXT("6 m/s^2"), TEXT("12 m/s^2"), TEXT("24 m/s^2")}, 0, TEXT("Find the change in velocity first."), TEXT("Show (18 - 6) / 4."), TEXT("Acceleration = 12 / 4 = 3 m/s^2."), EBHDiagramType::VelocityGraph, TEXT("a = (v - u) / t"), 3.0f, 0.1f, {.LabelA=TEXT("6 to 18 m/s"), .LabelB=TEXT("4 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=2}},
		{TEXT("Newton's second law"), TEXT("A 2.5 kg cart accelerates at 4 m/s^2. What resultant force acts?"), {TEXT("10 N"), TEXT("1.6 N"), TEXT("6.5 N"), TEXT("25 N")}, 0, TEXT("Use F = ma."), TEXT("Multiply mass by acceleration."), TEXT("F = 2.5 x 4 = 10 N."), EBHDiagramType::ForceArrows, TEXT("F = ma"), 10.0f, 0.1f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("m = 2.5 kg"), .LabelB=TEXT("a = 4 m/s2")}},
		{TEXT("Pressure"), TEXT("A 200 N force acts over 0.50 m^2. What pressure is produced?"), {TEXT("400 Pa"), TEXT("100 Pa"), TEXT("200.5 Pa"), TEXT("0.0025 Pa")}, 0, TEXT("Pressure is force divided by area."), TEXT("Show 200 / 0.50."), TEXT("Pressure = 200 / 0.50 = 400 Pa."), EBHDiagramType::ForceArrows, TEXT("p = F / A"), 400.0f, 0.5f, {.ValueA=200.0f, .ValueB=200.0f, .LabelA=TEXT("200 N"), .LabelB=TEXT("0.50 m2")}},
		{TEXT("Momentum"), TEXT("A 0.80 kg trolley moves at 5 m/s. What is its momentum?"), {TEXT("4 kg m/s"), TEXT("6.25 kg m/s"), TEXT("0.16 kg m/s"), TEXT("5.8 kg m/s")}, 0, TEXT("Use p = mv."), TEXT("Multiply 0.80 by 5."), TEXT("Momentum = 0.80 x 5 = 4 kg m/s."), EBHDiagramType::ForceArrows, TEXT("p = mv"), 4.0f, 0.1f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("m = 0.80 kg"), .LabelB=TEXT("v = 5 m/s")}},
		{TEXT("Moment"), TEXT("A 15 N force acts 0.20 m from a pivot. What is the moment?"), {TEXT("3 Nm"), TEXT("75 Nm"), TEXT("15.2 Nm"), TEXT("0.013 Nm")}, 0, TEXT("Moment is force times perpendicular distance."), TEXT("Show 15 x 0.20."), TEXT("Moment = 15 x 0.20 = 3 Nm."), EBHDiagramType::MomentBeam, TEXT("moment = Fd"), 3.0f, 0.1f, {.ValueA=0.20f, .ValueB=15.0f, .LabelA=TEXT("0.20 m"), .LabelB=TEXT("15 N")}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Free-body diagrams"), TEXT("A lift moves upward at constant speed. Which force explanation earns full marks?"), {TEXT("Upward cable tension balances weight, so resultant force is zero"), TEXT("Tension must be larger because the lift is moving up"), TEXT("Weight disappears because the lift is moving"), TEXT("There is no normal contact force in a lift")}, 0, TEXT("Constant speed means no acceleration."), TEXT("Use balanced forces, not just direction of motion."), TEXT("At constant velocity, the resultant force is zero, so tension balances weight."), EBHDiagramType::ForceArrows, TEXT("exam skill: constant velocity means balanced forces"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelC=TEXT("weight"), .LabelD=TEXT("tension"), .ShapeVariant=1}},
		{TEXT("Stopping distance"), TEXT("A car's braking distance is longer on ice. Which explanation is best?"), {TEXT("Reduced friction gives a smaller braking force, so deceleration is lower"), TEXT("Reaction time is always zero on ice"), TEXT("Ice increases the driver's mass"), TEXT("The car has no kinetic energy on ice")}, 0, TEXT("Braking distance depends on braking force and speed."), TEXT("Link friction to resultant force and deceleration."), TEXT("Icy roads reduce friction, reducing braking force and increasing braking distance."), EBHDiagramType::MotionGraph, TEXT("exam skill: explain braking distance factors"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Terminal velocity"), TEXT("A parachute opens during a fall. What should an IGCSE answer say about the forces?"), {TEXT("Air resistance increases, resultant force acts upward, and the person decelerates"), TEXT("Weight increases, so speed increases forever"), TEXT("Both forces become zero immediately"), TEXT("Mass decreases because the parachute opens")}, 0, TEXT("Opening the parachute increases drag."), TEXT("Compare air resistance with weight just after opening."), TEXT("The large upward air resistance gives an upward resultant force, so the falling person decelerates."), EBHDiagramType::VelocityGraph, TEXT("exam skill: explain parachute motion"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=3}},
		{TEXT("Spring practical"), TEXT("A force-extension graph has one point far from a straight-line trend. What is the best action?"), {TEXT("Repeat that reading and check for a measurement error"), TEXT("Join every point with a sharp zig-zag"), TEXT("Delete all small-force readings"), TEXT("Change the unit to make it fit")}, 0, TEXT("Anomalies should be checked."), TEXT("Think about repeat readings and fair testing."), TEXT("A suspect anomalous point should be repeated and equipment checked before drawing a best-fit line."), EBHDiagramType::SpringGraph, TEXT("exam skill: handle anomalous data"), 0.0f, 0.0f, {.XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Impact safety"), TEXT("Why does a crumple zone reduce injury risk in a crash?"), {TEXT("It increases collision time, reducing average force for the same momentum change"), TEXT("It makes momentum become zero before impact"), TEXT("It removes the car's mass"), TEXT("It increases acceleration on the passenger")}, 0, TEXT("Use momentum change over time."), TEXT("State same change in momentum but larger time."), TEXT("Increasing impact time lowers average force for a similar momentum change."), EBHDiagramType::ForceArrows, TEXT("exam skill: force = momentum change / time"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("force"), .LabelB=TEXT("impact time")}},
		{TEXT("Vectors"), TEXT("Two perpendicular forces of 3 N and 4 N act on a point. What exam method is most suitable?"), {TEXT("Draw a scale vector triangle and find the resultant"), TEXT("Add the numbers and ignore direction"), TEXT("Subtract the smaller force from the larger force"), TEXT("Use speed = distance / time")}, 0, TEXT("Perpendicular vectors need direction."), TEXT("A scale diagram or Pythagoras can be used."), TEXT("Perpendicular forces combine as vectors; a scale triangle gives the resultant size and direction."), EBHDiagramType::ForceArrows, TEXT("exam skill: vector addition"), 0.0f, 0.0f, {.ValueA=3.0f, .ValueB=4.0f, .LabelA=TEXT("3 N"), .LabelB=TEXT("4 N")}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Distance-time graph"), TEXT("On a distance-time graph, which line shows the fastest object?"), {TEXT("The line with the greatest gradient"), TEXT("The horizontal line"), TEXT("The shortest line label"), TEXT("The line nearest the axis")}, 0, TEXT("Gradient is speed."), TEXT("Compare rise divided by run."), TEXT("The greatest gradient represents the greatest speed."), EBHDiagramType::MotionGraph, TEXT("speed = gradient"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Velocity-time graph"), TEXT("A velocity-time graph rises from 0 to 12 m/s in 3 s. What is the gradient?"), {TEXT("4 m/s^2"), TEXT("15 m/s^2"), TEXT("36 m/s^2"), TEXT("0.25 m/s^2")}, 0, TEXT("Gradient on a velocity-time graph is acceleration."), TEXT("Use rise over run."), TEXT("Gradient = 12 / 3 = 4 m/s^2."), EBHDiagramType::VelocityGraph, TEXT("gradient = acceleration"), 4.0f, 0.1f, {.LabelA=TEXT("0 to 12 m/s"), .LabelB=TEXT("3 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("v / m/s"), .ShapeVariant=2}},
		{TEXT("Force-extension graph"), TEXT("A force-extension graph stops being straight. What has probably been reached?"), {TEXT("The limit of proportionality"), TEXT("Terminal velocity"), TEXT("Critical angle"), TEXT("Thinking distance")}, 0, TEXT("Hooke's law gives a straight line."), TEXT("Name the point where proportionality ends."), TEXT("When the graph curves, the limit of proportionality has been reached."), EBHDiagramType::SpringGraph, TEXT("linear region -> Hooke's law"), 0.0f, 0.0f, {.XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Moment balance"), TEXT("A beam balances with 8 N at 0.30 m on one side. What moment must act on the other side?"), {TEXT("2.4 Nm opposite"), TEXT("8.3 Nm opposite"), TEXT("0.30 Nm opposite"), TEXT("24 Nm opposite")}, 0, TEXT("Balanced means clockwise moment equals anticlockwise moment."), TEXT("Calculate 8 x 0.30."), TEXT("The balancing moment is 2.4 Nm in the opposite direction."), EBHDiagramType::MomentBeam, TEXT("clockwise moment = anticlockwise moment"), 2.4f, 0.1f, {.ValueA=0.30f, .ValueB=8.0f, .LabelA=TEXT("0.30 m"), .LabelB=TEXT("8 N")}},
		{TEXT("Steepest line"), TEXT("On the distance-time graph three lines are drawn. What does the steepest line represent?"), {TEXT("The fastest object"), TEXT("The slowest object"), TEXT("A stationary object"), TEXT("The heaviest object")}, 0, TEXT("Gradient of a distance-time graph is speed."), TEXT("Steeper means more distance per second."), TEXT("The steepest distance-time line has the greatest speed."), EBHDiagramType::MotionGraph, TEXT("speed = gradient"), 0.0f, 0.0f, {.LabelA=TEXT("steepest = fastest"), .XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Turning effect"), TEXT("Two spanners undo the same bolt with equal force. The diagram shows a long spanner (0.30 m) and a short one (0.15 m). Which gives the larger moment?"), {TEXT("The 0.30 m spanner"), TEXT("The 0.15 m spanner"), TEXT("Both the same"), TEXT("Neither turns the bolt")}, 0, TEXT("moment = force x distance."), TEXT("Compare the handle lengths shown."), TEXT("The longer 0.30 m spanner gives a larger moment for the same force."), EBHDiagramType::MomentBeam, TEXT("moment = F x d"), 0.0f, 0.0f, {.ValueA=0.15f, .ValueC=0.30f, .LabelA=TEXT("short 0.15 m"), .LabelC=TEXT("long 0.30 m")}},
		{TEXT("Acceleration from v-t"), TEXT("A velocity-time graph rises from 2 m/s to 14 m/s in 4 s. What is the acceleration shown by the gradient?"), {TEXT("3 m/s^2"), TEXT("12 m/s^2"), TEXT("48 m/s^2"), TEXT("0.33 m/s^2")}, 0, TEXT("Gradient of a velocity-time graph is acceleration."), TEXT("Use (v - u) / t."), TEXT("a = (14 - 2) / 4 = 3 m/s^2."), EBHDiagramType::VelocityGraph, TEXT("a = gradient of v-t"), 3.0f, 0.1f, {.ValueA=14.0f, .ValueB=4.0f, .ValueC=2.0f, .LabelA=TEXT("2 -> 14 m/s in 4 s"), .XAxis=TEXT("t / s"), .YAxis=TEXT("v / m per s")}},
		{TEXT("Balance calculation"), TEXT("A balanced beam has a 20 N weight 0.6 m left of the pivot and a weight 0.4 m to the right. What is the right-hand weight?"), {TEXT("30 N"), TEXT("12 N"), TEXT("8 N"), TEXT("48 N")}, 0, TEXT("Clockwise moment equals anticlockwise moment."), TEXT("Set 20 x 0.6 = W x 0.4."), TEXT("20 x 0.6 = 12 Nm, so W = 12 / 0.4 = 30 N."), EBHDiagramType::MomentBeam, TEXT("F1 d1 = F2 d2"), 30.0f, 0.1f, {.ValueA=0.6f, .ValueB=20.0f, .ValueC=0.4f, .LabelA=TEXT("0.6 m"), .LabelB=TEXT("20 N"), .LabelC=TEXT("0.4 m")}},
		{TEXT("Area under F-x"), TEXT("On a force-extension graph obeying Hooke's law, what does the area under the straight line represent?"), {TEXT("Elastic potential energy stored"), TEXT("The spring constant"), TEXT("The terminal velocity"), TEXT("The momentum")}, 0, TEXT("Area under a force-extension graph."), TEXT("Think energy = average force x extension."), TEXT("The triangular area under the force-extension line is the elastic potential energy stored."), EBHDiagramType::SpringGraph, TEXT("E = area under F-x"), 0.0f, 0.0f, {.LabelA=TEXT("area = energy"), .XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}},
		{TEXT("Curved d-t graph"), TEXT("A distance-time graph curves upward, getting steeper each second. What motion does this show?"), {TEXT("Acceleration"), TEXT("Constant speed"), TEXT("Being stationary"), TEXT("Constant deceleration to rest")}, 0, TEXT("Increasing gradient means increasing speed."), TEXT("Compare the gradient early and late."), TEXT("A distance-time graph that steepens shows increasing speed, which is acceleration."), EBHDiagramType::MotionGraph, TEXT("steepening d-t -> accelerating"), 0.0f, 0.0f, {.LabelA=TEXT("gradient increases"), .XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Units"), TEXT("Choose the correct matching pair."), {TEXT("Force -> N; pressure -> Pa"), TEXT("Force -> kg; pressure -> m/s"), TEXT("Momentum -> W; moment -> A"), TEXT("Acceleration -> J; speed -> C")}, 0, TEXT("Use base equation units."), TEXT("Match each quantity to its standard unit."), TEXT("Force is measured in newtons; pressure is measured in pascals."), EBHDiagramType::ForceArrows, TEXT("units: N and Pa"), 0.0f, 0.0f},
		{TEXT("Laws"), TEXT("Match the law to the statement."), {TEXT("First law -> zero resultant keeps constant velocity; second law -> F = ma"), TEXT("First law -> P = IV; second law -> wave speed"), TEXT("Third law -> same object; second law -> no acceleration ever"), TEXT("Hooke's law -> conservation of momentum; first law -> refraction")}, 0, TEXT("Separate Newton's first and second laws."), TEXT("First law links zero resultant force to constant velocity."), TEXT("Newton's first law concerns constant velocity with zero resultant force; Newton's second law is F = ma."), EBHDiagramType::ForceArrows, TEXT("Newton laws"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("resultant"), .LabelB=TEXT("motion")}},
		{TEXT("Stopping factors"), TEXT("Match each factor to the distance it mainly affects."), {TEXT("Alcohol -> thinking; worn tyres -> braking"), TEXT("Alcohol -> braking only; worn tyres -> thinking"), TEXT("Road ice -> thinking only; tiredness -> braking only"), TEXT("Reaction time -> braking; speed -> no effect")}, 0, TEXT("Thinking is before braking starts."), TEXT("Braking depends on grip and road conditions."), TEXT("Alcohol can increase reaction time and thinking distance; worn tyres increase braking distance."), EBHDiagramType::MotionGraph, TEXT("thinking vs braking"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Practical evidence"), TEXT("Match the graph feature to the conclusion."), {TEXT("Straight force-extension line -> constant k; curve -> beyond proportionality"), TEXT("Straight line -> no spring constant; curve -> no force"), TEXT("Horizontal velocity-time line -> accelerating; steep distance-time line -> stationary"), TEXT("Equal moments -> beam spins faster")}, 0, TEXT("A straight force-extension line shows proportionality."), TEXT("Connect graph shape to the physics conclusion."), TEXT("A straight force-extension graph shows constant spring constant; a curve shows the limit has been exceeded."), EBHDiagramType::SpringGraph, TEXT("graph evidence"), 0.0f, 0.0f, {.XAxis=TEXT("x / m"), .YAxis=TEXT("F / N")}}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Speed method"), TEXT("Order a speed calculation."), {TEXT("Read distance -> read time -> divide distance by time -> add m/s"), TEXT("Read mass -> multiply by g -> divide by area -> add Pa"), TEXT("Read current -> square it -> multiply resistance -> add W"), TEXT("Read angle -> find normal -> reflect -> add Hz")}, 0, TEXT("Speed compares distance with time."), TEXT("Put the equation before the unit."), TEXT("Calculate speed by dividing distance by time and giving the unit m/s."), EBHDiagramType::MotionGraph, TEXT("speed = distance / time"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Stopping sequence"), TEXT("Order the stages when a driver stops a car."), {TEXT("Hazard seen -> driver reacts -> brakes applied -> car decelerates to rest"), TEXT("Brakes applied -> hazard seen -> driver reacts -> speed rises"), TEXT("Car stops -> driver reacts -> brakes heat -> hazard appears"), TEXT("Tyres grip -> mass vanishes -> thinking distance begins -> car accelerates")}, 0, TEXT("Thinking comes before braking."), TEXT("Separate human reaction from vehicle braking."), TEXT("The driver reacts before braking starts; then braking decelerates the car to rest."), EBHDiagramType::MotionGraph, TEXT("thinking then braking"), 0.0f, 0.0f, {.XAxis=TEXT("t / s"), .YAxis=TEXT("d / m")}},
		{TEXT("Moments method"), TEXT("Order a balancing moments method."), {TEXT("Choose pivot -> calculate clockwise moment -> calculate anticlockwise moment -> compare"), TEXT("Measure speed -> find wavelength -> multiply -> compare"), TEXT("Find current -> add voltage -> choose fuse -> compare"), TEXT("Use distance -> square mass -> subtract force -> compare")}, 0, TEXT("Moments act about a pivot."), TEXT("Calculate each side separately."), TEXT("For equilibrium, clockwise and anticlockwise moments about the same pivot are compared."), EBHDiagramType::MomentBeam, TEXT("moment = Fd"), 0.0f, 0.0f, {.LabelA=TEXT("distance"), .LabelB=TEXT("force")}},
		{TEXT("Momentum explanation"), TEXT("Order a collision safety explanation."), {TEXT("Impact time increases -> same momentum change occurs more slowly -> average force decreases"), TEXT("Force decreases -> time becomes zero -> momentum increases -> crash safer"), TEXT("Mass disappears -> velocity becomes charge -> force becomes zero"), TEXT("Momentum changes before impact -> time decreases -> force decreases")}, 0, TEXT("Use force linked to momentum change per time."), TEXT("Start with the longer collision time."), TEXT("A longer impact time spreads the momentum change, reducing average force."), EBHDiagramType::ForceArrows, TEXT("F = delta p / delta t"), 0.0f, 0.0f, {.ValueA=1.0f, .ValueB=1.0f, .LabelA=TEXT("force"), .LabelB=TEXT("impact time")}}
	};

	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces_ext"), TEXT("Forces and Motion"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces_ext"), TEXT("Forces and Motion"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces_ext"), TEXT("Forces and Motion"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces_ext"), TEXT("Forces and Motion"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces_ext"), TEXT("Forces and Motion"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces_ext"), TEXT("Forces and Motion"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::ForcesAndMotion, TEXT("forces_ext"), TEXT("Forces and Motion"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

void BuildElectricityExtension(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Current"), TEXT("What is electric current?"), {TEXT("Rate of flow of charge"), TEXT("Energy stored per kilogram"), TEXT("Force per unit area"), TEXT("Distance travelled per second")}, 0, TEXT("Current compares charge with time."), TEXT("Use the unit ampere."), TEXT("Electric current is the rate of flow of charge."), EBHDiagramType::Circuit, TEXT("I = Q / t"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("cell"), .ShapeVariant=0}},
		{TEXT("Potential difference"), TEXT("What does potential difference measure?"), {TEXT("Energy transferred per unit charge"), TEXT("Charge transferred per second"), TEXT("Mass per unit volume"), TEXT("Distance per unit time")}, 0, TEXT("Voltage is energy per charge."), TEXT("Use joules per coulomb."), TEXT("Potential difference is energy transferred per unit charge."), EBHDiagramType::Circuit, TEXT("V = E / Q"), 0.0f, 0.0f, {.LabelA=TEXT("lamp"), .LabelB=TEXT("V supply"), .ShapeVariant=0}},
		{TEXT("Series circuits"), TEXT("In a series circuit, what is the same through every component?"), {TEXT("Current"), TEXT("Potential difference"), TEXT("Resistance"), TEXT("Power")}, 0, TEXT("Series has one path."), TEXT("Trace the single loop."), TEXT("The current is the same everywhere in a series circuit."), EBHDiagramType::Circuit, TEXT("series current same"), 0.0f, 0.0f, {.LabelA=TEXT("R1"), .LabelB=TEXT("supply"), .LabelC=TEXT("R2"), .ShapeVariant=1}},
		{TEXT("Parallel circuits"), TEXT("In a parallel circuit, what is the same across each branch?"), {TEXT("Potential difference"), TEXT("Current in every branch"), TEXT("Resistance in every component"), TEXT("Fuse rating")}, 0, TEXT("Branches share the supply terminals."), TEXT("Think about voltage across each branch."), TEXT("The potential difference is the same across parallel branches."), EBHDiagramType::Circuit, TEXT("parallel p.d. same"), 0.0f, 0.0f, {.LabelA=TEXT("branch 1"), .LabelB=TEXT("supply"), .LabelC=TEXT("branch 2"), .ShapeVariant=2}},
		{TEXT("Resistance"), TEXT("What happens to current if resistance increases while voltage stays the same?"), {TEXT("Current decreases"), TEXT("Current increases"), TEXT("Current becomes voltage"), TEXT("Current is unchanged for all components")}, 0, TEXT("Use V = IR."), TEXT("Rearrange to I = V / R."), TEXT("For the same voltage, increasing resistance reduces current."), EBHDiagramType::IVGraph, TEXT("I = V / R"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Fuses"), TEXT("Where should a fuse be placed in a mains plug?"), {TEXT("In the live wire"), TEXT("In the earth wire"), TEXT("In the plastic case"), TEXT("Across the neutral and earth only")}, 0, TEXT("The fuse must disconnect the live supply."), TEXT("Name the conductor that carries dangerous voltage."), TEXT("A fuse is placed in the live wire so it can disconnect the appliance from the live supply."), EBHDiagramType::Circuit, TEXT("fuse in live wire"), 0.0f, 0.0f, {.LabelA=TEXT("fuse?"), .LabelB=TEXT("mains"), .ShapeVariant=0}},
		{TEXT("Diodes"), TEXT("What does a diode do in a simple circuit?"), {TEXT("Allows current mainly in one direction"), TEXT("Stores gravitational energy"), TEXT("Measures temperature directly"), TEXT("Always reduces voltage to zero")}, 0, TEXT("A diode has a forward direction."), TEXT("Think of one-way current flow."), TEXT("A diode conducts mainly in one direction and blocks the reverse direction."), EBHDiagramType::Circuit, TEXT("diode one-way"), 0.0f, 0.0f, {.LabelA=TEXT("diode"), .LabelB=TEXT("supply"), .ShapeVariant=0}},
		{TEXT("LDRs"), TEXT("What happens to an LDR's resistance in brighter light?"), {TEXT("It decreases"), TEXT("It increases"), TEXT("It becomes infinite every time"), TEXT("It turns into a fuse")}, 0, TEXT("LDR means light-dependent resistor."), TEXT("Bright light gives lower resistance."), TEXT("An LDR's resistance decreases as light intensity increases."), EBHDiagramType::IVGraph, TEXT("bright LDR -> lower R"), 0.0f, 0.0f, {.LabelA=TEXT("LDR"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=1}},
		{TEXT("Thermistors"), TEXT("For an NTC thermistor, what happens as temperature increases?"), {TEXT("Resistance decreases"), TEXT("Resistance increases"), TEXT("Current must be zero"), TEXT("Voltage becomes charge")}, 0, TEXT("NTC means negative temperature coefficient."), TEXT("Hotter NTC thermistors have lower resistance."), TEXT("An NTC thermistor's resistance decreases as temperature increases."), EBHDiagramType::IVGraph, TEXT("hot NTC -> lower R"), 0.0f, 0.0f, {.LabelA=TEXT("NTC"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=1}},
		{TEXT("Static charge"), TEXT("Two objects with the same type of static charge do what?"), {TEXT("Repel each other"), TEXT("Attract each other"), TEXT("Always discharge silently"), TEXT("Become neutral instantly")}, 0, TEXT("Like charges repel."), TEXT("Compare same and opposite charges."), TEXT("Objects with like charges repel; opposite charges attract."), EBHDiagramType::StaticCharge, TEXT("like charges repel"), 0.0f, 0.0f, {.LabelA=TEXT("+ + +"), .LabelB=TEXT("+ + +")}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Charge"), TEXT("True or false: charge is measured in coulombs."), {TEXT("True"), TEXT("False"), TEXT("Only in volts"), TEXT("Only in ohms")}, 0, TEXT("Current is charge per second."), TEXT("Use Q for charge."), TEXT("Electric charge is measured in coulombs, C."), EBHDiagramType::Circuit, TEXT("charge unit C"), 0.0f, 0.0f, {.LabelA=TEXT("Q in C"), .LabelB=TEXT("cell"), .ShapeVariant=0}},
		{TEXT("Parallel circuits"), TEXT("True or false: a break in one parallel branch can leave other branches working."), {TEXT("True"), TEXT("False"), TEXT("Only in series"), TEXT("Only with no battery")}, 0, TEXT("Parallel circuits have separate paths."), TEXT("Trace the remaining branch."), TEXT("Other parallel branches can still have a complete circuit."), EBHDiagramType::Circuit, TEXT("parallel has branches"), 0.0f, 0.0f, {.LabelA=TEXT("branch 1"), .LabelB=TEXT("supply"), .LabelC=TEXT("branch 2"), .ShapeVariant=2}},
		{TEXT("Ohmic conductors"), TEXT("True or false: an ohmic conductor has constant resistance at constant temperature."), {TEXT("True"), TEXT("False"), TEXT("Only when current is zero"), TEXT("Only when voltage is zero")}, 0, TEXT("Ohmic means V proportional to I."), TEXT("Look for a straight IV graph through the origin."), TEXT("At constant temperature, an ohmic conductor has constant resistance."), EBHDiagramType::IVGraph, TEXT("V proportional to I"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Mains electricity"), TEXT("True or false: the earth wire normally carries current in a correctly working appliance."), {TEXT("False"), TEXT("True"), TEXT("Only in plastic appliances"), TEXT("Only when switched off")}, 0, TEXT("The earth wire is for faults."), TEXT("Separate normal operation from fault protection."), TEXT("The earth wire carries current only during a fault, not during normal operation."), EBHDiagramType::Circuit, TEXT("earth wire fault path"), 0.0f, 0.0f, {.LabelA=TEXT("earth"), .LabelB=TEXT("mains"), .ShapeVariant=0}},
		{TEXT("Static induction"), TEXT("True or false: a charged object can attract a neutral insulator by inducing charge separation."), {TEXT("True"), TEXT("False"), TEXT("Only if both are metals"), TEXT("Only if current is AC")}, 0, TEXT("Charges can shift slightly inside neutral objects."), TEXT("Think of the near side becoming oppositely charged."), TEXT("Charge separation can make a neutral insulator attracted to a charged object."), EBHDiagramType::StaticCharge, TEXT("induced charge separation"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Charge flow"), TEXT("A 2 A current flows for 30 s. How much charge passes?"), {TEXT("60 C"), TEXT("15 C"), TEXT("32 C"), TEXT("0.067 C")}, 0, TEXT("Use Q = It."), TEXT("Multiply current by time."), TEXT("Q = 2 x 30 = 60 C."), EBHDiagramType::Circuit, TEXT("Q = It"), 60.0f, 0.1f, {.ValueA=2.0f, .LabelA=TEXT("I = 2 A"), .LabelB=TEXT("t = 30 s"), .ShapeVariant=0}},
		{TEXT("Current"), TEXT("48 C of charge passes a point in 12 s. What is the current?"), {TEXT("4 A"), TEXT("60 A"), TEXT("0.25 A"), TEXT("36 A")}, 0, TEXT("Use I = Q / t."), TEXT("Divide charge by time."), TEXT("I = 48 / 12 = 4 A."), EBHDiagramType::Circuit, TEXT("I = Q / t"), 4.0f, 0.1f, {.LabelA=TEXT("Q = 48 C"), .LabelB=TEXT("t = 12 s"), .ShapeVariant=0}},
		{TEXT("Potential difference"), TEXT("A device transfers 90 J to 15 C. What is the potential difference?"), {TEXT("6 V"), TEXT("75 V"), TEXT("105 V"), TEXT("1350 V")}, 0, TEXT("Use V = E / Q."), TEXT("Divide energy by charge."), TEXT("V = 90 / 15 = 6 V."), EBHDiagramType::Circuit, TEXT("V = E / Q"), 6.0f, 0.1f, {.LabelA=TEXT("E = 90 J"), .LabelB=TEXT("Q = 15 C"), .ShapeVariant=0}},
		{TEXT("Resistance"), TEXT("A resistor has 9 V across it and current 0.30 A. What is its resistance?"), {TEXT("30 ohms"), TEXT("2.7 ohms"), TEXT("9.3 ohms"), TEXT("0.033 ohms")}, 0, TEXT("Use R = V / I."), TEXT("Divide voltage by current."), TEXT("R = 9 / 0.30 = 30 ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 30.0f, 0.1f, {.ValueA=9.0f, .ValueB=0.3f, .LabelA=TEXT("9 V, 0.3 A"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Power"), TEXT("A lamp has 6 V across it and current 0.50 A. What is its power?"), {TEXT("3 W"), TEXT("12 W"), TEXT("6.5 W"), TEXT("0.083 W")}, 0, TEXT("Use P = IV."), TEXT("Multiply current by voltage."), TEXT("P = 0.50 x 6 = 3 W."), EBHDiagramType::Circuit, TEXT("P = IV"), 3.0f, 0.1f, {.ValueA=6.0f, .LabelA=TEXT("V = 6 V"), .LabelB=TEXT("I = 0.5 A"), .ShapeVariant=0}},
		{TEXT("Energy transferred"), TEXT("A 40 W lamp is on for 60 s. How much energy is transferred?"), {TEXT("2400 J"), TEXT("100 J"), TEXT("1.5 J"), TEXT("240 J")}, 0, TEXT("Use E = Pt."), TEXT("Multiply power by time."), TEXT("E = 40 x 60 = 2400 J."), EBHDiagramType::Circuit, TEXT("E = Pt"), 2400.0f, 1.0f, {.LabelA=TEXT("P = 40 W"), .LabelB=TEXT("t = 60 s"), .ShapeVariant=0}},
		{TEXT("Parallel current"), TEXT("A supply current is 5 A. One parallel branch has 1.5 A. What is in the other branch?"), {TEXT("3.5 A"), TEXT("6.5 A"), TEXT("7.5 A"), TEXT("0.30 A")}, 0, TEXT("Current is conserved at a junction."), TEXT("Subtract the branch current from the total."), TEXT("Other branch current = 5 - 1.5 = 3.5 A."), EBHDiagramType::Circuit, TEXT("current in = current out"), 3.5f, 0.1f, {.ValueB=5.0f, .LabelA=TEXT("1.5 A"), .LabelB=TEXT("supply 5 A"), .LabelC=TEXT("I2 = ?"), .ShapeVariant=2}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Meter placement"), TEXT("A student gets no useful IV results. The ammeter is across the resistor and the voltmeter is in series. What correction is needed?"), {TEXT("Ammeter in series and voltmeter in parallel"), TEXT("Both meters in parallel"), TEXT("Both meters in series"), TEXT("Remove the resistor")}, 0, TEXT("Current through, voltage across."), TEXT("Place the ammeter in the main loop."), TEXT("The ammeter must be in series and the voltmeter in parallel with the component."), EBHDiagramType::Circuit, TEXT("exam skill: correct meter placement"), 0.0f, 0.0f, {.LabelA=TEXT("A"), .LabelB=TEXT("resistor"), .LabelC=TEXT("V"), .ShapeVariant=0}},
		{TEXT("IV graph"), TEXT("A resistor's IV graph is straight through the origin. What conclusion is best?"), {TEXT("Current is proportional to voltage, so resistance is constant"), TEXT("Resistance is zero at all voltages"), TEXT("The component is a thermistor heating up"), TEXT("The voltmeter was connected incorrectly")}, 0, TEXT("Straight through origin means proportional."), TEXT("Link graph shape to Ohm's law."), TEXT("A straight IV graph through the origin shows an ohmic conductor at constant temperature."), EBHDiagramType::IVGraph, TEXT("exam skill: interpret an ohmic graph"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Fuse choice"), TEXT("A 1200 W heater runs from 230 V. Which reasoning helps choose a fuse?"), {TEXT("Normal current is about 5.2 A, so use the next rating above it"), TEXT("Use a fuse below 1 A because heaters are hot"), TEXT("Use the largest fuse available every time"), TEXT("Fuse rating must equal power in watts")}, 0, TEXT("Estimate current using I = P / V."), TEXT("Choose a rating just above normal current."), TEXT("The normal current is about 1200 / 230 = 5.2 A, so the fuse should be just above that."), EBHDiagramType::Circuit, TEXT("exam skill: choose a fuse"), 0.0f, 0.0f, {.ValueB=230.0f, .LabelA=TEXT("P = 1200 W"), .LabelB=TEXT("V = 230 V"), .ShapeVariant=0}},
		{TEXT("Static prevention"), TEXT("Why are aircraft often earthed during refuelling?"), {TEXT("To let charge flow away and reduce spark risk"), TEXT("To increase the fuel's resistance"), TEXT("To make the fuel alternating current"), TEXT("To remove all gravitational energy")}, 0, TEXT("Fuel flow can cause static charge."), TEXT("Link charge build-up to sparks."), TEXT("Earthing prevents dangerous charge build-up and reduces the risk of sparks near fuel vapour."), EBHDiagramType::StaticCharge, TEXT("exam skill: explain earthing"), 0.0f, 0.0f},
		{TEXT("Sensor circuit"), TEXT("A light sensor should turn on in darkness using an LDR. What physics must be used?"), {TEXT("LDR resistance is high in darkness and low in bright light"), TEXT("LDR resistance is zero only in darkness"), TEXT("LDRs store charge like cells"), TEXT("LDRs measure force directly")}, 0, TEXT("Darkness gives high LDR resistance."), TEXT("Use the resistance change in a potential divider."), TEXT("An LDR changes resistance with light intensity, so a circuit can respond to dark and bright conditions."), EBHDiagramType::Circuit, TEXT("exam skill: apply LDR behaviour"), 0.0f, 0.0f, {.LabelA=TEXT("LDR"), .LabelB=TEXT("supply"), .LabelC=TEXT("R"), .ShapeVariant=1}},
		{TEXT("Domestic safety"), TEXT("A double-insulated drill has a plastic case. Why is no earth wire needed?"), {TEXT("There is no exposed metal case for a live fault to make dangerous"), TEXT("The drill uses no current"), TEXT("The fuse is removed"), TEXT("The neutral wire becomes the earth wire")}, 0, TEXT("Double insulation protects the user from live parts."), TEXT("Mention no exposed conductive case."), TEXT("Double insulation keeps live parts separated from the user, so there is no exposed metal case to earth."), EBHDiagramType::Circuit, TEXT("exam skill: explain double insulation"), 0.0f, 0.0f, {.LabelA=TEXT("plastic"), .LabelB=TEXT("mains"), .ShapeVariant=0}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("IV graph"), TEXT("On an IV graph, what does a steeper straight line mean if voltage is on the x-axis and current on the y-axis?"), {TEXT("Lower resistance"), TEXT("Higher resistance"), TEXT("No current"), TEXT("No potential difference")}, 0, TEXT("Gradient is I / V."), TEXT("Resistance is V / I."), TEXT("A steeper I against V graph means more current per volt, so lower resistance."), EBHDiagramType::IVGraph, TEXT("gradient = 1 / R"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Junction diagram"), TEXT("A junction diagram shows 2 A and 3 A entering, with 4 A leaving one branch. What leaves the other branch?"), {TEXT("1 A"), TEXT("9 A"), TEXT("5 A"), TEXT("24 A")}, 0, TEXT("Total current in equals total current out."), TEXT("Add entering currents, then subtract known leaving current."), TEXT("2 + 3 = 5 A enters, so the other leaving branch carries 1 A."), EBHDiagramType::Circuit, TEXT("current in = current out"), 1.0f, 0.1f, {.LabelA=TEXT("in 2 A"), .LabelB=TEXT("in 3 A"), .LabelC=TEXT("out 4 A"), .LabelD=TEXT("out ?"), .ShapeVariant=2}},
		{TEXT("Lamp IV graph"), TEXT("Why does a filament lamp IV graph curve at higher current?"), {TEXT("The filament gets hotter and resistance increases"), TEXT("The voltage becomes zero"), TEXT("The lamp changes to a battery"), TEXT("Charge is destroyed in the filament")}, 0, TEXT("Metal resistance can rise with temperature."), TEXT("Link heating to resistance."), TEXT("Higher current heats the filament, increasing resistance and curving the IV graph."), EBHDiagramType::IVGraph, TEXT("hot filament -> higher R"), 0.0f, 0.0f, {.LabelA=TEXT("filament"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=1}},
		{TEXT("Static diagram"), TEXT("A charged rod attracts small paper pieces. Which diagram explanation is best?"), {TEXT("Charges in the paper separate, so the near side is oppositely charged"), TEXT("The paper becomes a permanent magnet"), TEXT("The rod loses all charge before contact"), TEXT("The paper emits gamma rays")}, 0, TEXT("Neutral objects can polarise."), TEXT("The nearest charges matter most."), TEXT("Induced charge separation makes the near side opposite in charge, causing attraction."), EBHDiagramType::StaticCharge, TEXT("induction attraction"), 0.0f, 0.0f, {.LabelA=TEXT("+ rod"), .LabelB=TEXT("paper")}},
		{TEXT("Electrical power"), TEXT("The circuit diagram shows a heater drawing 3 A from a 230 V supply. What input power is shown?"), {TEXT("690 W"), TEXT("76.7 W"), TEXT("233 W"), TEXT("0.013 W")}, 0, TEXT("Use P = I x V."), TEXT("Multiply current by voltage."), TEXT("P = IV = 3 x 230 = 690 W."), EBHDiagramType::Circuit, TEXT("P = IV"), 690.0f, 1.0f, {.ValueA=3.0f, .ValueB=230.0f, .LabelA=TEXT("I = 3 A"), .LabelB=TEXT("V = 230 V")}},
		{TEXT("Like charges"), TEXT("The diagram shows two rods each marked with + charges held near each other. What do they do?"), {TEXT("Repel"), TEXT("Attract"), TEXT("Nothing"), TEXT("Combine into neutral")}, 0, TEXT("Like charges repel."), TEXT("Compare the signs shown on the rods."), TEXT("Two positively charged rods repel each other."), EBHDiagramType::StaticCharge, TEXT("like charges repel"), 0.0f, 0.0f, {.LabelA=TEXT("+ + +"), .LabelB=TEXT("+ + +")}},
		{TEXT("Series resistance"), TEXT("The series circuit shows a 12 V supply with R1 = 2 ohm and R2 = 4 ohm. What current flows?"), {TEXT("2 A"), TEXT("6 A"), TEXT("72 A"), TEXT("1 A")}, 0, TEXT("Total R = R1 + R2, then I = V / R."), TEXT("Add the resistances first."), TEXT("RT = 2 + 4 = 6 ohm; I = 12 / 6 = 2 A."), EBHDiagramType::Circuit, TEXT("RT = R1 + R2; I = V / RT"), 2.0f, 0.1f, {.ValueA=2.0f, .ValueB=12.0f, .ValueC=4.0f, .LabelA=TEXT("R1 = 2 ohm"), .LabelB=TEXT("V = 12 V"), .LabelC=TEXT("R2 = 4 ohm")}},
		{TEXT("Diode IV graph"), TEXT("An IV graph stays near zero current for reverse voltage, then rises sharply for forward voltage. Which component is this?"), {TEXT("Diode"), TEXT("Fixed resistor"), TEXT("Filament lamp"), TEXT("Thermistor")}, 0, TEXT("One-way conduction."), TEXT("Look at the asymmetry of the graph."), TEXT("A diode conducts in one direction, giving this asymmetric IV graph."), EBHDiagramType::IVGraph, TEXT("diode: one-way current"), 0.0f, 0.0f, {.LabelA=TEXT("forward only"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A")}},
		{TEXT("Parallel branches"), TEXT("The parallel circuit diagram shows a 6 V supply across two branches, each with a 6 ohm resistor. What current does each branch carry?"), {TEXT("1 A"), TEXT("2 A"), TEXT("0.5 A"), TEXT("12 A")}, 0, TEXT("Each branch sees the full supply voltage."), TEXT("Use I = V / R for one branch."), TEXT("Each branch: I = 6 / 6 = 1 A, so 2 A total from the supply."), EBHDiagramType::Circuit, TEXT("parallel: each branch V = supply"), 1.0f, 0.1f, {.ValueA=6.0f, .ValueB=6.0f, .ValueC=6.0f, .LabelA=TEXT("6 ohm"), .LabelB=TEXT("V = 6 V"), .LabelC=TEXT("6 ohm")}},
		{TEXT("Non-ohmic reasoning"), TEXT("A filament lamp's IV graph is an S-shaped curve that flattens at high current. What happens to its resistance as current rises?"), {TEXT("It increases because the filament heats up"), TEXT("It stays constant"), TEXT("It falls to zero"), TEXT("It becomes negative")}, 0, TEXT("R = V / I read along the curve."), TEXT("Compare V / I at low and high current."), TEXT("As current rises the filament heats up and its resistance increases, flattening the IV curve."), EBHDiagramType::IVGraph, TEXT("hot filament -> higher R"), 0.0f, 0.0f, {.LabelA=TEXT("curve flattens"), .XAxis=TEXT("V / V"), .YAxis=TEXT("I / A")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Equations"), TEXT("Choose the correct matching pair."), {TEXT("Current -> I = Q / t; power -> P = IV"), TEXT("Current -> P = IV; power -> V = E / Q"), TEXT("Resistance -> Q = It; charge -> R = V / I"), TEXT("Energy -> p = mv; voltage -> F = ma")}, 0, TEXT("Match each quantity to the equation that defines it."), TEXT("Use symbols carefully."), TEXT("Current is charge per time; electrical power is current times potential difference."), EBHDiagramType::Circuit, TEXT("I = Q/t, P = IV"), 0.0f, 0.0f},
		{TEXT("Circuit rules"), TEXT("Match the circuit type to the rule."), {TEXT("Series -> same current; parallel -> same potential difference"), TEXT("Series -> same potential difference; parallel -> same current"), TEXT("Series -> no resistance; parallel -> no voltage"), TEXT("Series -> branches; parallel -> one path only")}, 0, TEXT("Series has one path; parallel has branches."), TEXT("Trace current and compare p.d."), TEXT("Series circuits have the same current; parallel branches share the same p.d."), EBHDiagramType::Circuit, TEXT("series I, parallel V"), 0.0f, 0.0f, {.LabelA=TEXT("series"), .LabelC=TEXT("parallel"), .ShapeVariant=1}},
		{TEXT("Components"), TEXT("Match the component to its behaviour."), {TEXT("Diode -> one-way current; fuse -> melts when current is too high"), TEXT("Diode -> stores energy; fuse -> measures voltage"), TEXT("LDR -> current only one way; ammeter -> parallel only"), TEXT("Cell -> protects earth wire; switch -> increases resistance only when hot")}, 0, TEXT("Think function, not symbol shape only."), TEXT("Use the safety role of a fuse."), TEXT("A diode controls current direction; a fuse protects by melting at high current."), EBHDiagramType::Circuit, TEXT("component functions"), 0.0f, 0.0f, {.LabelA=TEXT("diode"), .LabelC=TEXT("fuse"), .ShapeVariant=0}},
		{TEXT("Static charge"), TEXT("Match the charge interaction."), {TEXT("Like charges repel; unlike charges attract"), TEXT("Like charges attract; unlike charges repel"), TEXT("Positive and neutral always repel"), TEXT("Negative charge is the same as no charge")}, 0, TEXT("Use like and unlike charge rules."), TEXT("Do not confuse neutral with negative."), TEXT("Like charges repel and unlike charges attract."), EBHDiagramType::StaticCharge, TEXT("charge rules"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Resistance method"), TEXT("Order a resistance calculation."), {TEXT("Write R = V / I -> substitute voltage and current -> divide -> add ohms"), TEXT("Write P = IV -> add current and voltage -> divide by mass -> add W"), TEXT("Write Q = It -> subtract time -> add volts -> multiply"), TEXT("Draw a wave -> count crests -> divide by wavelength -> add ohms")}, 0, TEXT("Resistance is voltage divided by current."), TEXT("Keep the unit as ohms."), TEXT("Use R = V / I, substitute, divide, and give the answer in ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Plug fault"), TEXT("Order the protection in an earthed metal appliance fault."), {TEXT("Live touches case -> large earth current flows -> fuse melts -> supply disconnects"), TEXT("Fuse melts -> live touches case -> earth current starts -> switch closes"), TEXT("Neutral touches case -> voltage doubles -> fuse charges -> current stops"), TEXT("Plastic melts -> p.d. stores -> current becomes zero -> live wire appears")}, 0, TEXT("The fault current happens before the fuse melts."), TEXT("Start with the live wire touching the metal case."), TEXT("The earth wire gives a low-resistance path so a large current melts the fuse and disconnects the supply."), EBHDiagramType::Circuit, TEXT("fault -> earth -> fuse"), 0.0f, 0.0f, {.LabelA=TEXT("earth"), .LabelB=TEXT("mains"), .LabelC=TEXT("fuse"), .ShapeVariant=0}},
		{TEXT("IV investigation"), TEXT("Order an IV investigation method."), {TEXT("Set voltage -> read voltage and current -> change voltage -> plot I against V"), TEXT("Plot graph -> build circuit -> choose random current -> remove meters"), TEXT("Heat resistor -> ignore voltage -> read mass -> plot force"), TEXT("Connect voltmeter in series -> use one reading -> never repeat")}, 0, TEXT("Several paired readings are needed."), TEXT("Change the independent variable in steps."), TEXT("An IV graph needs repeated voltage and current readings over a range."), EBHDiagramType::IVGraph, TEXT("vary V, measure I"), 0.0f, 0.0f, {.XAxis=TEXT("V / V"), .YAxis=TEXT("I / A"), .ShapeVariant=0}},
		{TEXT("Static charging"), TEXT("Order charging by rubbing."), {TEXT("Surfaces rub -> electrons transfer -> one object gains electrons -> it becomes negative"), TEXT("Protons transfer -> both become neutral -> current alternates -> charge disappears"), TEXT("Neutrons transfer -> object gains mass -> voltage becomes zero -> spark stops"), TEXT("Fuse melts -> LDR brightens -> charge flows to Earth -> object becomes AC")}, 0, TEXT("Electrons move between materials."), TEXT("The object gaining electrons becomes negative."), TEXT("Rubbing transfers electrons; the object that gains electrons becomes negatively charged."), EBHDiagramType::StaticCharge, TEXT("gain electrons -> negative"), 0.0f, 0.0f}
	};

	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity_ext"), TEXT("Electricity"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity_ext"), TEXT("Electricity"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity_ext"), TEXT("Electricity"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity_ext"), TEXT("Electricity"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity_ext"), TEXT("Electricity"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity_ext"), TEXT("Electricity"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::Electricity, TEXT("electricity_ext"), TEXT("Electricity"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

void BuildWavesExtension(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Wavefronts"), TEXT("What is the distance between two neighbouring crests called?"), {TEXT("Wavelength"), TEXT("Amplitude"), TEXT("Period"), TEXT("Frequency")}, 0, TEXT("Look from crest to crest."), TEXT("Use the symbol lambda if needed."), TEXT("Wavelength is the distance between matching points on neighbouring waves."), EBHDiagramType::Wave, TEXT("wavelength = crest to crest"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("crest"), .LabelB=TEXT("crest")}},
		{TEXT("Amplitude"), TEXT("What does larger amplitude usually mean for a sound wave?"), {TEXT("Louder sound"), TEXT("Higher pitch only"), TEXT("Lower wave speed in air"), TEXT("Shorter period always")}, 0, TEXT("Amplitude links to loudness."), TEXT("Compare wave height."), TEXT("A larger sound-wave amplitude is heard as a louder sound."), EBHDiagramType::Wave, TEXT("larger amplitude -> louder"), 0.0f, 0.0f, {.ValueA=0.40f, .ValueB=3.0f, .LabelA=TEXT("amplitude")}},
		{TEXT("Period"), TEXT("What is the period of a wave?"), {TEXT("Time for one complete oscillation"), TEXT("Distance between crests"), TEXT("Maximum displacement"), TEXT("Energy per charge")}, 0, TEXT("Period is a time."), TEXT("It is linked to frequency by f = 1 / T."), TEXT("The period is the time for one complete wave or oscillation."), EBHDiagramType::Wave, TEXT("T = 1 / f"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=2.0f, .XAxis=TEXT("time s")}},
		{TEXT("Reflection"), TEXT("Angles in reflection are measured from which line?"), {TEXT("The normal"), TEXT("The mirror surface"), TEXT("The horizon"), TEXT("The wavefront label")}, 0, TEXT("The normal is perpendicular to the surface."), TEXT("Draw the normal before measuring."), TEXT("Angles of incidence and reflection are measured from the normal."), EBHDiagramType::RayDiagram, TEXT("measure from normal"), 0.0f, 0.0f, {.AngleOrShape=40.0f}},
		{TEXT("Refraction"), TEXT("When light enters glass from air, which quantity stays the same?"), {TEXT("Frequency"), TEXT("Speed"), TEXT("Wavelength"), TEXT("Direction every time")}, 0, TEXT("Frequency is set by the source."), TEXT("Speed and wavelength change at a boundary."), TEXT("During refraction, frequency stays the same while speed and wavelength change."), EBHDiagramType::RayDiagram, TEXT("frequency unchanged"), 0.0f, 0.0f, {.LabelA=TEXT("air"), .LabelB=TEXT("glass"), .AngleOrShape=35.0f}},
		{TEXT("Sound range"), TEXT("Roughly what frequency range can a young healthy human hear?"), {TEXT("20 Hz to 20,000 Hz"), TEXT("0.2 Hz to 2 Hz"), TEXT("2 MHz to 20 MHz"), TEXT("Only exactly 1 kHz")}, 0, TEXT("The upper limit is about 20 kHz."), TEXT("Use hertz for frequency."), TEXT("The typical human hearing range is about 20 Hz to 20,000 Hz."), EBHDiagramType::Wave, TEXT("20 Hz to 20 kHz"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=4.0f, .XAxis=TEXT("time s")}},
		{TEXT("Ultrasound"), TEXT("What is ultrasound?"), {TEXT("Sound above 20,000 Hz"), TEXT("Sound below 20 Hz"), TEXT("Visible light above red"), TEXT("Any transverse wave")}, 0, TEXT("Ultra means above the hearing range."), TEXT("Compare with 20 kHz."), TEXT("Ultrasound has frequency above the upper limit of human hearing."), EBHDiagramType::Wave, TEXT("ultrasound > 20 kHz"), 0.0f, 0.0f, {.ValueA=0.20f, .ValueB=5.0f, .XAxis=TEXT("time s")}},
		{TEXT("Infrared"), TEXT("Which EM wave is commonly used by TV remote controls?"), {TEXT("Infrared"), TEXT("Gamma"), TEXT("X-rays"), TEXT("Ultrasound")}, 0, TEXT("Remote controls use invisible radiation just beyond red."), TEXT("Separate infrared from radio and sound."), TEXT("TV remote controls commonly use infrared radiation."), EBHDiagramType::EMSpectrum, TEXT("infrared remote control"), 0.0f, 0.0f, {.ShapeVariant=0}},
		{TEXT("Radio waves"), TEXT("Why are radio waves suitable for broadcasting over long distances?"), {TEXT("They are electromagnetic waves with long wavelengths"), TEXT("They are ionising gamma rays"), TEXT("They need copper wires every metre"), TEXT("They are longitudinal sound waves")}, 0, TEXT("Radio waves are part of the EM spectrum."), TEXT("Think about communication uses."), TEXT("Radio waves are long-wavelength electromagnetic waves used for broadcasting and communication."), EBHDiagramType::EMSpectrum, TEXT("radio communication"), 0.0f, 0.0f, {.ShapeVariant=0}},
		{TEXT("Gamma rays"), TEXT("Which statement about gamma rays is correct?"), {TEXT("They have very high frequency and are ionising"), TEXT("They are the longest-wavelength EM waves"), TEXT("They are sound waves in air"), TEXT("They are safer than radio waves at any dose")}, 0, TEXT("Gamma is at the high-frequency end."), TEXT("Ionising radiation can damage cells."), TEXT("Gamma rays have very high frequency and can ionise atoms."), EBHDiagramType::EMSpectrum, TEXT("gamma high f ionising"), 0.0f, 0.0f, {.ShapeVariant=0}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Mechanical waves"), TEXT("True or false: sound needs a medium to travel through."), {TEXT("True"), TEXT("False"), TEXT("Only in glass"), TEXT("Only above 20 kHz")}, 0, TEXT("Sound is a mechanical wave."), TEXT("Think about sound in a vacuum."), TEXT("Sound needs particles to vibrate, so it cannot travel through a vacuum."), EBHDiagramType::Wave, TEXT("sound needs medium"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("sound")}},
		{TEXT("EM waves"), TEXT("True or false: electromagnetic waves can travel through a vacuum."), {TEXT("True"), TEXT("False"), TEXT("Only radio waves"), TEXT("Only visible light in glass")}, 0, TEXT("Light reaches us from space."), TEXT("EM waves do not need particles."), TEXT("Electromagnetic waves can travel through a vacuum."), EBHDiagramType::EMSpectrum, TEXT("EM waves travel in vacuum"), 0.0f, 0.0f, {.ShapeVariant=0}},
		{TEXT("Refraction"), TEXT("True or false: refraction happens because wave speed changes at a boundary."), {TEXT("True"), TEXT("False"), TEXT("Only because mass changes"), TEXT("Only because charge flows")}, 0, TEXT("Speed change causes bending."), TEXT("Link medium change to speed."), TEXT("Refraction occurs when waves change speed on entering a new medium."), EBHDiagramType::RayDiagram, TEXT("speed change -> refraction"), 0.0f, 0.0f, {.LabelA=TEXT("boundary"), .AngleOrShape=35.0f}},
		{TEXT("Pitch"), TEXT("True or false: higher frequency sound has higher pitch."), {TEXT("True"), TEXT("False"), TEXT("Only if amplitude is zero"), TEXT("Only if it is ultrasound")}, 0, TEXT("Frequency links to pitch."), TEXT("Count cycles per second."), TEXT("A higher sound frequency is heard as a higher pitch."), EBHDiagramType::Wave, TEXT("higher f -> higher pitch"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=4.0f, .XAxis=TEXT("time s")}},
		{TEXT("EM speed"), TEXT("True or false: all electromagnetic waves travel at the same speed in a vacuum."), {TEXT("True"), TEXT("False"), TEXT("Only visible light"), TEXT("Only radio waves")}, 0, TEXT("They differ by frequency and wavelength."), TEXT("Use the vacuum speed of light."), TEXT("All EM waves travel at the same speed in a vacuum."), EBHDiagramType::EMSpectrum, TEXT("c = f lambda"), 0.0f, 0.0f, {.ShapeVariant=0}}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Wave speed"), TEXT("A wave has frequency 12 Hz and wavelength 0.50 m. What is its speed?"), {TEXT("6 m/s"), TEXT("24 m/s"), TEXT("12.5 m/s"), TEXT("0.042 m/s")}, 0, TEXT("Use v = f lambda."), TEXT("Multiply frequency by wavelength."), TEXT("v = 12 x 0.50 = 6 m/s."), EBHDiagramType::Wave, TEXT("v = f lambda"), 6.0f, 0.1f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("f 12 Hz"), .LabelB=TEXT("lambda 0.5 m")}},
		{TEXT("Frequency"), TEXT("A wave speed is 20 m/s and wavelength is 4 m. What is frequency?"), {TEXT("5 Hz"), TEXT("80 Hz"), TEXT("16 Hz"), TEXT("0.20 Hz")}, 0, TEXT("Rearrange v = f lambda."), TEXT("Divide speed by wavelength."), TEXT("f = 20 / 4 = 5 Hz."), EBHDiagramType::Wave, TEXT("f = v / lambda"), 5.0f, 0.1f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("v 20 m/s"), .LabelB=TEXT("lambda 4 m")}},
		{TEXT("Wavelength"), TEXT("A wave travels at 30 m/s with frequency 10 Hz. What is its wavelength?"), {TEXT("3 m"), TEXT("300 m"), TEXT("20 m"), TEXT("0.33 m")}, 0, TEXT("Use wavelength = speed / frequency."), TEXT("Divide 30 by 10."), TEXT("Wavelength = 30 / 10 = 3 m."), EBHDiagramType::Wave, TEXT("lambda = v / f"), 3.0f, 0.1f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("v 30 m/s"), .LabelB=TEXT("f 10 Hz")}},
		{TEXT("Period"), TEXT("A wave has frequency 8 Hz. What is its period?"), {TEXT("0.125 s"), TEXT("8 s"), TEXT("64 s"), TEXT("0.8 s")}, 0, TEXT("Use T = 1 / f."), TEXT("Find one divided by 8."), TEXT("T = 1 / 8 = 0.125 s."), EBHDiagramType::Wave, TEXT("T = 1 / f"), 0.125f, 0.01f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("f 8 Hz"), .XAxis=TEXT("time s")}},
		{TEXT("Echo"), TEXT("An echo returns after 0.40 s from a wall 68 m away. What speed of sound is measured?"), {TEXT("340 m/s"), TEXT("170 m/s"), TEXT("27.2 m/s"), TEXT("68.4 m/s")}, 0, TEXT("The sound travels there and back."), TEXT("Use total distance 136 m."), TEXT("Speed = 2 x 68 / 0.40 = 340 m/s."), EBHDiagramType::Wave, TEXT("speed = distance / time"), 340.0f, 1.0f, {.ValueA=0.25f, .ValueB=2.0f, .LabelA=TEXT("t 0.40 s"), .LabelB=TEXT("d 68 m")}},
		{TEXT("Critical angle"), TEXT("A material has refractive index 1.25. For material to air, what is sin c?"), {TEXT("0.8"), TEXT("1.25"), TEXT("0.25"), TEXT("2.5")}, 0, TEXT("Use n = 1 / sin c."), TEXT("Rearrange to sin c = 1 / n."), TEXT("sin c = 1 / 1.25 = 0.8."), EBHDiagramType::RayDiagram, TEXT("n = 1 / sin c"), 0.8f, 0.01f, {.LabelA=TEXT("n 1.25"), .AngleOrShape=42.0f}},
		{TEXT("EM frequency"), TEXT("An EM wave has speed 3.0 x 10^8 m/s and wavelength 0.01 m. What is frequency?"), {TEXT("3.0 x 10^10 Hz"), TEXT("3.0 x 10^6 Hz"), TEXT("3.0 x 10^8 Hz"), TEXT("0.01 Hz")}, 0, TEXT("Use f = v / wavelength."), TEXT("Divide 3.0 x 10^8 by 0.01."), TEXT("Frequency = 3.0 x 10^10 Hz."), EBHDiagramType::EMSpectrum, TEXT("f = v / lambda"), 30000000000.0f, 1000000.0f, {.LabelA=TEXT("v 3e8 m/s"), .LabelB=TEXT("lambda 0.01 m"), .ShapeVariant=0}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Ripple tank"), TEXT("A ripple tank photo shows 5 wavelengths over 0.20 m. What improves accuracy?"), {TEXT("Measure several wavelengths and divide by the number of wavelengths"), TEXT("Measure one crest with a stopwatch"), TEXT("Ignore the scale and estimate speed"), TEXT("Use amplitude as frequency")}, 0, TEXT("A larger measured distance reduces percentage uncertainty."), TEXT("Divide total distance by the number of wavelengths."), TEXT("Measuring several wavelengths and dividing gives a more reliable wavelength."), EBHDiagramType::Wave, TEXT("exam skill: reduce percentage uncertainty"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=5.0f, .LabelA=TEXT("5 lambda"), .LabelB=TEXT("0.20 m")}},
		{TEXT("Refraction explanation"), TEXT("A ray bends away from the normal leaving glass for air. Which explanation is best?"), {TEXT("It speeds up in air, frequency stays the same, and wavelength increases"), TEXT("It slows down in air and frequency becomes zero"), TEXT("It changes from transverse to longitudinal"), TEXT("It loses all energy at the surface")}, 0, TEXT("Air is less optically dense than glass."), TEXT("Track speed, frequency, and wavelength."), TEXT("Leaving glass for air increases wave speed; frequency is unchanged, so wavelength increases and the ray bends away from the normal."), EBHDiagramType::RayDiagram, TEXT("exam skill: explain bending"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .LabelB=TEXT("glass to air"), .AngleOrShape=35.0f}},
		{TEXT("Medical ultrasound"), TEXT("Why can ultrasound be used for imaging soft tissue?"), {TEXT("It reflects at boundaries between tissues and the echoes can be timed"), TEXT("It is ionising like gamma rays"), TEXT("It only travels through a vacuum"), TEXT("It has no frequency")}, 0, TEXT("Echoes carry information."), TEXT("Link reflection and timing."), TEXT("Ultrasound reflects at tissue boundaries; echo times help build an image."), EBHDiagramType::Wave, TEXT("exam skill: ultrasound echoes"), 0.0f, 0.0f, {.ValueA=0.20f, .ValueB=5.0f, .LabelA=TEXT("ultrasound")}},
		{TEXT("EM safety"), TEXT("A phone uses microwaves for communication. What safety idea is most relevant?"), {TEXT("Microwaves can cause heating, so power levels and exposure are controlled"), TEXT("Microwaves are ionising like gamma rays"), TEXT("Microwaves are sound waves"), TEXT("Microwaves cannot transfer energy")}, 0, TEXT("Microwaves are non-ionising but can heat tissue."), TEXT("Separate heating from ionisation."), TEXT("Microwaves can heat water-containing tissue, so exposure and power are controlled."), EBHDiagramType::EMSpectrum, TEXT("exam skill: compare EM hazards"), 0.0f, 0.0f, {.LabelA=TEXT("microwave"), .ShapeVariant=2}},
		{TEXT("Doppler"), TEXT("A star's spectral lines are shifted toward red. What conclusion can be drawn?"), {TEXT("The star is moving away from the observer"), TEXT("The star has become a sound source"), TEXT("The star's mass is zero"), TEXT("The star is reflecting from a mirror")}, 0, TEXT("Red shift means wavelength is increased."), TEXT("Link longer wavelength to recession."), TEXT("A red shift indicates the source is moving away from the observer."), EBHDiagramType::EMSpectrum, TEXT("exam skill: interpret red shift"), 0.0f, 0.0f, {.LabelA=TEXT("red shift"), .ShapeVariant=4}},
		{TEXT("Optical fibres"), TEXT("Why must light hit the core-cladding boundary above the critical angle?"), {TEXT("So total internal reflection keeps the signal inside the fibre"), TEXT("So the light becomes sound"), TEXT("So frequency becomes zero"), TEXT("So the fibre conducts current")}, 0, TEXT("Optical fibres use repeated TIR."), TEXT("Use the critical angle condition."), TEXT("Angles above the critical angle cause total internal reflection, keeping light inside the fibre."), EBHDiagramType::RayDiagram, TEXT("exam skill: apply TIR"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .AngleOrShape=60.0f}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Wave graph"), TEXT("On a displacement-distance graph, what is the wavelength?"), {TEXT("Distance between neighbouring crests"), TEXT("Height from middle to crest"), TEXT("Number of waves per second"), TEXT("Time for one oscillation")}, 0, TEXT("Measure along the distance axis."), TEXT("Use matching points on neighbouring waves."), TEXT("Wavelength is the distance between two neighbouring crests or matching points."), EBHDiagramType::Wave, TEXT("wavelength = crest spacing"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .XAxis=TEXT("distance m"), .YAxis=TEXT("displ m")}},
		{TEXT("Oscilloscope"), TEXT("An oscilloscope trace shows 5 cycles in 0.10 s. What is frequency?"), {TEXT("50 Hz"), TEXT("0.5 Hz"), TEXT("5 Hz"), TEXT("500 Hz")}, 0, TEXT("Frequency is cycles per second."), TEXT("Divide cycles by time."), TEXT("Frequency = 5 / 0.10 = 50 Hz."), EBHDiagramType::Wave, TEXT("f = cycles / time"), 50.0f, 0.1f, {.ValueA=0.25f, .ValueB=5.0f, .LabelA=TEXT("5 cycles"), .LabelB=TEXT("0.10 s"), .XAxis=TEXT("time s")}},
		{TEXT("Ray diagram"), TEXT("A ray in glass hits the glass-air boundary at exactly the critical angle. What happens?"), {TEXT("It refracts along the boundary"), TEXT("It bends toward the normal"), TEXT("It stops moving"), TEXT("It becomes gamma radiation")}, 0, TEXT("At the critical angle, refraction angle is 90 degrees."), TEXT("Draw the refracted ray along the surface."), TEXT("At the critical angle, the refracted ray travels along the boundary."), EBHDiagramType::RayDiagram, TEXT("critical angle -> r = 90"), 0.0f, 0.0f, {.LabelA=TEXT("glass to air"), .AngleOrShape=42.0f}},
		{TEXT("EM spectrum"), TEXT("On an EM spectrum diagram, which side has the highest frequency?"), {TEXT("Gamma-ray end"), TEXT("Radio-wave end"), TEXT("Infrared only"), TEXT("Microwave only")}, 0, TEXT("Frequency increases from radio to gamma."), TEXT("Use the standard spectrum order."), TEXT("Gamma rays are at the highest-frequency end of the EM spectrum."), EBHDiagramType::EMSpectrum, TEXT("radio low f, gamma high f"), 0.0f, 0.0f, {.LabelA=TEXT("radio -> gamma")}},
		{TEXT("Reflection angle"), TEXT("A ray strikes a mirror at 50 deg to the normal, as drawn. What is the angle between the incident and reflected rays?"), {TEXT("100 deg"), TEXT("50 deg"), TEXT("25 deg"), TEXT("90 deg")}, 0, TEXT("Both rays are 50 deg from the normal."), TEXT("Add the two equal angles."), TEXT("Reflection gives r = 50 deg, so the rays are 50 + 50 = 100 deg apart."), EBHDiagramType::RayDiagram, TEXT("i = r"), 100.0f, 0.1f, {.LabelA=TEXT("i = 50 deg"), .AngleOrShape=50.0f}},
		{TEXT("Comparing waves"), TEXT("Two transverse waves are drawn with the same wavelength, but wave M is taller than wave N. What is greater for wave M?"), {TEXT("Amplitude"), TEXT("Wavelength"), TEXT("Frequency"), TEXT("Speed")}, 0, TEXT("Height from the middle line is amplitude."), TEXT("Same wavelength rules out the others."), TEXT("A taller wave with the same wavelength has the greater amplitude."), EBHDiagramType::Wave, TEXT("taller -> larger amplitude"), 0.0f, 0.0f, {.ValueA=0.38f, .ValueB=3.0f, .LabelA=TEXT("M taller"), .LabelB=TEXT("same lambda")}},
		{TEXT("Period and frequency"), TEXT("An oscilloscope shows one complete wave taking 0.005 s. What is the frequency?"), {TEXT("200 Hz"), TEXT("0.005 Hz"), TEXT("5 Hz"), TEXT("2000 Hz")}, 0, TEXT("f = 1 / T."), TEXT("Take the reciprocal of the period."), TEXT("f = 1 / 0.005 = 200 Hz."), EBHDiagramType::Wave, TEXT("f = 1 / T"), 200.0f, 1.0f, {.ValueA=0.30f, .ValueB=1.0f, .LabelA=TEXT("T = 0.005 s")}},
		{TEXT("Critical angle"), TEXT("The ray diagram shows light in glass meeting the boundary; the angle is increased until the refracted ray runs along the surface. What is this angle called?"), {TEXT("The critical angle"), TEXT("The normal"), TEXT("The reflection angle"), TEXT("The focal angle")}, 0, TEXT("The refracted ray is at 90 deg to the normal."), TEXT("Name the special incidence angle."), TEXT("When the refracted ray travels along the boundary, the incidence angle is the critical angle."), EBHDiagramType::RayDiagram, TEXT("critical angle: r = 90"), 0.0f, 0.0f, {.LabelA=TEXT("r = 90 deg"), .AngleOrShape=42.0f}},
		{TEXT("Wave speed"), TEXT("A water-wave diagram shows wavelength 2 m and the source makes 3 waves each second. What is the wave speed?"), {TEXT("6 m/s"), TEXT("1.5 m/s"), TEXT("0.67 m/s"), TEXT("5 m/s")}, 0, TEXT("Use v = f x lambda."), TEXT("Multiply frequency by wavelength."), TEXT("v = 3 x 2 = 6 m/s."), EBHDiagramType::Wave, TEXT("v = f lambda"), 6.0f, 0.1f, {.ValueA=0.26f, .ValueB=2.0f, .LabelA=TEXT("lambda = 2 m"), .LabelB=TEXT("f = 3 Hz")}},
		{TEXT("Spectrum and energy"), TEXT("On the EM spectrum strip, moving from microwaves towards X-rays, how do wavelength and photon energy change?"), {TEXT("Wavelength decreases, energy increases"), TEXT("Wavelength increases, energy increases"), TEXT("Both decrease"), TEXT("Both increase")}, 0, TEXT("Higher frequency means shorter wavelength and more energy."), TEXT("Use v = f lambda and link energy to frequency."), TEXT("Towards X-rays the wavelength gets shorter and the photon energy increases."), EBHDiagramType::EMSpectrum, TEXT("shorter lambda -> higher energy"), 0.0f, 0.0f, {.LabelA=TEXT("micro -> X-ray"), .LabelB=TEXT("lambda down")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Wave quantities"), TEXT("Choose the correct matching pair."), {TEXT("Amplitude -> maximum displacement; period -> time for one wave"), TEXT("Amplitude -> cycles per second; period -> distance between crests"), TEXT("Frequency -> wave height; wavelength -> time"), TEXT("Speed -> charge per second; wavelength -> voltage")}, 0, TEXT("Amplitude is distance from equilibrium."), TEXT("Period is measured in seconds."), TEXT("Amplitude is maximum displacement; period is time for one complete wave."), EBHDiagramType::Wave, TEXT("wave definitions"), 0.0f, 0.0f},
		{TEXT("EM uses"), TEXT("Match the EM wave to a common use."), {TEXT("Infrared -> remote control; gamma -> sterilising equipment"), TEXT("Radio -> medical bones; X-ray -> TV remote"), TEXT("Microwave -> sun tanning; ultraviolet -> satellite TV"), TEXT("Visible -> cancer treatment; ultrasound -> EM communication")}, 0, TEXT("Some uses depend on penetration or ionisation."), TEXT("Separate infrared from ultraviolet and gamma."), TEXT("Infrared is used in remote controls; gamma radiation can sterilise equipment."), EBHDiagramType::EMSpectrum, TEXT("EM uses"), 0.0f, 0.0f, {.ShapeVariant=0}},
		{TEXT("Wave types"), TEXT("Match each wave to its type."), {TEXT("Water surface wave -> transverse and longitudinal parts; sound in air -> longitudinal"), TEXT("Sound in air -> transverse only; light -> longitudinal"), TEXT("Radio -> mechanical; ultrasound -> electromagnetic"), TEXT("Gamma -> sound; water wave -> no oscillation")}, 0, TEXT("Sound in air has compressions."), TEXT("Water surface waves are more complex than simple rope waves."), TEXT("Sound in air is longitudinal; water surface waves have both transverse and longitudinal motion."), EBHDiagramType::Wave, TEXT("wave types"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f}},
		{TEXT("Boundary behaviour"), TEXT("Match the boundary change to the effect."), {TEXT("Into slower medium -> bends toward normal; into faster medium -> bends away"), TEXT("Into slower medium -> bends away; into faster medium -> bends toward"), TEXT("Reflection -> frequency doubles; refraction -> frequency zero"), TEXT("TIR -> ray leaves boundary; critical angle -> no ray")}, 0, TEXT("Direction depends on wave speed."), TEXT("Use the normal as the reference line."), TEXT("A wave entering a slower medium bends toward the normal; entering a faster medium bends away."), EBHDiagramType::RayDiagram, TEXT("speed and refraction"), 0.0f, 0.0f, {.LabelA=TEXT("boundary"), .AngleOrShape=35.0f}}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Wave speed method"), TEXT("Order a wave speed calculation."), {TEXT("Read frequency -> read wavelength -> multiply f by wavelength -> add m/s"), TEXT("Read current -> read voltage -> multiply -> add Hz"), TEXT("Read angle -> find normal -> divide by mass -> add N"), TEXT("Read power -> read energy -> subtract -> add Pa")}, 0, TEXT("Use v = f lambda."), TEXT("The unit of wave speed is m/s."), TEXT("Wave speed is frequency multiplied by wavelength."), EBHDiagramType::Wave, TEXT("v = f lambda"), 0.0f, 0.0f, {.ValueA=0.25f, .ValueB=3.0f, .LabelA=TEXT("amplitude"), .LabelB=TEXT("wavelength")}},
		{TEXT("Reflection construction"), TEXT("Order drawing a reflected ray."), {TEXT("Draw normal -> measure angle of incidence -> copy equal angle on other side -> draw reflected ray"), TEXT("Draw ray along mirror -> ignore normal -> double frequency -> draw wavefront"), TEXT("Draw current -> add resistance -> choose fuse -> reflect"), TEXT("Draw tangent -> find acceleration -> add wavelength -> reflect")}, 0, TEXT("Angles are measured from the normal."), TEXT("Use i = r."), TEXT("A reflected ray is drawn by measuring the incident angle from the normal and copying it on the other side."), EBHDiagramType::RayDiagram, TEXT("i = r"), 0.0f, 0.0f, {.LabelA=TEXT("normal"), .AngleOrShape=40.0f}},
		{TEXT("Refraction explanation"), TEXT("Order refraction entering glass from air."), {TEXT("Wave enters glass -> speed decreases -> wavelength decreases -> ray bends toward normal"), TEXT("Wave enters glass -> frequency increases -> wavelength zero -> ray stops"), TEXT("Wave leaves air -> speed increases -> bends away -> becomes sound"), TEXT("Ray reflects -> voltage rises -> current flows -> ray bends")}, 0, TEXT("Frequency stays the same."), TEXT("Glass is optically denser than air."), TEXT("In glass, light slows and its wavelength decreases, bending the ray toward the normal."), EBHDiagramType::RayDiagram, TEXT("air to glass"), 0.0f, 0.0f, {.LabelA=TEXT("air to glass"), .AngleOrShape=35.0f}},
		{TEXT("EM spectrum"), TEXT("Order these from lowest frequency to highest frequency."), {TEXT("Radio -> infrared -> visible -> ultraviolet -> gamma"), TEXT("Gamma -> ultraviolet -> visible -> infrared -> radio"), TEXT("Visible -> radio -> gamma -> infrared -> ultraviolet"), TEXT("Infrared -> gamma -> radio -> visible -> ultraviolet")}, 0, TEXT("Start with radio and end with gamma."), TEXT("Use the EM spectrum order."), TEXT("Frequency increases from radio through infrared, visible, ultraviolet, and gamma."), EBHDiagramType::EMSpectrum, TEXT("radio to gamma"), 0.0f, 0.0f, {.ShapeVariant=0}}
	};

	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves_ext"), TEXT("Waves"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves_ext"), TEXT("Waves"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves_ext"), TEXT("Waves"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves_ext"), TEXT("Waves"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves_ext"), TEXT("Waves"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves_ext"), TEXT("Waves"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::Waves, TEXT("waves_ext"), TEXT("Waves"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

void BuildEnergyExtension(TArray<FBHRevisionQuestion>& Bank)
{
	static const FRevisionSpec MC[] = {
		{TEXT("Energy stores"), TEXT("Which store increases when a spring is stretched?"), {TEXT("Elastic potential"), TEXT("Nuclear"), TEXT("Magnetic only"), TEXT("Chemical in air")}, 0, TEXT("Stretching stores energy elastically."), TEXT("Name the store linked to deformation."), TEXT("A stretched spring has increased elastic potential energy."), EBHDiagramType::EnergyChain, TEXT("elastic store"), 0.0f, 0.0f, {.LabelA=TEXT("stretch"), .LabelB=TEXT("spring"), .LabelC=TEXT("store = ?")}},
		{TEXT("Kinetic energy"), TEXT("What happens to kinetic energy if speed doubles and mass stays the same?"), {TEXT("It becomes four times larger"), TEXT("It doubles"), TEXT("It halves"), TEXT("It becomes zero")}, 0, TEXT("Kinetic energy depends on speed squared."), TEXT("Use Ek = 1/2 mv^2."), TEXT("Doubling speed makes v^2 four times larger, so kinetic energy quadruples."), EBHDiagramType::EnergyChain, TEXT("Ek = 1/2 mv^2"), 0.0f, 0.0f, {.LabelA=TEXT("speed x2"), .LabelB=TEXT("mass same"), .LabelC=TEXT("KE = ?")}},
		{TEXT("Conservation"), TEXT("Where does wasted energy usually end up?"), {TEXT("Dissipated to the surroundings, often thermally"), TEXT("Destroyed forever"), TEXT("Stored only as nuclear energy"), TEXT("Converted into mass every time")}, 0, TEXT("Energy is conserved."), TEXT("Use the word dissipated."), TEXT("Wasted energy is dissipated to the surroundings, often by heating."), EBHDiagramType::EnergyChain, TEXT("energy conserved"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("useful out"), .LabelC=TEXT("wasted out")}},
		{TEXT("Power"), TEXT("Which definition of power is correct?"), {TEXT("Rate of energy transfer"), TEXT("Total distance travelled"), TEXT("Force per unit area"), TEXT("Charge per unit voltage")}, 0, TEXT("Power is measured in watts."), TEXT("One watt is one joule per second."), TEXT("Power is the rate of energy transfer or doing work."), EBHDiagramType::EnergyChain, TEXT("P = E / t"), 0.0f, 0.0f, {.LabelA=TEXT("energy J"), .LabelB=TEXT("time s"), .LabelC=TEXT("power = ?")}},
		{TEXT("Efficiency"), TEXT("Which device is more efficient?"), {TEXT("One with greater useful output for the same input"), TEXT("One with greater wasted output for the same input"), TEXT("One with no energy input"), TEXT("One that destroys energy")}, 0, TEXT("Efficiency compares useful output with total input."), TEXT("Useful output should be a larger fraction."), TEXT("A more efficient device transfers a greater fraction of input energy usefully."), EBHDiagramType::Sankey, TEXT("efficiency = useful / input"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("useful"), .LabelC=TEXT("wasted")}},
		{TEXT("Insulation"), TEXT("Why does a lid reduce energy loss from hot water?"), {TEXT("It reduces convection and evaporation"), TEXT("It increases the water's mass"), TEXT("It creates energy"), TEXT("It makes water stop radiating completely")}, 0, TEXT("Warm air and vapour can escape from an open top."), TEXT("Name a thermal transfer route."), TEXT("A lid reduces convection currents and evaporation from the water surface."), EBHDiagramType::EnergyChain, TEXT("reduce thermal transfer"), 0.0f, 0.0f, {.LabelA=TEXT("hot water"), .LabelB=TEXT("heat loss"), .LabelC=TEXT("surroundings")}},
		{TEXT("Renewable resources"), TEXT("Which energy resource is renewable?"), {TEXT("Geothermal"), TEXT("Coal"), TEXT("Oil"), TEXT("Natural gas")}, 0, TEXT("Renewable resources are replenished naturally."), TEXT("Fossil fuels are non-renewable."), TEXT("Geothermal energy is renewable; fossil fuels are not."), EBHDiagramType::EnergyChain, TEXT("renewable resource"), 0.0f, 0.0f, {.LabelA=TEXT("resource"), .LabelB=TEXT("renewable ?")}},
		{TEXT("Power stations"), TEXT("In most power stations, what does the generator do?"), {TEXT("Transfers kinetic energy to electrical energy"), TEXT("Burns fuel into light directly"), TEXT("Stores all wasted energy"), TEXT("Turns electricity into coal")}, 0, TEXT("A turbine turns the generator."), TEXT("Think about electromagnetic induction."), TEXT("The generator transfers kinetic energy from the turbine into electrical energy."), EBHDiagramType::EnergyChain, TEXT("generator output electrical"), 0.0f, 0.0f, {.LabelA=TEXT("turbine"), .LabelB=TEXT("generator"), .LabelC=TEXT("output ?")}},
		{TEXT("Specific heat capacity"), TEXT("What does a high specific heat capacity mean?"), {TEXT("More energy is needed for each kg to rise by 1 degree C"), TEXT("The material has no mass"), TEXT("The material cannot cool down"), TEXT("Less energy is always stored chemically")}, 0, TEXT("Use E = mc delta T."), TEXT("Compare energy for the same mass and temperature rise."), TEXT("A high specific heat capacity means more energy is needed per kg per degree C."), EBHDiagramType::EnergyChain, TEXT("E = mc delta T"), 0.0f, 0.0f, {.LabelA=TEXT("energy J"), .LabelB=TEXT("mass kg"), .LabelC=TEXT("temp rise")}},
		{TEXT("Energy transfer"), TEXT("When a battery lights a lamp, which pathway transfers energy to the lamp?"), {TEXT("Electrically"), TEXT("Mechanically"), TEXT("By convection only"), TEXT("By nuclear radiation only")}, 0, TEXT("A current transfers energy around the circuit."), TEXT("Name the pathway, not the store."), TEXT("Energy is transferred electrically from the battery to the lamp."), EBHDiagramType::Circuit, TEXT("electrical transfer"), 0.0f, 0.0f, {.LabelA=TEXT("battery"), .LabelB=TEXT("lamp"), .ShapeVariant=0}}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Energy conservation"), TEXT("True or false: energy can be transferred usefully or dissipated, but not destroyed."), {TEXT("True"), TEXT("False"), TEXT("Only in circuits"), TEXT("Only for waves")}, 0, TEXT("This is conservation of energy."), TEXT("Use the word dissipated for wasted energy."), TEXT("Energy is conserved; it can be transferred or dissipated but not destroyed."), EBHDiagramType::EnergyChain, TEXT("energy conserved"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("transferred"), .LabelC=TEXT("dissipated")}},
		{TEXT("Work done"), TEXT("True or false: work is done when a force moves an object through a distance."), {TEXT("True"), TEXT("False"), TEXT("Only if the force is zero"), TEXT("Only for electricity")}, 0, TEXT("Work done is energy transferred mechanically."), TEXT("Use force times distance."), TEXT("Work is done when a force causes movement through a distance."), EBHDiagramType::EnergyChain, TEXT("W = Fd"), 0.0f, 0.0f, {.LabelA=TEXT("force"), .LabelB=TEXT("distance"), .LabelC=TEXT("work")}},
		{TEXT("Sankey diagrams"), TEXT("True or false: the width of a Sankey arrow represents the amount of energy."), {TEXT("True"), TEXT("False"), TEXT("Only the input arrow"), TEXT("Only wasted arrows")}, 0, TEXT("Wider arrows mean more energy."), TEXT("Compare input, useful, and wasted arrows."), TEXT("In a Sankey diagram, arrow width represents energy amount."), EBHDiagramType::Sankey, TEXT("arrow width = energy"), 0.0f, 0.0f, {.LabelA=TEXT("input"), .LabelB=TEXT("useful"), .LabelC=TEXT("wasted")}},
		{TEXT("Thermal radiation"), TEXT("True or false: thermal radiation can travel through a vacuum."), {TEXT("True"), TEXT("False"), TEXT("Only by convection"), TEXT("Only through metals")}, 0, TEXT("Radiation uses electromagnetic waves."), TEXT("The Sun warms Earth through space."), TEXT("Thermal radiation can travel through a vacuum."), EBHDiagramType::EnergyChain, TEXT("radiation no medium needed"), 0.0f, 0.0f, {.LabelA=TEXT("source"), .LabelB=TEXT("vacuum"), .LabelC=TEXT("receiver")}},
		{TEXT("Fossil fuels"), TEXT("True or false: fossil fuels are renewable on human timescales."), {TEXT("False"), TEXT("True"), TEXT("Only coal is renewable"), TEXT("Only gas is renewable")}, 0, TEXT("They take millions of years to form."), TEXT("Compare formation time with use time."), TEXT("Fossil fuels are non-renewable on human timescales."), EBHDiagramType::EnergyChain, TEXT("fossil fuels non-renewable"), 0.0f, 0.0f, {.LabelA=TEXT("fossil fuel"), .LabelB=TEXT("energy store")}}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Efficiency"), TEXT("A motor takes 800 J and gives 200 J useful output. What is efficiency?"), {TEXT("25%"), TEXT("75%"), TEXT("400%"), TEXT("600%")}, 0, TEXT("Use useful output divided by input times 100."), TEXT("Show 200 / 800 x 100."), TEXT("Efficiency = 200 / 800 x 100 = 25%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / input x 100%"), 25.0f, 0.1f, {.ValueA=800.0f, .ValueB=200.0f, .LabelA=TEXT("in 800 J"), .LabelB=TEXT("useful 200 J"), .LabelC=TEXT("wasted ?")}},
		{TEXT("Work done"), TEXT("A 50 N force moves a box 4 m. What work is done?"), {TEXT("200 J"), TEXT("54 J"), TEXT("12.5 J"), TEXT("46 J")}, 0, TEXT("Use W = Fd."), TEXT("Multiply force by distance."), TEXT("Work done = 50 x 4 = 200 J."), EBHDiagramType::EnergyChain, TEXT("W = Fd"), 200.0f, 0.1f, {.LabelA=TEXT("50 N"), .LabelB=TEXT("4 m"), .LabelC=TEXT("W = ?")}},
		{TEXT("GPE"), TEXT("A 5 kg mass is lifted 3 m. Use g = 10 N/kg. What GPE is gained?"), {TEXT("150 J"), TEXT("18 J"), TEXT("50 J"), TEXT("15 J")}, 0, TEXT("Use Ep = mgh."), TEXT("Multiply mass, g, and height."), TEXT("Ep = 5 x 10 x 3 = 150 J."), EBHDiagramType::EnergyChain, TEXT("Ep = mgh"), 150.0f, 0.1f, {.LabelA=TEXT("5 kg"), .LabelB=TEXT("3 m"), .LabelC=TEXT("g 10 N/kg")}},
		{TEXT("Kinetic energy"), TEXT("A 4 kg trolley moves at 3 m/s. What is its kinetic energy?"), {TEXT("18 J"), TEXT("12 J"), TEXT("36 J"), TEXT("6 J")}, 0, TEXT("Use Ek = 1/2 mv^2."), TEXT("Square the velocity first."), TEXT("Ek = 0.5 x 4 x 3^2 = 18 J."), EBHDiagramType::EnergyChain, TEXT("Ek = 1/2 mv^2"), 18.0f, 0.1f, {.LabelA=TEXT("4 kg"), .LabelB=TEXT("3 m/s"), .LabelC=TEXT("Ek = ?")}},
		{TEXT("Power"), TEXT("A student transfers 600 J in 15 s. What is the power?"), {TEXT("40 W"), TEXT("9000 W"), TEXT("615 W"), TEXT("0.025 W")}, 0, TEXT("Power is energy divided by time."), TEXT("Show 600 / 15."), TEXT("Power = 600 / 15 = 40 W."), EBHDiagramType::EnergyChain, TEXT("P = E / t"), 40.0f, 0.1f, {.LabelA=TEXT("600 J"), .LabelB=TEXT("15 s"), .LabelC=TEXT("P = ?")}},
		{TEXT("Specific heat capacity"), TEXT("A 2 kg block with c = 500 J/kg degree C warms by 3 degree C. What energy is transferred?"), {TEXT("3000 J"), TEXT("505 J"), TEXT("1500 J"), TEXT("333 J")}, 0, TEXT("Use E = mc delta T."), TEXT("Multiply 2 x 500 x 3."), TEXT("E = 2 x 500 x 3 = 3000 J."), EBHDiagramType::EnergyChain, TEXT("E = mc delta T"), 3000.0f, 1.0f, {.LabelA=TEXT("2 kg"), .LabelB=TEXT("c 500"), .LabelC=TEXT("dT 3 deg")}},
		{TEXT("Wasted energy"), TEXT("A device takes 250 J and gives 160 J useful output. How much is wasted?"), {TEXT("90 J"), TEXT("410 J"), TEXT("1.56 J"), TEXT("160 J")}, 0, TEXT("Input equals useful plus wasted."), TEXT("Subtract useful output from input."), TEXT("Wasted energy = 250 - 160 = 90 J."), EBHDiagramType::Sankey, TEXT("wasted = input - useful"), 90.0f, 0.1f, {.ValueA=250.0f, .ValueB=160.0f, .LabelA=TEXT("in 250 J"), .LabelB=TEXT("useful 160 J"), .LabelC=TEXT("wasted ?")}}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Sankey evaluation"), TEXT("Two lamps use the same input energy. Lamp A has a wider useful light arrow. What should the conclusion say?"), {TEXT("Lamp A is more efficient because a larger fraction is useful light"), TEXT("Lamp A wastes more energy because useful arrows are ignored"), TEXT("Both lamps must be 100% efficient"), TEXT("Efficiency cannot be compared from a Sankey diagram")}, 0, TEXT("Compare useful fractions."), TEXT("Use arrow widths as evidence."), TEXT("A wider useful output arrow for the same input means a greater useful fraction and higher efficiency."), EBHDiagramType::Sankey, TEXT("exam skill: compare Sankey diagrams"), 0.0f, 0.0f, {.LabelA=TEXT("same input"), .LabelB=TEXT("A wide useful"), .LabelC=TEXT("B narrow")}},
		{TEXT("Thermal insulation"), TEXT("A student compares cups with different insulation. Which method is fairest?"), {TEXT("Use equal water volumes and same starting temperature"), TEXT("Use different volumes to save time"), TEXT("Start one cup hotter to see it better"), TEXT("Change the room temperature for each cup")}, 0, TEXT("Only insulation should change."), TEXT("Keep starting conditions the same."), TEXT("A fair insulation comparison keeps volume, starting temperature, container size, and surroundings controlled."), EBHDiagramType::EnergyChain, TEXT("exam skill: control variables"), 0.0f, 0.0f, {.LabelA=TEXT("cup A"), .LabelB=TEXT("cup B"), .LabelC=TEXT("heat loss")}},
		{TEXT("Specific heat practical"), TEXT("In an electrical heating experiment, why should the block be insulated?"), {TEXT("To reduce energy transfer to surroundings so the calculated c is more accurate"), TEXT("To stop current flowing through the heater"), TEXT("To make the mass zero"), TEXT("To increase room temperature deliberately")}, 0, TEXT("Energy losses affect the result."), TEXT("Think about where supplied energy goes."), TEXT("Insulation reduces energy losses, so more supplied energy heats the block."), EBHDiagramType::EnergyChain, TEXT("exam skill: reduce heat loss"), 0.0f, 0.0f, {.LabelA=TEXT("heater"), .LabelB=TEXT("block"), .LabelC=TEXT("surroundings")}},
		{TEXT("Resource comparison"), TEXT("A school considers solar panels. Which evaluation is strongest?"), {TEXT("Solar is renewable and low-emission in use, but output depends on light intensity"), TEXT("Solar is non-renewable because panels are black"), TEXT("Solar always gives full power at night"), TEXT("Solar produces greenhouse gases whenever light hits it")}, 0, TEXT("Give both advantage and limitation."), TEXT("Mention variable output."), TEXT("Solar power is renewable and low-emission during use, but output varies with light intensity and weather."), EBHDiagramType::EnergyChain, TEXT("exam skill: evaluate resources"), 0.0f, 0.0f, {.LabelA=TEXT("sunlight"), .LabelB=TEXT("solar panel"), .LabelC=TEXT("electrical")}},
		{TEXT("Energy stores"), TEXT("A ball is thrown upward. Which account is best as it rises?"), {TEXT("Kinetic energy decreases while gravitational potential energy increases"), TEXT("Chemical energy becomes nuclear energy only"), TEXT("Energy is destroyed because speed decreases"), TEXT("Thermal energy becomes charge")}, 0, TEXT("Track speed and height."), TEXT("Energy is transferred between stores."), TEXT("As the ball rises, kinetic energy is transferred to gravitational potential energy, with small losses."), EBHDiagramType::EnergyChain, TEXT("exam skill: track stores"), 0.0f, 0.0f, {.LabelA=TEXT("thrown up"), .LabelB=TEXT("rising"), .LabelC=TEXT("highest pt")}},
		{TEXT("Power explanation"), TEXT("Two motors lift the same load to the same height. Motor A takes less time. What is true?"), {TEXT("Motor A has greater power because it transfers the same energy faster"), TEXT("Motor A does less work because time is shorter"), TEXT("Motor A has no energy input"), TEXT("Motor A must be less efficient")}, 0, TEXT("Power is energy transfer per second."), TEXT("Same work in less time means greater power."), TEXT("For the same energy transfer, a shorter time means a higher power."), EBHDiagramType::EnergyChain, TEXT("exam skill: relate power and time"), 0.0f, 0.0f, {.LabelA=TEXT("same work"), .LabelB=TEXT("A less time"), .LabelC=TEXT("B more time")}}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Sankey"), TEXT("A Sankey input is 500 J and useful output is 300 J. What is the wasted output?"), {TEXT("200 J"), TEXT("800 J"), TEXT("60 J"), TEXT("1.67 J")}, 0, TEXT("Input splits into useful and wasted."), TEXT("Subtract useful from total input."), TEXT("Wasted output = 500 - 300 = 200 J."), EBHDiagramType::Sankey, TEXT("input = useful + wasted"), 200.0f, 0.1f, {.ValueA=500.0f, .ValueB=300.0f, .LabelA=TEXT("in 500 J"), .LabelB=TEXT("useful 300 J"), .LabelC=TEXT("wasted ?")}},
		{TEXT("Cooling graph"), TEXT("Which cooling curve shows the best insulation?"), {TEXT("The curve with the smallest temperature drop in the same time"), TEXT("The curve dropping fastest"), TEXT("The curve with the highest final room temperature"), TEXT("The curve with no starting point")}, 0, TEXT("Good insulation slows energy loss."), TEXT("Compare temperature change over the same time."), TEXT("The best insulation gives the slowest temperature decrease."), EBHDiagramType::EnergyChain, TEXT("less drop -> better insulation"), 0.0f, 0.0f, {.LabelA=TEXT("hot start"), .LabelB=TEXT("cooling"), .LabelC=TEXT("room temp")}},
		{TEXT("Efficiency bar"), TEXT("A device is 65% efficient with 1000 J input. How much useful energy is output?"), {TEXT("650 J"), TEXT("65 J"), TEXT("350 J"), TEXT("1538 J")}, 0, TEXT("Find 65% of the input."), TEXT("Use 0.65 x 1000."), TEXT("Useful energy = 0.65 x 1000 = 650 J."), EBHDiagramType::Sankey, TEXT("useful = efficiency x input"), 650.0f, 1.0f, {.ValueA=1000.0f, .LabelA=TEXT("in 1000 J"), .LabelB=TEXT("65% eff"), .LabelC=TEXT("useful ?")}},
		{TEXT("Heating graph"), TEXT("A temperature-time graph has a steeper rising line for the same heater. What does that suggest?"), {TEXT("The sample has lower thermal capacity or lower mass"), TEXT("The sample has no energy input"), TEXT("The heater is switched off"), TEXT("Energy is destroyed faster")}, 0, TEXT("Steeper temperature rise means faster temperature change."), TEXT("For the same power, less energy is needed per degree."), TEXT("A steeper rise suggests less energy is needed for each degree C, such as lower mass or lower heat capacity."), EBHDiagramType::EnergyChain, TEXT("temperature gradient"), 0.0f, 0.0f, {.LabelA=TEXT("steeper rise")}},
		{TEXT("Pendulum transfers"), TEXT("The diagram shows a swinging pendulum at its highest point. Which transfer happens as it falls to the bottom?"), {TEXT("Gravitational to kinetic"), TEXT("Kinetic to gravitational"), TEXT("Chemical to light"), TEXT("Elastic to sound")}, 0, TEXT("Falling speeds it up."), TEXT("The top is the high store, the bottom is fastest."), TEXT("As it falls, gravitational potential energy transfers to kinetic energy."), EBHDiagramType::EnergyChain, TEXT("GPE -> KE"), 0.0f, 0.0f, {.LabelA=TEXT("top: GPE"), .LabelB=TEXT("bottom: KE")}},
		{TEXT("Sankey arrows"), TEXT("A Sankey diagram for an old bulb shows a thin light arrow and a very wide heat arrow. What does the wide arrow tell you?"), {TEXT("Most energy is wasted as heat"), TEXT("Most energy becomes light"), TEXT("No energy is input"), TEXT("The bulb is very efficient")}, 0, TEXT("Arrow width shows the amount of energy."), TEXT("Compare the two output widths."), TEXT("The wide heat arrow means most of the input energy is wasted as heat."), EBHDiagramType::Sankey, TEXT("arrow width = energy"), 0.0f, 0.0f, {.LabelA=TEXT("thin light"), .LabelB=TEXT("wide heat")}},
		{TEXT("Sankey input"), TEXT("A Sankey diagram shows a 90 J useful arrow and a 60 J wasted arrow. What was the input energy?"), {TEXT("150 J"), TEXT("30 J"), TEXT("5400 J"), TEXT("1.5 J")}, 0, TEXT("Input = useful + wasted."), TEXT("Add the two output arrows."), TEXT("Input = 90 + 60 = 150 J."), EBHDiagramType::Sankey, TEXT("input = useful + wasted"), 150.0f, 0.1f, {.ValueB=90.0f, .ValueC=60.0f, .LabelA=TEXT("in = ?"), .LabelB=TEXT("useful 90 J"), .LabelC=TEXT("wasted 60 J")}},
		{TEXT("Efficiency from Sankey"), TEXT("A motor's Sankey diagram shows 500 J input and a 350 J useful arrow. What is its efficiency?"), {TEXT("70%"), TEXT("30%"), TEXT("150 J"), TEXT("143%")}, 0, TEXT("efficiency = useful / total x 100%."), TEXT("Divide useful by input."), TEXT("efficiency = 350 / 500 x 100% = 70%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / input x 100%"), 70.0f, 0.1f, {.ValueA=500.0f, .ValueB=350.0f, .LabelA=TEXT("in 500 J"), .LabelB=TEXT("useful 350 J")}},
		{TEXT("Reducing waste"), TEXT("A motor's Sankey diagram shows heat wasted through friction. Which change would make the useful arrow wider for the same input?"), {TEXT("Lubricating the bearings"), TEXT("Adding more friction"), TEXT("Reducing the input only"), TEXT("Painting it black")}, 0, TEXT("Less wasted energy means more useful energy for the same input."), TEXT("Target the friction loss shown."), TEXT("Lubrication reduces frictional heat loss, so more of the input becomes useful output."), EBHDiagramType::EnergyChain, TEXT("less waste -> more useful"), 0.0f, 0.0f, {.LabelA=TEXT("friction heat"), .LabelB=TEXT("widen useful")}},
		{TEXT("Useful from efficiency"), TEXT("A Sankey-labelled appliance is 40% efficient with an 800 J input. How much useful energy does the output arrow show?"), {TEXT("320 J"), TEXT("480 J"), TEXT("40 J"), TEXT("2000 J")}, 0, TEXT("useful = efficiency x input."), TEXT("Use 0.40 x 800."), TEXT("Useful = 0.40 x 800 = 320 J."), EBHDiagramType::Sankey, TEXT("useful = efficiency x input"), 320.0f, 1.0f, {.ValueA=800.0f, .LabelA=TEXT("in 800 J"), .LabelB=TEXT("40% useful")}}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Stores"), TEXT("Choose the correct matching pair."), {TEXT("Moving object -> kinetic; hot object -> thermal"), TEXT("Moving object -> nuclear; hot object -> magnetic"), TEXT("Raised object -> chemical; stretched object -> electrostatic"), TEXT("Battery -> gravitational; fuel -> kinetic")}, 0, TEXT("Match the store to the physical change."), TEXT("Movement and temperature are clues."), TEXT("Motion means kinetic energy; higher temperature means thermal energy."), EBHDiagramType::EnergyChain, TEXT("energy stores"), 0.0f, 0.0f},
		{TEXT("Pathways"), TEXT("Match the pathway to the example."), {TEXT("Mechanically -> lifting a box; electrically -> current in a lamp"), TEXT("Mechanically -> infrared from heater; electrically -> sound in air"), TEXT("Radiation -> current in wire; heating -> lifting a box"), TEXT("Nuclear -> pushing a door; convection -> light through space")}, 0, TEXT("A force doing work is mechanical."), TEXT("A current transfers energy electrically."), TEXT("Lifting a box transfers energy mechanically; a lamp circuit transfers energy electrically."), EBHDiagramType::EnergyChain, TEXT("transfer pathways"), 0.0f, 0.0f, {.LabelA=TEXT("pathway"), .LabelB=TEXT("example")}},
		{TEXT("Resources"), TEXT("Match the resource to a limitation."), {TEXT("Wind -> variable output; fossil fuels -> greenhouse gases"), TEXT("Wind -> non-renewable fuel; fossil fuels -> no emissions"), TEXT("Solar -> works best at night; nuclear -> no waste issues"), TEXT("Tidal -> depends on sunlight; geothermal -> burns coal")}, 0, TEXT("Use realistic advantages and limits."), TEXT("Variable output is common for weather-dependent resources."), TEXT("Wind output varies; fossil fuels release greenhouse gases when burned."), EBHDiagramType::EnergyChain, TEXT("resource limitations"), 0.0f, 0.0f, {.LabelA=TEXT("resource"), .LabelB=TEXT("limit")}},
		{TEXT("Thermal transfer"), TEXT("Match the transfer to its description."), {TEXT("Conduction -> particle collisions; convection -> bulk fluid movement"), TEXT("Conduction -> vacuum waves; convection -> solid-only electrons"), TEXT("Radiation -> needs water; convection -> no medium"), TEXT("Evaporation -> current flow; conduction -> gamma rays")}, 0, TEXT("Convection needs a fluid."), TEXT("Conduction uses neighbouring particles and electrons in metals."), TEXT("Conduction transfers energy by collisions; convection transfers energy by moving fluid."), EBHDiagramType::EnergyChain, TEXT("thermal transfer"), 0.0f, 0.0f, {.LabelA=TEXT("transfer"), .LabelB=TEXT("description")}}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Efficiency method"), TEXT("Order an efficiency calculation."), {TEXT("Find useful output -> find total input -> divide useful by input -> multiply by 100"), TEXT("Find wasted only -> multiply by time -> divide by force -> add percent"), TEXT("Find speed -> square charge -> divide by current -> add percent"), TEXT("Find mass -> add voltage -> subtract wavelength -> add percent")}, 0, TEXT("Efficiency is a useful fraction."), TEXT("Convert to a percentage at the end."), TEXT("Efficiency percentage is useful output divided by total input, multiplied by 100."), EBHDiagramType::Sankey, TEXT("efficiency = useful / input x 100%"), 0.0f, 0.0f},
		{TEXT("Power station"), TEXT("Order a coal power station energy chain."), {TEXT("Chemical store in coal -> thermal energy in steam -> kinetic turbine -> electrical generator"), TEXT("Electrical generator -> coal forms -> steam freezes -> turbine stops"), TEXT("Nuclear store in air -> static charge -> elastic turbine -> chemical output"), TEXT("Solar panel -> coal burns -> current becomes mass -> generator cools")}, 0, TEXT("Fuel heats water to make steam."), TEXT("The generator comes after the turbine."), TEXT("Coal's chemical energy heats steam, which turns a turbine that drives a generator."), EBHDiagramType::EnergyChain, TEXT("coal to electricity"), 0.0f, 0.0f, {.LabelA=TEXT("coal"), .LabelB=TEXT("steps ?"), .LabelC=TEXT("electrical")}},
		{TEXT("Specific heat method"), TEXT("Order a specific heat capacity calculation."), {TEXT("Measure mass -> measure energy supplied -> measure temperature rise -> use c = E / (m delta T)"), TEXT("Measure current only -> square mass -> divide by time -> add Pa"), TEXT("Measure wavelength -> count crests -> divide by force -> add J/kg"), TEXT("Measure fuse rating -> find charge -> subtract voltage -> add degree C")}, 0, TEXT("Use E = mc delta T."), TEXT("Rearrange for c."), TEXT("Specific heat capacity is calculated from energy supplied, mass, and temperature rise."), EBHDiagramType::EnergyChain, TEXT("c = E / m delta T"), 0.0f, 0.0f, {.LabelA=TEXT("energy J"), .LabelB=TEXT("mass kg"), .LabelC=TEXT("temp rise")}},
		{TEXT("Energy store transfer"), TEXT("Order the main energy change as a roller cart rolls downhill."), {TEXT("Gravitational store decreases -> kinetic store increases -> some energy dissipates thermally"), TEXT("Thermal store decreases -> nuclear store increases -> kinetic becomes zero"), TEXT("Chemical store appears -> gravitational store increases -> speed falls"), TEXT("Electrical store rises -> charge flows -> height increases")}, 0, TEXT("Height decreases and speed increases."), TEXT("Include dissipated energy for real motion."), TEXT("As the cart descends, gravitational potential energy transfers mostly to kinetic energy, with some dissipated thermally."), EBHDiagramType::EnergyChain, TEXT("GPE to KE plus losses"), 0.0f, 0.0f, {.LabelA=TEXT("hill top"), .LabelB=TEXT("rolling down"), .LabelC=TEXT("bottom")}}
	};

	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy_ext"), TEXT("Energy"), EBHQuestionType::MultipleChoice, MC, UE_ARRAY_COUNT(MC));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy_ext"), TEXT("Energy"), EBHQuestionType::TrueFalse, TF, UE_ARRAY_COUNT(TF));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy_ext"), TEXT("Energy"), EBHQuestionType::Calculation, Calc, UE_ARRAY_COUNT(Calc));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy_ext"), TEXT("Energy"), EBHQuestionType::FormulaFill, Skill, UE_ARRAY_COUNT(Skill));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy_ext"), TEXT("Energy"), EBHQuestionType::GraphReading, Graph, UE_ARRAY_COUNT(Graph));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy_ext"), TEXT("Energy"), EBHQuestionType::DragDropMatching, Match, UE_ARRAY_COUNT(Match));
	AddSpecs(Bank, EBHPhysicsTopic::Energy, TEXT("energy_ext"), TEXT("Energy"), EBHQuestionType::Ordering, Order, UE_ARRAY_COUNT(Order));
}

TArray<FBHRevisionQuestion> BuildBuiltInQuestionBank()
{
	TArray<FBHRevisionQuestion> Bank;
	Bank.Reserve(368);
	BuildForces(Bank);
	BuildElectricity(Bank);
	BuildWaves(Bank);
	BuildEnergy(Bank);
	BuildForcesExtension(Bank);
	BuildElectricityExtension(Bank);
	BuildWavesExtension(Bank);
	BuildEnergyExtension(Bank);
	return Bank;
}

int32 TopicIndex(EBHPhysicsTopic Topic)
{
	return FMath::Clamp(static_cast<int32>(Topic), 0, 3);
}

int32 DifficultyIndex(EBHQuestionDifficulty Difficulty)
{
	return FMath::Clamp(static_cast<int32>(Difficulty), 0, 2);
}

int32 TypeIndex(EBHQuestionType Type)
{
	return FMath::Clamp(static_cast<int32>(Type), 0, 6);
}

// --- JSON support (Pillar 5: editable question content) ---------------------------------------

// Enums round-trip as their reflected names (e.g. "ForcesAndMotion", "MultipleChoice") so the
// JSON stays readable and stable across enum reorders.
template <typename TEnum>
FString EnumToKey(TEnum Value)
{
	if (const UEnum* EnumPtr = StaticEnum<TEnum>())
	{
		return EnumPtr->GetNameStringByValue(static_cast<int64>(Value));
	}
	return FString::FromInt(static_cast<int64>(Value));
}

template <typename TEnum>
TEnum KeyToEnum(const FString& Key, TEnum Default)
{
	if (const UEnum* EnumPtr = StaticEnum<TEnum>())
	{
		const int64 Value = EnumPtr->GetValueByNameString(Key);
		if (Value != INDEX_NONE)
		{
			return static_cast<TEnum>(Value);
		}
	}
	return Default;
}

FString JsonStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Default = FString())
{
	FString Value;
	return (Obj.IsValid() && Obj->TryGetStringField(Field, Value)) ? Value : Default;
}

double JsonNum(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double Default = 0.0)
{
	double Value = Default;
	return (Obj.IsValid() && Obj->TryGetNumberField(Field, Value)) ? Value : Default;
}

int32 JsonInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Default = 0)
{
	return Obj.IsValid() ? static_cast<int32>(FMath::RoundToInt(JsonNum(Obj, Field, Default))) : Default;
}

// Structural validation shared by the active-bank Validate() and the override loader: well-formed
// questions, unique ids, and every physics topic represented (so each station type can pull one).
bool ValidateQuestionSet(const TArray<FBHRevisionQuestion>& Bank, FString& OutSummary)
{
	int32 TopicCounts[4] = {0, 0, 0, 0};
	TSet<FString> Ids;
	bool bValid = Bank.Num() > 0;
	for (const FBHRevisionQuestion& Question : Bank)
	{
		if (Question.Id.IsEmpty() || Ids.Contains(Question.Id) || Question.Prompt.IsEmpty() || Question.Hint.IsEmpty() || Question.Explanation.IsEmpty() || Question.Answer.Choices.Num() != 4 || !Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex))
		{
			bValid = false;
		}
		Ids.Add(Question.Id);
		++TopicCounts[TopicIndex(Question.Topic)];
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		bValid = bValid && TopicCounts[Index] > 0;
	}
	OutSummary = FString::Printf(TEXT("Physics revision bank %s: total=%d topics=[%d,%d,%d,%d]"),
		bValid ? TEXT("valid") : TEXT("INVALID"),
		Bank.Num(),
		TopicCounts[0], TopicCounts[1], TopicCounts[2], TopicCounts[3]);
	return bValid;
}
}

const TArray<FBHRevisionQuestion>& FBHRevisionQuestionBank::GetBuiltInQuestions()
{
	static const TArray<FBHRevisionQuestion> Bank = BuildBuiltInQuestionBank();
	return Bank;
}

const TArray<FBHRevisionQuestion>& FBHRevisionQuestionBank::GetQuestions()
{
	// Prefer an editable JSON override when present and structurally valid; otherwise fall back
	// to the compiled built-in bank. Resolved once and cached for the process.
	static const TArray<FBHRevisionQuestion> Bank = []() -> TArray<FBHRevisionQuestion>
	{
		const FString Path = GetOverrideFilePath();
		FString JsonText;
		if (FFileHelper::LoadFileToString(JsonText, *Path))
		{
			TArray<FBHRevisionQuestion> Loaded;
			FString Error;
			if (ParseQuestionsFromJson(JsonText, Loaded, Error))
			{
				FString ValidateSummary;
				if (ValidateQuestionSet(Loaded, ValidateSummary))
				{
					UE_LOG(LogTemp, Log, TEXT("[BHRevision] Loaded %d question(s) from override %s"), Loaded.Num(), *Path);
					return Loaded;
				}
				UE_LOG(LogTemp, Warning, TEXT("[BHRevision] Override question bank failed validation (%s) - using built-in bank. %s"), *Path, *ValidateSummary);
			}
			else
			{
				// Back up the unreadable file (mirrors FBHLessonPresetStore) and fall back.
				const FString Backup = Path + TEXT(".invalid-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
				IFileManager::Get().Copy(*Backup, *Path);
				UE_LOG(LogTemp, Warning, TEXT("[BHRevision] Could not parse override question bank %s (%s). Backed up to %s; using built-in bank."), *Path, *Error, *Backup);
			}
		}
		return GetBuiltInQuestions();
	}();
	return Bank;
}

bool FBHRevisionQuestionBank::Validate(FString& OutSummary)
{
	// Structural validation of whatever bank is active (built-in or JSON override), so teacher
	// content of any size is accepted as long as each question is well-formed.
	return ValidateQuestionSet(GetQuestions(), OutSummary);
}

bool FBHRevisionQuestionBank::ValidateBuiltInDistribution(FString& OutSummary)
{
	// Strict guard for the shipped compiled bank: exact totals/distribution. Tests assert this so
	// the built-in content cannot silently drift; teacher JSON overrides are not held to it.
	const TArray<FBHRevisionQuestion>& Bank = GetBuiltInQuestions();
	int32 TopicCounts[4] = {0, 0, 0, 0};
	int32 DifficultyCounts[4][3] = {};
	int32 TypeCounts[7] = {0, 0, 0, 0, 0, 0, 0};
	TSet<FString> Ids;
	bool bValid = Bank.Num() == 368;
	for (const FBHRevisionQuestion& Question : Bank)
	{
		if (Question.Id.IsEmpty() || Ids.Contains(Question.Id) || Question.Prompt.IsEmpty() || Question.Hint.IsEmpty() || Question.Explanation.IsEmpty() || Question.Answer.Choices.Num() != 4 || !Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex))
		{
			bValid = false;
		}
		Ids.Add(Question.Id);
		const int32 TIndex = TopicIndex(Question.Topic);
		const int32 DIndex = DifficultyIndex(Question.Difficulty);
		++TopicCounts[TIndex];
		++DifficultyCounts[TIndex][DIndex];
		++TypeCounts[TypeIndex(Question.Type)];
	}

	for (int32 Index = 0; Index < 4; ++Index)
	{
		bValid = bValid && TopicCounts[Index] == 92;
		bValid = bValid && DifficultyCounts[Index][0] == 28;
		bValid = bValid && DifficultyCounts[Index][1] == 36;
		bValid = bValid && DifficultyCounts[Index][2] == 28;
	}

	const int32 ExpectedTypeCounts[7] = {80, 40, 56, 48, 80, 32, 32};
	for (int32 Index = 0; Index < 7; ++Index)
	{
		bValid = bValid && TypeCounts[Index] == ExpectedTypeCounts[Index];
	}

	OutSummary = FString::Printf(TEXT("Physics revision bank %s: total=%d topics=[%d,%d,%d,%d] types=[mc:%d tf:%d calc:%d skill:%d graph:%d match:%d order:%d]"),
		bValid ? TEXT("valid") : TEXT("INVALID"),
		Bank.Num(),
		TopicCounts[0], TopicCounts[1], TopicCounts[2], TopicCounts[3],
		TypeCounts[0], TypeCounts[1], TypeCounts[2], TypeCounts[3], TypeCounts[4], TypeCounts[5], TypeCounts[6]);
	return bValid;
}

bool FBHRevisionQuestionBank::FindQuestion(const FString& Id, FBHRevisionQuestion& OutQuestion)
{
	for (const FBHRevisionQuestion& Question : GetQuestions())
	{
		if (Question.Id.Equals(Id, ESearchCase::IgnoreCase))
		{
			OutQuestion = Question;
			return true;
		}
	}
	return false;
}

bool FBHRevisionQuestionBank::SelectQuestion(EBHPhysicsTopic Topic, EBHRevisionDifficultyMix DifficultyMix, int32 Seed, const TArray<EBHPhysicsTopic>& WeakTopics, FBHRevisionQuestion& OutQuestion)
{
	TArray<const FBHRevisionQuestion*> Candidates;
	TArray<const FBHRevisionQuestion*> TopicCandidates;
	const bool bWeakTopic = WeakTopics.Contains(Topic);
	for (const FBHRevisionQuestion& Question : GetQuestions())
	{
		if (Question.Topic != Topic)
		{
			continue;
		}

		TopicCandidates.Add(&Question);
		bool bDifficultyAllowed = true;
		if (DifficultyMix == EBHRevisionDifficultyMix::Easy)
		{
			bDifficultyAllowed = Question.Difficulty == EBHQuestionDifficulty::Easy || Question.Difficulty == EBHQuestionDifficulty::Medium;
		}
		else if (DifficultyMix == EBHRevisionDifficultyMix::Hard)
		{
			bDifficultyAllowed = Question.Difficulty == EBHQuestionDifficulty::Medium || Question.Difficulty == EBHQuestionDifficulty::Hard;
		}
		else if (DifficultyMix == EBHRevisionDifficultyMix::Adaptive && bWeakTopic)
		{
			bDifficultyAllowed = Question.Difficulty != EBHQuestionDifficulty::Easy;
		}
		if (bDifficultyAllowed)
		{
			Candidates.Add(&Question);
		}
	}

	if (Candidates.IsEmpty())
	{
		Candidates = TopicCandidates;
	}
	if (Candidates.IsEmpty())
	{
		return false;
	}

	const int32 ChosenIndex = FMath::Abs(Seed) % Candidates.Num();
	OutQuestion = *Candidates[ChosenIndex];
	return true;
}

bool FBHRevisionQuestionBank::SelectQuestionByDifficulty(EBHPhysicsTopic Topic, EBHQuestionDifficulty Difficulty, int32 Seed, FBHRevisionQuestion& OutQuestion)
{
	TArray<const FBHRevisionQuestion*> Candidates;
	TArray<const FBHRevisionQuestion*> TopicCandidates;
	for (const FBHRevisionQuestion& Question : GetQuestions())
	{
		if (Question.Topic != Topic)
		{
			continue;
		}

		TopicCandidates.Add(&Question);
		if (Question.Difficulty == Difficulty)
		{
			Candidates.Add(&Question);
		}
	}

	if (Candidates.IsEmpty())
	{
		Candidates = TopicCandidates;
	}
	if (Candidates.IsEmpty())
	{
		return false;
	}

	const int32 ChosenIndex = FMath::Abs(Seed) % Candidates.Num();
	OutQuestion = *Candidates[ChosenIndex];
	return true;
}

EBHPhysicsTopic FBHRevisionQuestionBank::TopicForStationType(EBHObjectiveStationType StationType)
{
	switch (StationType)
	{
	case EBHObjectiveStationType::Valve:
		return EBHPhysicsTopic::ForcesAndMotion;
	case EBHObjectiveStationType::Terminal:
		return EBHPhysicsTopic::Electricity;
	case EBHObjectiveStationType::Antenna:
		return EBHPhysicsTopic::Waves;
	case EBHObjectiveStationType::Evidence:
		return EBHPhysicsTopic::Energy;
	default:
		return EBHPhysicsTopic::ForcesAndMotion;
	}
}

FString FBHRevisionQuestionBank::TopicToString(EBHPhysicsTopic Topic)
{
	switch (Topic)
	{
	case EBHPhysicsTopic::ForcesAndMotion:
		return TEXT("Forces and Motion");
	case EBHPhysicsTopic::Electricity:
		return TEXT("Electricity");
	case EBHPhysicsTopic::Waves:
		return TEXT("Waves");
	case EBHPhysicsTopic::Energy:
		return TEXT("Energy");
	default:
		return TEXT("Physics");
	}
}

FString FBHRevisionQuestionBank::QuestionTypeToString(EBHQuestionType Type)
{
	switch (Type)
	{
	case EBHQuestionType::MultipleChoice:
		return TEXT("Multiple-choice");
	case EBHQuestionType::TrueFalse:
		return TEXT("True/false");
	case EBHQuestionType::Calculation:
		return TEXT("Calculation");
	case EBHQuestionType::FormulaFill:
		return TEXT("IGCSE skill");
	case EBHQuestionType::GraphReading:
		return TEXT("Graph-reading");
	case EBHQuestionType::DragDropMatching:
		return TEXT("Matching");
	case EBHQuestionType::Ordering:
		return TEXT("Ordering");
	default:
		return TEXT("Question");
	}
}

FString FBHRevisionQuestionBank::DifficultyToString(EBHQuestionDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EBHQuestionDifficulty::Easy:
		return TEXT("Easy");
	case EBHQuestionDifficulty::Medium:
		return TEXT("Medium");
	case EBHQuestionDifficulty::Hard:
		return TEXT("Hard");
	default:
		return TEXT("Mixed");
	}
}

int32 FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic Topic)
{
	return 1 << TopicIndex(Topic);
}

FString FBHRevisionQuestionBank::GetOverrideFilePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ClassroomPresets") / TEXT("QuestionBank.json"));
}

FString FBHRevisionQuestionBank::SerializeQuestionsToJson(const TArray<FBHRevisionQuestion>& Questions)
{
	TArray<TSharedPtr<FJsonValue>> QuestionValues;
	QuestionValues.Reserve(Questions.Num());
	for (const FBHRevisionQuestion& Question : Questions)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Question.Id);
		Obj->SetStringField(TEXT("topic"), EnumToKey(Question.Topic));
		Obj->SetStringField(TEXT("topicName"), Question.TopicName);
		Obj->SetStringField(TEXT("subtopic"), Question.Subtopic);
		Obj->SetStringField(TEXT("difficulty"), EnumToKey(Question.Difficulty));
		Obj->SetStringField(TEXT("type"), EnumToKey(Question.Type));
		Obj->SetStringField(TEXT("diagramType"), EnumToKey(Question.DiagramType));
		Obj->SetStringField(TEXT("prompt"), Question.Prompt);

		TSharedRef<FJsonObject> Answer = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> ChoiceValues;
		for (const FString& Choice : Question.Answer.Choices)
		{
			ChoiceValues.Add(MakeShared<FJsonValueString>(Choice));
		}
		Answer->SetArrayField(TEXT("choices"), ChoiceValues);
		Answer->SetNumberField(TEXT("correctChoiceIndex"), Question.Answer.CorrectChoiceIndex);
		Answer->SetStringField(TEXT("formula"), Question.Answer.Formula);
		Answer->SetNumberField(TEXT("numericAnswer"), Question.Answer.NumericAnswer);
		Answer->SetNumberField(TEXT("numericTolerance"), Question.Answer.NumericTolerance);
		Obj->SetObjectField(TEXT("answer"), Answer);

		TSharedRef<FJsonObject> Diagram = MakeShared<FJsonObject>();
		Diagram->SetNumberField(TEXT("valueA"), Question.Diagram.ValueA);
		Diagram->SetNumberField(TEXT("valueB"), Question.Diagram.ValueB);
		Diagram->SetNumberField(TEXT("valueC"), Question.Diagram.ValueC);
		Diagram->SetNumberField(TEXT("valueD"), Question.Diagram.ValueD);
		Diagram->SetStringField(TEXT("labelA"), Question.Diagram.LabelA);
		Diagram->SetStringField(TEXT("labelB"), Question.Diagram.LabelB);
		Diagram->SetStringField(TEXT("labelC"), Question.Diagram.LabelC);
		Diagram->SetStringField(TEXT("labelD"), Question.Diagram.LabelD);
		Diagram->SetStringField(TEXT("xAxis"), Question.Diagram.XAxis);
		Diagram->SetStringField(TEXT("yAxis"), Question.Diagram.YAxis);
		Diagram->SetNumberField(TEXT("angleOrShape"), Question.Diagram.AngleOrShape);
		Diagram->SetNumberField(TEXT("shapeVariant"), Question.Diagram.ShapeVariant);
		Diagram->SetStringField(TEXT("imageSoftPath"), Question.Diagram.ImageSoftPath);
		Obj->SetObjectField(TEXT("diagram"), Diagram);

		Obj->SetStringField(TEXT("hint"), Question.Hint);
		Obj->SetStringField(TEXT("correctionPrompt"), Question.CorrectionPrompt);
		Obj->SetStringField(TEXT("explanation"), Question.Explanation);
		Obj->SetNumberField(TEXT("masteryWeight"), Question.MasteryWeight);

		QuestionValues.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetArrayField(TEXT("questions"), QuestionValues);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

bool FBHRevisionQuestionBank::ParseQuestionsFromJson(const FString& JsonText, TArray<FBHRevisionQuestion>& OutQuestions, FString& OutError)
{
	OutQuestions.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("file is not valid JSON");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* QuestionValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("questions"), QuestionValues) || QuestionValues == nullptr)
	{
		OutError = TEXT("missing 'questions' array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *QuestionValues)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || ObjPtr == nullptr || !ObjPtr->IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FBHRevisionQuestion Question;
		Question.Id = JsonStr(Obj, TEXT("id"));
		Question.Topic = KeyToEnum(JsonStr(Obj, TEXT("topic")), EBHPhysicsTopic::ForcesAndMotion);
		Question.TopicName = JsonStr(Obj, TEXT("topicName"));
		if (Question.TopicName.IsEmpty())
		{
			Question.TopicName = TopicToString(Question.Topic);
		}
		Question.Subtopic = JsonStr(Obj, TEXT("subtopic"));
		Question.Difficulty = KeyToEnum(JsonStr(Obj, TEXT("difficulty")), EBHQuestionDifficulty::Easy);
		Question.Type = KeyToEnum(JsonStr(Obj, TEXT("type")), EBHQuestionType::MultipleChoice);
		Question.DiagramType = KeyToEnum(JsonStr(Obj, TEXT("diagramType")), EBHDiagramType::None);
		Question.Prompt = JsonStr(Obj, TEXT("prompt"));
		Question.Hint = JsonStr(Obj, TEXT("hint"));
		Question.CorrectionPrompt = JsonStr(Obj, TEXT("correctionPrompt"));
		Question.Explanation = JsonStr(Obj, TEXT("explanation"));

		const TSharedPtr<FJsonObject>* AnswerPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("answer"), AnswerPtr) && AnswerPtr && AnswerPtr->IsValid())
		{
			const TSharedPtr<FJsonObject>& Answer = *AnswerPtr;
			const TArray<TSharedPtr<FJsonValue>>* ChoiceValues = nullptr;
			if (Answer->TryGetArrayField(TEXT("choices"), ChoiceValues) && ChoiceValues)
			{
				for (const TSharedPtr<FJsonValue>& ChoiceValue : *ChoiceValues)
				{
					FString Choice;
					if (ChoiceValue.IsValid() && ChoiceValue->TryGetString(Choice))
					{
						Question.Answer.Choices.Add(Choice);
					}
				}
			}
			Question.Answer.CorrectChoiceIndex = JsonInt(Answer, TEXT("correctChoiceIndex"), 0);
			Question.Answer.Formula = JsonStr(Answer, TEXT("formula"));
			Question.Answer.NumericAnswer = static_cast<float>(JsonNum(Answer, TEXT("numericAnswer"), 0.0));
			Question.Answer.NumericTolerance = static_cast<float>(JsonNum(Answer, TEXT("numericTolerance"), 0.0));
		}

		const TSharedPtr<FJsonObject>* DiagramPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("diagram"), DiagramPtr) && DiagramPtr && DiagramPtr->IsValid())
		{
			const TSharedPtr<FJsonObject>& Diagram = *DiagramPtr;
			Question.Diagram.ValueA = static_cast<float>(JsonNum(Diagram, TEXT("valueA"), 0.0));
			Question.Diagram.ValueB = static_cast<float>(JsonNum(Diagram, TEXT("valueB"), 0.0));
			Question.Diagram.ValueC = static_cast<float>(JsonNum(Diagram, TEXT("valueC"), 0.0));
			Question.Diagram.ValueD = static_cast<float>(JsonNum(Diagram, TEXT("valueD"), 0.0));
			Question.Diagram.LabelA = JsonStr(Diagram, TEXT("labelA"));
			Question.Diagram.LabelB = JsonStr(Diagram, TEXT("labelB"));
			Question.Diagram.LabelC = JsonStr(Diagram, TEXT("labelC"));
			Question.Diagram.LabelD = JsonStr(Diagram, TEXT("labelD"));
			Question.Diagram.XAxis = JsonStr(Diagram, TEXT("xAxis"));
			Question.Diagram.YAxis = JsonStr(Diagram, TEXT("yAxis"));
			Question.Diagram.AngleOrShape = static_cast<float>(JsonNum(Diagram, TEXT("angleOrShape"), 0.0));
			Question.Diagram.ShapeVariant = static_cast<int32>(JsonNum(Diagram, TEXT("shapeVariant"), 0.0));
			Question.Diagram.ImageSoftPath = JsonStr(Diagram, TEXT("imageSoftPath"));
		}

		// masteryWeight is optional; derive it from difficulty when absent.
		Question.MasteryWeight = static_cast<float>(JsonNum(Obj, TEXT("masteryWeight"), MasteryWeightFor(Question.Difficulty)));

		OutQuestions.Add(Question);
	}

	if (OutQuestions.Num() == 0)
	{
		OutError = TEXT("no questions parsed from file");
		return false;
	}
	return true;
}

bool FBHRevisionQuestionBank::ExportQuestionBankToOverrideFile(FString& OutMessage)
{
	const FString Path = GetOverrideFilePath();
	const TArray<FBHRevisionQuestion>& BuiltIn = GetBuiltInQuestions();
	const FString Json = SerializeQuestionsToJson(BuiltIn);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutMessage = FString::Printf(TEXT("Could not write question bank to %s."), *Path);
		return false;
	}
	OutMessage = FString::Printf(TEXT("Exported %d question(s) to %s. Edit it and restart to use the custom bank; delete it to restore the built-in set."), BuiltIn.Num(), *Path);
	return true;
}

// Host console command: `bh.ExportQuestionBank` writes the built-in bank to the editable
// override path so a teacher can edit questions without recompiling.
static FAutoConsoleCommand GBHExportQuestionBankCommand(
	TEXT("bh.ExportQuestionBank"),
	TEXT("Writes the built-in physics question bank to Saved/ClassroomPresets/QuestionBank.json for editing."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FString Message;
		FBHRevisionQuestionBank::ExportQuestionBankToOverrideFile(Message);
		UE_LOG(LogTemp, Display, TEXT("[bh.ExportQuestionBank] %s"), *Message);
	}));

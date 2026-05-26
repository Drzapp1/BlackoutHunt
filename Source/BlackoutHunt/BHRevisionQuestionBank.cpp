#include "BHRevisionQuestionBank.h"

#include "Containers/Set.h"

#include <initializer_list>

namespace
{
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
		return Index == 0 ? EBHQuestionDifficulty::Easy : (Index < 3 ? EBHQuestionDifficulty::Medium : EBHQuestionDifficulty::Hard);
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
		{TEXT("Speed"), TEXT("A student sprints from one locked classroom to another. What is speed?"), {TEXT("Distance travelled per unit time"), TEXT("Force per unit area"), TEXT("Energy transferred per second"), TEXT("Mass times velocity")}, 0, TEXT("Speed compares distance with time."), TEXT("Say the definition, then identify the units."), TEXT("Speed is distance travelled per unit time and is measured in m/s."), EBHDiagramType::MotionGraph, TEXT("speed = distance / time"), 0.0f, 0.0f},
		{TEXT("Velocity"), TEXT("Which statement makes velocity different from speed?"), {TEXT("It includes direction"), TEXT("It uses kilograms"), TEXT("It cannot be negative"), TEXT("It is always constant")}, 0, TEXT("Velocity is a vector."), TEXT("Compare scalar and vector quantities."), TEXT("Velocity is speed in a given direction, so it has magnitude and direction."), EBHDiagramType::ForceArrows, TEXT("v = displacement / time"), 0.0f, 0.0f},
		{TEXT("Acceleration"), TEXT("The Teacher speeds up down a corridor. Which equation gives acceleration?"), {TEXT("a = (v - u) / t"), TEXT("a = vt"), TEXT("a = m / F"), TEXT("a = d / t")}, 0, TEXT("Acceleration is change in velocity per second."), TEXT("Write the initial and final velocity symbols before choosing."), TEXT("Acceleration is change in velocity divided by time taken."), EBHDiagramType::VelocityGraph, TEXT("a = (v - u) / t"), 0.0f, 0.0f},
		{TEXT("Vectors"), TEXT("Which card should go in the vector tray?"), {TEXT("Force"), TEXT("Time"), TEXT("Energy"), TEXT("Distance")}, 0, TEXT("Vectors need direction."), TEXT("Name the magnitude and the direction."), TEXT("Force is a vector because it has magnitude and direction."), EBHDiagramType::ForceArrows, TEXT("vector = magnitude + direction"), 0.0f, 0.0f},
		{TEXT("Resultant force"), TEXT("Two students push a desk east with 30 N and 20 N. What is the resultant?"), {TEXT("50 N east"), TEXT("10 N east"), TEXT("50 N west"), TEXT("0 N")}, 0, TEXT("Forces in the same direction add."), TEXT("Draw both arrows in the same direction."), TEXT("30 N + 20 N = 50 N east."), EBHDiagramType::ForceArrows, TEXT("resultant = sum of forces"), 50.0f, 0.0f},
		{TEXT("Newton's first law"), TEXT("A trolley keeps rolling at constant velocity. What does that tell you?"), {TEXT("Resultant force is zero"), TEXT("There must be a forward resultant force"), TEXT("Its weight is zero"), TEXT("Friction is the only force")}, 0, TEXT("Constant velocity means no acceleration."), TEXT("Link constant velocity to resultant force."), TEXT("Newton's first law says an object stays at constant velocity unless acted on by a resultant force."), EBHDiagramType::ForceArrows, TEXT("resultant force = 0"), 0.0f, 0.0f},
		{TEXT("Weight"), TEXT("Where does weight act through for a balanced object diagram?"), {TEXT("Centre of gravity"), TEXT("Largest surface"), TEXT("Direction of motion"), TEXT("Front edge only")}, 0, TEXT("Weight is a gravitational force."), TEXT("Mark the point where the weight arrow starts."), TEXT("Weight acts through the centre of gravity."), EBHDiagramType::ForceArrows, TEXT("W = mg"), 0.0f, 0.0f},
		{TEXT("Hooke's law"), TEXT("On a straight force-extension graph, what does the gradient represent?"), {TEXT("Spring constant"), TEXT("Momentum"), TEXT("Weight"), TEXT("Terminal speed")}, 0, TEXT("F = kx, so F divided by x gives k."), TEXT("Use gradient = rise/run on the graph."), TEXT("The gradient of a force-extension graph is the spring constant k."), EBHDiagramType::SpringGraph, TEXT("F = kx"), 0.0f, 0.0f},
		{TEXT("Terminal velocity"), TEXT("A falling prop reaches terminal velocity. Which forces are balanced?"), {TEXT("Weight and air resistance"), TEXT("Mass and speed"), TEXT("Friction and time"), TEXT("Momentum and distance")}, 0, TEXT("At terminal velocity the resultant force is zero."), TEXT("Draw the downward and upward force arrows equal length."), TEXT("At terminal velocity, weight equals air resistance so acceleration is zero."), EBHDiagramType::ForceArrows, TEXT("resultant force = 0"), 0.0f, 0.0f},
		{TEXT("Momentum"), TEXT("In a closed collision, what remains the same if no external resultant force acts?"), {TEXT("Total momentum"), TEXT("Each object's speed"), TEXT("Each object's kinetic energy"), TEXT("The impact time")}, 0, TEXT("This is conservation of momentum."), TEXT("Compare total before with total after."), TEXT("Total momentum before equals total momentum after in a closed system."), EBHDiagramType::ForceArrows, TEXT("p = mv"), 0.0f, 0.0f}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Displacement-time graphs"), TEXT("True or false: a horizontal displacement-time graph means the object is stationary."), {TEXT("True"), TEXT("False"), TEXT("Only during braking"), TEXT("Only if mass is zero")}, 0, TEXT("Gradient gives velocity."), TEXT("Point to the graph gradient."), TEXT("Horizontal gradient is zero, so velocity is zero."), EBHDiagramType::MotionGraph, TEXT("gradient = velocity"), 0.0f, 0.0f},
		{TEXT("Velocity-time graphs"), TEXT("True or false: the area under a velocity-time graph gives distance travelled."), {TEXT("True"), TEXT("False"), TEXT("Only for springs"), TEXT("Only for waves")}, 0, TEXT("Area represents velocity multiplied by time."), TEXT("Shade the area and name its unit."), TEXT("Velocity times time gives distance, so area under the graph is distance."), EBHDiagramType::VelocityGraph, TEXT("distance = area under v-t graph"), 0.0f, 0.0f},
		{TEXT("Mass and weight"), TEXT("True or false: mass and weight are the same quantity."), {TEXT("False"), TEXT("True"), TEXT("Only on Earth"), TEXT("Only in free fall")}, 0, TEXT("Mass is kg; weight is N."), TEXT("State both units before revoting."), TEXT("Mass is amount of matter in kg; weight is gravitational force in N."), EBHDiagramType::ForceArrows, TEXT("W = mg"), 0.0f, 0.0f},
		{TEXT("Stopping distance"), TEXT("True or false: tiredness can increase thinking distance."), {TEXT("True"), TEXT("False"), TEXT("Only braking distance"), TEXT("Only vehicle mass")}, 0, TEXT("Thinking distance depends on reaction time."), TEXT("Separate thinking distance from braking distance."), TEXT("Tiredness slows reaction time, increasing thinking distance."), EBHDiagramType::MotionGraph, TEXT("stopping = thinking + braking"), 0.0f, 0.0f},
		{TEXT("Newton's third law"), TEXT("True or false: Newton's third-law forces act on the same object."), {TEXT("False"), TEXT("True"), TEXT("Only for gravity"), TEXT("Only in collisions")}, 0, TEXT("Action-reaction pairs act on different objects."), TEXT("Name both objects in the force pair."), TEXT("Equal and opposite forces act on different objects, not the same one."), EBHDiagramType::ForceArrows, TEXT("action = reaction on different objects"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Average speed"), TEXT("A student covers 120 m in 30 s. What is the average speed?"), {TEXT("4 m/s"), TEXT("0.25 m/s"), TEXT("90 m/s"), TEXT("150 m/s")}, 0, TEXT("Use total distance divided by total time."), TEXT("Write speed = distance / time and substitute."), TEXT("120 / 30 = 4 m/s."), EBHDiagramType::MotionGraph, TEXT("average speed = distance / time"), 4.0f, 0.1f},
		{TEXT("Acceleration"), TEXT("A trolley speeds up from 4 m/s to 20 m/s in 8 s. Find acceleration."), {TEXT("2 m/s^2"), TEXT("3 m/s^2"), TEXT("4 m/s^2"), TEXT("16 m/s^2")}, 0, TEXT("Subtract initial velocity from final velocity."), TEXT("Show (20 - 4) / 8."), TEXT("a = (20 - 4) / 8 = 2 m/s^2."), EBHDiagramType::VelocityGraph, TEXT("a = (v - u) / t"), 2.0f, 0.1f},
		{TEXT("Newton's second law"), TEXT("A 3 kg book trolley accelerates at 2 m/s^2. What force acts?"), {TEXT("6 N"), TEXT("1.5 N"), TEXT("5 N"), TEXT("9 N")}, 0, TEXT("Use F = ma."), TEXT("Multiply mass by acceleration."), TEXT("F = 3 x 2 = 6 N."), EBHDiagramType::ForceArrows, TEXT("F = ma"), 6.0f, 0.1f},
		{TEXT("Weight"), TEXT("A 6 kg backpack is on Earth. Use g = 10 N/kg. What is its weight?"), {TEXT("60 N"), TEXT("0.6 N"), TEXT("16 N"), TEXT("6 N")}, 0, TEXT("Weight equals mass times gravitational field strength."), TEXT("Substitute into W = mg."), TEXT("W = 6 x 10 = 60 N."), EBHDiagramType::ForceArrows, TEXT("W = mg"), 60.0f, 0.1f},
		{TEXT("Motion equation"), TEXT("A cart starts from rest and accelerates at 2 m/s^2 for 9 m. What is v^2?"), {TEXT("36 m^2/s^2"), TEXT("18 m^2/s^2"), TEXT("11 m^2/s^2"), TEXT("4.5 m^2/s^2")}, 0, TEXT("Use v^2 = u^2 + 2as and u = 0."), TEXT("Show 0 + 2 x 2 x 9."), TEXT("v^2 = 0 + 2 x 2 x 9 = 36."), EBHDiagramType::VelocityGraph, TEXT("v^2 = u^2 + 2as"), 36.0f, 0.1f},
		{TEXT("Hooke's law"), TEXT("A spring has k = 40 N/m and extension 0.20 m. What force is needed?"), {TEXT("8 N"), TEXT("40.2 N"), TEXT("200 N"), TEXT("0.005 N")}, 0, TEXT("Use F = kx."), TEXT("Multiply 40 by 0.20."), TEXT("F = 40 x 0.20 = 8 N."), EBHDiagramType::SpringGraph, TEXT("F = kx"), 8.0f, 0.1f},
		{TEXT("Moments"), TEXT("A 12 N force acts 0.50 m from a pivot. What is the moment?"), {TEXT("6 Nm"), TEXT("24 Nm"), TEXT("12.5 Nm"), TEXT("0.042 Nm")}, 0, TEXT("Moment equals force times perpendicular distance."), TEXT("Show force x distance from pivot."), TEXT("moment = 12 x 0.50 = 6 Nm."), EBHDiagramType::MomentBeam, TEXT("moment = Fd"), 6.0f, 0.1f}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Motion investigation"), TEXT("A trolley's displacement-time graph is curved and gets steeper each second. Which IGCSE conclusion is best?"), {TEXT("The trolley is accelerating because velocity is increasing"), TEXT("The trolley is stationary because the line is not straight"), TEXT("The trolley has constant velocity because displacement increases"), TEXT("The graph shows force, not motion")}, 0, TEXT("Gradient on a displacement-time graph gives velocity."), TEXT("Explain how the changing gradient proves the velocity changes."), TEXT("A steeper gradient means greater velocity; if the gradient increases, the trolley is accelerating."), EBHDiagramType::MotionGraph, TEXT("exam skill: interpret changing graph gradient"), 0.0f, 0.0f},
		{TEXT("Resultant force"), TEXT("A box moves at constant speed along the floor while being pushed. Which answer would earn the explanation mark?"), {TEXT("Push force equals friction, so resultant force is zero"), TEXT("The push force is larger because the box is moving"), TEXT("There is no friction because speed is constant"), TEXT("Weight is larger than normal contact force")}, 0, TEXT("Constant speed means no acceleration."), TEXT("Use Newton's first law and name the balanced horizontal forces."), TEXT("Constant velocity means zero resultant force; the forward push is balanced by friction."), EBHDiagramType::ForceArrows, TEXT("exam skill: link constant velocity to balanced forces"), 0.0f, 0.0f},
		{TEXT("Free-fall reasoning"), TEXT("A student says a heavier ball falls faster because it has more weight. What is the best correction for an IGCSE answer?"), {TEXT("Both have the same acceleration if air resistance is negligible"), TEXT("The heavier ball has no air resistance"), TEXT("The lighter ball has no weight"), TEXT("Mass is the same as speed")}, 0, TEXT("Separate weight from acceleration due to gravity."), TEXT("Mention negligible air resistance and the same gravitational acceleration."), TEXT("With negligible air resistance, objects near Earth accelerate at the same rate even if their weights differ."), EBHDiagramType::ForceArrows, TEXT("exam skill: challenge a misconception"), 0.0f, 0.0f},
		{TEXT("Hooke's law practical"), TEXT("In a spring experiment, the last two points stop lying on the straight line. What should the student write?"), {TEXT("The limit of proportionality has been exceeded"), TEXT("The spring constant has become zero from the start"), TEXT("The extension readings must be ignored because all graphs curve"), TEXT("Mass and weight are the same quantity")}, 0, TEXT("Hooke's law only applies in the straight-line region."), TEXT("Name the limit and describe what changed on the graph."), TEXT("Once the force-extension graph curves, extension is no longer directly proportional to force; the limit of proportionality has been exceeded."), EBHDiagramType::SpringGraph, TEXT("exam skill: evaluate practical graph evidence"), 0.0f, 0.0f},
		{TEXT("Momentum safety"), TEXT("Two identical crash trolleys stop from the same speed. One has a soft bumper that increases collision time. Why is the force smaller?"), {TEXT("Same momentum change occurs over a longer time"), TEXT("The trolley has no momentum before impact"), TEXT("Increasing time increases acceleration"), TEXT("The bumper removes the trolley's mass")}, 0, TEXT("Use impulse: force depends on change in momentum per time."), TEXT("State that momentum change is similar but time is larger."), TEXT("For the same change in momentum, increasing the impact time reduces the average force."), EBHDiagramType::ForceArrows, TEXT("exam skill: explain safety using momentum change"), 0.0f, 0.0f},
		{TEXT("Moments practical"), TEXT("A metre rule balances when clockwise and anticlockwise moments are equal. Which improvement gives a more reliable result?"), {TEXT("Read distances from the pivot to the line of action of each force"), TEXT("Measure from the table edge instead of the pivot"), TEXT("Ignore the rule's own weight in every setup"), TEXT("Use masses but never convert them to weights")}, 0, TEXT("Moments use perpendicular distance from the pivot."), TEXT("Name the line of action and the pivot."), TEXT("Reliable moment calculations need the perpendicular distance from the pivot to each force's line of action."), EBHDiagramType::MomentBeam, TEXT("exam skill: use perpendicular distance correctly"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Displacement-time graph"), TEXT("On the projected displacement-time graph, what does the gradient show?"), {TEXT("Velocity"), TEXT("Weight"), TEXT("Energy"), TEXT("Mass")}, 0, TEXT("Gradient is change in displacement over time."), TEXT("Draw a tangent or rise/run triangle."), TEXT("The gradient of a displacement-time graph is velocity."), EBHDiagramType::MotionGraph, TEXT("gradient = velocity"), 0.0f, 0.0f},
		{TEXT("Velocity-time graph"), TEXT("A velocity-time graph has a horizontal section at 5 m/s for 4 s. What distance is that section?"), {TEXT("20 m"), TEXT("9 m"), TEXT("1.25 m"), TEXT("5 m")}, 0, TEXT("Area under the graph is distance."), TEXT("Use rectangle area: velocity x time."), TEXT("5 x 4 = 20 m."), EBHDiagramType::VelocityGraph, TEXT("distance = area under v-t graph"), 20.0f, 0.1f},
		{TEXT("Force-extension graph"), TEXT("A straight force-extension graph rises 10 N over 0.25 m. What is k?"), {TEXT("40 N/m"), TEXT("2.5 N/m"), TEXT("10.25 N/m"), TEXT("0.025 N/m")}, 0, TEXT("Spring constant is gradient."), TEXT("Divide force by extension."), TEXT("k = 10 / 0.25 = 40 N/m."), EBHDiagramType::SpringGraph, TEXT("k = F / x"), 40.0f, 0.1f},
		{TEXT("Terminal velocity graph"), TEXT("On a speed-time graph for a falling object, what does a flat final section mean?"), {TEXT("Terminal velocity"), TEXT("Increasing acceleration"), TEXT("Negative mass"), TEXT("No air resistance")}, 0, TEXT("A flat speed-time graph means constant speed."), TEXT("Link constant speed to balanced forces."), TEXT("The object has reached terminal velocity: resultant force and acceleration are zero."), EBHDiagramType::VelocityGraph, TEXT("resultant force = 0"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Vectors and scalars"), TEXT("Choose the correct matching set for the sorting lockers."), {TEXT("Vector: force; Scalar: time"), TEXT("Vector: energy; Scalar: velocity"), TEXT("Vector: distance; Scalar: acceleration"), TEXT("Vector: speed; Scalar: force")}, 0, TEXT("Vectors have direction; scalars do not."), TEXT("Sort one quantity at a time."), TEXT("Force is a vector; time is a scalar."), EBHDiagramType::ForceArrows, TEXT("vector = magnitude + direction"), 0.0f, 0.0f},
		{TEXT("Stopping distances"), TEXT("Match the factor to the distance it mainly affects."), {TEXT("Tiredness -> thinking; icy road -> braking"), TEXT("Tiredness -> braking; icy road -> thinking"), TEXT("Mass -> thinking only; distraction -> braking only"), TEXT("Reaction time -> braking only; tyres -> thinking only")}, 0, TEXT("Reaction affects thinking; road grip affects braking."), TEXT("Split the journey into before braking and during braking."), TEXT("Tiredness worsens reaction time, while icy roads increase braking distance."), EBHDiagramType::MotionGraph, TEXT("stopping = thinking + braking"), 0.0f, 0.0f},
		{TEXT("Springs"), TEXT("Match each spring graph label correctly."), {TEXT("Straight section -> Hooke's law; bend -> beyond limit"), TEXT("Straight section -> fracture; bend -> balanced force"), TEXT("Gradient -> extension; x-axis -> spring constant"), TEXT("Origin -> terminal velocity; bend -> velocity")}, 0, TEXT("Hooke's law gives a straight line."), TEXT("Look for proportional force and extension."), TEXT("A straight force-extension graph obeys Hooke's law until the limit of proportionality."), EBHDiagramType::SpringGraph, TEXT("F = kx"), 0.0f, 0.0f},
		{TEXT("Newton's laws"), TEXT("Match the law to the example."), {TEXT("First: constant velocity; Second: F = ma"), TEXT("First: action-reaction; Second: no resultant force"), TEXT("Third: F = ma; Second: balanced forces"), TEXT("First: spring extension; Third: current split")}, 0, TEXT("First law is about no resultant force; second law is F = ma."), TEXT("Say each law in one sentence before choosing."), TEXT("Newton's first law links zero resultant force with constant velocity; the second law is F = ma."), EBHDiagramType::ForceArrows, TEXT("F = ma"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Terminal velocity"), TEXT("Order the falling object story."), {TEXT("Weight acts -> speed rises -> air resistance rises -> forces balance"), TEXT("Forces balance -> speed rises -> air resistance disappears -> weight acts"), TEXT("Air resistance starts largest -> speed zero -> acceleration rises -> balance breaks"), TEXT("Weight disappears -> speed falls -> air resistance rises -> terminal velocity")}, 0, TEXT("Air resistance grows as speed grows."), TEXT("Start with weight before much air resistance."), TEXT("At first weight is greater; speed and air resistance increase; finally forces balance."), EBHDiagramType::ForceArrows, TEXT("resultant force decreases to zero"), 0.0f, 0.0f},
		{TEXT("Stopping distance"), TEXT("Order the stopping distance chain."), {TEXT("See hazard -> react -> brakes act -> vehicle stops"), TEXT("Brakes act -> see hazard -> react -> vehicle stops"), TEXT("React -> vehicle stops -> see hazard -> brakes act"), TEXT("Vehicle stops -> react -> brakes act -> see hazard")}, 0, TEXT("Thinking happens before braking."), TEXT("Put reaction before brake force."), TEXT("Stopping distance is thinking distance followed by braking distance."), EBHDiagramType::MotionGraph, TEXT("stopping = thinking + braking"), 0.0f, 0.0f},
		{TEXT("Acceleration calculation"), TEXT("Order the method to find acceleration from u, v, and t."), {TEXT("Find v - u -> divide by t -> add m/s^2"), TEXT("Divide by t -> find v - u -> add kg"), TEXT("Multiply u by v -> divide by force -> add N"), TEXT("Find distance -> divide by mass -> add m/s")}, 0, TEXT("Acceleration is change in velocity per time."), TEXT("Calculate change in velocity first."), TEXT("Use a = (v - u) / t, with units m/s^2."), EBHDiagramType::VelocityGraph, TEXT("a = (v - u) / t"), 0.0f, 0.0f},
		{TEXT("Momentum collision"), TEXT("Order a conservation of momentum answer."), {TEXT("Write total before -> write total after -> set equal -> solve"), TEXT("Solve -> ignore direction -> write mass only -> set equal"), TEXT("Write force first -> choose graph -> divide by voltage -> solve"), TEXT("Set speeds to zero -> add weights -> solve")}, 0, TEXT("Use totals before and after."), TEXT("Keep direction signs consistent."), TEXT("In a closed system, total momentum before equals total momentum after."), EBHDiagramType::ForceArrows, TEXT("total p before = total p after"), 0.0f, 0.0f}
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
		{TEXT("Current"), TEXT("What does current measure in the haunted lab circuit?"), {TEXT("Rate of flow of charge"), TEXT("Energy per charge"), TEXT("Charge stored in a wire"), TEXT("Resistance per metre")}, 0, TEXT("Current is charge per second."), TEXT("State the link between charge and time."), TEXT("Current is the rate of flow of charge and is measured in amperes."), EBHDiagramType::Circuit, TEXT("I = Q / t"), 0.0f, 0.0f},
		{TEXT("Potential difference"), TEXT("What is potential difference?"), {TEXT("Work done per unit charge"), TEXT("Charge per unit time"), TEXT("Mass per unit volume"), TEXT("Distance per unit time")}, 0, TEXT("Voltage transfers energy to each coulomb."), TEXT("Use the unit J/C."), TEXT("Potential difference is work done or energy transferred per unit charge."), EBHDiagramType::Circuit, TEXT("V = E / Q"), 0.0f, 0.0f},
		{TEXT("Resistance"), TEXT("Which equation defines resistance?"), {TEXT("R = V / I"), TEXT("R = VI"), TEXT("R = I / V"), TEXT("R = Q / t")}, 0, TEXT("Rearrange V = IR."), TEXT("Resistance is voltage divided by current."), TEXT("R = V / I, measured in ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 0.0f, 0.0f},
		{TEXT("Meters"), TEXT("Where should an ammeter be connected?"), {TEXT("In series"), TEXT("In parallel"), TEXT("Across the battery only"), TEXT("Outside the circuit")}, 0, TEXT("An ammeter measures current through a component."), TEXT("It must be in the path of the charge."), TEXT("An ammeter is connected in series."), EBHDiagramType::Circuit, TEXT("ammeter in series"), 0.0f, 0.0f},
		{TEXT("Parallel circuits"), TEXT("In a parallel lighting circuit, what is the same across each branch?"), {TEXT("Potential difference"), TEXT("Current in every branch"), TEXT("Resistance of every branch"), TEXT("Fuse rating")}, 0, TEXT("Parallel branches share the supply voltage."), TEXT("Compare branch voltage, not total current."), TEXT("Potential difference is the same across each parallel branch."), EBHDiagramType::Circuit, TEXT("parallel p.d. is same"), 0.0f, 0.0f},
		{TEXT("Series circuits"), TEXT("In a series circuit, what happens to total resistance when another resistor is added?"), {TEXT("It increases"), TEXT("It decreases to zero"), TEXT("It always halves"), TEXT("It becomes current")}, 0, TEXT("Series resistances add."), TEXT("Add R1 + R2 + ..."), TEXT("Total resistance in series is the sum of each resistance."), EBHDiagramType::Circuit, TEXT("RT = R1 + R2 + ..."), 0.0f, 0.0f},
		{TEXT("Components"), TEXT("A thermistor warms up near the boiler. What happens to its resistance?"), {TEXT("It decreases"), TEXT("It increases"), TEXT("It becomes infinite"), TEXT("It becomes a fuse")}, 0, TEXT("Thermistor resistance falls with temperature."), TEXT("Remember hot thermistor means easier current path."), TEXT("A thermistor's resistance decreases as temperature increases."), EBHDiagramType::IVGraph, TEXT("thermistor: hot -> lower R"), 0.0f, 0.0f},
		{TEXT("Safety"), TEXT("Which fuse rating is best for a device that normally uses 4 A?"), {TEXT("5 A"), TEXT("3 A"), TEXT("13 A"), TEXT("0 A")}, 0, TEXT("Choose slightly above normal current."), TEXT("Avoid a rating below normal current."), TEXT("A fuse should be rated slightly above normal current, so 5 A is best."), EBHDiagramType::Circuit, TEXT("fuse rating just above normal current"), 5.0f, 0.0f},
		{TEXT("Static electricity"), TEXT("When an insulator gains electrons, what charge does it have?"), {TEXT("Negative"), TEXT("Positive"), TEXT("Neutral forever"), TEXT("Alternating")}, 0, TEXT("Electrons are negative."), TEXT("Track whether electrons are added or removed."), TEXT("Gaining electrons gives an object a negative charge."), EBHDiagramType::StaticCharge, TEXT("electron charge = -1"), 0.0f, 0.0f},
		{TEXT("Earthing"), TEXT("Why does an earth wire help protect a metal-cased appliance?"), {TEXT("It provides a low-resistance fault path"), TEXT("It stores extra voltage"), TEXT("It makes current zero in normal use"), TEXT("It replaces the live wire")}, 0, TEXT("Fault current goes safely to Earth."), TEXT("Explain how the fuse then disconnects."), TEXT("The earth wire provides a low-resistance path so a large fault current blows the fuse."), EBHDiagramType::Circuit, TEXT("earth wire -> large fault current -> fuse blows"), 0.0f, 0.0f}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Current direction"), TEXT("True or false: conventional current is opposite to electron flow."), {TEXT("True"), TEXT("False"), TEXT("Only in batteries"), TEXT("Only in AC")}, 0, TEXT("Conventional current is positive-charge direction."), TEXT("Draw electron flow, then conventional current."), TEXT("Electrons are negative, so electron flow is opposite conventional current."), EBHDiagramType::Circuit, TEXT("conventional current opposite electron flow"), 0.0f, 0.0f},
		{TEXT("Junctions"), TEXT("True or false: current is conserved at a junction."), {TEXT("True"), TEXT("False"), TEXT("Only in series"), TEXT("Only with lamps")}, 0, TEXT("Charge does not disappear at a junction."), TEXT("Add currents entering and leaving."), TEXT("Total current entering a junction equals total current leaving."), EBHDiagramType::Circuit, TEXT("current in = current out"), 0.0f, 0.0f},
		{TEXT("Voltmeters"), TEXT("True or false: a voltmeter is connected in series."), {TEXT("False"), TEXT("True"), TEXT("Only for cells"), TEXT("Only for fuses")}, 0, TEXT("A voltmeter compares energy difference across a component."), TEXT("Place it across the component."), TEXT("A voltmeter is connected in parallel."), EBHDiagramType::Circuit, TEXT("voltmeter in parallel"), 0.0f, 0.0f},
		{TEXT("Power"), TEXT("True or false: P = IV can be used for electrical power."), {TEXT("True"), TEXT("False"), TEXT("Only for sound"), TEXT("Only for springs")}, 0, TEXT("Power is current times potential difference."), TEXT("Check units: A x V = W."), TEXT("Electrical power is P = IV."), EBHDiagramType::Circuit, TEXT("P = IV"), 0.0f, 0.0f},
		{TEXT("Double insulation"), TEXT("True or false: a double-insulated plastic appliance must have an earth wire."), {TEXT("False"), TEXT("True"), TEXT("Only if small"), TEXT("Only if DC")}, 0, TEXT("Double insulation prevents touching live metal."), TEXT("Explain why no exposed metal case needs earthing."), TEXT("Double-insulated appliances do not need an earth wire."), EBHDiagramType::Circuit, TEXT("double insulation -> no earth wire needed"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Current"), TEXT("18 C of charge flows in 6 s through the door lock. What is current?"), {TEXT("3 A"), TEXT("12 A"), TEXT("108 A"), TEXT("0.33 A")}, 0, TEXT("Use I = Q / t."), TEXT("Divide charge by time."), TEXT("I = 18 / 6 = 3 A."), EBHDiagramType::Circuit, TEXT("I = Q / t"), 3.0f, 0.1f},
		{TEXT("Potential difference"), TEXT("A cell transfers 24 J to 6 C. What is the potential difference?"), {TEXT("4 V"), TEXT("18 V"), TEXT("144 V"), TEXT("0.25 V")}, 0, TEXT("Use V = E / Q."), TEXT("Divide energy by charge."), TEXT("V = 24 / 6 = 4 V."), EBHDiagramType::Circuit, TEXT("V = E / Q"), 4.0f, 0.1f},
		{TEXT("Resistance"), TEXT("A lamp has 12 V across it and current 3 A. What is its resistance?"), {TEXT("4 ohms"), TEXT("36 ohms"), TEXT("15 ohms"), TEXT("0.25 ohms")}, 0, TEXT("Use R = V / I."), TEXT("Divide voltage by current."), TEXT("R = 12 / 3 = 4 ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 4.0f, 0.1f},
		{TEXT("Power"), TEXT("A heater uses 2 A from a 230 V supply. What is its power?"), {TEXT("460 W"), TEXT("115 W"), TEXT("232 W"), TEXT("228 W")}, 0, TEXT("Use P = IV."), TEXT("Multiply current by voltage."), TEXT("P = 2 x 230 = 460 W."), EBHDiagramType::Circuit, TEXT("P = IV"), 460.0f, 1.0f},
		{TEXT("Series resistance"), TEXT("Three resistors of 2 ohms, 5 ohms, and 8 ohms are in series. What is total resistance?"), {TEXT("15 ohms"), TEXT("10 ohms"), TEXT("80 ohms"), TEXT("1.5 ohms")}, 0, TEXT("Series resistances add directly."), TEXT("Add all three values."), TEXT("RT = 2 + 5 + 8 = 15 ohms."), EBHDiagramType::Circuit, TEXT("RT = R1 + R2 + R3"), 15.0f, 0.1f},
		{TEXT("Energy transferred"), TEXT("A 5 A device at 12 V runs for 10 s. How much energy is transferred?"), {TEXT("600 J"), TEXT("27 J"), TEXT("60 J"), TEXT("120 J")}, 0, TEXT("Use E = IVt."), TEXT("Multiply current, voltage, and time."), TEXT("E = 5 x 12 x 10 = 600 J."), EBHDiagramType::Circuit, TEXT("E = IVt"), 600.0f, 1.0f},
		{TEXT("Power from resistance"), TEXT("A 3 A current passes through a 4 ohm resistor. Use P = I^2R. What is power?"), {TEXT("36 W"), TEXT("12 W"), TEXT("7 W"), TEXT("48 W")}, 0, TEXT("Square the current first."), TEXT("Show 3^2 x 4."), TEXT("P = 3^2 x 4 = 36 W."), EBHDiagramType::IVGraph, TEXT("P = I^2R"), 36.0f, 0.1f}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Circuit practical"), TEXT("A student measures the I-V graph of a resistor. Which method is most suitable?"), {TEXT("Vary the p.d., record V and I pairs, and keep temperature as constant as possible"), TEXT("Use one reading only because resistance never changes"), TEXT("Connect the voltmeter in series and the ammeter in parallel"), TEXT("Heat the resistor deliberately for every reading")}, 0, TEXT("An I-V investigation needs several paired readings."), TEXT("Mention meter placement and controlling temperature."), TEXT("A good I-V experiment varies p.d., measures current and voltage correctly, and controls temperature so resistance is meaningful."), EBHDiagramType::IVGraph, TEXT("exam skill: plan an I-V investigation"), 0.0f, 0.0f},
		{TEXT("Junction reasoning"), TEXT("At a junction, 0.60 A enters and two branches carry 0.20 A and 0.40 A. What conclusion is best?"), {TEXT("The data supports conservation of charge"), TEXT("Charge is used up in the first branch"), TEXT("The 0.40 A branch must have the larger p.d. because it has more current"), TEXT("The circuit cannot be parallel")}, 0, TEXT("Current entering a junction equals current leaving."), TEXT("Add the branch currents and compare with the input."), TEXT("0.20 A + 0.40 A = 0.60 A, so the readings support conservation of charge at the junction."), EBHDiagramType::Circuit, TEXT("exam skill: use junction evidence"), 0.0f, 0.0f},
		{TEXT("Component behaviour"), TEXT("A filament lamp's I-V graph curves and becomes less steep at higher voltage. What is the IGCSE explanation?"), {TEXT("The filament heats up, increasing its resistance"), TEXT("The lamp becomes an ohmic conductor at high temperature"), TEXT("The current has stopped flowing"), TEXT("The voltmeter is connected in series")}, 0, TEXT("A hot filament has more resistance."), TEXT("Link temperature rise to resistance rise."), TEXT("As the filament gets hotter, metal ion vibrations increase, causing greater resistance and a curved I-V graph."), EBHDiagramType::IVGraph, TEXT("exam skill: explain non-ohmic behaviour"), 0.0f, 0.0f},
		{TEXT("Mains safety"), TEXT("A metal-cased heater has a live wire touching the case. Which explanation earns full credit?"), {TEXT("The earth wire gives a low-resistance path, causing a large current that blows the fuse"), TEXT("The earth wire stores the charge until the switch is opened"), TEXT("The fuse stops the case becoming live before any current flows"), TEXT("The neutral wire becomes an insulator")}, 0, TEXT("Explain both the earth wire and the fuse."), TEXT("Use low resistance, large current, and disconnection."), TEXT("The earth wire provides a low-resistance fault path; the large current melts the fuse and disconnects the supply."), EBHDiagramType::Circuit, TEXT("exam skill: explain fault protection"), 0.0f, 0.0f},
		{TEXT("Static hazards"), TEXT("A tanker is earthed before fuel is transferred. Why?"), {TEXT("To prevent charge build-up causing a spark near flammable vapour"), TEXT("To increase the fuel's potential difference"), TEXT("To make the fuel flow as conventional current"), TEXT("To make the tanker positively charged")}, 0, TEXT("Moving fuel can cause static charge."), TEXT("Link charge build-up, spark, and ignition risk."), TEXT("Earthing lets charge flow away, reducing the risk of a spark igniting fuel vapour."), EBHDiagramType::StaticCharge, TEXT("exam skill: apply electrostatic safety"), 0.0f, 0.0f},
		{TEXT("Domestic electricity"), TEXT("A 900 W kettle is used on a 230 V supply. A student chooses a 3 A fuse. What is the best evaluation?"), {TEXT("Unsuitable, because the normal current is about 3.9 A so the fuse may melt in normal use"), TEXT("Suitable, because lower fuse ratings always make appliances safer"), TEXT("Unsuitable, because the fuse rating must equal the voltage"), TEXT("Suitable, because kettles do not use current")}, 0, TEXT("Estimate current using power divided by voltage."), TEXT("Compare normal current with the fuse rating."), TEXT("I = P / V = 900 / 230, about 3.9 A; a 3 A fuse is below normal current, so it is unsuitable."), EBHDiagramType::Circuit, TEXT("exam skill: evaluate a fuse choice"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Circuit diagrams"), TEXT("In the projected circuit, which meter placement is correct?"), {TEXT("Ammeter series, voltmeter parallel"), TEXT("Ammeter parallel, voltmeter series"), TEXT("Both in series"), TEXT("Both outside the circuit")}, 0, TEXT("Current through, voltage across."), TEXT("Trace the current path and branch across component."), TEXT("Ammeters go in series; voltmeters go in parallel."), EBHDiagramType::Circuit, TEXT("A series, V parallel"), 0.0f, 0.0f},
		{TEXT("IV graphs"), TEXT("Which IV graph shape shows an ohmic conductor at constant temperature?"), {TEXT("Straight line through origin"), TEXT("Curve flattening at high current"), TEXT("Vertical line only"), TEXT("Random spikes")}, 0, TEXT("Ohmic means current proportional to voltage."), TEXT("Proportional relationships are straight through origin."), TEXT("An ohmic conductor has a straight-line IV graph through the origin."), EBHDiagramType::IVGraph, TEXT("I proportional to V"), 0.0f, 0.0f},
		{TEXT("Filament lamps"), TEXT("On the lamp IV graph, why does the curve get shallower at high current?"), {TEXT("Resistance increases as temperature increases"), TEXT("Resistance becomes zero"), TEXT("Voltage disappears"), TEXT("Charge stops existing")}, 0, TEXT("A hot filament has higher resistance."), TEXT("Link heating to resistance."), TEXT("The filament heats up, so resistance increases and the IV graph curves."), EBHDiagramType::IVGraph, TEXT("hot filament -> higher R"), 0.0f, 0.0f},
		{TEXT("Parallel current"), TEXT("A junction shows 6 A entering, with 2 A in one branch. What current is in the other branch?"), {TEXT("4 A"), TEXT("8 A"), TEXT("12 A"), TEXT("3 A")}, 0, TEXT("Current entering equals current leaving."), TEXT("Subtract the known branch current from total."), TEXT("6 A = 2 A + 4 A, so the other branch is 4 A."), EBHDiagramType::Circuit, TEXT("current in = current out"), 4.0f, 0.1f}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Series and parallel"), TEXT("Choose the correct matching pair."), {TEXT("Series: same current; Parallel: same p.d."), TEXT("Series: same p.d.; Parallel: same current"), TEXT("Series: broken branch still works; Parallel: one loop only"), TEXT("Series: lower total resistance; Parallel: always one component")}, 0, TEXT("Series has one path; parallel has branches."), TEXT("Trace the paths in the circuit."), TEXT("Series circuits have the same current; parallel branches have the same p.d."), EBHDiagramType::Circuit, TEXT("series I same, parallel V same"), 0.0f, 0.0f},
		{TEXT("Hazards"), TEXT("Match each mains hazard to the danger."), {TEXT("Damaged insulation -> shock; thin cable high current -> fire"), TEXT("Damaged insulation -> lower bill; thin cable -> cooling"), TEXT("Damp room -> perfect insulation; live wire -> no risk"), TEXT("Fuse -> shock source; earth -> live supply")}, 0, TEXT("Expose live metal or overheating can harm people."), TEXT("Connect each hazard to shock or fire."), TEXT("Damaged insulation can cause shock; overheating cables can cause fire."), EBHDiagramType::Circuit, TEXT("fault hazards: shock/fire"), 0.0f, 0.0f},
		{TEXT("Components"), TEXT("Match the component behaviour."), {TEXT("Thermistor hot -> lower R; LDR bright -> lower R"), TEXT("Thermistor hot -> higher R; LDR bright -> higher R"), TEXT("Fuse hot -> lower rating; lamp cool -> no resistance"), TEXT("Cell bright -> lower p.d.; switch open -> more current")}, 0, TEXT("Both thermistor and LDR can lower resistance when stimulated."), TEXT("Remember heat for thermistor, light for LDR."), TEXT("Thermistor resistance decreases as temperature increases; LDR resistance decreases as light intensity increases."), EBHDiagramType::IVGraph, TEXT("thermistor hot lower R, LDR bright lower R"), 0.0f, 0.0f},
		{TEXT("Static uses"), TEXT("Match the static effect to its use."), {TEXT("Charged ink droplets -> inkjet printer; toner attraction -> photocopier"), TEXT("Charged ink -> fuse; toner -> voltmeter"), TEXT("Balloon wall -> circuit breaker; lightning -> LDR"), TEXT("Electron loss -> AC; proton flow -> DC")}, 0, TEXT("Printers and copiers both use charged particles."), TEXT("Name which charged material moves."), TEXT("Inkjet printers deflect charged droplets; photocopiers attract toner to charged image areas."), EBHDiagramType::StaticCharge, TEXT("static charge attracts/deflects"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Earthing"), TEXT("Order the fault protection sequence."), {TEXT("Live touches case -> earth current large -> fuse melts -> supply disconnects"), TEXT("Fuse melts -> live touches case -> current stops -> earth wire heats first"), TEXT("Earth wire removed -> fuse melts -> voltage rises -> current falls"), TEXT("Case becomes plastic -> live wire grows -> fuse rating rises -> shock")}, 0, TEXT("A fault current must happen before the fuse melts."), TEXT("Start with live wire touching the metal case."), TEXT("The earth wire carries a large current, causing the fuse to melt and disconnect the supply."), EBHDiagramType::Circuit, TEXT("fault -> earth current -> fuse blows"), 0.0f, 0.0f},
		{TEXT("Static charging"), TEXT("Order charging an insulator by rubbing."), {TEXT("Rub surfaces -> electrons transfer -> one gains electrons -> it becomes negative"), TEXT("Protons move -> object loses neutrons -> it becomes AC -> sparks stop"), TEXT("Neutrons transfer -> both become positive -> electrons vanish -> no charge"), TEXT("Current alternates -> fuse melts -> object becomes neutral -> rubbing starts")}, 0, TEXT("Static charging usually moves electrons."), TEXT("Track the object gaining electrons."), TEXT("Rubbing transfers electrons; the object gaining electrons becomes negative."), EBHDiagramType::StaticCharge, TEXT("gain electrons -> negative"), 0.0f, 0.0f},
		{TEXT("Circuit calculation"), TEXT("Order a resistance calculation from V and I."), {TEXT("Write R = V / I -> substitute V and I -> divide -> add ohms"), TEXT("Write I = Q / t -> multiply by voltage -> add N -> divide"), TEXT("Add V and I -> square answer -> add amps -> draw graph"), TEXT("Choose fuse -> subtract charge -> add volts -> divide by time")}, 0, TEXT("Resistance is voltage divided by current."), TEXT("Do not multiply V and I for resistance."), TEXT("Use R = V / I and give the unit ohms."), EBHDiagramType::IVGraph, TEXT("R = V / I"), 0.0f, 0.0f},
		{TEXT("Circuit breaker"), TEXT("Order how a circuit breaker protects the classroom supply."), {TEXT("Current too high -> electromagnet pulls switch -> circuit opens -> breaker can reset"), TEXT("Switch opens -> current too high -> electromagnet disappears -> breaker melts"), TEXT("Fuse warms -> plastic case charges -> circuit opens -> voltage doubles"), TEXT("Earth wire glows -> current lowers -> breaker stores charge -> switch closes")}, 0, TEXT("A circuit breaker uses an electromagnet."), TEXT("Start with excessive current."), TEXT("High current strengthens the electromagnet, opening the switch; the breaker can be reset."), EBHDiagramType::Circuit, TEXT("high current -> electromagnet switch opens"), 0.0f, 0.0f}
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
		{TEXT("Wave transfer"), TEXT("What do waves transfer across the dark hall?"), {TEXT("Energy and information without matter transfer"), TEXT("Only matter"), TEXT("Only mass"), TEXT("Only charge")}, 0, TEXT("Particles oscillate about fixed positions."), TEXT("State what moves and what does not."), TEXT("Waves transfer energy and information without transferring matter overall."), EBHDiagramType::Wave, TEXT("wave transfers energy"), 0.0f, 0.0f},
		{TEXT("Transverse waves"), TEXT("Which description fits a transverse wave?"), {TEXT("Vibrations at right angles to travel"), TEXT("Vibrations parallel to travel"), TEXT("No oscillations"), TEXT("Only compressions")}, 0, TEXT("Light is transverse."), TEXT("Compare vibration direction with wave direction."), TEXT("In transverse waves, vibrations are perpendicular to the direction of travel."), EBHDiagramType::Wave, TEXT("transverse: perpendicular vibrations"), 0.0f, 0.0f},
		{TEXT("Longitudinal waves"), TEXT("Which feature belongs to longitudinal waves?"), {TEXT("Compressions and rarefactions"), TEXT("Crests and troughs only"), TEXT("No frequency"), TEXT("No wavelength")}, 0, TEXT("Sound is longitudinal."), TEXT("Look for squashed and spread-out regions."), TEXT("Longitudinal waves have compressions and rarefactions."), EBHDiagramType::Wave, TEXT("longitudinal: parallel vibrations"), 0.0f, 0.0f},
		{TEXT("Frequency"), TEXT("What does frequency mean?"), {TEXT("Number of waves passing per second"), TEXT("Maximum displacement"), TEXT("Distance between mirrors"), TEXT("Energy wasted")}, 0, TEXT("Frequency is measured in hertz."), TEXT("Count complete waves each second."), TEXT("Frequency is the number of waves passing a point per second."), EBHDiagramType::Wave, TEXT("f = 1 / T"), 0.0f, 0.0f},
		{TEXT("EM spectrum"), TEXT("Which EM wave has the longest wavelength in the standard order?"), {TEXT("Radio"), TEXT("Gamma"), TEXT("X-rays"), TEXT("Ultraviolet")}, 0, TEXT("The order starts with radio."), TEXT("Say radio, microwave, infrared..."), TEXT("Radio waves have the longest wavelength and lowest frequency in the EM spectrum list."), EBHDiagramType::EMSpectrum, TEXT("radio -> microwave -> infrared -> visible -> ultraviolet -> X-rays -> gamma"), 0.0f, 0.0f},
		{TEXT("Doppler effect"), TEXT("If a siren source moves toward you, what happens to observed pitch?"), {TEXT("It increases"), TEXT("It decreases"), TEXT("It becomes zero"), TEXT("It changes to DC")}, 0, TEXT("Wavefronts bunch together in front."), TEXT("Shorter wavelength means higher frequency."), TEXT("When a source approaches, observed frequency and pitch increase."), EBHDiagramType::Wave, TEXT("approaching source -> higher f"), 0.0f, 0.0f},
		{TEXT("Reflection"), TEXT("A ray hits a mirror at 35 degrees to the normal. What is the angle of reflection?"), {TEXT("35 degrees"), TEXT("55 degrees"), TEXT("70 degrees"), TEXT("0 degrees")}, 0, TEXT("Angle of incidence equals angle of reflection."), TEXT("Angles are measured from the normal."), TEXT("The law of reflection gives 35 degrees."), EBHDiagramType::RayDiagram, TEXT("i = r"), 35.0f, 0.1f},
		{TEXT("Refraction"), TEXT("Light enters a more optically dense medium at an angle. Which way does it bend?"), {TEXT("Towards the normal"), TEXT("Away from the normal"), TEXT("It never bends"), TEXT("Into a circle")}, 0, TEXT("Speed decreases in the denser medium."), TEXT("Draw the normal first."), TEXT("Entering a more optically dense medium makes light bend towards the normal."), EBHDiagramType::RayDiagram, TEXT("denser -> slower -> towards normal"), 0.0f, 0.0f},
		{TEXT("Sound"), TEXT("What wave type is sound in air?"), {TEXT("Longitudinal"), TEXT("Transverse"), TEXT("Electrostatic"), TEXT("Nuclear")}, 0, TEXT("Air particles vibrate parallel to the direction of travel."), TEXT("Look for compressions and rarefactions."), TEXT("Sound in air is a longitudinal wave."), EBHDiagramType::Wave, TEXT("sound is longitudinal"), 0.0f, 0.0f},
		{TEXT("Total internal reflection"), TEXT("Which condition is required for total internal reflection?"), {TEXT("More dense to less dense and i greater than c"), TEXT("Less dense to more dense and i less than c"), TEXT("Any angle in any medium"), TEXT("Only radio waves in air")}, 0, TEXT("TIR needs the critical angle condition."), TEXT("Name both medium direction and angle condition."), TEXT("TIR occurs from more to less optically dense medium when incidence angle exceeds critical angle."), EBHDiagramType::RayDiagram, TEXT("TIR: i > c, dense to less dense"), 0.0f, 0.0f}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Reflection"), TEXT("True or false: reflection changes wave frequency."), {TEXT("False"), TEXT("True"), TEXT("Only for mirrors"), TEXT("Only for sound")}, 0, TEXT("Reflection changes direction, not frequency."), TEXT("List what stays unchanged."), TEXT("Frequency, wavelength, and speed are unchanged during reflection in the same medium."), EBHDiagramType::RayDiagram, TEXT("reflection: i = r"), 0.0f, 0.0f},
		{TEXT("Refraction"), TEXT("True or false: frequency stays the same during refraction."), {TEXT("True"), TEXT("False"), TEXT("Only in glass"), TEXT("Only in air")}, 0, TEXT("Speed and wavelength change, frequency stays fixed."), TEXT("Separate source frequency from medium speed."), TEXT("During refraction, frequency stays the same while speed and wavelength change."), EBHDiagramType::RayDiagram, TEXT("frequency unchanged in refraction"), 0.0f, 0.0f},
		{TEXT("Light"), TEXT("True or false: light is transverse."), {TEXT("True"), TEXT("False"), TEXT("Only red light"), TEXT("Only sound")}, 0, TEXT("Light is an electromagnetic wave."), TEXT("Recall EM waves are transverse."), TEXT("Light is a transverse wave."), EBHDiagramType::Wave, TEXT("light is transverse"), 0.0f, 0.0f},
		{TEXT("EM hazards"), TEXT("True or false: X-rays and gamma rays are ionising and can increase cancer risk."), {TEXT("True"), TEXT("False"), TEXT("Only radio waves ionise"), TEXT("Only visible light ionises")}, 0, TEXT("High-frequency EM waves are more hazardous."), TEXT("Link ionising radiation to mutations."), TEXT("X-rays and gamma rays are ionising and can cause mutations and cancer risk."), EBHDiagramType::EMSpectrum, TEXT("X-rays/gamma: ionising"), 0.0f, 0.0f},
		{TEXT("Critical angle"), TEXT("True or false: n = 1 / sin c for the critical angle from glass to air."), {TEXT("True"), TEXT("False"), TEXT("Only for sound"), TEXT("Only for mirrors")}, 0, TEXT("This is the IGCSE critical angle relationship."), TEXT("Use c as the critical angle."), TEXT("For a material to air, refractive index n = 1 / sin c."), EBHDiagramType::RayDiagram, TEXT("n = 1 / sin c"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Wave speed"), TEXT("A wave has frequency 5 Hz and wavelength 3 m. What is its speed?"), {TEXT("15 m/s"), TEXT("8 m/s"), TEXT("1.67 m/s"), TEXT("0.6 m/s")}, 0, TEXT("Use v = f lambda."), TEXT("Multiply frequency by wavelength."), TEXT("v = 5 x 3 = 15 m/s."), EBHDiagramType::Wave, TEXT("v = f lambda"), 15.0f, 0.1f},
		{TEXT("Period"), TEXT("A sound has frequency 4 Hz. What is its period?"), {TEXT("0.25 s"), TEXT("4 s"), TEXT("8 s"), TEXT("16 s")}, 0, TEXT("Use T = 1 / f."), TEXT("Find one divided by frequency."), TEXT("T = 1 / 4 = 0.25 s."), EBHDiagramType::Wave, TEXT("f = 1 / T"), 0.25f, 0.01f},
		{TEXT("Echo speed"), TEXT("An echo returns in 0.50 s from a wall 85 m away. What distance did sound travel?"), {TEXT("170 m"), TEXT("85 m"), TEXT("42.5 m"), TEXT("0.5 m")}, 0, TEXT("Echo distance is there and back."), TEXT("Double the one-way distance."), TEXT("Sound travels to the wall and back: 2 x 85 = 170 m."), EBHDiagramType::Wave, TEXT("echo distance = 2 x one-way distance"), 170.0f, 0.1f},
		{TEXT("Reflection angle"), TEXT("A ray's angle of incidence is 42 degrees. What is the angle of reflection?"), {TEXT("42 degrees"), TEXT("48 degrees"), TEXT("84 degrees"), TEXT("21 degrees")}, 0, TEXT("Use i = r."), TEXT("Angles are from the normal."), TEXT("The angle of reflection equals 42 degrees."), EBHDiagramType::RayDiagram, TEXT("i = r"), 42.0f, 0.1f},
		{TEXT("Wave speed"), TEXT("A radio wave has wavelength 2 m and speed 300,000,000 m/s. What is frequency?"), {TEXT("150,000,000 Hz"), TEXT("600,000,000 Hz"), TEXT("299,999,998 Hz"), TEXT("2 Hz")}, 0, TEXT("Rearrange v = f lambda to f = v / lambda."), TEXT("Divide speed by wavelength."), TEXT("f = 300,000,000 / 2 = 150,000,000 Hz."), EBHDiagramType::EMSpectrum, TEXT("f = v / lambda"), 150000000.0f, 1000.0f},
		{TEXT("Snell's law"), TEXT("n1 sin i = n2 sin r. If n1=1.0, sin i=0.6, and n2=1.5, what is sin r?"), {TEXT("0.4"), TEXT("0.9"), TEXT("2.5"), TEXT("0.6")}, 0, TEXT("Divide n1 sin i by n2."), TEXT("Show 1.0 x 0.6 / 1.5."), TEXT("sin r = 0.6 / 1.5 = 0.4."), EBHDiagramType::RayDiagram, TEXT("n1 sin i = n2 sin r"), 0.4f, 0.01f},
		{TEXT("Critical angle"), TEXT("A glass block has critical angle 30 degrees. Use sin 30 = 0.5. What is n?"), {TEXT("2.0"), TEXT("0.5"), TEXT("30"), TEXT("1.5")}, 0, TEXT("Use n = 1 / sin c."), TEXT("Divide one by 0.5."), TEXT("n = 1 / 0.5 = 2.0."), EBHDiagramType::RayDiagram, TEXT("n = 1 / sin c"), 2.0f, 0.01f}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Wave investigation"), TEXT("A ripple tank photo shows 6 wavefront gaps covering 0.30 m. The vibrator frequency is 20 Hz. What is the best method?"), {TEXT("Find wavelength from spacing, then multiply by frequency"), TEXT("Divide frequency by the full 0.30 m without finding wavelength"), TEXT("Use amplitude instead of wavelength"), TEXT("Ignore frequency because the photo already gives speed")}, 0, TEXT("Use distance across several wavelengths to reduce percentage uncertainty."), TEXT("Find one wavelength first, then use wave speed."), TEXT("Measure several wavelengths, divide to get one wavelength, then use v = f lambda."), EBHDiagramType::Wave, TEXT("exam skill: process ripple-tank data"), 0.0f, 0.0f},
		{TEXT("Sound experiment"), TEXT("In an echo experiment, the measured time is for the sound to travel to the wall and back. What mistake must be avoided?"), {TEXT("Using the one-way wall distance instead of the total sound distance"), TEXT("Using seconds instead of minutes"), TEXT("Measuring the wall distance with a ruler"), TEXT("Repeating the timing")}, 0, TEXT("An echo makes a round trip."), TEXT("Double the wall distance before using speed = distance / time."), TEXT("The sound travels to the wall and back, so the total distance is twice the wall distance."), EBHDiagramType::Wave, TEXT("exam skill: avoid echo distance error"), 0.0f, 0.0f},
		{TEXT("Refraction explanation"), TEXT("A ray bends towards the normal when it enters glass from air. Which explanation is most complete?"), {TEXT("It slows down in glass; frequency stays the same, so wavelength decreases"), TEXT("Its frequency increases and wavelength stays the same"), TEXT("Its speed increases because glass is denser"), TEXT("It reflects before entering the glass")}, 0, TEXT("Frequency is fixed by the source."), TEXT("Link denser medium, lower speed, unchanged frequency, and shorter wavelength."), TEXT("In glass the wave speed is lower; frequency is unchanged, so wavelength decreases and the ray bends towards the normal."), EBHDiagramType::RayDiagram, TEXT("exam skill: explain refraction, not just name it"), 0.0f, 0.0f},
		{TEXT("Critical angle reasoning"), TEXT("A ray in glass meets the glass-air boundary at an angle greater than the critical angle. What happens?"), {TEXT("Total internal reflection occurs"), TEXT("It refracts out along the normal"), TEXT("It stops at the boundary"), TEXT("Its frequency becomes zero")}, 0, TEXT("TIR happens from more dense to less dense above the critical angle."), TEXT("State both the medium direction and angle condition."), TEXT("When travelling from glass to air with incidence angle greater than the critical angle, the ray is totally internally reflected."), EBHDiagramType::RayDiagram, TEXT("exam skill: apply the TIR conditions"), 0.0f, 0.0f},
		{TEXT("EM spectrum evaluation"), TEXT("A hospital uses X-rays for imaging but limits exposure time. What is the best IGCSE reason?"), {TEXT("X-rays are ionising, so dose should be kept as low as practical"), TEXT("X-rays are longitudinal sound waves"), TEXT("X-rays have the longest wavelength in the EM spectrum"), TEXT("X-rays cannot pass through soft tissue")}, 0, TEXT("High-frequency EM radiation can ionise cells."), TEXT("Link ionisation to cell damage and dose control."), TEXT("X-rays are ionising and can damage living cells, so exposure is limited while still getting a useful image."), EBHDiagramType::EMSpectrum, TEXT("exam skill: balance use and hazard"), 0.0f, 0.0f},
		{TEXT("Doppler evidence"), TEXT("A moving ambulance passes a student. The pitch is higher as it approaches and lower as it moves away. What causes this?"), {TEXT("Wavefronts are compressed in front and spread out behind"), TEXT("The sound changes from longitudinal to transverse"), TEXT("The ambulance changes the speed of sound in air"), TEXT("The siren emits gamma rays when moving")}, 0, TEXT("Observed frequency changes because wavefront spacing changes."), TEXT("Use shorter wavelength in front and longer wavelength behind."), TEXT("Motion compresses wavefronts ahead of the source, increasing observed frequency; behind it, wavefronts spread out and frequency decreases."), EBHDiagramType::Wave, TEXT("exam skill: explain Doppler observations"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Wave labels"), TEXT("On the wave sketch, what is amplitude?"), {TEXT("Distance from equilibrium to maximum displacement"), TEXT("Distance between two wavefronts only"), TEXT("Number of waves per second"), TEXT("Speed times time")}, 0, TEXT("Amplitude is wave height from the middle line."), TEXT("Do not measure crest to trough."), TEXT("Amplitude is the distance from equilibrium to maximum displacement."), EBHDiagramType::Wave, TEXT("amplitude = max displacement from equilibrium"), 0.0f, 0.0f},
		{TEXT("Oscilloscope"), TEXT("Which oscilloscope trace has the louder sound?"), {TEXT("The trace with greater amplitude"), TEXT("The trace with greater period"), TEXT("The trace with lower frequency"), TEXT("The trace with a flat line")}, 0, TEXT("Loudness is linked to amplitude."), TEXT("Compare wave height."), TEXT("Greater amplitude means louder sound."), EBHDiagramType::Wave, TEXT("greater amplitude -> louder"), 0.0f, 0.0f},
		{TEXT("Pitch"), TEXT("Which trace has higher pitch?"), {TEXT("More cycles per second"), TEXT("Lower amplitude only"), TEXT("Longer period only"), TEXT("No wavefronts")}, 0, TEXT("Pitch is linked to frequency."), TEXT("Count cycles in the same time window."), TEXT("Higher frequency means higher pitch."), EBHDiagramType::Wave, TEXT("higher f -> higher pitch"), 0.0f, 0.0f},
		{TEXT("TIR ray diagram"), TEXT("On the ray diagram, what happens when i is greater than the critical angle?"), {TEXT("Total internal reflection"), TEXT("Refraction along the normal"), TEXT("Absorption only"), TEXT("Frequency becomes zero")}, 0, TEXT("The ray stays inside the denser material."), TEXT("Check the direction is dense to less dense."), TEXT("If i is greater than c, total internal reflection occurs."), EBHDiagramType::RayDiagram, TEXT("i > c -> TIR"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Wave type"), TEXT("Choose the correct matching pair."), {TEXT("Light -> transverse; sound -> longitudinal"), TEXT("Light -> longitudinal; sound -> transverse"), TEXT("Radio -> longitudinal; sound -> transverse"), TEXT("Water only -> EM; gamma -> sound")}, 0, TEXT("All EM waves are transverse; sound is longitudinal."), TEXT("Match examples to vibration direction."), TEXT("Light is transverse; sound is longitudinal."), EBHDiagramType::Wave, TEXT("light transverse, sound longitudinal"), 0.0f, 0.0f},
		{TEXT("EM uses"), TEXT("Match the EM wave to a use."), {TEXT("Microwave -> satellite; X-ray -> medical imaging"), TEXT("Gamma -> TV remote; radio -> sterilising food"), TEXT("Infrared -> security scan bones; ultraviolet -> cooking food"), TEXT("Visible -> ionising cancer treatment; microwave -> fluorescent lamp")}, 0, TEXT("Microwaves communicate with satellites; X-rays image bones."), TEXT("Separate uses from hazards."), TEXT("Microwaves are used for satellite transmissions; X-rays for medical imaging."), EBHDiagramType::EMSpectrum, TEXT("EM uses by frequency"), 0.0f, 0.0f},
		{TEXT("EM hazards"), TEXT("Match the hazard correctly."), {TEXT("Ultraviolet -> skin cancer risk; microwave -> internal heating"), TEXT("Radio -> ionising mutations; visible -> deep tissue heating"), TEXT("Infrared -> no hazard; gamma -> harmless photos"), TEXT("Microwave -> sunburn only; X-rays -> skin warming only")}, 0, TEXT("High-energy waves can ionise; microwaves heat water-containing tissue."), TEXT("Put hazard beside the wave."), TEXT("Ultraviolet increases skin cancer risk; microwaves can cause internal heating."), EBHDiagramType::EMSpectrum, TEXT("hazards depend on frequency/energy"), 0.0f, 0.0f},
		{TEXT("Refraction"), TEXT("Match the medium change to bending direction."), {TEXT("Denser -> towards normal; less dense -> away from normal"), TEXT("Denser -> away; less dense -> towards"), TEXT("Both -> along boundary"), TEXT("Both -> no change in wavelength")}, 0, TEXT("Slower in denser means towards normal."), TEXT("Draw the normal before deciding."), TEXT("Light bends towards the normal entering a denser medium and away entering a less dense medium."), EBHDiagramType::RayDiagram, TEXT("denser towards, less dense away"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("EM spectrum"), TEXT("Order the EM spectrum from longest wavelength to shortest."), {TEXT("Radio -> microwave -> infrared -> visible -> ultraviolet -> X-rays -> gamma"), TEXT("Gamma -> X-rays -> ultraviolet -> visible -> infrared -> microwave -> radio"), TEXT("Visible -> radio -> gamma -> microwave -> infrared -> X-rays -> ultraviolet"), TEXT("Radio -> visible -> infrared -> gamma -> microwave -> ultraviolet -> X-rays")}, 0, TEXT("Start with radio and end with gamma."), TEXT("Use the standard EM spectrum order."), TEXT("The standard order is radio, microwave, infrared, visible, ultraviolet, X-rays, gamma."), EBHDiagramType::EMSpectrum, TEXT("radio to gamma"), 0.0f, 0.0f},
		{TEXT("Visible colours"), TEXT("Order visible light from longest wavelength to shortest."), {TEXT("Red -> orange -> yellow -> green -> blue -> indigo -> violet"), TEXT("Violet -> indigo -> blue -> green -> yellow -> orange -> red"), TEXT("Green -> red -> blue -> violet -> orange -> yellow -> indigo"), TEXT("Red -> violet -> orange -> indigo -> yellow -> blue -> green")}, 0, TEXT("Use ROYGBIV."), TEXT("Red is longest in visible light."), TEXT("Visible colours from longest to shortest wavelength are ROYGBIV."), EBHDiagramType::EMSpectrum, TEXT("ROYGBIV"), 0.0f, 0.0f},
		{TEXT("Doppler effect"), TEXT("Order the approaching-source explanation."), {TEXT("Source approaches -> wavefronts bunch -> wavelength shorter -> frequency higher"), TEXT("Frequency higher -> source approaches -> wavelength longer -> wavefronts spread"), TEXT("Wavefronts spread -> source approaches -> frequency lower -> pitch higher"), TEXT("Source stops -> wavelength zero -> frequency zero -> pitch high")}, 0, TEXT("Approaching source bunches wavefronts."), TEXT("Short wavelength means high frequency for same wave speed."), TEXT("Approach bunches wavefronts, reducing wavelength and increasing observed frequency."), EBHDiagramType::Wave, TEXT("approach -> higher observed f"), 0.0f, 0.0f},
		{TEXT("Optical fibre"), TEXT("Order how an optical fibre carries a light signal."), {TEXT("Light enters core -> hits boundary above c -> TIR repeats -> signal travels"), TEXT("Light enters air -> frequency stops -> TIR breaks -> signal disappears"), TEXT("Cladding absorbs light -> core heats -> signal becomes sound -> exits"), TEXT("Ray bends out -> angle lowers -> charge builds -> printer fires")}, 0, TEXT("Optical fibres use repeated total internal reflection."), TEXT("Keep the ray inside the glass core."), TEXT("Light repeatedly undergoes total internal reflection inside the fibre."), EBHDiagramType::RayDiagram, TEXT("optical fibre uses TIR"), 0.0f, 0.0f}
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
		{TEXT("Energy stores"), TEXT("Which store does a moving student have while escaping?"), {TEXT("Kinetic"), TEXT("Nuclear"), TEXT("Electrostatic only"), TEXT("Magnetic only")}, 0, TEXT("Movement means kinetic energy."), TEXT("Name the store linked to motion."), TEXT("A moving object has kinetic energy."), EBHDiagramType::EnergyChain, TEXT("Ek = 1/2 mv^2"), 0.0f, 0.0f},
		{TEXT("Energy pathways"), TEXT("Which pathway transfers energy when a force moves a desk?"), {TEXT("Mechanically"), TEXT("By heating only"), TEXT("By radiation only"), TEXT("Nuclearly")}, 0, TEXT("A force moving an object is mechanical work."), TEXT("Link force and distance."), TEXT("Energy is transferred mechanically when a force moves an object."), EBHDiagramType::EnergyChain, TEXT("work done = energy transferred"), 0.0f, 0.0f},
		{TEXT("Conservation"), TEXT("What does conservation of energy mean?"), {TEXT("Total energy before equals total energy after"), TEXT("Energy is always useful"), TEXT("Energy can be destroyed by friction"), TEXT("Only batteries store energy")}, 0, TEXT("Energy changes store or pathway."), TEXT("Separate useful and wasted transfers."), TEXT("Energy is conserved; total energy before equals total energy after."), EBHDiagramType::Sankey, TEXT("energy in = energy out"), 0.0f, 0.0f},
		{TEXT("Efficiency"), TEXT("What do wider arrows show on a Sankey diagram?"), {TEXT("More energy"), TEXT("Less time"), TEXT("More mass"), TEXT("Higher current only")}, 0, TEXT("Sankey arrow width is proportional to energy."), TEXT("Compare useful and wasted arrows."), TEXT("Wider arrows represent larger energy transfers."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 0.0f, 0.0f},
		{TEXT("Conduction"), TEXT("Why are metals usually good thermal conductors?"), {TEXT("They have free electrons"), TEXT("They have no particles"), TEXT("They trap all infrared"), TEXT("They are always black")}, 0, TEXT("Free electrons transfer energy by collisions."), TEXT("Mention vibrating particles and free electrons."), TEXT("Metals conduct well because free electrons transfer thermal energy through collisions."), EBHDiagramType::EnergyChain, TEXT("metals: free electrons transfer thermal energy"), 0.0f, 0.0f},
		{TEXT("Convection"), TEXT("What happens to heated fluid in a convection current?"), {TEXT("It expands, becomes less dense, and rises"), TEXT("It contracts, becomes denser, and rises"), TEXT("It freezes immediately"), TEXT("It emits gamma rays")}, 0, TEXT("Warm fluid rises."), TEXT("Use density changes to explain motion."), TEXT("Heated fluid expands, becomes less dense, and rises."), EBHDiagramType::EnergyChain, TEXT("heated fluid rises"), 0.0f, 0.0f},
		{TEXT("Radiation"), TEXT("Which surface is best at absorbing and emitting infrared?"), {TEXT("Black and dull"), TEXT("White and shiny"), TEXT("Transparent and cold"), TEXT("Silver and polished only")}, 0, TEXT("Black dull surfaces are best absorbers and emitters."), TEXT("Shiny white surfaces reflect infrared."), TEXT("Black dull surfaces are the best absorbers and emitters of infrared radiation."), EBHDiagramType::EnergyChain, TEXT("black dull absorbs/emits best"), 0.0f, 0.0f},
		{TEXT("Renewables"), TEXT("Which resource is renewable?"), {TEXT("Wind"), TEXT("Coal"), TEXT("Oil"), TEXT("Natural gas")}, 0, TEXT("Renewable resources are replenished quickly."), TEXT("Ask whether it will run out on human timescales."), TEXT("Wind is renewable because it is replenished as it is used."), EBHDiagramType::EnergyChain, TEXT("renewable = replenished"), 0.0f, 0.0f},
		{TEXT("Nuclear"), TEXT("What is a main disadvantage of nuclear power?"), {TEXT("Radioactive waste must be stored safely"), TEXT("It releases no useful energy"), TEXT("It can only work at night"), TEXT("It burns coal directly")}, 0, TEXT("Nuclear power produces long-lived radioactive waste."), TEXT("Balance high output against waste."), TEXT("Nuclear power uses small fuel amounts but produces radioactive waste needing safe storage."), EBHDiagramType::EnergyChain, TEXT("nuclear -> radioactive waste"), 0.0f, 0.0f},
		{TEXT("Generation chains"), TEXT("What is the main useful energy transfer in a solar cell?"), {TEXT("Light energy directly to electrical energy"), TEXT("Chemical to kinetic to sound only"), TEXT("Nuclear to thermal to light"), TEXT("Electrical to gravitational")}, 0, TEXT("Solar cells do not need a turbine."), TEXT("Name input and output stores."), TEXT("A solar cell transfers light energy directly to electrical energy."), EBHDiagramType::EnergyChain, TEXT("solar cell: light -> electrical"), 0.0f, 0.0f}
	};
	static const FRevisionSpec TF[] = {
		{TEXT("Conservation"), TEXT("True or false: wasted energy is destroyed."), {TEXT("False"), TEXT("True"), TEXT("Only in motors"), TEXT("Only in lamps")}, 0, TEXT("Wasted energy is dissipated to surroundings."), TEXT("Use conservation of energy."), TEXT("Wasted energy is transferred to less useful stores, usually thermal, not destroyed."), EBHDiagramType::Sankey, TEXT("energy conserved"), 0.0f, 0.0f},
		{TEXT("Radiation"), TEXT("True or false: infrared radiation can transfer energy through a vacuum."), {TEXT("True"), TEXT("False"), TEXT("Only through solids"), TEXT("Only by convection")}, 0, TEXT("Radiation does not need a medium."), TEXT("Think of heat from the Sun."), TEXT("Infrared radiation can travel through a vacuum."), EBHDiagramType::EnergyChain, TEXT("radiation needs no medium"), 0.0f, 0.0f},
		{TEXT("Power"), TEXT("True or false: power is the rate of energy transfer."), {TEXT("True"), TEXT("False"), TEXT("Only force"), TEXT("Only distance")}, 0, TEXT("Power is energy per time."), TEXT("Use watts as joules per second."), TEXT("Power is the rate of energy transfer or rate of doing work."), EBHDiagramType::EnergyChain, TEXT("P = W / t"), 0.0f, 0.0f},
		{TEXT("Resources"), TEXT("True or false: fossil fuels release greenhouse gases that contribute to global warming."), {TEXT("True"), TEXT("False"), TEXT("Only wind turbines do"), TEXT("Only solar cells do")}, 0, TEXT("Coal, oil, and gas are fossil fuels."), TEXT("Link combustion to carbon dioxide."), TEXT("Burning fossil fuels releases greenhouse gases that contribute to global warming."), EBHDiagramType::EnergyChain, TEXT("fossil fuels -> greenhouse gases"), 0.0f, 0.0f},
		{TEXT("Convection"), TEXT("True or false: convection is thermal transfer mainly in fluids."), {TEXT("True"), TEXT("False"), TEXT("Only in solids"), TEXT("Only by light")}, 0, TEXT("Fluids are liquids and gases."), TEXT("Use rising warm fluid and sinking cool fluid."), TEXT("Convection transfers thermal energy in fluids by circulation."), EBHDiagramType::EnergyChain, TEXT("convection in fluids"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Calc[] = {
		{TEXT("Efficiency"), TEXT("A generator gets 500 J input and gives 350 J useful output. What is efficiency?"), {TEXT("70%"), TEXT("35%"), TEXT("150%"), TEXT("850%")}, 0, TEXT("Useful output divided by total input times 100."), TEXT("Show 350 / 500 x 100."), TEXT("Efficiency = 350 / 500 x 100 = 70%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 70.0f, 0.5f},
		{TEXT("Work done"), TEXT("A 20 N force moves a crate 3 m. What work is done?"), {TEXT("60 J"), TEXT("6.7 J"), TEXT("23 J"), TEXT("17 J")}, 0, TEXT("Use W = Fd."), TEXT("Multiply force by distance."), TEXT("W = 20 x 3 = 60 J."), EBHDiagramType::EnergyChain, TEXT("W = Fd"), 60.0f, 0.1f},
		{TEXT("Kinetic energy"), TEXT("A 2 kg object moves at 4 m/s. What is its kinetic energy?"), {TEXT("16 J"), TEXT("8 J"), TEXT("32 J"), TEXT("4 J")}, 0, TEXT("Use Ek = 1/2 mv^2."), TEXT("Square the velocity first."), TEXT("Ek = 0.5 x 2 x 4^2 = 16 J."), EBHDiagramType::EnergyChain, TEXT("Ek = 1/2 mv^2"), 16.0f, 0.1f},
		{TEXT("GPE"), TEXT("A 3 kg box is lifted 2 m. Use g = 10 N/kg. What GPE is gained?"), {TEXT("60 J"), TEXT("15 J"), TEXT("6 J"), TEXT("30 J")}, 0, TEXT("Use Ep = mgh."), TEXT("Multiply mass, g, and height."), TEXT("Ep = 3 x 10 x 2 = 60 J."), EBHDiagramType::EnergyChain, TEXT("Ep = mgh"), 60.0f, 0.1f},
		{TEXT("Power"), TEXT("A machine transfers 900 J in 30 s. What is its power?"), {TEXT("30 W"), TEXT("27000 W"), TEXT("930 W"), TEXT("0.033 W")}, 0, TEXT("Use P = W / t."), TEXT("Divide energy transferred by time."), TEXT("P = 900 / 30 = 30 W."), EBHDiagramType::EnergyChain, TEXT("P = W / t"), 30.0f, 0.1f},
		{TEXT("Sankey"), TEXT("A lamp takes 100 J. 25 J is useful light. How much is wasted?"), {TEXT("75 J"), TEXT("125 J"), TEXT("25 J"), TEXT("4 J")}, 0, TEXT("Input = useful + wasted."), TEXT("Subtract useful from total input."), TEXT("Wasted energy = 100 - 25 = 75 J."), EBHDiagramType::Sankey, TEXT("input = useful + wasted"), 75.0f, 0.1f},
		{TEXT("Power and work"), TEXT("A student does 240 J of work in 12 s lifting equipment. What is power?"), {TEXT("20 W"), TEXT("2880 W"), TEXT("252 W"), TEXT("0.05 W")}, 0, TEXT("Power is work divided by time."), TEXT("Show 240 / 12."), TEXT("P = 240 / 12 = 20 W."), EBHDiagramType::EnergyChain, TEXT("P = W / t"), 20.0f, 0.1f}
	};
	static const FRevisionSpec Skill[] = {
		{TEXT("Sankey evaluation"), TEXT("A motor has a wide wasted thermal arrow and a narrow useful kinetic arrow. Which IGCSE conclusion is best?"), {TEXT("The motor is inefficient because most input energy is dissipated as thermal energy"), TEXT("The motor is efficient because wasted arrows are always ignored"), TEXT("Energy has been destroyed in the motor"), TEXT("The useful output is larger because the arrow is narrower")}, 0, TEXT("Sankey arrow width represents energy."), TEXT("Compare useful output with wasted output and use the word dissipated."), TEXT("A wide wasted arrow means much of the input energy is dissipated, so the motor has low efficiency."), EBHDiagramType::Sankey, TEXT("exam skill: interpret a Sankey diagram"), 0.0f, 0.0f},
		{TEXT("Energy stores"), TEXT("A toy car rolls down a ramp and speeds up. Which energy account is most complete?"), {TEXT("Gravitational potential energy decreases while kinetic energy and thermal energy increase"), TEXT("Kinetic energy changes directly into nuclear energy"), TEXT("Energy is created because the car speeds up"), TEXT("Only elastic energy changes")}, 0, TEXT("Track the store before and after the ramp."), TEXT("Include the useful kinetic increase and wasted thermal transfer."), TEXT("As height decreases, gravitational potential energy is transferred mainly to kinetic energy, with some dissipated thermally by friction."), EBHDiagramType::EnergyChain, TEXT("exam skill: describe stores and dissipation"), 0.0f, 0.0f},
		{TEXT("Insulation practical"), TEXT("Two identical cans of hot water are tested; one has a lid and insulation. Which is the best control variable?"), {TEXT("Same starting temperature and same volume of water"), TEXT("Different room temperatures"), TEXT("Different can sizes"), TEXT("Different thermometers and no repeats")}, 0, TEXT("A fair test changes only the insulation."), TEXT("Name variables that must be kept the same."), TEXT("To compare insulation fairly, keep the water volume, starting temperature, container size, and surroundings the same."), EBHDiagramType::EnergyChain, TEXT("exam skill: plan a fair thermal experiment"), 0.0f, 0.0f},
		{TEXT("Specific heat capacity"), TEXT("A metal block is heated electrically. Which data must be measured to find its specific heat capacity?"), {TEXT("Mass, energy supplied, and temperature rise"), TEXT("Only final temperature"), TEXT("Colour, surface area, and room brightness"), TEXT("Current only")}, 0, TEXT("Use energy transferred, mass, and change in temperature."), TEXT("Think of E = mc delta T but focus on measurements."), TEXT("Specific heat capacity is found from energy supplied divided by mass and temperature rise."), EBHDiagramType::EnergyChain, TEXT("exam skill: identify required measurements"), 0.0f, 0.0f},
		{TEXT("Resource evaluation"), TEXT("A village chooses between diesel generators and wind turbines. Which is the strongest physics comparison?"), {TEXT("Wind is renewable and has no fuel cost, but output varies with weather"), TEXT("Diesel is renewable because it can be delivered by truck"), TEXT("Wind always gives the same power output"), TEXT("Diesel produces no greenhouse gases")}, 0, TEXT("Compare reliability, renewability, and emissions."), TEXT("Give one advantage and one limitation."), TEXT("Wind is renewable and low-emission in use, but its power output is variable; diesel is reliable but non-renewable and emits greenhouse gases."), EBHDiagramType::EnergyChain, TEXT("exam skill: evaluate energy resources"), 0.0f, 0.0f},
		{TEXT("Power station explanation"), TEXT("In a fossil-fuel power station, where is energy transferred to the generator?"), {TEXT("Steam turns a turbine, and the turbine drives the generator"), TEXT("Coal changes directly into electrical energy in the wires"), TEXT("The cooling tower produces the current"), TEXT("The transformer burns fuel to make steam")}, 0, TEXT("Follow fuel, boiler, steam, turbine, generator."), TEXT("Name the turbine before the generator."), TEXT("Fuel heats water to make steam; steam turns a turbine, and the turbine drives the generator to produce electrical energy."), EBHDiagramType::EnergyChain, TEXT("exam skill: explain an energy transfer chain"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Graph[] = {
		{TEXT("Sankey diagram"), TEXT("On the Sankey display, which arrow is useful energy?"), {TEXT("The labelled output doing the intended job"), TEXT("Only the widest wasted arrow"), TEXT("The input arrow after it splits"), TEXT("Any arrow pointing downward")}, 0, TEXT("Useful means what the device is meant to produce."), TEXT("Compare useful output with wasted output."), TEXT("The useful arrow is the output energy transferred for the intended purpose."), EBHDiagramType::Sankey, TEXT("useful output / total input"), 0.0f, 0.0f},
		{TEXT("Sankey calculation"), TEXT("The Sankey input is 200 J and wasted arrow is 80 J. What is useful output?"), {TEXT("120 J"), TEXT("280 J"), TEXT("80 J"), TEXT("40 J")}, 0, TEXT("Input equals useful plus wasted."), TEXT("Subtract wasted from input."), TEXT("Useful output = 200 - 80 = 120 J."), EBHDiagramType::Sankey, TEXT("input = useful + wasted"), 120.0f, 0.1f},
		{TEXT("Heating graph"), TEXT("Which surface would give the strongest infrared emission in the display?"), {TEXT("Black dull large hot surface"), TEXT("White shiny small cool surface"), TEXT("Transparent cold surface"), TEXT("Silver polished cold surface")}, 0, TEXT("Hotter, larger, black dull surfaces emit more infrared."), TEXT("Check colour, texture, temperature, and area."), TEXT("Black dull, hot, large surfaces are strongest emitters."), EBHDiagramType::EnergyChain, TEXT("emission increases with temperature and area"), 0.0f, 0.0f},
		{TEXT("Efficiency bar"), TEXT("A Sankey shows 80 J useful from 100 J input. Which efficiency bar is correct?"), {TEXT("80%"), TEXT("20%"), TEXT("125%"), TEXT("180%")}, 0, TEXT("Useful divided by input times 100."), TEXT("Show 80 / 100 x 100."), TEXT("Efficiency = 80%."), EBHDiagramType::Sankey, TEXT("efficiency = useful / total x 100%"), 80.0f, 0.1f}
	};
	static const FRevisionSpec Match[] = {
		{TEXT("Stores"), TEXT("Choose the correct matching pair of energy stores."), {TEXT("Raised book -> gravitational; stretched band -> elastic"), TEXT("Raised book -> nuclear; stretched band -> thermal"), TEXT("Moving cart -> chemical; battery -> kinetic"), TEXT("Hot pan -> magnetic; fuel -> electrostatic")}, 0, TEXT("Height links to gravitational; stretching links to elastic."), TEXT("Name the store from what changed."), TEXT("A raised book has gravitational potential energy; a stretched band has elastic energy."), EBHDiagramType::EnergyChain, TEXT("energy stores"), 0.0f, 0.0f},
		{TEXT("Pathways"), TEXT("Match the transfer pathway to the example."), {TEXT("Electrical: lamp circuit; radiation: infrared from heater"), TEXT("Mechanical: current in wire; heating: light through vacuum"), TEXT("Radiation: push on desk; electrical: hot air rising"), TEXT("Convection: work done by force; nuclear: sound wave")}, 0, TEXT("Electrical uses current; radiation uses waves."), TEXT("Do not confuse convection with radiation."), TEXT("A lamp circuit transfers energy electrically; a heater can transfer energy by infrared radiation."), EBHDiagramType::EnergyChain, TEXT("transfer pathways"), 0.0f, 0.0f},
		{TEXT("Resources"), TEXT("Match each resource to renewable/non-renewable."), {TEXT("Wind -> renewable; gas -> non-renewable"), TEXT("Coal -> renewable; solar -> non-renewable"), TEXT("Nuclear fuel -> renewable; oil -> renewable"), TEXT("Tides -> non-renewable; geothermal -> non-renewable")}, 0, TEXT("Fossil fuels run out; wind is replenished."), TEXT("Sort by whether it is replaced as quickly as used."), TEXT("Wind is renewable; natural gas is non-renewable."), EBHDiagramType::EnergyChain, TEXT("renewable vs non-renewable"), 0.0f, 0.0f},
		{TEXT("Thermal transfer"), TEXT("Match each thermal transfer correctly."), {TEXT("Conduction -> solids; convection -> fluids"), TEXT("Conduction -> vacuum only; convection -> metals only"), TEXT("Radiation -> needs particles; convection -> no medium"), TEXT("Conduction -> gamma rays; radiation -> liquid circulation")}, 0, TEXT("Convection needs fluid movement."), TEXT("Use particle collisions for conduction."), TEXT("Conduction mainly transfers through solids; convection transfers through fluids."), EBHDiagramType::EnergyChain, TEXT("conduction solids, convection fluids"), 0.0f, 0.0f}
	};
	static const FRevisionSpec Order[] = {
		{TEXT("Convection"), TEXT("Order a convection current."), {TEXT("Fluid heats -> expands -> density decreases -> rises -> cool fluid sinks"), TEXT("Cool fluid rises -> density decreases -> heats -> contracts -> sinks"), TEXT("Fluid freezes -> radiation stops -> density zero -> rises"), TEXT("Particles leave solid -> current flows -> fuse melts -> fluid rises")}, 0, TEXT("Warm fluid becomes less dense."), TEXT("Start with heating."), TEXT("Heated fluid expands, becomes less dense and rises; cooler denser fluid sinks."), EBHDiagramType::EnergyChain, TEXT("heated fluid rises"), 0.0f, 0.0f},
		{TEXT("Conduction in metals"), TEXT("Order thermal conduction in a metal rod."), {TEXT("Hot end particles vibrate more -> free electrons gain energy -> collisions transfer energy -> cooler end warms"), TEXT("Cool end emits gamma -> electrons stop -> hot end freezes -> rod warms"), TEXT("Current splits -> fuse blows -> particles vanish -> energy destroyed"), TEXT("Air rises -> particles leave metal -> rod becomes vacuum -> warms")}, 0, TEXT("Metals have free electrons."), TEXT("Use vibrations and electron collisions."), TEXT("In metals, vibrating particles and free electrons transfer energy by collisions."), EBHDiagramType::EnergyChain, TEXT("metal conduction uses free electrons"), 0.0f, 0.0f},
		{TEXT("Power station"), TEXT("Order a fossil-fuel power station transfer chain."), {TEXT("Chemical store -> thermal transfer -> turbine kinetic -> generator electrical"), TEXT("Electrical -> chemical -> turbine nuclear -> thermal"), TEXT("Kinetic water -> static charge -> elastic cable -> nuclear"), TEXT("Light -> chemical coal -> magnetic heater -> gravitational")}, 0, TEXT("Fuel heats water; steam turns turbine."), TEXT("End with generator electrical output."), TEXT("Fossil fuel chemical energy heats water, producing kinetic turbine energy and electrical energy."), EBHDiagramType::EnergyChain, TEXT("chemical -> thermal -> kinetic -> electrical"), 0.0f, 0.0f},
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

TArray<FBHRevisionQuestion> BuildQuestionBank()
{
	TArray<FBHRevisionQuestion> Bank;
	Bank.Reserve(160);
	BuildForces(Bank);
	BuildElectricity(Bank);
	BuildWaves(Bank);
	BuildEnergy(Bank);
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
}

const TArray<FBHRevisionQuestion>& FBHRevisionQuestionBank::GetQuestions()
{
	static const TArray<FBHRevisionQuestion> Bank = BuildQuestionBank();
	return Bank;
}

bool FBHRevisionQuestionBank::Validate(FString& OutSummary)
{
	const TArray<FBHRevisionQuestion>& Bank = GetQuestions();
	int32 TopicCounts[4] = {0, 0, 0, 0};
	int32 DifficultyCounts[4][3] = {};
	int32 TypeCounts[7] = {0, 0, 0, 0, 0, 0, 0};
	TSet<FString> Ids;
	bool bValid = Bank.Num() == 160;
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
		bValid = bValid && TopicCounts[Index] == 40;
		bValid = bValid && DifficultyCounts[Index][0] == 12;
		bValid = bValid && DifficultyCounts[Index][1] == 16;
		bValid = bValid && DifficultyCounts[Index][2] == 12;
	}

	const int32 ExpectedTypeCounts[7] = {40, 20, 28, 24, 16, 16, 16};
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

#if WITH_DEV_AUTOMATION_TESTS

#include "BHFeedbackSubsystem.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBHFeedbackLogSinkTest,
	"BlackoutHunt.Feedback.LogSink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBHFeedbackLogSinkTest::RunTest(const FString& Parameters)
{
	// Verbosity floor: with a Warning floor, Error/Warning are kept and Display/Log are dropped.
	{
		FBHFeedbackLogSink Sink(16, ELogVerbosity::Warning);
		Sink.Serialize(TEXT("an error happened"), ELogVerbosity::Error, FName(TEXT("LogBH")));
		Sink.Serialize(TEXT("a warning happened"), ELogVerbosity::Warning, FName(TEXT("LogBH")));
		Sink.Serialize(TEXT("a display line"), ELogVerbosity::Display, FName(TEXT("LogBH")));
		Sink.Serialize(TEXT("a verbose line"), ELogVerbosity::Log, FName(TEXT("LogBH")));

		const TArray<FString> Lines = Sink.Snapshot();
		TestEqual(TEXT("Only Error and Warning are captured at a Warning floor."), Lines.Num(), 2);
		if (Lines.Num() == 2)
		{
			TestTrue(TEXT("First captured line is the error, oldest-first."), Lines[0].Contains(TEXT("an error happened")));
			TestTrue(TEXT("Captured line is prefixed with its log category."), Lines[0].Contains(TEXT("LogBH")));
			TestTrue(TEXT("Second captured line is the warning."), Lines[1].Contains(TEXT("a warning happened")));
		}
	}

	// A more permissive floor (Log) keeps Display and Log lines too.
	{
		FBHFeedbackLogSink Sink(16, ELogVerbosity::Log);
		Sink.Serialize(TEXT("display line"), ELogVerbosity::Display, FName(TEXT("LogBH")));
		Sink.Serialize(TEXT("log line"), ELogVerbosity::Log, FName(TEXT("LogBH")));
		Sink.Serialize(TEXT("very verbose"), ELogVerbosity::VeryVerbose, FName(TEXT("LogBH")));
		const TArray<FString> Lines = Sink.Snapshot();
		TestEqual(TEXT("Display and Log captured, VeryVerbose dropped, at a Log floor."), Lines.Num(), 2);
	}

	// Ring-buffer wraparound: capacity 3, write 5, keep the newest 3 oldest-first.
	{
		FBHFeedbackLogSink Sink(3, ELogVerbosity::Log);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Sink.Serialize(*FString::Printf(TEXT("line %d"), Index), ELogVerbosity::Log, FName(TEXT("LogBH")));
		}
		const TArray<FString> Lines = Sink.Snapshot();
		TestEqual(TEXT("Ring buffer keeps only its capacity."), Lines.Num(), 3);
		if (Lines.Num() == 3)
		{
			TestTrue(TEXT("Oldest retained line is line 2 (0 and 1 evicted)."), Lines[0].Contains(TEXT("line 2")));
			TestTrue(TEXT("Newest retained line is line 4."), Lines[2].Contains(TEXT("line 4")));
		}
	}

	// Long lines are clamped so a single spammy log entry cannot bloat the payload.
	{
		FBHFeedbackLogSink Sink(4, ELogVerbosity::Log);
		const FString Huge = FString::ChrN(5000, TEXT('x'));
		Sink.Serialize(*Huge, ELogVerbosity::Warning, FName(TEXT("LogBH")));
		const TArray<FString> Lines = Sink.Snapshot();
		TestEqual(TEXT("One long line is captured."), Lines.Num(), 1);
		if (Lines.Num() == 1)
		{
			TestTrue(TEXT("Captured line is clamped to a sane length."), Lines[0].Len() <= 1024);
		}
	}

	// A zero-capacity sink quietly captures nothing.
	{
		FBHFeedbackLogSink Sink(0, ELogVerbosity::Log);
		Sink.Serialize(TEXT("ignored"), ELogVerbosity::Error, FName(TEXT("LogBH")));
		TestEqual(TEXT("Zero-capacity sink stores nothing."), Sink.Snapshot().Num(), 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

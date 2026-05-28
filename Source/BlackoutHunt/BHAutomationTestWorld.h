#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class UPackage;
class UWorld;

class FBHScopedAutomationWorld
{
public:
	FBHScopedAutomationWorld(const TCHAR* InBaseName, bool bCreatePhysicsScene = false, bool bCreateNavigation = false, bool bCreateAISystem = false);
	~FBHScopedAutomationWorld();

	FBHScopedAutomationWorld(const FBHScopedAutomationWorld&) = delete;
	FBHScopedAutomationWorld& operator=(const FBHScopedAutomationWorld&) = delete;

	UWorld* Get() const { return World; }
	UWorld* operator->() const { return World; }
	explicit operator bool() const { return World != nullptr; }

private:
	UWorld* World = nullptr;
	UPackage* Package = nullptr;
};

#endif

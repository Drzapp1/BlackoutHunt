// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

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

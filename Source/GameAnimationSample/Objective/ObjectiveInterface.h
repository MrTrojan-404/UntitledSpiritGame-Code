#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ObjectiveTypes.h"
#include "ObjectiveInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UObjectiveInterface : public UInterface
{
	GENERATED_BODY()
};

class GAMEANIMATIONSAMPLE_API IObjectiveInterface
{
	GENERATED_BODY()

public:
	// Called by game mode to start this objective
	virtual void Activate() = 0;

	// Called when objective is completed
	// either by overlap or external actor
	virtual void Complete() = 0;

	// Called by game mode on mission fail
	// or explicit failure condition
	virtual void Fail() = 0;

	// Query current state
	virtual EObjectiveState GetObjectiveState() const = 0;

	// Returns data for subsystem and HUD
	virtual FObjectiveInfo GetObjectiveInfo() const = 0;

	// Whether this objective blocks mission completion
	virtual bool IsOptional() const = 0;
};
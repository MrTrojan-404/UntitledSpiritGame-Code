// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractWithCrosshairsInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UInteractWithCrosshairsInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAMEANIMATIONSAMPLE_API IInteractWithCrosshairsInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnCrosshairFocused() {}
	virtual void OnCrosshairUnfocused() {}
};

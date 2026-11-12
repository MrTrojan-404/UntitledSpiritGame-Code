// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAMEANIMATIONSAMPLE_API IInteractionInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// Called on server when interaction is confirmed
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(AActor* Interactor);

	// UI text
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	FText GetInteractionText() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ShowInteractionPrompt(bool bShow);

	// Checks if the conditions are met (e.g., enemy is unaware and player is behind them)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanPerformStealthTakedown(AActor* Interactor) const;

	// The actual event that fires when the player presses the button
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void ExecuteStealthTakedown(AActor* Interactor);
};

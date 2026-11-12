// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReturnToMainMenu.generated.h"

/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API UReturnToMainMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void MenuSetup();
	void MenuTearDown();

protected:
	virtual bool Initialize() override;

	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);

	
	UFUNCTION()
	void OnPlayerLeftGame();


private:
	UPROPERTY(meta = (BindWidget))
	class UButton* ReturnButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RestartLevelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RestartCheckpointButton;
	
	UFUNCTION()
	void ReturnButtonClicked();

	UFUNCTION()
	void RestartLevelClicked();

	UFUNCTION()
	void RestartCheckpointClicked();
	
	UPROPERTY()
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	UPROPERTY()
	class APlayerController* PlayerController;
};

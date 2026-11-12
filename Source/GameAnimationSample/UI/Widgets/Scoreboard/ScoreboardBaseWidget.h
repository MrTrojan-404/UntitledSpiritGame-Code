// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreboardBaseWidget.generated.h"

class UVerticalBox;
class AGASPGameState;
/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API UScoreboardBaseWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Refresh();

protected:

	// DM
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* PlayerListBox;

	// Team modes
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* RedTeamBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* BlueTeamBox;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> RowWidgetClass;

	void PopulateDeathmatch(AGASPGameState* GS);
	void PopulateTeams(AGASPGameState* GS);

	void ClearLists();
	
};

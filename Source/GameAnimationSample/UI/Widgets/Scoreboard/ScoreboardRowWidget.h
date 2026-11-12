// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreboardRowWidget.generated.h"

class AGASPCorePlayerState;
class UTextBlock;
class UBorder;
/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API UScoreboardRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayerState(AGASPCorePlayerState* InPlayerState);

protected:

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PlayerNameText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* KillsText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DeathsText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PingText;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* Background;

private:

	UPROPERTY()
	AGASPCorePlayerState* PlayerState;

	void UpdateRow();
	
};

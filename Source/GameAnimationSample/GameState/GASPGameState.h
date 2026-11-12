// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GASPGameState.generated.h"

/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPGameState : public AGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void UpdateTopScore(class AGASPArenaPlayerState* ScoringPlayer);
	
	UPROPERTY(Replicated)
	TSubclassOf<class UScoreboardBaseWidget> ScoreboardWidgetClass;
	
	UPROPERTY(Replicated)
	TArray<AGASPArenaPlayerState*> TopScoringPlayers;
	
	/** 
* Teams
*/
	void RedTeamScores();
	void BlueTeamScores();

	UPROPERTY()
	TArray<AGASPArenaPlayerState*> RedTeam;

	UPROPERTY()
	TArray<AGASPArenaPlayerState*> BlueTeam;
	
	UPROPERTY(ReplicatedUsing = OnRep_RedTeamScore)
	float RedTeamScore = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_BlueTeamScore)
	float BlueTeamScore = 0.f;

	UFUNCTION()
	void OnRep_RedTeamScore();

	UFUNCTION()
	void OnRep_BlueTeamScore();
	
private:
	float TopScore = 0.f;
	
};

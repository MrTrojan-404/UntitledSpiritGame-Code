// Fill out your copyright notice in the Description page of Project Settings.


#include "GASPGameState.h"

#include "GameAnimationSample/PlayerController/GASPArenaPlayerController.h"
#include "GameAnimationSample/PlayerState/GASPArenaPlayerState.h"
#include "Net/UnrealNetwork.h"

void AGASPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AGASPGameState, TopScoringPlayers);
	DOREPLIFETIME(AGASPGameState, RedTeamScore);
	DOREPLIFETIME(AGASPGameState, BlueTeamScore);
	DOREPLIFETIME(AGASPGameState, ScoreboardWidgetClass);
}

void AGASPGameState::UpdateTopScore(AGASPArenaPlayerState* ScoringPlayer)
{
	if (TopScoringPlayers.Num() == 0)
	{
		TopScoringPlayers.Add(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}
	else if (ScoringPlayer->GetScore() == TopScore)
	{
		TopScoringPlayers.AddUnique(ScoringPlayer);
	}
	else if (ScoringPlayer->GetScore() > TopScore)
	{
		TopScoringPlayers.Empty();
		TopScoringPlayers.AddUnique(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}
}

void AGASPGameState::RedTeamScores()
{
	++RedTeamScore;
	AGASPArenaPlayerController* BPlayer = Cast<AGASPArenaPlayerController>(GetWorld()->GetFirstPlayerController());
	if (BPlayer)
	{
		BPlayer->SetHUDRedTeamScore(RedTeamScore);
	}
}

void AGASPGameState::BlueTeamScores()
{
	++BlueTeamScore;

	AGASPArenaPlayerController* BPlayer = Cast<AGASPArenaPlayerController>(GetWorld()->GetFirstPlayerController());
	if (BPlayer)
	{
		BPlayer->SetHUDBlueTeamScore(BlueTeamScore);
	}
}

void AGASPGameState::OnRep_RedTeamScore()
{
	AGASPArenaPlayerController* PlayerController = Cast<AGASPArenaPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PlayerController)
	{
		PlayerController->SetHUDRedTeamScore(RedTeamScore);
	}
}

void AGASPGameState::OnRep_BlueTeamScore()
{
	AGASPArenaPlayerController* PlayerController = Cast<AGASPArenaPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PlayerController)
	{
		PlayerController->SetHUDBlueTeamScore(BlueTeamScore);
	}
}

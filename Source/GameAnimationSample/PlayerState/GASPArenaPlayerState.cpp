// Fill out your copyright notice in the Description page of Project Settings.


#include "GASPArenaPlayerState.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/PlayerController/GASPArenaPlayerController.h"
#include "Net/UnrealNetwork.h"

void AGASPArenaPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGASPArenaPlayerState, Defeats);
	DOREPLIFETIME(AGASPArenaPlayerState, Team);
}

void AGASPArenaPlayerState::AddToScore(float ScoreAmount)
{
	SetScore(GetScore() + ScoreAmount);
	Character = Character == nullptr ? Cast<AGASPPlayerCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<AGASPArenaPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDScore(GetScore());
		}
	}
}

void AGASPArenaPlayerState::OnRep_Score()
{
	Super::OnRep_Score();

	Character = Character == nullptr ? Cast<AGASPPlayerCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<AGASPArenaPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDScore(GetScore());
		}
	}
}

void AGASPArenaPlayerState::AddToDefeats(int32 DefeatsAmount)
{
	Defeats += DefeatsAmount;
	Character = Character == nullptr ? Cast<AGASPPlayerCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<AGASPArenaPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

void AGASPArenaPlayerState::OnRep_Team()
{
	AGASPPlayerCharacter* PlayerCharacter = Cast <AGASPPlayerCharacter>(GetPawn());
	if (PlayerCharacter)
	{
		PlayerCharacter->SetTeamColor(Team);
	}
	
}

void AGASPArenaPlayerState::SetTeam(ETeam TeamToSet)
{
	Team = TeamToSet;

	AGASPPlayerCharacter* PlayerCharacter = Cast <AGASPPlayerCharacter>(GetPawn());
	if (PlayerCharacter)
	{
		PlayerCharacter->SetTeamColor(Team);
	}
}


void AGASPArenaPlayerState::OnRep_Defeats()
{
	Character = Character == nullptr ? Cast<AGASPPlayerCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<AGASPArenaPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

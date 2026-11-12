// Fill out your copyright notice in the Description page of Project Settings.


#include "GASPArenaGameMode.h"


#include "GameAnimationSample/GameState/GASPGameState.h"
#include "GameAnimationSample/PlayerController/GASPArenaPlayerController.h"
#include "GameAnimationSample/PlayerState/GASPArenaPlayerState.h"


namespace MatchState
{
	const FName Cooldown = FName("Cooldown");
}


AGASPArenaGameMode::AGASPArenaGameMode()
{
	bDelayedStart = true;
}
void AGASPArenaGameMode::BeginPlay()
{
	Super::BeginPlay();

	LevelStartingTime = GetWorld()->GetTimeSeconds();
	
	AGASPGameState* GS = GetGameState<AGASPGameState>();
	if (GS)
	{
		GS->ScoreboardWidgetClass = ScoreboardWidgetClass;
	}
}

void AGASPArenaGameMode::PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController)
{
	// 1. Call Core to physically kill the character
	Super::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);

	// 2. Do Arena Scoring
	if (VictimController == nullptr || VictimController->PlayerState == nullptr) return;
	AGASPArenaPlayerState* AttackerPlayerState = AttackerController ? Cast<AGASPArenaPlayerState>(AttackerController->PlayerState) : nullptr;
	AGASPArenaPlayerState* VictimPlayerState = VictimController ? Cast<AGASPArenaPlayerState>(VictimController->PlayerState) : nullptr;

	AGASPGameState* ShooterGameState = GetGameState<AGASPGameState>();
	
	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && ShooterGameState)
	{
		AttackerPlayerState->AddToScore(1.f);
		ShooterGameState->UpdateTopScore(AttackerPlayerState);
		// ... (Keep the rest of your lead/loser checking here) ...
	}
	
	if (VictimPlayerState)
	{
		VictimPlayerState->AddToDefeats(1);
	}

	// Broadcast the killfeed
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AGASPArenaPlayerController* PlayerController = Cast<AGASPArenaPlayerController>(*It);
		if (PlayerController && AttackerPlayerState && VictimPlayerState)
		{
			PlayerController->BroadcastElim(AttackerPlayerState, VictimPlayerState);
		}
	}
}

void AGASPArenaGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();
	
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AGASPArenaPlayerController* ShooterPlayerController = Cast<AGASPArenaPlayerController>(*It);
		if (ShooterPlayerController)
		{
			ShooterPlayerController->OnMatchStateSet(MatchState, bTeamsMatch);
		}
	}
}

void AGASPArenaGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MatchState == MatchState::WaitingToStart)
	{
		CountdownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			StartMatch();
		}
	}
	else if (MatchState == MatchState::InProgress)
	{
		CountdownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			SetMatchState(MatchState::Cooldown);
		}
	}
	else if (MatchState == MatchState::Cooldown)
	{
		CountdownTime = CooldownTime + WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			RestartGame();
		}
	}
}

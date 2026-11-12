// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamsGameMode.h"

#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/GameState/GASPGameState.h"
#include "GameAnimationSample/GameVariables/Team.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/PlayerState/GASPArenaPlayerState.h"
#include "Kismet/GameplayStatics.h"

ATeamsGameMode::ATeamsGameMode()
{
	bTeamsMatch = true;
}

void ATeamsGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	AGASPGameState* SGameState = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
	if (SGameState)
	{
		AGASPArenaPlayerState* SPState = NewPlayer->GetPlayerState<AGASPArenaPlayerState>();
		if (SPState && SPState->GetTeam() == ETeam::ET_NoTeam)
		{
			if (SGameState->BlueTeam.Num() >= SGameState->RedTeam.Num())
			{
				SGameState->RedTeam.AddUnique(SPState);
				SPState->SetTeam(ETeam::ET_RedTeam);
			}
			else
			{
				SGameState->BlueTeam.AddUnique(SPState);
				SPState->SetTeam(ETeam::ET_BlueTeam);
			}
		}
	}
}

void ATeamsGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	AGASPGameState* SGameState = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
	AGASPArenaPlayerState* SPState = Exiting->GetPlayerState<AGASPArenaPlayerState>();
	if (SGameState && SPState)
	{
		if (SGameState->RedTeam.Contains(SPState))
		{
			SGameState->RedTeam.Remove(SPState);
		}
		if (SGameState->BlueTeam.Contains(SPState))
		{
			SGameState->BlueTeam.Remove(SPState);
		}
	}
}

float ATeamsGameMode::CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage)
{
	AGASPArenaPlayerState* AttackerPState = Attacker->GetPlayerState<AGASPArenaPlayerState>();
	AGASPArenaPlayerState* VictimPState = Victim->GetPlayerState<AGASPArenaPlayerState>();
	if (AttackerPState == nullptr || VictimPState == nullptr) return BaseDamage;
	if (VictimPState == AttackerPState)
	{
		return BaseDamage;
	}
	if (AttackerPState->GetTeam() == VictimPState->GetTeam())
	{
		return 0.f;
	}
	return BaseDamage;
}

void ATeamsGameMode::PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController)
{
	Super::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);

	AGASPGameState* BGameState = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
	AGASPArenaPlayerState* AttackerPlayerState = AttackerController ? Cast<AGASPArenaPlayerState>(AttackerController->PlayerState) : nullptr;
	if (BGameState && AttackerPlayerState)
	{
		if (AttackerPlayerState->GetTeam() == ETeam::ET_BlueTeam)
		{
			BGameState->BlueTeamScores();
		}
		if (AttackerPlayerState->GetTeam() == ETeam::ET_RedTeam)
		{
			BGameState->RedTeamScores();
		}
	}
}

void ATeamsGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	AGASPGameState* BGameState = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
	if (BGameState)
	{
		for (auto PState : BGameState->PlayerArray)
		{
			AGASPArenaPlayerState* BPState = Cast<AGASPArenaPlayerState>(PState.Get());
			if (BPState && BPState->GetTeam() == ETeam::ET_NoTeam)
			{
				if (BGameState->BlueTeam.Num() >= BGameState->RedTeam.Num())
				{
					BGameState->RedTeam.AddUnique(BPState);
					BPState->SetTeam(ETeam::ET_RedTeam);
				}
				else
				{
					BGameState->BlueTeam.AddUnique(BPState);
					BPState->SetTeam(ETeam::ET_BlueTeam);
				}
			}
		}
	}
}

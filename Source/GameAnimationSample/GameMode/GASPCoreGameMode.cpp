// Fill out your copyright notice in the Description page of Project Settings.


#include "GASPCoreGameMode.h"

#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/GameState/GASPGameState.h"
#include "GameAnimationSample/PlayerState/GASPCorePlayerState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AGASPCoreGameMode::AGASPCoreGameMode()
{
}

void AGASPCoreGameMode::RequestRespawn(AGASPPlayerCharacter* ElimmedCharacter, AController* ElimmedController)
{
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Reset();
		ElimmedCharacter->Destroy();
	}
	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
	}
}

void AGASPCoreGameMode::PlayerLeftGame(class AGASPCorePlayerState* PlayerLeaving)
{
	if (PlayerLeaving == nullptr) return;

	AGASPPlayerCharacter* CharacterLeaving = Cast<AGASPPlayerCharacter>(PlayerLeaving->GetPawn());
	if (CharacterLeaving)
	{
		CharacterLeaving->Elim(true);
	}
}

void AGASPCoreGameMode::PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController)
{
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Elim(false);
	}
}

float AGASPCoreGameMode::CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage)
{
	return BaseDamage;
}

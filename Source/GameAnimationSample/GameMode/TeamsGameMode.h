// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GASPArenaGameMode.h"
#include "TeamsGameMode.generated.h"

/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API ATeamsGameMode : public AGASPArenaGameMode
{
	GENERATED_BODY()

		
public:
	ATeamsGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual float CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage) override;
	void PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController) override;
	
protected:
	virtual void HandleMatchHasStarted() override;
};

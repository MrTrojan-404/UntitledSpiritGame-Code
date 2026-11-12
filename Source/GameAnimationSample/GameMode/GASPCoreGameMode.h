#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GASPCoreGameMode.generated.h"

class AGASPPlayerCharacter;
class AGASPPlayerController;

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPCoreGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AGASPCoreGameMode();

	// Universal Damage Calculation (e.g., base armor/resistances)
	virtual float CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage);

	// Universal Elim Logic (Just broadcasting the death, no scoring)
	virtual void PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController);
	
	// Universal Respawn Logic
	virtual void RequestRespawn(AGASPPlayerCharacter* ElimmedCharacter, AController* ElimmedController);

	// Clean up when players leave
	virtual void PlayerLeftGame(class AGASPCorePlayerState* PlayerLeaving);

};
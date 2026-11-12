// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GASPCoreGameMode.h"
#include "GASPArenaGameMode.generated.h"

namespace MatchState
{
	extern GAMEANIMATIONSAMPLE_API const FName Cooldown; // Match duration has been reached. Display winner and begin cooldown timer.
}


/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPArenaGameMode : public AGASPCoreGameMode
{
	GENERATED_BODY()

		
public:
	AGASPArenaGameMode();
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;

	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;

	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 10.f;

	float LevelStartingTime = 0.f;

	bool bTeamsMatch = false;
	
	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<class UScoreboardBaseWidget> ScoreboardWidgetClass;

	void PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;


private:
	float CountdownTime = 0.f;

	
};

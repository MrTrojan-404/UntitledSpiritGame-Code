// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GASPCorePlayerState.h"
#include "GameAnimationSample/GameVariables/Team.h"
#include "GASPArenaPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPArenaPlayerState : public AGASPCorePlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;
	virtual void OnRep_Score() override;
    
	UFUNCTION()
	virtual void OnRep_Defeats();

	void AddToScore(float ScoreAmount);
	void AddToDefeats(int32 DefeatsAmount);

	FORCEINLINE ETeam GetTeam() const { return Team; }
	void SetTeam(ETeam TeamToSet);
	FORCEINLINE int32 GetDefeats() const { return Defeats; }

private:
	UPROPERTY()
	class AGASPPlayerCharacter* Character;

	UPROPERTY()
	class AGASPArenaPlayerController* Controller;
	
	UPROPERTY(ReplicatedUsing = OnRep_Defeats) int32 Defeats;
	UPROPERTY(ReplicatedUsing = OnRep_Team) ETeam Team = ETeam::ET_NoTeam;

	UFUNCTION() void OnRep_Team();
};
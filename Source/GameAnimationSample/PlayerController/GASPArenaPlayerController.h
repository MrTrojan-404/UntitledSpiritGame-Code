// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GASPCorePlayerController.h"
#include "GASPArenaPlayerController.generated.h"

class AGASPArenaPlayerState;
/**
 * 
 */
UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPArenaPlayerController : public AGASPCorePlayerController
{
	GENERATED_BODY()

public:
	// --- ARENA HUD ---
	void SetHUDScore(float Score);
	void SetHUDDefeats(int32 Defeats);
	void SetHUDMatchCountdown(float CountdownTime);
	void SetHUDAnnouncementCountdown(float CountdownTime);
	void SetHUDRedTeamScore(int32 RedScore);
	void SetHUDBlueTeamScore(int32 BlueScore);

	// --- MATCH STATES ---
	void OnMatchStateSet(FName State, bool bTeamsMatch = false);
	void HandleMatchHasStarted(bool bTeamsMatch = false);
	void HandleCooldown();

	// --- ELIM ANNOUNCEMENTS ---
	UFUNCTION(Client, Reliable)
	void ClientElimAnnouncement(AGASPArenaPlayerState* Attacker, AGASPArenaPlayerState* Victim);
	void BroadcastElim(AGASPArenaPlayerState* Attacker, AGASPArenaPlayerState* Victim);

	// --- SCOREBOARD ---
	void ShowScoreboard();
	void HideScoreboard();

	FString GetInfoText(const TArray<class AGASPArenaPlayerState*>& Players);
	FString GetTeamsInfoText(class AGASPGameState* ShooterGameState);
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PollInit() override;
	virtual void SetupInitialHUD() override;
	void SetHUDTime();

// Tells the server "I just joined, what's the state of the game?"
	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();

	// The server's response back to the client to build their UI
	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime, bool bIsTeamsMatch);
	
private:
	// --- ARENA VARIABLES ---
	float HUDScore;
	int32 HUDDefeats;

	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;

	UFUNCTION()
	void OnRep_MatchState();

	UPROPERTY(ReplicatedUsing = OnRep_ShowTeamScores)
	bool bShowTeamScores = false;

	UFUNCTION()
	void OnRep_ShowTeamScores();

	void InitTeamScores();
	void HideTeamScores();

	UPROPERTY()
	class UScoreboardBaseWidget* ScoreboardWidget;

	FTimerHandle ScoreboardRefreshTimer;
	void RefreshScoreboard() const;
	
	// --- ARENA TIMERS ---
	float LevelStartingTime = 0.f;
	float MatchTime = 0.f;
	float WarmupTime = 0.f;
	float CooldownTime = 0.f;
	uint32 CountdownInt = 0;

};
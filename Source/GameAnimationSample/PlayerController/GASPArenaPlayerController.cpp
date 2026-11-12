// Fill out your copyright notice in the Description page of Project Settings.


#include "GASPArenaPlayerController.h"

#include "Components/TextBlock.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/GameState/GASPGameState.h"
#include "GameAnimationSample/PlayerState/GASPArenaPlayerState.h"
#include "GameAnimationSample/UI/Announcements.h"
#include "GameAnimationSample/UI/HUD/GASPHUD.h"
#include "Net/UnrealNetwork.h"
#include "GameAnimationSample/UI/Widgets/Scoreboard/ScoreboardBaseWidget.h"
#include "GameAnimationSample/GameMode/GASPArenaGameMode.h"
#include "GameAnimationSample/UI/Widgets/Announcement.h"
#include "GameAnimationSample/UI/Widgets/ArenaCharacterOverlay.h"
#include "GameAnimationSample/UI/Widgets/CharacterOverlay.h"
#include "Kismet/GameplayStatics.h"


void AGASPArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// As soon as the client spawns, ask the server what the match state is
	ServerCheckMatchState();
}

void AGASPArenaPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Keep the match timer ticking on the screen!
	SetHUDTime();
}

void AGASPArenaPlayerController::ServerCheckMatchState_Implementation()
{
	AGASPArenaGameMode* GameMode = Cast<AGASPArenaGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		MatchState = GameMode->GetMatchState();
		CooldownTime = GameMode->CooldownTime;
		
		// ADDED: GameMode->bTeamsMatch at the end!
		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime, GameMode->bTeamsMatch);
	}
}

void AGASPArenaPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime, bool bIsTeamsMatch)
{
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;

	// Set this locally IMMEDIATELY so the UI knows it's a team game
	bShowTeamScores = bIsTeamsMatch; 
	
	OnMatchStateSet(MatchState, bIsTeamsMatch);

	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && MatchState == MatchState::WaitingToStart)
	{
		GASPHUD->AddAnnouncement();
	}
}

void AGASPArenaPlayerController::SetHUDTime()
{
	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::InProgress) TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::Cooldown) TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	
	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	if (HasAuthority())
	{
		AGASPArenaGameMode* ArenaGameMode = Cast<AGASPArenaGameMode>(UGameplayStatics::GetGameMode(this));
		if (ArenaGameMode)
		{
			SecondsLeft = FMath::CeilToInt(ArenaGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}

	if (CountdownInt != SecondsLeft)
	{
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		if (MatchState == MatchState::InProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
		}
	}

	CountdownInt = SecondsLeft;
}

void AGASPArenaPlayerController::PollInit()
{
	// Check if the overlay is null BEFORE we let Core create it
	bool bOverlayWasNull = (CharacterOverlay == nullptr);

	// 1. Let the Core handle Health, Ammo, Ping, etc.
	Super::PollInit();

	// 2.Handle Arena specific UI ONLY ONCE
	if (bOverlayWasNull && CharacterOverlay)
	{
		if (bInitializeCharacterOverlay) SetHUDScore(HUDScore);
		if (bInitializeCharacterOverlay) SetHUDDefeats(HUDDefeats);
		
		if (bShowTeamScores) 
		{
			InitTeamScores();
			
			// Catch up late-joiners with the current score!
			AGASPGameState* GS = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
			if (GS)
			{
				SetHUDRedTeamScore(GS->RedTeamScore);
				SetHUDBlueTeamScore(GS->BlueTeamScore);
			}
		}
		else 
		{
			HideTeamScores();
		}
	}
}

void AGASPArenaPlayerController::SetupInitialHUD()
{
	// DO NOTHING! 
	// Arena relies on Match States (Warmup -> InProgress) to spawn the Character Overlay, 
	// so we intentionally block the Core from spawning it instantly.
}

void AGASPArenaPlayerController::SetHUDScore(float Score)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		// CAST TO THE CHILD CLASS
		if (UArenaCharacterOverlay* ArenaOverlay = Cast<UArenaCharacterOverlay>(GASPHUD->CharacterOverlay))
		{
			FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
			ArenaOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
			return;
		}
	}
    
	bInitializeCharacterOverlay = true;
	HUDScore = Score;
}
void AGASPArenaPlayerController::SetHUDDefeats(int32 Defeats)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		// CAST TO THE CHILD CLASS
		if (UArenaCharacterOverlay* ArenaOverlay = Cast<UArenaCharacterOverlay>(GASPHUD->CharacterOverlay))
		{
			FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
			ArenaOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
			return;
		}
	}
    
	bInitializeCharacterOverlay = true;
	HUDDefeats = Defeats;
}

void AGASPArenaPlayerController::SetHUDMatchCountdown(float CountdownTime)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		if (UArenaCharacterOverlay* ArenaOverlay = Cast<UArenaCharacterOverlay>(GASPHUD->CharacterOverlay))
		{
			if (CountdownTime < 0.f)
			{
				ArenaOverlay->MatchCountdownText->SetText(FText());
				return;
			}

			int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
			int32 Seconds = CountdownTime - Minutes * 60;

			FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
			ArenaOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
		}
	}
}

void AGASPArenaPlayerController::SetHUDAnnouncementCountdown(float CountdownTime)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	bool bHUDValid = GASPHUD &&
		GASPHUD->Announcement &&
		GASPHUD->Announcement->WarmupTime;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			GASPHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}

		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		GASPHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void AGASPArenaPlayerController::SetHUDRedTeamScore(int32 RedScore)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		if (UArenaCharacterOverlay* ArenaOverlay = Cast<UArenaCharacterOverlay>(GASPHUD->CharacterOverlay))
		{
			FString ScoreText = FString::Printf(TEXT("%d"), RedScore);
			ArenaOverlay->RedTeamScore->SetText(FText::FromString(ScoreText));
		}
	}
}

void AGASPArenaPlayerController::SetHUDBlueTeamScore(int32 BlueScore)
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		if (UArenaCharacterOverlay* ArenaOverlay = Cast<UArenaCharacterOverlay>(GASPHUD->CharacterOverlay))
		{
			FString ScoreText = FString::Printf(TEXT("%d"), BlueScore);
			ArenaOverlay->BlueTeamScore->SetText(FText::FromString(ScoreText));
		}
	}
}

void AGASPArenaPlayerController::OnMatchStateSet(FName State, bool bTeamsMatch)
{
	MatchState = State;

	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted(bTeamsMatch);
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void AGASPArenaPlayerController::HandleMatchHasStarted(bool bTeamsMatch)
{
	bShowTeamScores = bTeamsMatch; 
    
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD)
	{
		if (GASPHUD->CharacterOverlay == nullptr) GASPHUD->AddCharacterOverlay();
		if (GASPHUD->Announcement)
		{
			GASPHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
		
		if (bShowTeamScores)
		{
			InitTeamScores();
			
			// Catch up on the current score immediately!
			AGASPGameState* GS = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
			if (GS)
			{
				SetHUDRedTeamScore(GS->RedTeamScore);
				SetHUDBlueTeamScore(GS->BlueTeamScore);
			}
		}
		else
		{
			HideTeamScores();
		}
	}
}

void AGASPArenaPlayerController::HandleCooldown()
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD)
	{
		GASPHUD->CharacterOverlay->RemoveFromParent();
		bool bHUDValid = GASPHUD->Announcement && 
			GASPHUD->Announcement->AnnouncementText && 
			GASPHUD->Announcement->InfoText;

		if (bHUDValid)
		{
			GASPHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText = Announcement::NewMatchStartsIn;
			GASPHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));
			
			AGASPGameState* ShooterGameState = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
			AGASPArenaPlayerState* ShooterPlayerState = GetPlayerState<AGASPArenaPlayerState>();
			if (ShooterGameState && ShooterPlayerState)
			{
				TArray<AGASPArenaPlayerState*> TopPlayers = ShooterGameState->TopScoringPlayers;
				FString InfoTextString = bShowTeamScores ? GetTeamsInfoText(ShooterGameState) : GetInfoText(TopPlayers);

				GASPHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
			}		}
	}
	AGASPPlayerCharacter* PlayerCharacter = Cast<AGASPPlayerCharacter>(GetPawn());
	if (PlayerCharacter && PlayerCharacter->GetCombatComponent())
	{
		PlayerCharacter->bDisableGameplay = true;
		PlayerCharacter->GetCombatComponent()->HandleFire(false);
	}
}

void AGASPArenaPlayerController::ClientElimAnnouncement_Implementation(AGASPArenaPlayerState* Attacker, AGASPArenaPlayerState* Victim)
{
	AGASPArenaPlayerState* Self = GetPlayerState<AGASPArenaPlayerState>();
	if (Attacker && Victim && Self)
	{
		GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
		if (GASPHUD)
		{
			if (Attacker == Self && Victim != Self)
			{
				GASPHUD->AddElimAnnouncement("You", Victim->GetPlayerName());
				return;
			}
			if (Victim == Self && Attacker != Self)
			{
				GASPHUD->AddElimAnnouncement(Attacker->GetPlayerName(), "you");
				return;
			}
			if (Attacker == Victim && Attacker == Self)
			{
				GASPHUD->AddElimAnnouncement("You", "yourself");
				return;
			}
			if (Attacker == Victim && Attacker != Self)
			{
				GASPHUD->AddElimAnnouncement(Attacker->GetPlayerName(), "themselves");
				return;
			}
			GASPHUD->AddElimAnnouncement(Attacker->GetPlayerName(), Victim->GetPlayerName());
		}
	}
}

void AGASPArenaPlayerController::BroadcastElim(AGASPArenaPlayerState* Attacker, AGASPArenaPlayerState* Victim)
{
	ClientElimAnnouncement(Attacker, Victim);
}

void AGASPArenaPlayerController::ShowScoreboard()
{
	if (!ScoreboardWidget)
	{
		AGASPGameState* GS = GetWorld()->GetGameState<AGASPGameState>();

		if (!GS || !GS->ScoreboardWidgetClass) return;

		ScoreboardWidget = Cast<UScoreboardBaseWidget>(
				CreateWidget(this, GS->ScoreboardWidgetClass)
			);
	}

	if (!ScoreboardWidget) return;

	if (!ScoreboardWidget->IsInViewport())
	{
		ScoreboardWidget->AddToViewport();
	}

	// immediate update
	ScoreboardWidget->Refresh();

	//refresh every 0.5 sec WHILE OPEN
	GetWorldTimerManager().SetTimer(
		ScoreboardRefreshTimer,
		this,
		&AGASPArenaPlayerController::RefreshScoreboard,
		0.5f,
		true
	);
}

void AGASPArenaPlayerController::RefreshScoreboard() const
{
	if (ScoreboardWidget && ScoreboardWidget->IsInViewport())
	{
		ScoreboardWidget->Refresh();
	}
}

void AGASPArenaPlayerController::HideScoreboard()
{
	// stop refreshing
	GetWorldTimerManager().ClearTimer(ScoreboardRefreshTimer);

	if (ScoreboardWidget)
	{
		ScoreboardWidget->RemoveFromParent();
	}
}

void AGASPArenaPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGASPArenaPlayerController, MatchState);
    DOREPLIFETIME(AGASPArenaPlayerController, bShowTeamScores);
}

void AGASPArenaPlayerController::OnRep_MatchState()
{
    if (MatchState == MatchState::InProgress)
    {
        HandleMatchHasStarted();
    }
    else if (MatchState == MatchState::Cooldown)
    {
        HandleCooldown();
    }
}

void AGASPArenaPlayerController::OnRep_ShowTeamScores()
{
	if (bShowTeamScores) 
	{
		InitTeamScores();
		
		// Grab the current scores so it doesn't just say 0 | 0
		AGASPGameState* GS = Cast<AGASPGameState>(UGameplayStatics::GetGameState(this));
		if (GS)
		{
			SetHUDRedTeamScore(GS->RedTeamScore);
			SetHUDBlueTeamScore(GS->BlueTeamScore);
		}
	}
	else 
	{
		HideTeamScores();
	}
}

void AGASPArenaPlayerController::InitTeamScores()
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		if (UArenaCharacterOverlay* ArenaOverlay = Cast<UArenaCharacterOverlay>(GASPHUD->CharacterOverlay))
		{
			FString Zero("0");
			FString Spacer("|");
			ArenaOverlay->RedTeamScore->SetText(FText::FromString(Zero));
			ArenaOverlay->BlueTeamScore->SetText(FText::FromString(Zero));
			ArenaOverlay->ScoreSpacerText->SetText(FText::FromString(Spacer));
		}
	}
}

void AGASPArenaPlayerController::HideTeamScores()
{
	GASPHUD = GASPHUD == nullptr ? Cast<AGASPHUD>(GetHUD()) : GASPHUD;
	if (GASPHUD && GASPHUD->CharacterOverlay)
	{
		if (UArenaCharacterOverlay* ArenaOverlay = Cast<UArenaCharacterOverlay>(GASPHUD->CharacterOverlay))
		{
			ArenaOverlay->RedTeamScore->SetText(FText());
			ArenaOverlay->BlueTeamScore->SetText(FText());
			ArenaOverlay->ScoreSpacerText->SetText(FText());
		}
	}
}

FString AGASPArenaPlayerController::GetInfoText(const TArray<class AGASPArenaPlayerState*>& Players)
{
    AGASPArenaPlayerState* ShooterPlayerState = GetPlayerState<AGASPArenaPlayerState>();
    if (ShooterPlayerState == nullptr) return FString();
    FString InfoTextString;
    if (Players.Num() == 0)
    {
        InfoTextString = Announcement::ThereIsNoWinner;
    }
    else if (Players.Num() == 1 && Players[0] == ShooterPlayerState)
    {
        InfoTextString = Announcement::YouAreTheWinner;
    }
    else if (Players.Num() == 1)
    {
        InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *Players[0]->GetPlayerName());
    }
    else if (Players.Num() > 1)
    {
        InfoTextString = Announcement::PlayersTiedForTheWin;
        InfoTextString.Append(FString("\n"));
        for (auto TiedPlayer : Players)
        {
            InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
        }
    }
    return InfoTextString;
}

FString AGASPArenaPlayerController::GetTeamsInfoText(class AGASPGameState* ShooterGameState)
{
    if (ShooterGameState == nullptr) return FString();
    FString InfoTextString;

    const int32 RedTeamScore = ShooterGameState->RedTeamScore;
    const int32 BlueTeamScore = ShooterGameState->BlueTeamScore;

    if (RedTeamScore == 0 && BlueTeamScore == 0)
    {
        InfoTextString = Announcement::ThereIsNoWinner;
    }
    else if (RedTeamScore == BlueTeamScore)
    {
        InfoTextString = FString::Printf(TEXT("%s\n"), *Announcement::TeamsTiedForTheWin);
        InfoTextString.Append(Announcement::RedTeam);
        InfoTextString.Append(TEXT("\n"));
        InfoTextString.Append(Announcement::BlueTeam);
        InfoTextString.Append(TEXT("\n"));
    }
    else if (RedTeamScore > BlueTeamScore)
    {
        InfoTextString = Announcement::RedTeamWins;
        InfoTextString.Append(TEXT("\n"));
        InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::RedTeam, RedTeamScore));
        InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::BlueTeam, BlueTeamScore));
    }
    else if (BlueTeamScore > RedTeamScore)
    {
        InfoTextString = Announcement::BlueTeamWins;
        InfoTextString.Append(TEXT("\n"));
        InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::BlueTeam, BlueTeamScore));
        InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::RedTeam, RedTeamScore));
    }

    return InfoTextString;
}
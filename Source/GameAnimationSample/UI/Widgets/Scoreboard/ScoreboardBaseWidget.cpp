// Fill out your copyright notice in the Description page of Project Settings.


#include "ScoreboardBaseWidget.h"

#include "ScoreboardRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameAnimationSample/GameState/GASPGameState.h"
#include "GameAnimationSample/GameVariables/Team.h"
#include "GameAnimationSample/PlayerState/GASPArenaPlayerState.h"
#include "GameAnimationSample/PlayerState/GASPCorePlayerState.h"

void UScoreboardBaseWidget::Refresh()
{
	AGASPGameState* GS = GetWorld()->GetGameState<AGASPGameState>();
	if (!GS) return;

	ClearLists();

	if (PlayerListBox)
	{
		PopulateDeathmatch(GS);
	}
	else
	{
		PopulateTeams(GS);
	}
}

void UScoreboardBaseWidget::ClearLists()
{
	if (PlayerListBox) PlayerListBox->ClearChildren();
	if (RedTeamBox) RedTeamBox->ClearChildren();
	if (BlueTeamBox) BlueTeamBox->ClearChildren();
}

void UScoreboardBaseWidget::PopulateDeathmatch(AGASPGameState* GS)
{
	check(RowWidgetClass); // must be set in BP

	TArray<APlayerState*> Players = GS->PlayerArray;

	Players.Sort([](const APlayerState& A, const APlayerState& B)
	{
		return A.GetScore() > B.GetScore();
	});

	for (APlayerState* PS : Players)
	{
		AGASPArenaPlayerState* SPS = Cast<AGASPArenaPlayerState>(PS);
		if (!SPS) continue;

		auto* Row =
			CreateWidget<UScoreboardRowWidget>(GetWorld(), RowWidgetClass);

		Row->SetPlayerState(SPS);
		PlayerListBox->AddChild(Row);
	}
}

void UScoreboardBaseWidget::PopulateTeams(AGASPGameState* GS)
{
	check(RowWidgetClass);

	for (APlayerState* PS : GS->PlayerArray)
	{
		AGASPArenaPlayerState* SPS = Cast<AGASPArenaPlayerState>(PS);
		if (!SPS) continue;

		auto* Row =
			CreateWidget<UScoreboardRowWidget>(GetWorld(), RowWidgetClass);

		Row->SetPlayerState(SPS);

		if (SPS->GetTeam() == ETeam::ET_RedTeam)
			RedTeamBox->AddChild(Row);
		else
			BlueTeamBox->AddChild(Row);
	}
}

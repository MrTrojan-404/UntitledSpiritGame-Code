// Fill out your copyright notice in the Description page of Project Settings.


#include "ScoreboardRowWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "GameAnimationSample/PlayerState/GASPArenaPlayerState.h"
#include "GameAnimationSample/PlayerState/GASPCorePlayerState.h"

void UScoreboardRowWidget::SetPlayerState(AGASPCorePlayerState* InPlayerState)
{
	PlayerState = InPlayerState;
	UpdateRow();
}

void UScoreboardRowWidget::UpdateRow()
{
	if (!PlayerState) return;

	PlayerNameText->SetText(
		FText::FromString(PlayerState->GetPlayerName())
	);

	KillsText->SetText(
		FText::AsNumber(FMath::FloorToInt(PlayerState->GetScore()))
	);

	if (AGASPArenaPlayerState* ArenaState = Cast<AGASPArenaPlayerState>(PlayerState))
	{
		DeathsText->SetText(
		FText::AsNumber(ArenaState->GetDefeats()));
	}

	PingText->SetText(
		FText::AsNumber(PlayerState->GetPingInMilliseconds())
	);

	// highlight local player
	if (Background)
	{
		const bool bLocal =
			PlayerState->GetOwningController() &&
			PlayerState->GetOwningController()->IsLocalController();

		const float Alpha = bLocal ? 0.35f : 0.f;

		Background->SetBrushColor(
		FLinearColor(0.f, 0.5f, 1.f, Alpha)
		);
	}
}

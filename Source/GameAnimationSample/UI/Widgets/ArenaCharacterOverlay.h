#pragma once

#include "CoreMinimal.h"
#include "GameAnimationSample/UI/Widgets/CharacterOverlay.h"
#include "ArenaCharacterOverlay.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UArenaCharacterOverlay : public UCharacterOverlay
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ScoreAmount;
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* RedTeamScore;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* BlueTeamScore;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScoreSpacerText;
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DefeatsAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MatchCountdownText;

};
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameAnimationSample/Objective/Checkpoint/CheckpointTypes.h" // Update this path to match yours!
#include "GASPSaveGame.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UGASPSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// This is the master payload that gets written to your hard drive
	UPROPERTY(VisibleAnywhere, Category = "Save Data")
	FCheckpointSaveData SavedCheckpointData;
};
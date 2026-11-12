#pragma once

#include "CoreMinimal.h"
#include "GASPDialogueTypes.h"
#include "Engine/DataAsset.h"
#include "DialogueSequenceData.generated.h"

UCLASS(BlueprintType)
class GAMEANIMATIONSAMPLE_API UDialogueSequenceData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueSequence Sequence;
};
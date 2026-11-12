#pragma once

#include "CoreMinimal.h"
#include "GASPDialogueTypes.generated.h"

USTRUCT(BlueprintType)
struct FDialogueEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SubtitleText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Duration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	USoundBase* SoundCue = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bPositional = false;

};

USTRUCT(BlueprintType)
struct FDialogueSequence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FDialogueEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SequencePriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInterruptible = true;
};
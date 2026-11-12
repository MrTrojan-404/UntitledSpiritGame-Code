#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameAnimationSample/Dialogue/GASPDialogueTypes.h"
#include "DialogueSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueUpdated, const FDialogueEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueCleared);

UCLASS()
class GAMEANIMATIONSAMPLE_API UDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Call this from objectives, game mode, or anywhere
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void QueueSequence(const FDialogueSequence& Sequence, AActor* Speaker = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StopAll();

	// HUD binds to these
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueUpdated OnDialogueUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueCleared OnDialogueCleared;

private:
	// Queue sorted by priority
	TArray<FDialogueSequence> SequenceQueue;

	FDialogueSequence CurrentSequence;
	int32 CurrentEntryIndex = 0;
	bool bIsPlaying = false;

	UPROPERTY()
	UAudioComponent* ActiveAudioComponent = nullptr;

	FTimerHandle EntryTimerHandle;

	void PlayNextEntry();
	void OnEntryFinished();
	void TryInterrupt(const FDialogueSequence& Incoming);
	void StopCurrent();
	float GetEntryDuration(const FDialogueEntry& Entry) const;

	// Keeps queue sorted after insert
	void InsertSorted(const FDialogueSequence& Sequence);

	// Store the current speaker temporarily while the sequence plays
	UPROPERTY()
	AActor* CurrentSpeaker = nullptr;
};
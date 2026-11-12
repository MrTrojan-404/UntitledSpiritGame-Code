#include "DialogueSubsystem.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

void UDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bIsPlaying = false;
    CurrentEntryIndex = 0;
}

void UDialogueSubsystem::Deinitialize()
{
    StopAll();
    Super::Deinitialize();
}

void UDialogueSubsystem::QueueSequence(const FDialogueSequence& Sequence, AActor* Speaker)
{
    if (Sequence.Entries.Num() == 0) return;

    // Save the speaker
    CurrentSpeaker = Speaker;
    
    if (bIsPlaying)
    {
        // If incoming is higher priority and current is interruptible
        if (Sequence.SequencePriority > CurrentSequence.SequencePriority
            && CurrentSequence.bInterruptible)
        {
            TryInterrupt(Sequence);
            return;
        }
    }

    InsertSorted(Sequence);

    if (!bIsPlaying)
    {
        PlayNextEntry();
    }
}

void UDialogueSubsystem::TryInterrupt(const FDialogueSequence& Incoming)
{
    StopCurrent();
    CurrentSequence = Incoming;
    CurrentEntryIndex = 0;
    PlayNextEntry();
}

void UDialogueSubsystem::PlayNextEntry()
{
    // If current sequence is exhausted, move to next in queue
    if (CurrentEntryIndex >= CurrentSequence.Entries.Num())
    {
        if (SequenceQueue.Num() > 0)
        {
            CurrentSequence = SequenceQueue[0];
            SequenceQueue.RemoveAt(0);
            CurrentEntryIndex = 0;
        }
        else
        {
            // Nothing left
            bIsPlaying = false;
            OnDialogueCleared.Broadcast();
            return;
        }
    }

    bIsPlaying = true;
    const FDialogueEntry& Entry = CurrentSequence.Entries[CurrentEntryIndex];

    // Broadcast to HUD
    OnDialogueUpdated.Broadcast(Entry);

    // Handle audio
    if (Entry.SoundCue)
    {
        // Check our dynamically passed speaker instead of the DataAsset
        if (Entry.bPositional && CurrentSpeaker) 
        {
            ActiveAudioComponent = UGameplayStatics::SpawnSoundAttached(
                Entry.SoundCue,
                CurrentSpeaker->GetRootComponent()
            );
        }
        else
        {
            ActiveAudioComponent = UGameplayStatics::SpawnSound2D(
                GetGameInstance(),
                Entry.SoundCue
            );
        }
    }

    // Set timer for next entry
    const float Duration = GetEntryDuration(Entry);
    GetGameInstance()->GetTimerManager().SetTimer(
        EntryTimerHandle,
        this,
        &UDialogueSubsystem::OnEntryFinished,
        Duration,
        false
    );

    CurrentEntryIndex++;
}

void UDialogueSubsystem::OnEntryFinished()
{
    StopCurrent();
    PlayNextEntry();
}

void UDialogueSubsystem::StopCurrent()
{
    GetGameInstance()->GetTimerManager().ClearTimer(EntryTimerHandle);

    if (ActiveAudioComponent && ActiveAudioComponent->IsPlaying())
    {
        ActiveAudioComponent->Stop();
        ActiveAudioComponent = nullptr;
    }
}

void UDialogueSubsystem::StopAll()
{
    StopCurrent();
    SequenceQueue.Empty();
    bIsPlaying = false;
    CurrentEntryIndex = 0;
    OnDialogueCleared.Broadcast();
}

float UDialogueSubsystem::GetEntryDuration(const FDialogueEntry& Entry) const
{
    if (Entry.SoundCue)
    {
        const float SoundDuration = Entry.SoundCue->GetDuration();
        // GetDuration returns INDEFINITELY_LOOPING_DURATION for looping sounds
        // so guard against that
        if (SoundDuration > 0.f && SoundDuration < INDEFINITELY_LOOPING_DURATION)
        {
            return SoundDuration;
        }
    }
    return Entry.Duration;
}

void UDialogueSubsystem::InsertSorted(const FDialogueSequence& Sequence)
{
    // Insert in descending priority order
    int32 InsertIndex = SequenceQueue.Num();
    for (int32 i = 0; i < SequenceQueue.Num(); i++)
    {
        if (Sequence.SequencePriority > SequenceQueue[i].SequencePriority)
        {
            InsertIndex = i;
            break;
        }
    }
    SequenceQueue.Insert(Sequence, InsertIndex);
}
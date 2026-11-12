#include "ObjectiveActor.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Dialogue/DialogueSubsystem.h"
#include "GameAnimationSample/GameMode/GASPCoopGameMode.h"
#include "GameAnimationSample/Objective/ObjectiveSubsystem.h"

AObjectiveActor::AObjectiveActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    OverlapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapBox"));
    RootComponent = OverlapBox;
    OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OverlapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    OverlapBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AObjectiveActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AObjectiveActor, CurrentState);
    DOREPLIFETIME(AObjectiveActor, ConditionData);
}

void AObjectiveActor::BeginPlay()
{
    Super::BeginPlay();

    // Prevent Checkpoint TMap corruption by guaranteeing a unique ID!
    if (ObjectiveID == NAME_None)
    {
        // Auto-generates an ID like "ObjectiveActor_1" so the TMap never overwrites it.
        ObjectiveID = FName(*GetName());
    }

    // Initialize condition runtime data
    ConditionData.RemainingTime = ConditionData.TimeLimit;
    ConditionData.CurrentCount = 0;

    // Only server binds overlap
    if (HasAuthority() && bCompleteOnOverlap)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        OverlapBox->OnComponentBeginOverlap.AddDynamic(
            this, &AObjectiveActor::OnOverlapBegin);
    }

    if (HasAuthority() && bIsFirstObjective)
    {
        if (AGASPCoopGameMode* CoopMode = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode()))
        {
            CoopMode->RegisterObjectiveChain(this);
        }
    }
    if (HasAuthority())
    {
        if (UObjectiveSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
        {
            Subsystem->OnObjectiveEventReported.AddDynamic(this, &AObjectiveActor::HandleGlobalEvent);
        }
    }

    // Mark children so they don't hijack the UI pointer
    for (AObjectiveActor* Parallel : ParallelObjectives)
    {
        if (Parallel)
        {
            Parallel->bIsSubObjective = true;
        }
    }
    
    RegisterWithSubsystem();
}

void AObjectiveActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentState != EObjectiveState::Active) return;

    if (ConditionData.ConditionType == EObjectiveCondition::Timed_Survive ||
        ConditionData.ConditionType == EObjectiveCondition::Timed_Action)
    {
        // Both Server and Client run this so the UI countdown is smooth!
        ConditionData.RemainingTime -= DeltaTime;
        ConditionData.RemainingTime = FMath::Max(ConditionData.RemainingTime, 0.f);

        // ONLY the Server is allowed to make the actual Win/Loss decision
        if (HasAuthority() && ConditionData.RemainingTime <= 0.f)
        {
            if (ConditionData.ConditionType == EObjectiveCondition::Timed_Survive)
            {
                Complete();
            }
            else
            {
                Fail();
            }
        }
    }
}

void AObjectiveActor::ResetObjective(EObjectiveState TargetState, FObjectiveConditionData TargetCondition)
{
    if (!HasAuthority()) return;

    CurrentState = TargetState;
    ConditionData = TargetCondition;

    if (OverlapBox)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (TargetState == EObjectiveState::Active && bCompleteOnOverlap)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    RegisterWithSubsystem();

    // Force the UI to update locally on the Server Host whenever time is rewound!
    if (UObjectiveSubsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
    {
        if (TargetState == EObjectiveState::Active && ObjectiveType == EObjectiveType::Main && !bIsSubObjective)
        {
            Subsystem->CurrentMainObjectiveActor = this;
        }
        else if ((TargetState == EObjectiveState::Completed || TargetState == EObjectiveState::Failed) && Subsystem->CurrentMainObjectiveActor == this)
        {
            Subsystem->CurrentMainObjectiveActor = nullptr;
        }

        Subsystem->OnGlobalObjectiveStateChanged.Broadcast(this);
        Subsystem->OnGlobalObjectiveProgressChanged.Broadcast(this);
    }
}

#pragma region IObjectiveInterface

void AObjectiveActor::Activate()
{
    if (!HasAuthority()) return;
    if (CurrentState != EObjectiveState::Inactive) return;

    for (AObjectiveActor* Parallel : ParallelObjectives)
    {
        if (Parallel)
        {
            Parallel->bIsSubObjective = true;
        }
    }

    PlayDialogue(OnActivateDialogue);
    SetState(EObjectiveState::Active);

    // If a checkpoint previously put this objective to sleep, 
    // we MUST wake up its collision box now that it is active!
    if (bCompleteOnOverlap && OverlapBox)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    if (ParallelObjectives.Num() > 0)
    {
        if (bSequentialParallel)
        {
            ActivateNextParallel();
        }
        else
        {
            for (AObjectiveActor* Parallel : ParallelObjectives)
            {
                if (Parallel)
                {
                    Parallel->OnObjectiveStateChanged.AddDynamic(
                        this, &AObjectiveActor::OnParallelObjectiveChanged);
                    Parallel->Activate();
                }
            }
        }
    }
}

void AObjectiveActor::Complete()
{
    if (!HasAuthority()) return;
    if (CurrentState != EObjectiveState::Active) return;

    PlayDialogue(OnCompleteDialogue);
    SetState(EObjectiveState::Completed);

    // Disable overlap
    if (OverlapBox)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Activate next in chain
    if (NextObjective)
    {
        NextObjective->Activate();
    }
}

void AObjectiveActor::Fail()
{
    if (!HasAuthority()) return;
    if (CurrentState != EObjectiveState::Active) return;

    PlayDialogue(OnFailDialogue);
    SetState(EObjectiveState::Failed);

    if (OverlapBox)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
EObjectiveState AObjectiveActor::GetObjectiveState() const
{
    return CurrentState;
}

FObjectiveInfo AObjectiveActor::GetObjectiveInfo() const
{
    FObjectiveInfo Info;
    Info.ObjectiveID = ObjectiveID;
    Info.DisplayText = DisplayText;
    Info.ObjectiveType = ObjectiveType;
    Info.State = CurrentState;
    Info.WorldLocation = GetActorLocation();
    Info.ConditionData = ConditionData;
    return Info;
}

bool AObjectiveActor::IsOptional() const
{
    return ObjectiveType == EObjectiveType::Optional;
}


#pragma endregion IObjectiveInterface

#pragma region Conditions

void AObjectiveActor::NotifyObjectiveConditionMet()
{
    if (!HasAuthority()) return;
    if (CurrentState != EObjectiveState::Active) return;

    switch (ConditionData.ConditionType)
    {
    case EObjectiveCondition::External:
    case EObjectiveCondition::Overlap:
    case EObjectiveCondition::Timed_Action:
        Complete();
        break;

    case EObjectiveCondition::Counter:
        ConditionData.CurrentCount++;
        
        // Use the new delegate name!
        if (UObjectiveSubsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
        {
            Subsystem->OnGlobalObjectiveProgressChanged.Broadcast(this);
        }

        if (ConditionData.CurrentCount >= ConditionData.RequiredCount)
        {
            Complete();
        }
        break;

    case EObjectiveCondition::Timed_Survive:
        break; // Survive only completes when the clock runs out

    case EObjectiveCondition::Composite:
        CheckParallelComplete();
        break;
    }
}
#pragma endregion Conditions

#pragma region Overlap

void AObjectiveActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!Cast<AGASPPlayerCharacter>(OtherActor)) return;
    
    //  Allow both Overlap AND Timed_Action to trigger from physical overlap!
    if (ConditionData.ConditionType != EObjectiveCondition::Overlap && 
        ConditionData.ConditionType != EObjectiveCondition::Timed_Action) return;

    NotifyObjectiveConditionMet();
}

#pragma endregion Overlap

#pragma region Parallel

void AObjectiveActor::ActivateNextParallel()
{
    if (ActiveParallelIndex >= ParallelObjectives.Num())
    {
        // All sequential parallels done
        Complete();
        return;
    }

    AObjectiveActor* Next = ParallelObjectives[ActiveParallelIndex];
    if (Next)
    {
        Next->OnObjectiveStateChanged.AddDynamic(
            this, &AObjectiveActor::OnParallelObjectiveChanged);
        Next->Activate();
    }
}

void AObjectiveActor::OnParallelObjectiveChanged(
    AObjectiveActor* Objective, EObjectiveState NewState)
{
    if (NewState == EObjectiveState::Completed)
    {
        CompletedParallelCount++;

        if (bSequentialParallel)
        {
            ActiveParallelIndex++;
            ActivateNextParallel();
        }
        else
        {
            CheckParallelComplete();
        }
    }
    else if (NewState == EObjectiveState::Failed)
    {
        // If a required parallel fails, this objective fails
        if (!Objective->IsOptional())
        {
            Fail();
        }
    }
}

void AObjectiveActor::CheckParallelComplete()
{
    // Count required parallel objectives
    int32 RequiredCount = 0;
    int32 CompletedRequired = 0;

    for (AObjectiveActor* Parallel : ParallelObjectives)
    {
        if (!Parallel) continue;
        if (Parallel->IsOptional()) continue;

        RequiredCount++;
        if (Parallel->GetObjectiveState() == EObjectiveState::Completed)
        {
            CompletedRequired++;
        }
    }

    if (CompletedRequired >= RequiredCount)
    {
        Complete();
    }
}

#pragma endregion Parallel

#pragma region Replication

void AObjectiveActor::OnRep_ObjectiveState()
{
    // Keep this for your Waypoints/HUD
    RegisterWithSubsystem();

    // Make the Client do the exact same UI logic the Server does in SetState!
    if (UObjectiveSubsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
    {
        if (CurrentState == EObjectiveState::Active && ObjectiveType == EObjectiveType::Main && !bIsSubObjective)
        {
            Subsystem->CurrentMainObjectiveActor = this;
        }
        else if ((CurrentState == EObjectiveState::Completed || CurrentState == EObjectiveState::Failed) && Subsystem->CurrentMainObjectiveActor == this)
        {
            Subsystem->CurrentMainObjectiveActor = nullptr;
        }

        Subsystem->OnGlobalObjectiveStateChanged.Broadcast(this);
    }
}

void AObjectiveActor::Multicast_PlayDialogue_Implementation(
    UDialogueSequenceData* Sequence)
{
    if (!Sequence) return;

    UDialogueSubsystem* DialogueSubsystem = 
        GetGameInstance()->GetSubsystem<UDialogueSubsystem>();

    if (DialogueSubsystem)
    {
        // TPass 'this' as the speaker so the positional audio knows where to play
        DialogueSubsystem->QueueSequence(Sequence->Sequence, this);
    }
}
#pragma endregion Replication

#pragma region Helpers

void AObjectiveActor::SetState(EObjectiveState NewState)
{
    CurrentState = NewState;
    OnObjectiveStateChanged.Broadcast(this, NewState);

    if (UObjectiveSubsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
    {
        // THE FIX: Only steal the UI pointer if we are NOT a sub-objective!
        if (NewState == EObjectiveState::Active && ObjectiveType == EObjectiveType::Main && !bIsSubObjective)
        {
            Subsystem->CurrentMainObjectiveActor = this;
        }
        else if ((NewState == EObjectiveState::Completed || NewState == EObjectiveState::Failed) && Subsystem->CurrentMainObjectiveActor == this)
        {
            Subsystem->CurrentMainObjectiveActor = nullptr;
        }

        Subsystem->OnGlobalObjectiveStateChanged.Broadcast(this);
    }
}

void AObjectiveActor::PlayDialogue(UDialogueSequenceData* Sequence)
{
    if (!Sequence) return;
    Multicast_PlayDialogue(Sequence);
}

void AObjectiveActor::RegisterWithSubsystem()
{
    UObjectiveSubsystem* ObjectiveSubsystem =
        GetGameInstance()->GetSubsystem<UObjectiveSubsystem>();

    if (ObjectiveSubsystem)
    {
        ObjectiveSubsystem->UpdateObjective(GetObjectiveInfo());
    }
}

#pragma endregion Helpers


void AObjectiveActor::HandleGlobalEvent(FName EventTag)
{
    if (!HasAuthority()) return;
    if (CurrentState != EObjectiveState::Active) return;

    // Only Counters and Externals care about global events
    if (ConditionData.ConditionType == EObjectiveCondition::Counter ||
        ConditionData.ConditionType == EObjectiveCondition::External)
    {
        // If the shouted tag matches our required tag, Complete it!
        if (ConditionData.EventTag != NAME_None && ConditionData.EventTag == EventTag)
        {
            NotifyObjectiveConditionMet();
        }
    }
}

FString AObjectiveActor::GetProgressText() const
{
    if (ConditionData.ConditionType == EObjectiveCondition::Counter)
    {
        return FString::Printf(TEXT("%d / %d"), ConditionData.CurrentCount, ConditionData.RequiredCount);
    }
    return FString("");
}
FString AObjectiveActor::GetFormattedTasks() const
{
    FString Result = TEXT("");

    if (ConditionData.ConditionType == EObjectiveCondition::Composite)
    {
        FString MainCheckbox = (CurrentState == EObjectiveState::Completed) ? TEXT("[X]") : TEXT("[ ]");
        Result += FString::Printf(TEXT("%s %s\n"), *MainCheckbox, *DisplayText.ToString());

        for (int32 i = 0; i < ParallelObjectives.Num(); ++i)
        {
            AObjectiveActor* Parallel = ParallelObjectives[i];
            if (!Parallel) continue;

            //  If sequential, hide future tasks until it is their turn!
            if (bSequentialParallel && i > ActiveParallelIndex)
            {
                break; 
            }

            FString Checkbox = (Parallel->GetObjectiveState() == EObjectiveState::Completed) ? TEXT("[X]") : TEXT("[ ]");
            Result += FString::Printf(TEXT("   %s %s\n"), *Checkbox, *Parallel->DisplayText.ToString());
        }
        return Result.TrimEnd();
    }
    else
    {
        FString Checkbox = (CurrentState == EObjectiveState::Completed) ? TEXT("[X]") : TEXT("[ ]");
        Result = FString::Printf(TEXT("%s %s"), *Checkbox, *DisplayText.ToString());
        return Result;
    }
}
FString AObjectiveActor::GetTimerText() const
{
    //  Force the clock to 00:00 if the mission is over!
    if (CurrentState == EObjectiveState::Completed || CurrentState == EObjectiveState::Failed)
    {
        return TEXT("00:00");
    }

    int32 TotalSeconds = FMath::CeilToInt(ConditionData.RemainingTime);
    int32 Minutes = TotalSeconds / 60;
    int32 Seconds = TotalSeconds % 60;
    
    return FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
}


void AObjectiveActor::OnRep_ConditionData()
{
    // When the Server sends us new numbers (like 1/3 enemies killed), tell the Client UI to update!
    if (UObjectiveSubsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
    {
        Subsystem->OnGlobalObjectiveProgressChanged.Broadcast(this);
    }
}

void AObjectiveActor::SyncParallelStates()
{
    if (!HasAuthority()) return;

    ActiveParallelIndex = 0;
    CompletedParallelCount = 0;

    for (int32 i = 0; i < ParallelObjectives.Num(); ++i)
    {
        AObjectiveActor* Parallel = ParallelObjectives[i];
        if (!Parallel) continue;

        // If the checkpoint restored this child as Completed, count it!
        if (Parallel->GetObjectiveState() == EObjectiveState::Completed)
        {
            CompletedParallelCount++;
            
            if (bSequentialParallel)
            {
                // Shift the index to the next uncompleted objective
                ActiveParallelIndex = i + 1; 
            }
        }
    }
}
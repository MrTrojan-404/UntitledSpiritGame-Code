#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectiveInterface.h"
#include "ObjectiveTypes.h"
#include "GameAnimationSample/Dialogue/DialogueSequenceData.h"
#include "ObjectiveActor.generated.h"

class UBoxComponent;
class UDialogueSequenceData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveStateChanged, 
    AObjectiveActor*, Objective, 
    EObjectiveState, NewState);

UCLASS()
class GAMEANIMATIONSAMPLE_API AObjectiveActor : public AActor, 
    public IObjectiveInterface
{
    GENERATED_BODY()

public:
    AObjectiveActor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // IObjectiveInterface
    virtual void Activate() override;
    virtual void Complete() override;
    virtual void Fail() override;
    virtual EObjectiveState GetObjectiveState() const override;
    virtual FObjectiveInfo GetObjectiveInfo() const override;
    virtual bool IsOptional() const override;

    // Game mode listens to this
    UPROPERTY(BlueprintAssignable, Category = "Objective")
    FOnObjectiveStateChanged OnObjectiveStateChanged;

    // External actors call this to complete objective
    // e.g enemy killed, door opened
    UFUNCTION(BlueprintCallable, Category = "Objective")
    void NotifyObjectiveConditionMet();

    // Silent reset for checkpoint restore
    // No dialogue, no delegate broadcast
    void ResetObjective(EObjectiveState TargetState, FObjectiveConditionData TargetCondition);
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    bool bIsFirstObjective = false;

    UFUNCTION(BlueprintPure, Category = "Objective")
    FString GetProgressText() const;

    UFUNCTION(BlueprintPure, Category = "Objective|UI")
    FString GetTimerText() const;
    
    // Generate a multi-line checklist for the UI
    UFUNCTION(BlueprintPure, Category = "Objective|UI")
    FString GetFormattedTasks() const;

    // True if this objective is inside a Parallel array
    UPROPERTY(BlueprintReadOnly, Category = "Objective")
    bool bIsSubObjective = false;

    // Forces composite objectives to recount their progress
    void SyncParallelStates();
    
#pragma region Configuration

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FName ObjectiveID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FText MissionName;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FText DisplayText; // The specific task (e.g., "Kill Guards")

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    EObjectiveType ObjectiveType = EObjectiveType::Main;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    bool bCompleteOnOverlap = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ConditionData, Category = "Objective")
    FObjectiveConditionData ConditionData;

    // Next objective in chain
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Chain")
    AObjectiveActor* NextObjective = nullptr;

    // Parallel cluster
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Chain")
    TArray<AObjectiveActor*> ParallelObjectives;

    // If true parallel objectives must all complete in order
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Chain")
    bool bSequentialParallel = false;

#pragma endregion Configuration

#pragma region Dialogue

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Dialogue")
    UDialogueSequenceData* OnActivateDialogue = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Dialogue")
    UDialogueSequenceData* OnCompleteDialogue = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Dialogue")
    UDialogueSequenceData* OnFailDialogue = nullptr;

#pragma endregion Dialogue

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleGlobalEvent(FName EventTag);
    
private:

#pragma region Replication

    UPROPERTY(ReplicatedUsing = OnRep_ObjectiveState)
    EObjectiveState CurrentState = EObjectiveState::Inactive;

    UFUNCTION()
    void OnRep_ObjectiveState();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayDialogue(UDialogueSequenceData* Sequence);

#pragma endregion Replication

#pragma region SubObjectives

    // Tracks which parallel objective is active
    // for sequential parallel clusters
    int32 ActiveParallelIndex = 0;

    // How many parallel objectives completed
    int32 CompletedParallelCount = 0;

    void ActivateNextParallel();

    UFUNCTION()
    void OnParallelObjectiveChanged(AObjectiveActor* Objective,
        EObjectiveState NewState);
    void CheckParallelComplete();

#pragma endregion SubObjectives

    void SetState(EObjectiveState NewState);
    void PlayDialogue(UDialogueSequenceData* Sequence);
    void RegisterWithSubsystem();

    UPROPERTY(VisibleAnywhere, Category = "Objective")
    UBoxComponent* OverlapBox = nullptr;

    UFUNCTION()
    void OnRep_ConditionData();
};
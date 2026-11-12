#pragma once

#include "CoreMinimal.h"
#include "GASPCoreGameMode.h"
#include "GameAnimationSample/Objective/ObjectiveTypes.h"
#include "GameAnimationSample/Objective/Checkpoint/CheckpointTypes.h"
#include "GASPCoopGameMode.generated.h"

class AGASPBaseCharacter;
class AGASPCompanionCharacter;
class AObjectiveActor;
class ACheckpointActor;
class AGASPPlayerCharacter;
class UDownedComponent;

namespace CoopMatchState
{
    extern GAMEANIMATIONSAMPLE_API const FName MissionSuccess;
    extern GAMEANIMATIONSAMPLE_API const FName MissionFailed;
}

UENUM(BlueprintType)
enum class EClientJoinBehavior : uint8
{
    PossessCompanion  UMETA(DisplayName = "Possess Existing Companion (Story Mode)"),
    SpawnNewPlayer    UMETA(DisplayName = "Spawn New Character (Traditional Co-op)")
};

UCLASS()
class GAMEANIMATIONSAMPLE_API AGASPCoopGameMode : public AGASPCoreGameMode
{
    GENERATED_BODY()

public:
    AGASPCoopGameMode();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop|Rules")
    EClientJoinBehavior ClientJoinBehavior = EClientJoinBehavior::PossessCompanion;
    
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    void PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController) override;
    void RegisterSquadMember(AGASPPlayerCharacter* Character);

    void RegisterCompanion(AGASPCompanionCharacter* Companion);
    
    virtual float CalculateDamage(
        AController* Attacker,
        AController* Victim,
        float BaseDamage) override;

    // Called by UDownedComponent
    UFUNCTION()
    void OnPlayerBleedOut(AGASPBaseCharacter* DownedPlayer);

    // Called by UDownedComponent
    UFUNCTION()
    void OnPlayerRevived(AGASPBaseCharacter* RevivedPlayer);

    virtual void RequestRespawn(AGASPPlayerCharacter* ElimmedCharacter, AController* ElimmedController) override;
    
    // Called when any squad member bleed out expires
    UFUNCTION()
    void OnBleedOutExpired(AGASPBaseCharacter* DownedCharacter);
    
    // Called by AObjectiveActor delegate
    UFUNCTION()
    void OnObjectiveStateChanged(
        AObjectiveActor* Objective,
        EObjectiveState NewState);

    // Called by ACheckpointActor delegate
    UFUNCTION()
    void OnCheckpointActivated(ACheckpointActor* Checkpoint);

    void RegisterObjectiveChain(AObjectiveActor* First);

    TMap<FName, FObjectiveConditionData> BuildProgressSnapshot() const;

    // Manually force a checkpoint reload from the pause menu
    void ForceRestartCheckpoint();
    
#pragma region Configuration

    // Max squad size
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop")
    int32 MaxSquadSize = 4;

    // Friendly fire toggle
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop")
    bool bFriendlyFire = false;

    // Delay before restore after mission fail
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop")
    float MissionFailedRestoreDelay = 3.f;

#pragma endregion Configuration

    UPROPERTY()
    ACheckpointActor* LatestCheckpoint = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop|Rules")
    ECheckpointType CheckpointType = ECheckpointType::Snapshot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coop|Rules")
    ERespawnRule RespawnRule = ERespawnRule::SquadWipeOnly;
    
    // Tracks individual checkpoints for Borderlands style
    UPROPERTY()
    TMap<AController*, ACheckpointActor*> PlayerActiveCheckpoints;

    void SetLatestGlobalCheckpoint(ACheckpointActor* Checkpoint);
    void SetPlayerActiveCheckpoint(AController* Player, ACheckpointActor* Checkpoint);

    UFUNCTION(BlueprintCallable, Category = "Save System")
    void SaveGameToDisk();

    UFUNCTION(BlueprintCallable, Category = "Save System")
    void LoadGameFromDisk();

protected:
    virtual void BeginPlay() override;
    virtual void OnMatchStateSet() override;

    virtual void StartPlay() override;
    
private:

#pragma region Squad

    UPROPERTY()
    TArray<AGASPPlayerCharacter*> Squad;

    UPROPERTY()
    TArray<AGASPCompanionCharacter*> Companions;
    
    void UnregisterSquadMember(AGASPPlayerCharacter* Character);
    void BindDownedEvents(AGASPPlayerCharacter* Character);

    bool AreAllSquadMembersDowned() const;
    void CheckMissionFailCondition();

#pragma endregion Squad

#pragma region Objectives

    // All objectives in the chain — populated on BeginPlay
    UPROPERTY()
    TArray<AObjectiveActor*> AllObjectives;

    // Currently active objective
    UPROPERTY()
    AObjectiveActor* ActiveObjective = nullptr;

    void CheckMissionComplete();

    // Builds snapshot of all objective states
    TMap<FName, EObjectiveState> BuildObjectiveSnapshot() const;

#pragma endregion Objectives

#pragma region Checkpoints

    // All checkpoints sorted by index
    UPROPERTY()
    TArray<ACheckpointActor*> AllCheckpoints;

    void RegisterCheckpoints();
    void RestoreFromCheckpoint(bool bIsManualRestart = false);
    void ResetObjectivesToSnapshot(
        const TMap<FName, EObjectiveState>& Snapshot,
        const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
        FName ActiveObjectiveID);

#pragma endregion Checkpoints

#pragma region MissionState

    FTimerHandle MissionFailedRestoreTimer;

    void TriggerMissionSuccess();
    void TriggerMissionFailed();
    void ExecuteRestore();

    // Notify all clients of mission state
    // HUD listens via PlayerController
    void BroadcastMissionState(FName State);

#pragma endregion MissionState
    
};
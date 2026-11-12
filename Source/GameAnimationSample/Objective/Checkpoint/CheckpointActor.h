#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointTypes.h"
#include "GameAnimationSample/Interface/InteractionInterface.h"
#include "CheckpointActor.generated.h"

class UWidgetComponent;
class UBoxComponent;
class AGASPPlayerCharacter;
class AGASPCorePlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointActivated,
    ACheckpointActor*, Checkpoint);

UCLASS()
class GAMEANIMATIONSAMPLE_API ACheckpointActor : public AActor, public IInteractionInterface
{
    GENERATED_BODY()

public:
    ACheckpointActor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Called by GM or overlap
    void Activate();

    // Called by GM on mission fail
    void RestoreAllPlayers(bool bForceRespawn = false);

    // Called by GM after Activate()
    void SetObjectiveSnapshot(
         const TMap<FName, EObjectiveState>& Snapshot,
         const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
         FName ActiveObjID);

    // GM listens to this
    UPROPERTY(BlueprintAssignable, Category = "Checkpoint")
    FOnCheckpointActivated OnCheckpointActivated;

    FORCEINLINE int32 GetCheckpointIndex() const { return CheckpointIndex; }
    FORCEINLINE bool IsActivated() const { return bActivated; }
    FORCEINLINE FCheckpointSaveData GetSaveData() const { return SavedData; }

#pragma region InteractionInterface

    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
    
#pragma endregion InteractionInterface
    
#pragma region Configuration

    // If true, the checkpoint turns off permanently after the first touch.
    // If false, players can return here to update their save file infinitely.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bTriggerOnce = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    int32 CheckpointIndex = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bActivateOnOverlap = false;

    // Where all players restore to
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    FVector RestoreLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    FRotator RestoreRotation = FRotator::ZeroRotator;

#pragma endregion Configuration

    void RestoreIndividual(AController* ControllerToRestore);
    void RestoreIndividualCompanion(AGASPBaseCharacter* CompanionToRestore);

    void ActivateCheckpoint(AGASPBaseCharacter* TriggeringBody);

    // Allows the GameMode to inject the Hard Drive data into this actor
    void InjectHardDriveData(const FCheckpointSaveData& InData) { SavedData = InData; }
    
protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

private:
    UPROPERTY(VisibleAnywhere, Category = "Checkpoint")
    UBoxComponent* OverlapBox = nullptr;

    // ADD THIS: The physical mesh for the checkpoint
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* CheckpointMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    UWidgetComponent* InteractWidgetComponent = nullptr;
    
    // Server only — never replicated
    FCheckpointSaveData SavedData;

    void SaveSinglePlayer(AGASPBaseCharacter* PlayerBody);
    void SaveSingleCompanion(AGASPCompanionCharacter* Companion);
    bool IsSquadSafeToSave(ERespawnRule Rule, AGASPBaseCharacter* TriggeringBody) const;
    
    UPROPERTY(ReplicatedUsing = OnRep_Activated)
    bool bActivated = false;

    UFUNCTION()
    void OnRep_Activated();

    void SaveAllPlayers();
    
    FPlayerSaveData SavePlayer(AGASPBaseCharacter* Character, AGASPCorePlayerController* Controller);

    void RestorePlayer(const FPlayerSaveData& SaveData, bool bForceRespawn);

    // Notify clients checkpoint is activated
    // e.g play visual/sound effect
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnActivated();
    
    void SaveCompanions();
    
    FCompanionSaveData SaveCompanion(AGASPBaseCharacter* Companion);
    
    void RestoreCompanion(const FCompanionSaveData& SaveData, bool bForceRespawn);

    void SaveEnemies();
    void RestoreEnemies();

    void SaveWorldWeapons();
    void RestoreWorldWeapons();

    // --- REPLICATION & INDIVIDUAL TRACKING ---
    UPROPERTY(Replicated)
    ERespawnRule CachedRespawnRule = ERespawnRule::SquadWipeOnly;

    UPROPERTY()
    TArray<AController*> IndividualActivations;
    
};
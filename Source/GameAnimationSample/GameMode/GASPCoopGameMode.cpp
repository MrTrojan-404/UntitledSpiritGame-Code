#include "GASPCoopGameMode.h"

#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/Objective/ObjectiveActor.h"
#include "GameAnimationSample/Objective/ObjectiveSubsystem.h"
#include "GameAnimationSample/Objective/Checkpoint/CheckpointActor.h"
#include "GameAnimationSample/Objective/SaveGame/GASPSaveGame.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/UI/HUD/GASPHUD.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

namespace CoopMatchState
{
    const FName MissionSuccess = FName("MissionSuccess");
    const FName MissionFailed  = FName("MissionFailed");
}

AGASPCoopGameMode::AGASPCoopGameMode()
{
    
}

void AGASPCoopGameMode::BeginPlay()
{
    Super::BeginPlay();

    // DEVELOPER SAFEGUARD: Warn about illogical combinations!
    if (RespawnRule == ERespawnRule::IndividualRespawn && CheckpointType == ECheckpointType::Snapshot)
    {
        UE_LOG(LogTemp, Error, TEXT("GASPCoopGameMode: Snapshot checkpoints cannot be used with Individual Respawn! EXPECT BUGS."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("WARNING: Snapshot + Individual Respawn is an invalid combination!"));
        }
    }

    RegisterCheckpoints();
}

void AGASPCoopGameMode::StartPlay()
{
    Super::StartPlay();

    // Give the map 1 second to finish spawning all AI companions, then build the initial UI
    FTimerHandle BeginPlayRefreshTimer;
    GetWorldTimerManager().SetTimer(BeginPlayRefreshTimer, [this]()
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(It->Get()))
            {
                PC->Client_RefreshSquadUI();
            }
        }
    }, 1.0f, false);
}

#pragma region Login

void AGASPCoopGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // 1. Prevent the Server Host from hijacking a companion on launch!
    if (HasAuthority() && !NewPlayer->IsLocalController())
    {
        if (ClientJoinBehavior == EClientJoinBehavior::PossessCompanion)
        {
            TArray<AActor*> FoundCompanions;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPCompanionCharacter::StaticClass(), FoundCompanions);

            bool bFoundBody = false;

            for (AActor* Actor : FoundCompanions)
            {
                AGASPCompanionCharacter* Companion = Cast<AGASPCompanionCharacter>(Actor);
				
                // Ensure they are actually alive before putting the player in them!
                if (Companion && Companion->IsAlive())
                {
                    if (AAIController* AICon = Cast<AAIController>(Companion->GetController()))
                    {
                        APawn* DefaultPawn = NewPlayer->GetPawn();

                        // Brain Swap
                        AICon->UnPossess();
                        NewPlayer->Possess(Companion);
                        
                        if (DefaultPawn) DefaultPawn->Destroy(); 
                        
                        if (AGASPCorePlayerController* CorePC = Cast<AGASPCorePlayerController>(NewPlayer))
                        {
                            CorePC->Client_FinishCharacterSwap(Companion);
                        }
						
                        bFoundBody = true;
                        break; 
                    }
                }
            }

            //THE FALLBACK: NO BODIES LEFT
            if (!bFoundBody)
            {
                // 1. Destroy the default pawn the engine tried to give them
                if (APawn* DefaultPawn = NewPlayer->GetPawn())
                {
                    DefaultPawn->Destroy();
                }

                // 2. Force Unreal Engine to put them in Spectator Mode
                NewPlayer->StartSpectatingOnly();

                // 3. Snap their camera to look at the Host (Player 1)
                if (APlayerController* HostPC = GetWorld()->GetFirstPlayerController())
                {
                    if (APawn* HostPawn = HostPC->GetPawn())
                    {
                        NewPlayer->SetViewTarget(HostPawn);
                    }
                }

                // 4. Tell their UI they are spectating
                if (AGASPCorePlayerController* CorePC = Cast<AGASPCorePlayerController>(NewPlayer))
                {
                    CorePC->Client_EnterSpectatorMode();
                }
            }
        }
    }
    // Wait 1.5 seconds for the client to finish loading the map, then refresh everything
    FTimerHandle JoinRefreshTimer;
    GetWorldTimerManager().SetTimer(JoinRefreshTimer, [this]()
    {
        // Grab the Server's active objective
        AObjectiveActor* CurrentServerObjective = nullptr;
        if (UObjectiveSubsystem* ServerSubsystem = GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
        {
            CurrentServerObjective = ServerSubsystem->CurrentMainObjectiveActor;
        }

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(It->Get()))
            {
                // 1. Sync the Squad UI (We did this earlier)
                PC->Client_RefreshSquadUI();
				
                // 2. Sync the Mission Objective Tracker!
                if (CurrentServerObjective)
                {
                    PC->Client_SyncObjective(CurrentServerObjective);
                }
            }
        }
    }, 1.5f, false);
}

void AGASPCoopGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    AGASPBaseCharacter* AbandonedBody = Cast<AGASPBaseCharacter>(Exiting->GetPawn());
    if (AbandonedBody)
    {
        // 1. Remove them from the UI squad list if they were a Player Character
        if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(AbandonedBody))
        {
            UnregisterSquadMember(PC);
        }
        
        // 2. THE BRAIN SWAP: Give the body back to the AI!
        // Make sure the body is an ally and isn't dead
        if (AbandonedBody->IsPlayerAlly() && AbandonedBody->GetHealth() > 0.f)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            
            // Spawn a fresh AI Controller
            if (AGASPAIController* AICon = GetWorld()->SpawnActor<AGASPAIController>(AGASPAIController::StaticClass(), AbandonedBody->GetActorLocation(), AbandonedBody->GetActorRotation(), SpawnParams))
            {
                // Inject the AI into the abandoned body
                AICon->Possess(AbandonedBody);
            }
        }
    }
}
#pragma endregion Login

#pragma region Squad

void AGASPCorePlayerController::Client_EnterSpectatorMode_Implementation()
{
    // If you have a specific "You are Dead / Spectating" widget, create and show it here!
    // For now, we can just use the HUD's announcement system to warn them.
    if (GASPHUD)
    {
        GASPHUD->AddAnnouncement();
        // You can update your Announcement widget to say "WAITING FOR RESCUE..."
    }
}

void AGASPCoopGameMode::RegisterSquadMember(AGASPPlayerCharacter* Character)
{
    if (!Character) return;
    if (Squad.Num() >= MaxSquadSize) return;

    Squad.AddUnique(Character);
    BindDownedEvents(Character);
    
    for (AGASPPlayerCharacter* Member : Squad)
    {
        if (Member)
        {
            // We use a Client RPC or call a function that refreshes locally
            Member->RefreshHUDWithFullSquad();
        }
    }
}


void AGASPCoopGameMode::RegisterCompanion(AGASPCompanionCharacter* Companion)
{
    if (!Companion) return;
    Companions.AddUnique(Companion);
    
    // If your UDownedComponent is attached to the companion, bind it here!
    UDownedComponent* Downed = Companion->FindComponentByClass<UDownedComponent>();
    if (Downed)
    {
        Downed->OnBleedOutExpired.AddDynamic(this, &AGASPCoopGameMode::OnBleedOutExpired);
    }
}

void AGASPCoopGameMode::UnregisterSquadMember(
    AGASPPlayerCharacter* Character)
{
    if (!Character) return;
    Squad.Remove(Character);
}

void AGASPCoopGameMode::BindDownedEvents(AGASPPlayerCharacter* Character)
{
    UDownedComponent* Downed = Character->FindComponentByClass<UDownedComponent>();
    if (!Downed) return;

    Downed->OnPlayerDowned.AddDynamic(this, &AGASPCoopGameMode::OnPlayerBleedOut);
    
    Downed->OnBleedOutExpired.AddDynamic(this, &AGASPCoopGameMode::OnBleedOutExpired); 
    
    Downed->OnPlayerRevived.AddDynamic(this, &AGASPCoopGameMode::OnPlayerRevived);
}

void AGASPCoopGameMode::OnBleedOutExpired(AGASPBaseCharacter* DownedCharacter)
{
    CheckMissionFailCondition();

    if (RespawnRule == ERespawnRule::IndividualRespawn && Cast<AGASPCompanionCharacter>(DownedCharacter))
    {
        FTimerHandle RespawnTimer;
        GetWorldTimerManager().SetTimer(RespawnTimer, [this, DownedCharacter]()
        {
            // 1. Find the player this companion is tethered to
            AGASPPlayerCharacter* TetheredPlayer = nullptr;
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            {
                if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(It->Get()->GetPawn()))
                {
                    if (PC->ActiveCompanion == DownedCharacter)
                    {
                        TetheredPlayer = PC;
                        break;
                    }
                }
            }

            // 2. Teleport the companion to their player's specific checkpoint!
            ACheckpointActor* TargetCP = LatestCheckpoint;
            if (TetheredPlayer && TetheredPlayer->GetController() && PlayerActiveCheckpoints.Contains(TetheredPlayer->GetController()))
            {
                TargetCP = PlayerActiveCheckpoints[TetheredPlayer->GetController()];
            }

            if (TargetCP)
            {
                TargetCP->RestoreIndividualCompanion(DownedCharacter);
            }
        }, 5.0f, false);
    }
}

bool AGASPCoopGameMode::AreAllSquadMembersDowned() const
{
    bool bAllPlayersDowned = true;
    bool bAllCompanionsDowned = true;

    if (Squad.Num() == 0) bAllPlayersDowned = false; 
    
    // Check Players
    for (AGASPPlayerCharacter* Member : Squad)
    {
        if (!Member) continue;
        
        // Account for players who have already bled out and are "Dead"
        if (Member->IsElimmed()) continue; 
        
        UDownedComponent* Downed = Member->FindComponentByClass<UDownedComponent>();
        if (!Downed || !Downed->IsDowned()) 
        {
            bAllPlayersDowned = false; // Found an alive, standing player
            break;
        }
    }

    // Check Companions
    for (AGASPCompanionCharacter* Comp : Companions)
    {
        if (!Comp) continue;
        
        // Account for dissolved AI
        if (!Comp->IsAlive()) continue;
        
        UDownedComponent* Downed = Comp->FindComponentByClass<UDownedComponent>();
        if (!Downed || !Downed->IsDowned()) 
        {
            bAllCompanionsDowned = false; 
            break;
        }
    }
    
    if (Companions.Num() == 0) return bAllPlayersDowned;

    return bAllPlayersDowned && bAllCompanionsDowned; 
}

void AGASPCoopGameMode::RequestRespawn(AGASPPlayerCharacter* ElimmedCharacter, AController* ElimmedController)
{
    if (RespawnRule == ERespawnRule::IndividualRespawn)
    {
        ACheckpointActor* ActiveCP = PlayerActiveCheckpoints.Contains(ElimmedController) ? PlayerActiveCheckpoints[ElimmedController] : nullptr;
        
        if (ActiveCP)
        {
            ActiveCP->RestoreIndividual(ElimmedController);
        }
        else if (LatestCheckpoint) 
        {
            LatestCheckpoint->RestoreIndividual(ElimmedController);
        }
        else
        {
            //If they never touched a checkpoint, teleport them to the start safely WITHOUT destroying them!
            TArray<AActor*> PlayerStarts;
            UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
            if (PlayerStarts.Num() > 0 && ElimmedCharacter)
            {
                int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
                ElimmedCharacter->SetActorLocationAndRotation(PlayerStarts[Selection]->GetActorLocation(), PlayerStarts[Selection]->GetActorRotation());
                
                ElimmedCharacter->Multicast_ResetToAlive();
                ElimmedCharacter->SetHealth(ElimmedCharacter->GetMaxHealth());
                
                if (AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(ElimmedController))
                {
                    PC->SetHUDHealth(ElimmedCharacter->GetHealth(), ElimmedCharacter->GetMaxHealth());
                }
            }
        }
    }
    else
    {
        CheckMissionFailCondition();
    }
}

void AGASPCoopGameMode::CheckMissionFailCondition()
{
    // ONLY fail the mission if the Rule is set to Squad Wipe. 
    // Otherwise, they will just keep individually respawning forever!
    if (RespawnRule == ERespawnRule::SquadWipeOnly)
    {
        if (AreAllSquadMembersDowned())
        {
            TriggerMissionFailed();
        }
    }
}

void AGASPCoopGameMode::OnPlayerBleedOut(AGASPBaseCharacter* DownedCharacter)
{
    CheckMissionFailCondition();
}

void AGASPCoopGameMode::OnPlayerRevived(AGASPBaseCharacter* RevivedCharacter)
{
    // Nothing needed here for now
    // Hook exists for future use
    // e.g achievement tracking, dialogue trigger
}

#pragma endregion Squad

#pragma region PlayerEliminated

void AGASPCoopGameMode::PlayerEliminated(AGASPPlayerCharacter* ElimmedCharacter, class AGASPCorePlayerController* VictimController, class AGASPCorePlayerController* AttackerController)
{
    // Do NOT call Super — coop has no elim/respawn
    // Route to downed state instead
    if (!ElimmedCharacter) return;

    UDownedComponent* Downed =
        ElimmedCharacter->FindComponentByClass<UDownedComponent>();
    if (Downed)
    {
        Downed->GoDown();
    }
}

float AGASPCoopGameMode::CalculateDamage(
    AController* Attacker,
    AController* Victim,
    float BaseDamage)
{
    if (!bFriendlyFire)
    {
        // Check if both are players
        AGASPPlayerCharacter* AttackerChar = Attacker
            ? Cast<AGASPPlayerCharacter>(Attacker->GetPawn())
            : nullptr;
        AGASPPlayerCharacter* VictimChar = Victim
            ? Cast<AGASPPlayerCharacter>(Victim->GetPawn())
            : nullptr;

        // Both players — block damage
        if (AttackerChar && VictimChar)
        {
            return 0.f;
        }
    }

    return BaseDamage;
}

#pragma endregion PlayerEliminated

#pragma region Objectives

void AGASPCoopGameMode::RegisterObjectiveChain(
    AObjectiveActor* First)
{
    if (!First) return;

    AObjectiveActor* Current = First;
    while (Current)
    {
        AllObjectives.AddUnique(Current);

        // Bind state change delegate
        Current->OnObjectiveStateChanged.AddDynamic(
            this, &AGASPCoopGameMode::OnObjectiveStateChanged);

        // Walk parallel objectives too
        for (AObjectiveActor* Parallel : Current->ParallelObjectives)
        {
            if (Parallel)
            {
                AllObjectives.AddUnique(Parallel);
                Parallel->OnObjectiveStateChanged.AddDynamic(
                    this, &AGASPCoopGameMode::OnObjectiveStateChanged);
            }
        }

        Current = Current->NextObjective;
    }

    // Activate first objective
    First->Activate();
    ActiveObjective = First;
}

void AGASPCoopGameMode::OnObjectiveStateChanged(
    AObjectiveActor* Objective,
    EObjectiveState NewState)
{
    if (NewState == EObjectiveState::Active)
    {
        ActiveObjective = Objective;
    }
    else if (NewState == EObjectiveState::Completed)
    {
        CheckMissionComplete();
    }
    else if (NewState == EObjectiveState::Failed)
    {
        // Required objective failed
        if (!Objective->IsOptional())
        {
            TriggerMissionFailed();
        }
    }
}

void AGASPCoopGameMode::CheckMissionComplete()
{
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;
        if (Objective->IsOptional()) continue;

        if (Objective->GetObjectiveState() != 
            EObjectiveState::Completed)
        {
            // Required objective not yet complete
            return;
        }
    }

    TriggerMissionSuccess();
}

TMap<FName, EObjectiveState> 
AGASPCoopGameMode::BuildObjectiveSnapshot() const
{
    TMap<FName, EObjectiveState> Snapshot;
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;
        Snapshot.Emplace(
            Objective->ObjectiveID,
            Objective->GetObjectiveState());
    }
    return Snapshot;
}

#pragma endregion Objectives

#pragma region Checkpoints

void AGASPCoopGameMode::RegisterCheckpoints()
{
    TArray<AActor*> CheckpointActors;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(),
        ACheckpointActor::StaticClass(),
        CheckpointActors);

    for (AActor* Actor : CheckpointActors)
    {
        ACheckpointActor* Checkpoint = 
            Cast<ACheckpointActor>(Actor);
        if (!Checkpoint) continue;

        AllCheckpoints.Add(Checkpoint);

        Checkpoint->OnCheckpointActivated.AddDynamic(
            this, &AGASPCoopGameMode::OnCheckpointActivated);
    }

    // Sort by checkpoint index
    AllCheckpoints.Sort([](
        const ACheckpointActor& A,
        const ACheckpointActor& B)
    {
        return A.GetCheckpointIndex() < B.GetCheckpointIndex();
    });
}

void AGASPCoopGameMode::OnCheckpointActivated(ACheckpointActor* Checkpoint)
{
    if (!Checkpoint) return;
    if (LatestCheckpoint && Checkpoint->GetCheckpointIndex() < LatestCheckpoint->GetCheckpointIndex()) return;

    LatestCheckpoint = Checkpoint;

    Checkpoint->SetObjectiveSnapshot(
        BuildObjectiveSnapshot(),
        BuildProgressSnapshot(),
        ActiveObjective ? ActiveObjective->ObjectiveID : NAME_None
    );

    SaveGameToDisk();
}

void AGASPCoopGameMode::RestoreFromCheckpoint(bool bIsManualRestart)
{
    // 1. IF MEMORY IS EMPTY, TRY THE HARD DRIVE FIRST!
    if (!LatestCheckpoint || !LatestCheckpoint->GetSaveData().IsValid())
    {
        if (UGameplayStatics::DoesSaveGameExist(TEXT("MainSaveSlot"), 0))
        {
            UE_LOG(LogTemp, Warning, TEXT("Memory empty! Found save on disk. Loading..."));
            LoadGameFromDisk();
            return; // Stop here! LoadGameFromDisk() will automatically call ForceRestartCheckpoint() once memory is populated.
        }

        // 2. If NO save exists on the hard drive either, fallback to restarting the map.
        GetWorld()->ServerTravel("?Restart", false);
        return;
    }
    
    LatestCheckpoint->RestoreAllPlayers(bIsManualRestart);

    for (AGASPPlayerCharacter* Member : Squad)
    {
        if (!Member) continue;
        UDownedComponent* Downed = Member->FindComponentByClass<UDownedComponent>();
        if (Downed) Downed->Reset();
    }

    // The GameMode checks its own variable now
    if (CheckpointType == ECheckpointType::Snapshot)
    {
        const FCheckpointSaveData& SaveData = LatestCheckpoint->GetSaveData();
        ResetObjectivesToSnapshot(
            SaveData.ObjectiveStates, 
            SaveData.ObjectiveProgress, 
            SaveData.ActiveObjectiveID
        );
    }
}

void AGASPCoopGameMode::ResetObjectivesToSnapshot(
    const TMap<FName, EObjectiveState>& Snapshot,
    const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
    FName ActiveObjectiveID)
{
    AObjectiveActor* ObjectiveToActivate = nullptr;

    // FIRST PASS: Restore the raw state data to every objective
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;

        const EObjectiveState* SavedState = Snapshot.Find(Objective->ObjectiveID);
        const FObjectiveConditionData* SavedProgress = ProgressSnapshot.Find(Objective->ObjectiveID);

        if (!SavedState || !SavedProgress)
        {
            Objective->ResetObjective(EObjectiveState::Inactive, Objective->ConditionData);
            continue;
        }

        if (Objective->ObjectiveID == ActiveObjectiveID)
        {
            // Set the active one to Inactive first so that Activate() plays the dialogue!
            Objective->ResetObjective(EObjectiveState::Inactive, *SavedProgress);
            ObjectiveToActivate = Objective;
        }
        else
        {
            Objective->ResetObjective(*SavedState, *SavedProgress);
        }
    }

    // SECOND PASS! Force composite parents to recount their children!
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (Objective)
        {
            Objective->SyncParallelStates();
        }
    }

    // Finally, handle the main activation
    if (ObjectiveToActivate)
    {
        ObjectiveToActivate->Activate();
        ActiveObjective = ObjectiveToActivate;
    }
}

#pragma endregion Checkpoints

#pragma region MissionState

void AGASPCoopGameMode::TriggerMissionSuccess()
{
    SetMatchState(CoopMatchState::MissionSuccess);
    BroadcastMissionState(CoopMatchState::MissionSuccess);
}

void AGASPCoopGameMode::TriggerMissionFailed()
{
    SetMatchState(CoopMatchState::MissionFailed);
    BroadcastMissionState(CoopMatchState::MissionFailed);

    // Delay before restore — gives clients time to show fail screen
    GetWorldTimerManager().SetTimer(
        MissionFailedRestoreTimer,
        this,
        &AGASPCoopGameMode::ExecuteRestore,
        MissionFailedRestoreDelay,
        false
    );
}

void AGASPCoopGameMode::ExecuteRestore()
{
    RestoreFromCheckpoint();

    // Re-enable gameplay for all players
    for (AGASPPlayerCharacter* Member : Squad)
    {
        if (!Member) continue;
        Member->bDisableGameplay = false;
    }

    // Return to InProgress state
    SetMatchState(MatchState::InProgress);
    BroadcastMissionState(MatchState::InProgress);
}

void AGASPCoopGameMode::BroadcastMissionState(FName State)
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(*It);
        if (PC)
        {
            PC->ClientNotifyMissionState(State);
        }
    }
}

void AGASPCoopGameMode::OnMatchStateSet()
{
    Super::OnMatchStateSet();
    // Handle new coop states here if needed
    // HUD reactions handled via BroadcastMissionState
}

#pragma endregion MissionState

TMap<FName, FObjectiveConditionData> AGASPCoopGameMode::BuildProgressSnapshot() const
{
    TMap<FName, FObjectiveConditionData> Snapshot;
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;
        Snapshot.Emplace(Objective->ObjectiveID, Objective->ConditionData);
    }
    return Snapshot;
}

void AGASPCoopGameMode::ForceRestartCheckpoint()
{
    // Pass TRUE to explicitly force all healthy players to teleport and heal
    RestoreFromCheckpoint(true); 

    // Re-enable gameplay for all players instantly
    for (AGASPPlayerCharacter* Member : Squad)
    {
        if (!Member) continue;
        Member->bDisableGameplay = false;
    }

    // Return to InProgress state
    SetMatchState(MatchState::InProgress);
    BroadcastMissionState(MatchState::InProgress);
}
void AGASPCoopGameMode::SetLatestGlobalCheckpoint(ACheckpointActor* Checkpoint)
{
    LatestCheckpoint = Checkpoint;
    Checkpoint->SetObjectiveSnapshot(BuildObjectiveSnapshot(), BuildProgressSnapshot(), ActiveObjective ? ActiveObjective->ObjectiveID : NAME_None);
}

void AGASPCoopGameMode::SetPlayerActiveCheckpoint(AController* Player, ACheckpointActor* Checkpoint)
{
    if (Player)
    {
        PlayerActiveCheckpoints.Add(Player, Checkpoint);
    }
}

void AGASPCoopGameMode::SaveGameToDisk()
{
    if (!LatestCheckpoint) return;

    // 1. Create the Save Object
    if (UGASPSaveGame* SaveInst = Cast<UGASPSaveGame>(UGameplayStatics::CreateSaveGameObject(UGASPSaveGame::StaticClass())))
    {
        // 2. Copy the data from the Checkpoint Actor into the Save Object
        SaveInst->SavedCheckpointData = LatestCheckpoint->GetSaveData();

        // 3. Write it to the Hard Drive (We are using Slot 0 for now)
        UGameplayStatics::SaveGameToSlot(SaveInst, TEXT("MainSaveSlot"), 0);
        
        UE_LOG(LogTemp, Warning, TEXT("Game Saved to Hard Drive!"));
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Checkpoint Saved to Disk!"));
    }
}

void AGASPCoopGameMode::LoadGameFromDisk()
{
    // 1. Check if a save file actually exists on the hard drive
    if (UGameplayStatics::DoesSaveGameExist(TEXT("MainSaveSlot"), 0))
    {
        // 2. Load it into memory
        if (UGASPSaveGame* LoadedGame = Cast<UGASPSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("MainSaveSlot"), 0)))
        {
            // 3. Find the specific Checkpoint Actor in the level that matches our saved Index
            TArray<AActor*> Checkpoints;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACheckpointActor::StaticClass(), Checkpoints);

            for (AActor* Actor : Checkpoints)
            {
                if (ACheckpointActor* CP = Cast<ACheckpointActor>(Actor))
                {
                    if (CP->GetCheckpointIndex() == LoadedGame->SavedCheckpointData.CheckpointIndex)
                    {
                        LatestCheckpoint = CP;
                        
                        // 4. Inject the Hard Drive data back into the Level Actor!
                        // (Because SavedData is private, you might need to make a setter in CheckpointActor, or just make SavedData public!)
                        LatestCheckpoint->InjectHardDriveData(LoadedGame->SavedCheckpointData); 
                        
                        // 5. Execute the time-travel restore!
                        ForceRestartCheckpoint(); 
                        
                        UE_LOG(LogTemp, Warning, TEXT("Game Loaded from Hard Drive!"));
                        break;
                    }
                }
            }
        }
    }
}

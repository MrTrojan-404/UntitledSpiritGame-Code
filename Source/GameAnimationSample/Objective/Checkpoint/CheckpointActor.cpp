#include "CheckpointActor.h"

#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/Actor/Weapon.h"
#include "GameAnimationSample/AI/Character/GASPEnemyBase.h"
#include "GameAnimationSample/AI/Character/Companion/GASPCompanionCharacter.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/GameMode/GASPCoopGameMode.h"
#include "GameAnimationSample/Objective/ObjectiveTriggerComponent.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameAnimationSample/UI/Widgets/InteractWidget.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

ACheckpointActor::ACheckpointActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    OverlapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapBox"));
    RootComponent = OverlapBox;
    OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OverlapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    OverlapBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    CheckpointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CheckpointMesh"));
    CheckpointMesh->SetupAttachment(RootComponent);
    CheckpointMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    InteractWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractWidget"));
    InteractWidgetComponent->SetupAttachment(RootComponent);
    InteractWidgetComponent->SetVisibility(false);
    
    // Optional but highly recommended: Forces the widget to render crisply on the player's screen
    InteractWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
    InteractWidgetComponent->SetDrawAtDesiredSize(true);
}

void ACheckpointActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACheckpointActor, bActivated);
    DOREPLIFETIME(ACheckpointActor, CachedRespawnRule);
}

void ACheckpointActor::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode()))
        {
            CachedRespawnRule = GM->RespawnRule;
        }
    }

    if (HasAuthority() && bTriggerOnce)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        OverlapBox->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointActor::OnOverlapBegin);
    }
}

#pragma region InteractionInterface

void ACheckpointActor::Interact_Implementation(AActor* Interactor)
{
    if (!HasAuthority()) return;
    
    // Cast to BaseCharacter and ensure a Human is driving it!
    AGASPBaseCharacter* TriggeringBody = Cast<AGASPBaseCharacter>(Interactor);
    if (!TriggeringBody || !TriggeringBody->GetController() || !TriggeringBody->GetController()->IsPlayerController()) return;

    if (CachedRespawnRule == ERespawnRule::SquadWipeOnly && !TriggeringBody->IsLocallyControlled()) return;

    if (!bTriggerOnce)
    {
        if (!IsSquadSafeToSave(CachedRespawnRule, TriggeringBody))
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Cannot save! Someone is down!"));
            return;
        }

        ActivateCheckpoint(TriggeringBody);
    }
}

FText ACheckpointActor::GetInteractionText_Implementation() const
{
    if (CachedRespawnRule == ERespawnRule::SquadWipeOnly && !HasAuthority()) return FText::GetEmpty();

    if (!bTriggerOnce)
    {
        AGASPBaseCharacter* LocalBody = Cast<AGASPBaseCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
        
        if (!IsSquadSafeToSave(CachedRespawnRule, LocalBody))
        {
            return FText::FromString(TEXT("Cannot Save Now!")); 
        }
        return FText::FromString(TEXT("Save Game"));
    }
    
    return FText::GetEmpty();
}

#pragma endregion InteractionInterface

#pragma region Activation

void ACheckpointActor::Activate()
{
    if (!HasAuthority()) return;
    if (bActivated && bTriggerOnce) return;

    bActivated = true;

    SaveAllPlayers();
    SaveCompanions();
    SaveEnemies();
    SaveWorldWeapons();
    
    Multicast_OnActivated();
    OnCheckpointActivated.Broadcast(this);

    if (bTriggerOnce && OverlapBox)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ACheckpointActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Check for BaseCharacter driven by a Human
    AGASPBaseCharacter* TriggeringBody = Cast<AGASPBaseCharacter>(OtherActor);
    if (!TriggeringBody || !TriggeringBody->GetController() || !TriggeringBody->GetController()->IsPlayerController()) return;

    if (CachedRespawnRule == ERespawnRule::SquadWipeOnly && !TriggeringBody->IsLocallyControlled()) return;

    if (!IsSquadSafeToSave(CachedRespawnRule, TriggeringBody)) return;

    ActivateCheckpoint(TriggeringBody);
}

void ACheckpointActor::ActivateCheckpoint(AGASPBaseCharacter* TriggeringBody)
{
    if (!HasAuthority()) return;
    AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM || !TriggeringBody) return;

    if (CachedRespawnRule == ERespawnRule::SquadWipeOnly)
    {
        if (bActivated && bTriggerOnce) return;
        bActivated = true;

        SaveAllPlayers();
        SaveCompanions();
        if (GM->CheckpointType == ECheckpointType::Snapshot)
        {
            SaveEnemies();
            SaveWorldWeapons();
        }
        
        GM->SetLatestGlobalCheckpoint(this);
        Multicast_OnActivated();
        OnCheckpointActivated.Broadcast(this);

        if (bTriggerOnce && OverlapBox) OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    else
    {
        AController* PlayerController = TriggeringBody->GetController();
        
        if (bTriggerOnce && IndividualActivations.Contains(PlayerController)) return;
        IndividualActivations.AddUnique(PlayerController);

        SaveSinglePlayer(TriggeringBody);

        // --- FIND THE COMPANION ---
        // If the human is driving the companion, we have to search the world for the Original Player Body
        // so we know which AI to save as the "tethered" companion.
        AGASPPlayerCharacter* OriginalPlayer = Cast<AGASPPlayerCharacter>(TriggeringBody);
        
        if (!OriginalPlayer)
        {
            TArray<AActor*> Players;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPPlayerCharacter::StaticClass(), Players);
            for (AActor* Actor : Players)
            {
                if (AGASPPlayerCharacter* P = Cast<AGASPPlayerCharacter>(Actor))
                {
                    if (P->ActiveCompanion == TriggeringBody)
                    {
                        OriginalPlayer = P;
                        break;
                    }
                }
            }
        }

        if (OriginalPlayer && OriginalPlayer->ActiveCompanion)
        {
            SaveSingleCompanion(Cast<AGASPCompanionCharacter>(OriginalPlayer->ActiveCompanion));
        }

        GM->SetPlayerActiveCheckpoint(PlayerController, this);
        Multicast_OnActivated(); 
    }
    if (AGASPCoopGameMode* COOPGM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode()))
    {
        COOPGM->SaveGameToDisk();
    }
}

void ACheckpointActor::OnRep_Activated()
{
    // Clients react to checkpoint activation
    // e.g show UI feedback, play sound
    // Visual/audio hookup can be done in Blueprint
}

void ACheckpointActor::Multicast_OnActivated_Implementation()
{
    // Broadcast to Blueprint for VFX/SFX
    // e.g checkpoint glow, sound effect
}

#pragma endregion Activation

#pragma region Save

void ACheckpointActor::SaveAllPlayers()
{
    if (!HasAuthority()) return;

    SavedData.Players.Empty();
    SavedData.CheckpointIndex = CheckpointIndex;
    SavedData.RestoreLocation = GetActorLocation();
    SavedData.RestoreRotation = GetActorRotation();

    // Loop through Controllers, NOT Pawns. Find out what body the Human is driving!
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AGASPCorePlayerController* Controller = Cast<AGASPCorePlayerController>(It->Get()))
        {
            if (AGASPBaseCharacter* BaseChar = Cast<AGASPBaseCharacter>(Controller->GetPawn()))
            {
                SavedData.Players.Add(SavePlayer(BaseChar, Controller));
            }
        }
    }
}

FPlayerSaveData ACheckpointActor::SavePlayer(AGASPBaseCharacter* Character, AGASPCorePlayerController* Controller)
{
    FPlayerSaveData Data;
    Data.PlayerController = Controller;
    Data.PossessedBody = Character; // Link the specific body

    //Grab a unique string. If the PlayerState isn't ready, fallback to their Local Controller ID.
    if (Controller->PlayerState)
    {
        Data.PlayerIDString = Controller->PlayerState->GetUniqueId().ToString();
    }
    else
    {
        Data.PlayerIDString = FString::FromInt(UGameplayStatics::GetPlayerControllerID(Controller));
    }

    //Save the name of the physical body they are currently driving
    Data.SavedCharacterName = Character->CharacterName;
    
    Data.Health = Character->GetHealth();

    if (UDownedComponent* Downed = Character->FindComponentByClass<UDownedComponent>())
    {
        Data.bIsDowned = Downed->IsDowned();
    }

    UCombatComponent* Combat = Character->GetCombatComponent();
    if (!Combat) return Data;

    Data.GrenadeCount = Combat->GetGrenades();
    Data.CarriedAmmoMap = Combat->CarriedAmmoMap;

    for (AWeapon* Wep : Combat->WeaponInventory)
    {
        if (Wep)
        {
            FWeaponSaveData WData;
            WData.WeaponClass = Wep->GetClass();
            WData.CurrentAmmo = Wep->GetAmmo();
            Data.SavedInventory.Add(WData);
        }
    }

    if (AWeapon* Primary = Combat->GetPrimaryWeapon())
    {
        Data.bHasPrimary = true;
        Data.PrimaryWeapon.WeaponClass = Primary->GetClass();
        Data.PrimaryWeapon.CurrentAmmo = Primary->GetAmmo();
    }
    if (AWeapon* Secondary = Combat->GetSecondaryWeapon())
    {
        Data.bHasSecondary = true;
        Data.SecondaryWeapon.WeaponClass = Secondary->GetClass();
        Data.SecondaryWeapon.CurrentAmmo = Secondary->GetAmmo();
    }

    return Data;
}

void ACheckpointActor::SetObjectiveSnapshot(
    const TMap<FName, EObjectiveState>& Snapshot,
    const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
    FName ActiveObjID)
{
    if (!HasAuthority()) return;
    SavedData.ObjectiveStates = Snapshot;
    SavedData.ObjectiveProgress = ProgressSnapshot; // Store the progress
    SavedData.ActiveObjectiveID = ActiveObjID;
}

void ACheckpointActor::SaveCompanions()
{
    if (!HasAuthority()) return;
    SavedData.Companions.Empty();

    // Find ALL characters in the world
    TArray<AActor*> FoundAllies;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPBaseCharacter::StaticClass(), FoundAllies);

    for (AActor* Actor : FoundAllies)
    {
        if (AGASPBaseCharacter* Ally = Cast<AGASPBaseCharacter>(Actor))
        {
            // Only save them if they are an ally (PlayerCharacter or CompanionCharacter)
            if (Ally->IsPlayerAlly())
            {
                // SKIP the body that the human player is currently driving, we already saved it!
                if (Ally->GetController() && Ally->GetController()->IsPlayerController()) continue;

                SavedData.Companions.Add(SaveCompanion(Ally));
            }
        }
    }
}

FCompanionSaveData ACheckpointActor::SaveCompanion(AGASPBaseCharacter* Companion)
{
    FCompanionSaveData Data;
    Data.CompanionActor = Companion;

    //Save their Name so we can find them on reboot
    if (AGASPCompanionCharacter* CompChar = Cast<AGASPCompanionCharacter>(Companion))
    {
        Data.CompanionNameString = CompChar->CompanionName;
    }
    
    Data.Health = Companion->GetHealth();

    if (UDownedComponent* Downed = Companion->FindComponentByClass<UDownedComponent>())
    {
        Data.bIsDowned = Downed->IsDowned();
    }

    UCombatComponent* Combat = Companion->GetCombatComponent();
    if (!Combat) return Data;

    Data.CarriedAmmoMap = Combat->CarriedAmmoMap;

    for (AWeapon* Wep : Combat->WeaponInventory)
    {
        if (Wep)
        {
            FWeaponSaveData WData;
            WData.WeaponClass = Wep->GetClass();
            WData.CurrentAmmo = Wep->GetAmmo();
            Data.SavedInventory.Add(WData);
        }
    }

    if (AWeapon* Primary = Combat->GetPrimaryWeapon())
    {
        Data.bHasPrimary = true;
        Data.PrimaryWeapon.WeaponClass = Primary->GetClass();
        Data.PrimaryWeapon.CurrentAmmo = Primary->GetAmmo();
    }
    if (AWeapon* Secondary = Combat->GetSecondaryWeapon())
    {
        Data.bHasSecondary = true;
        Data.SecondaryWeapon.WeaponClass = Secondary->GetClass();
        Data.SecondaryWeapon.CurrentAmmo = Secondary->GetAmmo();
    }

    return Data;
}

void ACheckpointActor::SaveEnemies()
{
    if (!HasAuthority()) return;
    SavedData.Enemies.Empty();

    TArray<AActor*> FoundEnemies;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPEnemyBase::StaticClass(), FoundEnemies);

    for (AActor* Actor : FoundEnemies)
    {
        if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(Actor))
        {
            FEnemySaveData EData;
            EData.EnemyClass = Enemy->GetClass();
            EData.Location = Enemy->GetActorLocation();
            EData.Rotation = Enemy->GetActorRotation();
            EData.PatrolOrigin = Enemy->PatrolOrigin;
            EData.Health = Enemy->GetHealth();
            EData.bWasAlive = Enemy->IsAlive(); 

            // Record EXACTLY what gun they are holding right now
            if (UCombatComponent* Combat = Enemy->GetCombatComponent())
            {
                if (AWeapon* Primary = Combat->GetPrimaryWeapon())
                {
                    EData.EquippedWeaponClass = Primary->GetClass();
                }
            }

            if (Enemy->ObjectiveTrigger)
            {
                EData.EventTag = Enemy->ObjectiveTrigger->EventTag;
            }

            //Save the path reference
            EData.WaypointPath = Enemy->WaypointPath;

            SavedData.Enemies.Add(EData);
        }
    }
}

void ACheckpointActor::SaveWorldWeapons()
{
    SavedData.WorldWeapons.Empty();
    TArray<AActor*> FoundWeapons;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWeapon::StaticClass(), FoundWeapons);

    for (AActor* Actor : FoundWeapons)
    {
        AWeapon* Weapon = Cast<AWeapon>(Actor);
        // If a weapon has no Owner, it means it is dropped on the floor OR pre-placed!
        if (Weapon && Weapon->GetOwner() == nullptr)
        {
            FWorldWeaponSaveData WData;
            WData.WeaponClass = Weapon->GetClass();
            WData.Location = Weapon->GetActorLocation();
            WData.Rotation = Weapon->GetActorRotation();
            WData.CurrentAmmo = Weapon->GetAmmo();
            
            // Capture the actual physical state of the weapon
            WData.SavedState = Weapon->GetWeaponState(); 
            
            SavedData.WorldWeapons.Add(WData);
        }
    }
}

void ACheckpointActor::SaveSinglePlayer(AGASPBaseCharacter* PlayerBody)
{
    AGASPCorePlayerController* PC = Cast<AGASPCorePlayerController>(PlayerBody->GetController());
    if (!PC) return;
    
    FPlayerSaveData NewData = SavePlayer(PlayerBody, PC);
    
    bool bFound = false;
    for (int32 i = 0; i < SavedData.Players.Num(); ++i)
    {
        if (SavedData.Players[i].PlayerController == PC)
        {
            SavedData.Players[i] = NewData;
            bFound = true;
            break;
        }
    }
    if (!bFound) SavedData.Players.Add(NewData);

    SavedData.RestoreLocation = GetActorLocation();
    SavedData.RestoreRotation = GetActorRotation();
}

void ACheckpointActor::SaveSingleCompanion(AGASPCompanionCharacter* Companion)
{
    if (!Companion) return;
    FCompanionSaveData NewData = SaveCompanion(Companion);

    bool bFound = false;
    for (int32 i = 0; i < SavedData.Companions.Num(); ++i)
    {
        if (SavedData.Companions[i].CompanionActor == Companion)
        {
            SavedData.Companions[i] = NewData;
            bFound = true;
            break;
        }
    }
    if (!bFound) SavedData.Companions.Add(NewData);
}

#pragma endregion Save

#pragma region Restore

void ACheckpointActor::RestoreAllPlayers(bool bForceRespawn)
{
    if (!HasAuthority() || !SavedData.IsValid()) return;
    AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    for (const FPlayerSaveData& PlayerData : SavedData.Players)
    {
        RestorePlayer(PlayerData, bForceRespawn); 
    }

    for (const FCompanionSaveData& CompData : SavedData.Companions)
    {
        RestoreCompanion(CompData, bForceRespawn);
    }

    if (GM->CheckpointType == ECheckpointType::Snapshot)
    {
        RestoreEnemies();
        RestoreWorldWeapons(); 
    }
}
void ACheckpointActor::RestorePlayer(const FPlayerSaveData& SaveData, bool bForceRespawn)
{
    //  Re-link the pointers from the hard drive string if they are null!
    AGASPCorePlayerController* TargetPC = SaveData.PlayerController;
    
    if (!TargetPC) 
    {
        // Search the world for the controller that matches our saved ID
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            AGASPCorePlayerController* FoundPC = Cast<AGASPCorePlayerController>(It->Get());
            if (FoundPC && FoundPC->PlayerState)
            {
                if (FoundPC->PlayerState->GetUniqueId().ToString() == SaveData.PlayerIDString)
                {
                    TargetPC = FoundPC;
                    break;
                }
            }
        }
    }

    if (!TargetPC) return; // If we still can't find them, abort.

    AGASPBaseCharacter* Character = SaveData.PossessedBody ? SaveData.PossessedBody : Cast<AGASPBaseCharacter>(TargetPC->GetPawn());
    if (!Character) return;

    if (!SaveData.PlayerController || !SaveData.PossessedBody) return;
    AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;
    
    // --- THE TIME REWIND BRAIN SWAP ---
    if (SaveData.PlayerController->GetPawn() != Character)
    {
        APawn* WrongBody = SaveData.PlayerController->GetPawn();
        AAIController* AICon = Cast<AAIController>(Character->GetController());
        
        if (AICon) AICon->UnPossess();
        SaveData.PlayerController->UnPossess();
        
        SaveData.PlayerController->Possess(Character);
        SaveData.PlayerController->Client_FinishCharacterSwap(Character);
        
        if (AICon && WrongBody) AICon->Possess(WrongBody);
    }

    // INTELLIGENT RESPAWN FILTER
    bool bNeedsRespawn = true;
    bool bCurrentlyDowned = false;

    if (UDownedComponent* Downed = Character->FindComponentByClass<UDownedComponent>())
    {
        bCurrentlyDowned = Downed->IsDowned();
    }

    // Check life state based on the specific body type
    bool bIsDead = false;
    if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(Character))
    {
        bIsDead = PC->IsElimmed();
    }
    else if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(Character))
    {
        bIsDead = !AIChar->IsAlive();
    }
    else
    {
        // Fallback for any future base characters
        bIsDead = Character->GetHealth() <= 0.f; 
    }

    // If it's a persistent world, ONLY teleport players who are dead or bleeding out!
    if (GM->CheckpointType == ECheckpointType::Persistent && !bForceRespawn)
    {
        if (!bCurrentlyDowned && !bIsDead)
        {
            bNeedsRespawn = false;
        }
    }

    if (bNeedsRespawn)
    {
        // 1. Teleport
        FVector SpawnOffset = FVector(FMath::RandRange(-100.f, 100.f), FMath::RandRange(-100.f, 100.f), 50.f);
        Character->SetActorLocationAndRotation(SavedData.RestoreLocation + SpawnOffset, SavedData.RestoreRotation);
        
        if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(Character))
        {
            PC->Multicast_ResetToAlive();
        }
        Character->bDisableGameplay = false;

        // 2. Clear Downed Status
        if (UDownedComponent* Downed = Character->FindComponentByClass<UDownedComponent>())
        {
            Downed->Reset(); 
        }

        // 3. Handle Health based on Checkpoint Type
        if (GM->CheckpointType == ECheckpointType::Persistent)
        {
            // Full heal for "New-U" style reconstruction
            Character->SetHealth(Character->GetMaxHealth());
        }
        else
        {
            // Snapshot restore
            const float MinHealth = Character->GetMaxHealth() * 0.25f;
            Character->SetHealth(FMath::Max(SaveData.Health, MinHealth));
        }

        SaveData.PlayerController->SetHUDHealth(Character->GetHealth(), Character->GetMaxHealth());
    }

    // --- INVENTORY REWIND (SNAPSHOT ONLY) ---
    if (GM->CheckpointType == ECheckpointType::Snapshot)
    {
        UCombatComponent* Combat = Character->GetCombatComponent();
        if (!Combat) return;

        Combat->Grenades = SaveData.GrenadeCount;
        Combat->UpdateHUDGrenades();
        Combat->CarriedAmmoMap = SaveData.CarriedAmmoMap;

        for (AWeapon* OldWep : Combat->WeaponInventory)
        {
            if (OldWep) OldWep->Destroy();
        }
        Combat->WeaponInventory.Empty();
        Combat->ClearWeaponPointers();

        AWeapon* RestoredPrimary = nullptr;
        AWeapon* RestoredSecondary = nullptr;

        for (const FWeaponSaveData& WData : SaveData.SavedInventory)
        {
            if (!WData.WeaponClass.IsNull())
            {
                UClass* ResolvedClass = WData.WeaponClass.LoadSynchronous();
                if (!ResolvedClass) continue; 
        
                AWeapon* NewWep = GetWorld()->SpawnActor<AWeapon>(ResolvedClass, SavedData.RestoreLocation, SavedData.RestoreRotation);
                if (NewWep)
                {
                    NewWep->AddAmmo(WData.CurrentAmmo);
                    NewWep->SetWeaponState(EWeaponState::EWS_Stored);
                    Combat->AddWeaponToInventory(NewWep);

                    if (SaveData.bHasPrimary && WData.WeaponClass == SaveData.PrimaryWeapon.WeaponClass && !RestoredPrimary)
                        RestoredPrimary = NewWep;
                    else if (SaveData.bHasSecondary && WData.WeaponClass == SaveData.SecondaryWeapon.WeaponClass && !RestoredSecondary)
                        RestoredSecondary = NewWep;
                }
            }
        }

        if (RestoredPrimary) Combat->EquipWeapon(RestoredPrimary); 
        if (RestoredSecondary) 
        {
            Combat->EquipWeapon(RestoredSecondary);
        }

        if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(Character)) PC->UpdateHUDAmmo();
    }
}

void ACheckpointActor::RestoreWorldWeapons()
{
    TArray<AActor*> FoundWeapons;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWeapon::StaticClass(), FoundWeapons);
    
    // 1. Wipe the floor clean of ALL dropped weapons
    for (AActor* Actor : FoundWeapons)
    {
        AWeapon* Weapon = Cast<AWeapon>(Actor);
        if (Weapon && Weapon->GetOwner() == nullptr)
        {
            Weapon->Destroy();
        }
    }

    // 2. Respawn the weapons exactly as they were
    for (const FWorldWeaponSaveData& WData : SavedData.WorldWeapons)
    {
        if (!WData.WeaponClass.IsNull())
        {
            UClass* ResolvedClass = WData.WeaponClass.LoadSynchronous();
            if (!ResolvedClass) continue;

            AWeapon* RestoredWeapon = GetWorld()->SpawnActor<AWeapon>(ResolvedClass, WData.Location, WData.Rotation);
            
            if (RestoredWeapon)
            {
                RestoredWeapon->AddAmmo(WData.CurrentAmmo);
                
                // Set it to the exact state it was in when saved!
                // If it was Initial, it will stay static. If it was Dropped, physics turn on.
                RestoredWeapon->SetWeaponState(WData.SavedState); 
            }
        }
    }
}

void ACheckpointActor::RestoreCompanion(const FCompanionSaveData& SaveData, bool bForceRespawn)
{
    AGASPBaseCharacter* Companion = SaveData.CompanionActor;

    // THE FIX: Re-link the Companion from the hard drive string!
    if (!Companion)
    {
        TArray<AActor*> FoundAllies;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPCompanionCharacter::StaticClass(), FoundAllies);
        for (AActor* Actor : FoundAllies)
        {
            if (AGASPCompanionCharacter* CompChar = Cast<AGASPCompanionCharacter>(Actor))
            {
                if (CompChar->CompanionName == SaveData.CompanionNameString)
                {
                    Companion = CompChar;
                    break;
                }
            }
        }
    }
    
    if (!Companion) return;
    AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    if (Companion->GetController() && Companion->GetController()->IsPlayerController()) return;

    // INTELLIGENT RESPAWN FILTER
    bool bNeedsRespawn = true;
    bool bCurrentlyDowned = false;

    if (UDownedComponent* Downed = Companion->FindComponentByClass<UDownedComponent>())
    {
        bCurrentlyDowned = Downed->IsDowned();
    }

    if (GM->CheckpointType == ECheckpointType::Persistent && !bForceRespawn)
    {
        bool bIsEliminated = true;
        if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(Companion))
        {
            bIsEliminated = !AIChar->IsAlive();
        }

        if (!bCurrentlyDowned && !bIsEliminated)
        {
            bNeedsRespawn = false;
        }
    }

    if (bNeedsRespawn)
    {
        FVector SpawnOffset = FVector(FMath::RandRange(-100.f, 100.f), FMath::RandRange(-100.f, 100.f), 50.f);
        Companion->SetActorLocationAndRotation(SavedData.RestoreLocation + SpawnOffset, SavedData.RestoreRotation);

        Companion->bDisableGameplay = false;

        if (UDownedComponent* Downed = Companion->FindComponentByClass<UDownedComponent>())
        {
            Downed->Reset();
        }

        if (GM->CheckpointType == ECheckpointType::Persistent)
        {
            Companion->SetHealth(Companion->GetMaxHealth());
        }
        else
        {
            const float MinHealth = Companion->GetMaxHealth() * 0.25f;
            Companion->SetHealth(FMath::Max(SaveData.Health, MinHealth));
        }
    }

    // --- INVENTORY REWIND (SNAPSHOT ONLY) ---
    if (GM->CheckpointType == ECheckpointType::Snapshot)
    {
        if (UCombatComponent* Combat = Companion->GetCombatComponent())
        {
            Combat->CarriedAmmoMap = SaveData.CarriedAmmoMap;

            for (AWeapon* OldWep : Combat->WeaponInventory)
            {
                if (OldWep) OldWep->Destroy();
            }
            Combat->WeaponInventory.Empty();
            Combat->ClearWeaponPointers();

            AWeapon* RestoredPrimary = nullptr;
            AWeapon* RestoredSecondary = nullptr;

            for (const FWeaponSaveData& WData : SaveData.SavedInventory)
            {
                if (!WData.WeaponClass.IsNull()) 
                {
                    UClass* ResolvedClass = WData.WeaponClass.LoadSynchronous();
                    if (!ResolvedClass) continue; 
        
                    AWeapon* NewWep = GetWorld()->SpawnActor<AWeapon>(ResolvedClass, SavedData.RestoreLocation, SavedData.RestoreRotation);
                    if (NewWep)
                    {
                        NewWep->AddAmmo(WData.CurrentAmmo);
                        NewWep->SetWeaponState(EWeaponState::EWS_Stored);
                        Combat->AddWeaponToInventory(NewWep);

                        if (SaveData.bHasPrimary && WData.WeaponClass == SaveData.PrimaryWeapon.WeaponClass && !RestoredPrimary)
                            RestoredPrimary = NewWep;
                        else if (SaveData.bHasSecondary && WData.WeaponClass == SaveData.SecondaryWeapon.WeaponClass && !RestoredSecondary)
                            RestoredSecondary = NewWep;
                    }
                }
            }

            if (RestoredPrimary) Combat->EquipWeapon(RestoredPrimary);
            if (RestoredSecondary) 
            {
                Combat->EquipWeapon(RestoredSecondary);
            }
        }
    }
}

void ACheckpointActor::RestoreEnemies()
{
    if (!HasAuthority()) return;

    // 1. Wipe the board clean
    TArray<AActor*> ExistingEnemies;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPEnemyBase::StaticClass(), ExistingEnemies);
    
    for (AActor* Actor : ExistingEnemies)
    {
        if (AGASPEnemyBase* Enemy = Cast<AGASPEnemyBase>(Actor))
        {
            // Tell the enemy it is being deleted by time-travel so it doesn't trigger objectives!
            Enemy->bIsResetting = true;

            // Force destroy all weapons the enemy is holding 
            if (UCombatComponent* Combat = Enemy->GetCombatComponent())
            {
                for (AWeapon* Wep : Combat->WeaponInventory)
                {
                    if (Wep) Wep->Destroy(); 
                }
                Combat->WeaponInventory.Empty();
                Combat->ClearWeaponPointers();
            }
        }
        
        Actor->Destroy(); // Now it's safe to destroy the enemy body
    }
    
    for (const FEnemySaveData& EData : SavedData.Enemies)
    {
        if (EData.bWasAlive && !EData.EnemyClass.IsNull()) 
        {
            UClass* ResolvedEnemyClass = EData.EnemyClass.LoadSynchronous();
            if (!ResolvedEnemyClass) continue;
            // DEFERRED SPAWN: This halts the Enemy's "BeginPlay" so we can inject variables into it first!
            AGASPEnemyBase* RespawnedEnemy = GetWorld()->SpawnActorDeferred<AGASPEnemyBase>(
                        ResolvedEnemyClass, FTransform(EData.Rotation, EData.Location));
            
            if (RespawnedEnemy)
            {
                // Force them to hold the exact gun they had, overriding any random spawners
                if (!EData.EquippedWeaponClass.IsNull())
                {
                    RespawnedEnemy->DefaultWeaponClass = EData.EquippedWeaponClass.LoadSynchronous();
                }

                RespawnedEnemy->SetHealth(EData.Health);
                RespawnedEnemy->PatrolOrigin = EData.PatrolOrigin;
                RespawnedEnemy->bIsResetting = false; 

                if (RespawnedEnemy->ObjectiveTrigger)
                {
                    RespawnedEnemy->ObjectiveTrigger->EventTag = EData.EventTag;
                }

                // Hand the path back to the enemy before it wakes up
                RespawnedEnemy->WaypointPath = EData.WaypointPath;

                // NOW we allow the Enemy to run its BeginPlay!
                UGameplayStatics::FinishSpawningActor(RespawnedEnemy, FTransform(EData.Rotation, EData.Location));
            }
        }
    }
}

void ACheckpointActor::RestoreIndividual(AController* ControllerToRestore)
{
    if (!HasAuthority() || !ControllerToRestore) return;

    // Search the Snapshot for this exact human player
    for (const FPlayerSaveData& PlayerData : SavedData.Players)
    {
        if (PlayerData.PlayerController == ControllerToRestore)
        {
            RestorePlayer(PlayerData, false); 
            return;
        }
    }
}
void ACheckpointActor::RestoreIndividualCompanion(AGASPBaseCharacter* CompanionToRestore)
{
    if (!HasAuthority() || !CompanionToRestore) return;

    // Search the Snapshot for this exact AI companion
    for (const FCompanionSaveData& CompData : SavedData.Companions)
    {
        if (CompData.CompanionActor == CompanionToRestore)
        {
            RestoreCompanion(CompData, false); 
            return;
        }
    }
}

#pragma endregion Restore

bool ACheckpointActor::IsSquadSafeToSave(ERespawnRule Rule, AGASPBaseCharacter* TriggeringBody) const
{
    if (Rule == ERespawnRule::IndividualRespawn)
    {
        if (!TriggeringBody) return true; 

        // Check if the body we are currently inside of is dead
        bool bIsDead = false;
        if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(TriggeringBody)) bIsDead = PC->IsElimmed();
        else if (AGASPAICharacterBase* AIChar = Cast<AGASPAICharacterBase>(TriggeringBody)) bIsDead = !AIChar->IsAlive();
        else bIsDead = TriggeringBody->GetHealth() <= 0.f;

        if (bIsDead) return false;
        
        if (UDownedComponent* Downed = TriggeringBody->FindComponentByClass<UDownedComponent>())
        {
            if (Downed->IsDowned()) return false;
        }
        
        // Find our "Partner" body (either the AI driving our original body, or our companion)
        AGASPPlayerCharacter* OriginalPlayer = Cast<AGASPPlayerCharacter>(TriggeringBody);
        if (!OriginalPlayer)
        {
            TArray<AActor*> Players;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPPlayerCharacter::StaticClass(), Players);
            for (AActor* Actor : Players)
            {
                if (AGASPPlayerCharacter* P = Cast<AGASPPlayerCharacter>(Actor))
                {
                    if (P->ActiveCompanion == TriggeringBody)
                    {
                        OriginalPlayer = P;
                        break;
                    }
                }
            }
        }

        if (OriginalPlayer && OriginalPlayer->ActiveCompanion)
        {
            if (!OriginalPlayer->ActiveCompanion->IsAlive()) return false;
            if (UDownedComponent* Downed = OriginalPlayer->ActiveCompanion->FindComponentByClass<UDownedComponent>())
            {
                if (Downed->IsDowned()) return false;
            }
        }
        return true; 
    }

    // Squad Wipe logic stays the same (It checks everyone anyway)
    TArray<AActor*> Players;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPPlayerCharacter::StaticClass(), Players);
    for (AActor* Actor : Players)
    {
        if (AGASPPlayerCharacter* Player = Cast<AGASPPlayerCharacter>(Actor))
        {
            if (Player->IsElimmed()) return false;
            if (UDownedComponent* Downed = Player->FindComponentByClass<UDownedComponent>())
            {
                if (Downed->IsDowned()) return false;
            }
        }
    }

    TArray<AActor*> Companions;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGASPCompanionCharacter::StaticClass(), Companions);
    for (AActor* Actor : Companions)
    {
        if (AGASPCompanionCharacter* Companion = Cast<AGASPCompanionCharacter>(Actor))
        {
            if (!Companion->IsAlive()) return false;
            if (UDownedComponent* Downed = Companion->FindComponentByClass<UDownedComponent>())
            {
                if (Downed->IsDowned()) return false;
            }
        }
    }
    return true;
}

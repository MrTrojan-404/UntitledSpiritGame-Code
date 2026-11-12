#include "DownedComponent.h"

#include "BrainComponent.h"
#include "CombatComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameAnimationSample/GameAnimationSample.h"
#include "GameAnimationSample/AI/Controller/GASPAIController.h"
#include "Net/UnrealNetwork.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/GameMode/GASPCoopGameMode.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "Kismet/GameplayStatics.h"

UDownedComponent::UDownedComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UDownedComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UDownedComponent, bIsDowned);
    DOREPLIFETIME(UDownedComponent, DownCount);
    DOREPLIFETIME(UDownedComponent, BleedOutTimeRemaining);
    DOREPLIFETIME(UDownedComponent, bIsBeingRevived);
    DOREPLIFETIME(UDownedComponent, ReviveProgress);
}
void UDownedComponent::BeginPlay()
{
    Super::BeginPlay();

    // The component now owns the Base Character!
    OwnerCharacter = Cast<AGASPBaseCharacter>(GetOwner());
    
    if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(OwnerCharacter))
    {
        OwnerController = Cast<AGASPCorePlayerController>(PC->GetController());
    }
}

void UDownedComponent::TickComponent(float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!HasAuthority()) return;
    if (!bIsDowned) return;

    // Bleed out pauses during revive
    if (bIsBeingRevived)
    {
        // ==========================================================
        // === NEW FIX: THE DISTANCE/TELEPATHY CHECK ===
        // ==========================================================
        if (CurrentReviver)
        {
            float Dist = FVector::Dist(OwnerCharacter->GetActorLocation(), CurrentReviver->GetActorLocation());
            if (Dist > 250.f) // Max revive distance
            {
                CancelRevive();
                return;
            }
        }
        else
        {
            CancelRevive();
            return;
        }
        // ==========================================================

        // Tick revive progress
        ReviveProgress += DeltaTime;
        const float ReviveTime = CalculateReviveTime();
        const float NormalizedProgress = FMath::Clamp(ReviveProgress / ReviveTime, 0.f, 1.f);

        Client_UpdateReviveProgress(NormalizedProgress);

        if (ReviveProgress >= ReviveTime)
        {
            ExitDownedState(CalculateReviveHealth());
        }
        return;
    }

    // Tick bleed out
    BleedOutTimeRemaining -= DeltaTime;
    BleedOutTimeRemaining = FMath::Max(BleedOutTimeRemaining, 0.f);

    if (BleedOutTimeRemaining <= 0.f)
    {
        // THE MAGIC FIX: We are officially dead. Stop the Downed state so this only fires ONCE!
        bIsDowned = false; 
        
        HandleBleedOut();
    }
}

void UDownedComponent::Reset()
{
    if (!HasAuthority()) return;

    bIsDowned = false;
    bIsBeingRevived = false;
    ReviveProgress = 0.f;
    CurrentReviver = nullptr;
    BleedOutTimeRemaining = 0.f;

    // Notify clients
    // OnRep_IsDowned fires automatically via replication
    // since bIsDowned changed
}

#pragma region GoDown

void UDownedComponent::GoDown()
{
    if (!HasAuthority()) return;
    if (bIsDowned) return;

    DownCount++;
    bIsDowned = true;
    bIsBeingRevived = false;
    ReviveProgress = 0.f;
    BleedOutTimeRemaining = CalculateBleedOutTime();

    EnterDownedState();

    OnPlayerDowned.Broadcast(OwnerCharacter);
}

void UDownedComponent::EnterDownedState()
{
    Multicast_EnterDowned();
}

void UDownedComponent::Multicast_EnterDowned_Implementation()
{
    if (!OwnerCharacter) return;

    // 1. Lock gameplay for EVERYONE (Player or Companion body)
    OwnerCharacter->bDisableGameplay = true;

    // 2. Stop the AI Brain if it has one
    if (AGASPAIController* AICon = Cast<AGASPAIController>(OwnerCharacter->GetController()))
    {
        if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(AICon->GetBrainComponent()))
        {
            BTComp->StopTree();
        }
    }

    // 3. Force the gun down LOCALLY so the sniper scope actually disappears on the client's screen!
    if (UCombatComponent* Combat = OwnerCharacter->GetCombatComponent())
    {
        Combat->HandleFire(false);
        Combat->SetAiming(false);
    }
}

#pragma endregion GoDown

#pragma region Revive

void UDownedComponent::StartRevive(AGASPBaseCharacter* Reviver)
{
    if (!HasAuthority()) return;
    if (!bIsDowned) return;
    if (bIsBeingRevived) return;

    CurrentReviver = Reviver;
    bIsBeingRevived = true;
    ReviveProgress = 0.f;
}

void UDownedComponent::CancelRevive()
{
    if (!HasAuthority()) return;

    bIsBeingRevived = false;
    ReviveProgress = 0.f;
    CurrentReviver = nullptr;

    // Notify HUD revive cancelled
    Client_UpdateReviveProgress(0.f);
}

void UDownedComponent::ExitDownedState(float RestoreHealth)
{
    bIsDowned = false;
    bIsBeingRevived = false;
    ReviveProgress = 0.f;
    CurrentReviver = nullptr;

    Multicast_ExitDowned(RestoreHealth);
    OnPlayerRevived.Broadcast(OwnerCharacter);

    if (HasAuthority() && OwnerCharacter)
    {
        if (AGASPCoopGameMode* GM = Cast<AGASPCoopGameMode>(GetWorld()->GetAuthGameMode()))
        {
            // Look for any connected player currently stuck in Spectator mode
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            {
                if (APlayerController* PC = It->Get())
                {
                    if (PC->GetStateName() == NAME_Spectating)
                    {
                        // We found the spectator! Rip the AI out and give the body to the player.
                        if (AAIController* AICon = Cast<AAIController>(OwnerCharacter->GetController()))
                        {
                            AICon->UnPossess();
                        }
                        
                        PC->Possess(OwnerCharacter);
                        
                        // Update their HUD to remove the "Waiting for Rescue" screen
                        if (AGASPCorePlayerController* CorePC = Cast<AGASPCorePlayerController>(PC))
                        {
                            CorePC->Client_FinishCharacterSwap(OwnerCharacter);
                        }
                        break; // Stop looking once we assign the body
                    }
                }
            }
        }
    }
}

void UDownedComponent::Multicast_ExitDowned_Implementation(
    float RestoreHealth)
{
    if (!OwnerCharacter) return;

    // Stop ragdoll
    OwnerCharacter->CharacterMesh->SetAllBodiesSimulatePhysics(false);
    OwnerCharacter->CharacterMesh->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);
    OwnerCharacter->CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    OwnerCharacter->CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    OwnerCharacter->CharacterMesh->SetCollisionObjectType(ECC_SkeletalMesh);
    OwnerCharacter->CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    OwnerCharacter->CharacterMesh->bPauseAnims = false;

    // Re-enable capsule
    OwnerCharacter->CapsuleComponent->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics);

    // Restore health
    OwnerCharacter->SetHealth(RestoreHealth);

    OwnerCharacter->bDisableGameplay = false;

    // Player vs AI specific restores
    if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(OwnerCharacter))
    {
        PC->UpdateHUDHealth();
        if (PC->IsLocallyControlled() && OwnerController)
        {
            OwnerController->SetViewTarget(PC);
        }
    }
    else if (AGASPAIController* AICon = Cast<AGASPAIController>(OwnerCharacter->GetController()))
    {
        // Cast to UBehaviorTreeComponent 
        if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(AICon->GetBrainComponent()))
        {
            BTComp->RestartTree();
        }
    }
}

void UDownedComponent::HandleBleedOut()
{
    OnBleedOutExpired.Broadcast(OwnerCharacter); 

    if (OwnerCharacter && HasAuthority())
    {
        if (AGASPPlayerCharacter* PC = Cast<AGASPPlayerCharacter>(OwnerCharacter))
        {
            PC->Elim(false); // Player Elim
        }
        else
        {
            UGameplayStatics::ApplyDamage(OwnerCharacter, 9999.f, nullptr, nullptr, nullptr);
        }
    }
}

#pragma endregion Revive

#pragma region IInteractionInterface

void UDownedComponent::Interact_Implementation(AActor* Interactor)
{
    AGASPBaseCharacter* Reviver = 
        Cast<AGASPBaseCharacter>(Interactor);
    if (!Reviver) return;

    if (bIsBeingRevived && CurrentReviver == Reviver)
    {
        // Already being revived by this player
        return;
    }

    StartRevive(Reviver);
}

FText UDownedComponent::GetInteractionText_Implementation() const
{
    if (bIsBeingRevived)
    {
        // 1. We are actively being revived! Calculate the live countdown:
        float TimeLeft = FMath::Max(0.f, CalculateReviveTime() - ReviveProgress);
        
        // Output formatting: "Reviving [4.2s]"
        return FText::FromString(FString::Printf(TEXT("Reviving [%.1fs]"), TimeLeft));
    }
    else
    {
        // 2. We are just lying on the floor. Show the bleedout timer!
        int32 BleedTime = FMath::CeilToInt(BleedOutTimeRemaining);
        
        // Output formatting: "Revive (Bleeding out: 45s)"
        return FText::FromString(FString::Printf(TEXT("Revive (Bleeding out: %ds)"), BleedTime));
    }
}

#pragma endregion IInteractionInterface

#pragma region Helpers

float UDownedComponent::CalculateBleedOutTime() const
{
    const float Reduced = BaseBleedOutTime - 
        (BleedOutReductionPerDown * (DownCount - 1));
    return FMath::Max(Reduced, MinBleedOutTime);
}

float UDownedComponent::CalculateReviveTime() const
{
    const float Increased = BaseReviveTime +
        (ReviveTimeIncreasePerDown * (DownCount - 1));
    return FMath::Min(Increased, MaxReviveTime);
}

float UDownedComponent::CalculateReviveHealth() const
{
    if (!OwnerCharacter) return 1.f;

    const float HealthPercent = BaseReviveHealth -
        (ReviveHealthReductionPerDown * (DownCount - 1));

    const float MaxHealth = OwnerCharacter->GetMaxHealth();
    const float Restored = MaxHealth * HealthPercent;

    // Minimum 1hp
    return FMath::Max(Restored, 1.f);
}

void UDownedComponent::OnRep_IsDowned()
{
    // Clients react to downed state change via multicast
    // OnRep just used as safety net for late joiners
}

void UDownedComponent::Client_UpdateReviveProgress_Implementation(
    float Progress)
{
    OnReviveProgress.Broadcast(Progress);
}

#pragma endregion Helpers
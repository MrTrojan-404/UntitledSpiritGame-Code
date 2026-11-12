#include "BTTask_PerformRevive.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameAnimationSample/Character/GASPBaseCharacter.h"
#include "GameAnimationSample/Component/DownedComponent.h"
#include "GameAnimationSample/Interface/CombatInterface.h"

UBTTask_PerformRevive::UBTTask_PerformRevive()
{
    NodeName = "Perform Revive";
    bNotifyTick = true; // Allows TickTask to run
}

EBTNodeResult::Type UBTTask_PerformRevive::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetToReviveKey.SelectedKeyName));
    AGASPBaseCharacter* AICharacter = Cast<AGASPBaseCharacter>(OwnerComp.GetAIOwner()->GetPawn());

    if (!TargetActor || !AICharacter) return EBTNodeResult::Failed;

    UDownedComponent* DownedComp = TargetActor->FindComponentByClass<UDownedComponent>();
    if (!DownedComp || !DownedComp->IsDowned()) return EBTNodeResult::Failed; // They died or got picked up already

    // USING MESH LOCATION FOR EXACT BODY DISTANCE
    float Distance = FVector::Dist(AICharacter->GetCombatMesh_Implementation()->GetComponentLocation(), TargetActor->GetActorLocation());
    if (Distance > 250.f)
    {
        // We are too far away! Fail the task so the Behavior Tree tries to Move To them again.
        return EBTNodeResult::Failed;
    }

    // 1. Play the AI Revive Animation (You can trigger your montage here via your CombatInterface)
    // ICombatInterface::Execute_PlayInteractMontage(AICharacter);

    // 2. Start the Revive!
    DownedComp->Interact_Implementation(AICharacter);

    // Keep the task running until they wake up
    return EBTNodeResult::InProgress; 
}

void UBTTask_PerformRevive::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetToReviveKey.SelectedKeyName));
    
    if (TargetActor)
    {
        UDownedComponent* DownedComp = TargetActor->FindComponentByClass<UDownedComponent>();
        
        // If they are no longer downed (Revived or Dead), the task is a success and we can move on!
        if (DownedComp && !DownedComp->IsDowned())
        {
            // Wipe the memory and auto-reset their order
            OwnerComp.GetBlackboardComponent()->ClearValue(TargetToReviveKey.SelectedKeyName);
            
            // Force them to go back to "Follow" mode (0) so they follow the player, not the revived guy!
            OwnerComp.GetBlackboardComponent()->SetValueAsInt(FName("CompanionOrder"), 0);

            FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        }
    }
    else
    {
        // Safe fallback just in case the target actor was completely destroyed
        OwnerComp.GetBlackboardComponent()->ClearValue(TargetToReviveKey.SelectedKeyName);
        OwnerComp.GetBlackboardComponent()->SetValueAsInt(FName("CompanionOrder"), 0);
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
    }
}

EBTNodeResult::Type UBTTask_PerformRevive::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // If the AI gets shot or commanded to do something else, safely cancel the revive!
    AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetToReviveKey.SelectedKeyName));
    
    if (TargetActor)
    {
        UDownedComponent* DownedComp = TargetActor->FindComponentByClass<UDownedComponent>();
        if (DownedComp)
        {
            DownedComp->CancelRevive();
        }
    }

    // If the player interrupted them with a new "Move To" order, wipe the revive target so they don't oscillate
    int32 CurrentOrder = OwnerComp.GetBlackboardComponent()->GetValueAsInt(FName("CompanionOrder"));
    if (CurrentOrder != 5) // 5 = ReviveAlly
    {
        OwnerComp.GetBlackboardComponent()->ClearValue(TargetToReviveKey.SelectedKeyName);
    }

    return Super::AbortTask(OwnerComp, NodeMemory);
}
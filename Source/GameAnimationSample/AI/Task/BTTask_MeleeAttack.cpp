#include "BTTask_MeleeAttack.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameAnimationSample/AI/Character/GASPMeleeEnemy.h"
#include "GameAnimationSample/Component/CombatComponent.h"
#include "GameAnimationSample/GameVariables/CombatState.h"

UBTTask_MeleeAttack::UBTTask_MeleeAttack()
{
    NodeName = TEXT("Melee Attack");

    TargetActorKey.AddObjectFilter(
        this,
        GET_MEMBER_NAME_CHECKED(
            UBTTask_MeleeAttack, TargetActorKey),
        AActor::StaticClass());
}

FString UBTTask_MeleeAttack::GetStaticDescription() const
{
    return FString::Printf(
        TEXT("Performs melee attack within %.0f units."),
        AttackRange);
}

EBTNodeResult::Type UBTTask_MeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!Controller) return EBTNodeResult::Failed;

    AGASPAICharacterBase* AICharacter = Cast<AGASPAICharacterBase>(Controller->GetPawn());
    if (!AICharacter) return EBTNodeResult::Failed;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    if (!Target) return EBTNodeResult::Failed;

    const float Distance = FVector::Dist(AICharacter->GetActorLocation(), Target->GetActorLocation());
    if (Distance > AttackRange)
    {
        return EBTNodeResult::Failed;
    }

    UCombatComponent* Combat = AICharacter->GetCombatComponent();
    if (!Combat || Combat->GetCombatState() != ECombatState::ECS_Unoccupied) 
    {
        return EBTNodeResult::Failed; // Don't interrupt if already attacking/reloading
    }

    // 1. Face target
    FVector Direction = (Target->GetActorLocation() - AICharacter->GetActorLocation()).GetSafeNormal();
    Controller->SetControlRotation(Direction.Rotation());

    // 2. Trigger the Universal Melee!
    Combat->Melee();

    // 3. Cache and Wait
    CachedEnemy = AICharacter;
    CachedOwnerComp = &OwnerComp;

    // Wait for the animation to finish before letting the AI move again
    AICharacter->GetWorldTimerManager().SetTimer(
        AttackTimer,
        this,
        &UBTTask_MeleeAttack::OnAttackTimerFired,
        AttackDuration,
        false
    );

    return EBTNodeResult::InProgress;
}

void UBTTask_MeleeAttack::OnAttackTimerFired()
{
    // We no longer call PerformMeleeAttack() here, because the AnimNotify handles the hit!
    // We just finish the node so the AI can move again.
    if (CachedOwnerComp)
    {
        FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
        CachedOwnerComp = nullptr;
    }
    CachedEnemy = nullptr;
}

EBTNodeResult::Type UBTTask_MeleeAttack::AbortTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    if (CachedEnemy)
    {
        CachedEnemy->GetWorldTimerManager()
            .ClearTimer(AttackTimer);
        CachedEnemy = nullptr;
    }
    CachedOwnerComp = nullptr;
    return EBTNodeResult::Aborted;
}